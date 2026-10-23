#include "joycon.h"
#include "joycon_descriptor.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// ---------- constants ---------- //

#define PAIRING_NEUTRAL_MS 1000
#define PAIRING_SL_SR_PRESS_MS 400
#define PAIRING_SL_SR_RELEASE_MS 300
#define PAIRING_SL_SR_PULSES 4
#define PAIRING_A_PRESS_MS 700
#define TAP_REPORT_COUNT 8 // about 120 ms at one report every 15 ms

// reported firmware version, 3.72 was the old value and made the switch ask for an update
#define FIRMWARE_MAJOR 0x0a
#define FIRMWARE_MINOR 0x00

#define STICK_CENTER 2048
#define STICK_RANGE 1800
#define ACCEL_PER_G 4096.0f
#define PI_F 3.14159265f

enum pairing_phase {
    PHASE_NEUTRAL,
    PHASE_SL_SR_PRESS,
    PHASE_SL_SR_RELEASE,
    PHASE_A_PRESS,
    PHASE_DONE
};

// where things live in the 50 byte report (byte 0 is the 0xa1 hid header)
enum report_offset {
    REPORT_HEADER = 0,
    REPORT_ID = 1,
    REPORT_TIMER = 2,
    REPORT_BATTERY = 3,
    REPORT_BUTTONS_RIGHT = 4,
    REPORT_BUTTONS_SHARED = 5,
    REPORT_BUTTONS_LEFT = 6,
    REPORT_LEFT_STICK = 7,
    REPORT_RIGHT_STICK = 10,
    REPORT_VIBRATION = 13,
    REPORT_DATA = 14 // imu samples, or the ack byte of a subcommand reply
};

// commands the switch sends inside output report 0x01
enum subcommand {
    SUBCMD_PAIRING = 0x01,
    SUBCMD_DEVICE_INFO = 0x02,
    SUBCMD_INPUT_MODE = 0x03,
    SUBCMD_TRIGGER_TIME = 0x04,
    SUBCMD_SHIPMENT = 0x08,
    SUBCMD_SPI_READ = 0x10,
    SUBCMD_MCU_CONFIG = 0x21,
    SUBCMD_MCU_STATE = 0x22,
    SUBCMD_PLAYER_LIGHTS = 0x30,
    SUBCMD_IMU_ENABLE = 0x40,
    SUBCMD_IMU_SENSITIVITY = 0x41,
    SUBCMD_VIBRATION_ENABLE = 0x48
};

// ---------- tables ---------- //

static const struct {
    const char *name;
    uint32_t mask;
} button_names[] = {
    {"Y", JC_BTN_Y}, {"X", JC_BTN_X}, {"B", JC_BTN_B}, {"A", JC_BTN_A},
    {"L", JC_BTN_L}, {"R", JC_BTN_R}, {"ZL", JC_BTN_ZL}, {"ZR", JC_BTN_ZR},
    {"MINUS", JC_BTN_MINUS}, {"PLUS", JC_BTN_PLUS},
    {"LSTICK", JC_BTN_LSTICK}, {"RSTICK", JC_BTN_RSTICK},
    {"HOME", JC_BTN_HOME}, {"CAPTURE", JC_BTN_CAPTURE},
    {"DUP", JC_BTN_DUP}, {"DDOWN", JC_BTN_DDOWN},
    {"DLEFT", JC_BTN_DLEFT}, {"DRIGHT", JC_BTN_DRIGHT},
    {"SL", JC_BTN_SL}, {"SR", JC_BTN_SR}
};

// which report byte and bit each button lives in (sl/sr are handled separately)
static const struct {
    uint32_t mask;
    uint8_t byte;
    uint8_t bit;
} button_bits[] = {
    {JC_BTN_Y, REPORT_BUTTONS_RIGHT, 0x01}, {JC_BTN_X, REPORT_BUTTONS_RIGHT, 0x02},
    {JC_BTN_B, REPORT_BUTTONS_RIGHT, 0x04}, {JC_BTN_A, REPORT_BUTTONS_RIGHT, 0x08},
    {JC_BTN_R, REPORT_BUTTONS_RIGHT, 0x40}, {JC_BTN_ZR, REPORT_BUTTONS_RIGHT, 0x80},
    {JC_BTN_MINUS, REPORT_BUTTONS_SHARED, 0x01}, {JC_BTN_PLUS, REPORT_BUTTONS_SHARED, 0x02},
    {JC_BTN_RSTICK, REPORT_BUTTONS_SHARED, 0x04}, {JC_BTN_LSTICK, REPORT_BUTTONS_SHARED, 0x08},
    {JC_BTN_HOME, REPORT_BUTTONS_SHARED, 0x10}, {JC_BTN_CAPTURE, REPORT_BUTTONS_SHARED, 0x20},
    {JC_BTN_DDOWN, REPORT_BUTTONS_LEFT, 0x01}, {JC_BTN_DUP, REPORT_BUTTONS_LEFT, 0x02},
    {JC_BTN_DRIGHT, REPORT_BUTTONS_LEFT, 0x04}, {JC_BTN_DLEFT, REPORT_BUTTONS_LEFT, 0x08},
    {JC_BTN_L, REPORT_BUTTONS_LEFT, 0x40}, {JC_BTN_ZL, REPORT_BUTTONS_LEFT, 0x80}
};

// body color then button color, as 0xrrggbb
static const uint32_t color_table[JC_COLOR_COUNT][2] = {
    [JC_COLOR_GRAY]        = {0x828282, 0x0f0f0f},
    [JC_COLOR_NEON_RED]    = {0xff3c28, 0x1e0a0a},
    [JC_COLOR_NEON_BLUE]   = {0x0ab9e6, 0x001e1e},
    [JC_COLOR_NEON_YELLOW] = {0xe6ff00, 0x1e1e0a},
    [JC_COLOR_NEON_GREEN]  = {0x1edc00, 0x0a1e0a},
    [JC_COLOR_NEON_PINK]   = {0xff3278, 0x1e0a0f},
    [JC_COLOR_NEON_PURPLE] = {0xb400e6, 0x160a1e},
    [JC_COLOR_NEON_ORANGE] = {0xff8c28, 0x1e140a}
};

// ---------- helpers ---------- //

uint32_t joycon_button_from_name(const char *name) {
    if (name == NULL) return 0;
    for (size_t i = 0; i < sizeof(button_names) / sizeof(button_names[0]); i++) {
        if (strcasecmp(name, button_names[i].name) == 0) return button_names[i].mask;
    }
    return 0;
}

bool joycon_color_from_char(char letter, joycon_color *color) {
    switch (letter) {
        case 'N': case 'n': *color = JC_COLOR_GRAY; return true;
        case 'R': case 'r': *color = JC_COLOR_NEON_RED; return true;
        case 'B': case 'b': *color = JC_COLOR_NEON_BLUE; return true;
        case 'Y': case 'y': *color = JC_COLOR_NEON_YELLOW; return true;
        case 'G': case 'g': *color = JC_COLOR_NEON_GREEN; return true;
        case 'P': case 'p': *color = JC_COLOR_NEON_PINK; return true;
        case 'U': case 'u': *color = JC_COLOR_NEON_PURPLE; return true;
        case 'O': case 'o': *color = JC_COLOR_NEON_ORANGE; return true;
        default: return false;
    }
}

static float clamp_float(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static void rgb_to_bytes(uint32_t rgb, uint8_t out[3]) {
    out[0] = (rgb >> 16) & 0xff;
    out[1] = (rgb >> 8) & 0xff;
    out[2] = rgb & 0xff;
}

// ---------- setters ---------- //

static bool joycon_press(Joycon *jc, const char *button) {
    uint32_t mask = joycon_button_from_name(button);
    jc->buttons_held |= mask;
    return mask != 0;
}

static bool joycon_release(Joycon *jc, const char *button) {
    uint32_t mask = joycon_button_from_name(button);
    jc->buttons_held &= ~mask;
    return mask != 0;
}

// presses for a few reports then lets go on its own, so games always see it
static bool joycon_tap(Joycon *jc, const char *button) {
    uint32_t mask = joycon_button_from_name(button);
    if (mask != 0) {
        jc->buttons_tapped |= mask;
        jc->tap_reports_left = TAP_REPORT_COUNT;
    }
    return mask != 0;
}

// the held buttons become exactly this list
static void joycon_btn_press_report(Joycon *jc, const char *buttons[], int count) {
    jc->buttons_held = 0;
    for (int i = 0; i < count; i++) {
        joycon_press(jc, buttons[i]);
    }
}

static void joycon_release_all(Joycon *jc) {
    jc->buttons_held = 0;
    jc->buttons_tapped = 0;
    jc->tap_reports_left = 0;
}

static void joycon_imu_update(Joycon *jc, const float positions[3]) {
    memcpy(jc->imu, positions, sizeof(jc->imu));
}

static void joycon_stick_update(Joycon *jc, const float sticks[4]) {
    for (int i = 0; i < 4; i++) {
        jc->sticks[i] = clamp_float(sticks[i], -1.0f, 1.0f);
    }
}

static void joycon_battery_update(Joycon *jc, float level) {
    jc->battery = clamp_float(level, 0.0f, 1.0f);
}

static void joycon_set_color_rgb(Joycon *jc, uint32_t body, uint32_t buttons) {
    rgb_to_bytes(body, jc->body_color);
    rgb_to_bytes(buttons, jc->button_color);
}

static void joycon_set_color(Joycon *jc, joycon_color color) {
    if (color >= JC_COLOR_COUNT) color = JC_COLOR_GRAY;
    joycon_set_color_rgb(jc, color_table[color][0], color_table[color][1]);
}

// ---------- getters ---------- //

static joycon_pair_state joycon_get_pair_state(const Joycon *jc) {
    return jc->pair_state;
}

static float joycon_get_rumble_state(const Joycon *jc) {
    return jc->rumble;
}

static uint8_t joycon_get_player_leds(const Joycon *jc) {
    return jc->player_leds;
}

// ---------- constructor ---------- //

void joycon_init(Joycon *jc, joycon_side side, joycon_color color) {
    memset(jc, 0, sizeof(*jc));
    jc->side = side;
    snprintf(jc->name, sizeof(jc->name), "Joy-Con (%c)", side == JOYCON_LEFT ? 'L' : 'R');
    jc->battery = 1.0f;

    jc->press = joycon_press;
    jc->release = joycon_release;
    jc->tap = joycon_tap;
    jc->btn_press_report = joycon_btn_press_report;
    jc->release_all = joycon_release_all;
    jc->imu_update = joycon_imu_update;
    jc->stick_update = joycon_stick_update;
    jc->battery_update = joycon_battery_update;
    jc->set_color = joycon_set_color;
    jc->set_color_rgb = joycon_set_color_rgb;
    jc->get_pair_state = joycon_get_pair_state;
    jc->get_rumble_state = joycon_get_rumble_state;
    jc->get_player_leds = joycon_get_player_leds;

    joycon_set_color(jc, color);
}

Joycon *joycon_create(joycon_side side, joycon_color color) {
    Joycon *jc = malloc(sizeof(Joycon));
    if (jc != NULL) joycon_init(jc, side, color);
    return jc;
}

// ---------- link state ---------- //

void joycon_connected(Joycon *jc, uint32_t now_ms) {
    jc->input_timer = 0;
    jc->connected_ms = now_ms;
    jc->pairing_phase = PHASE_DONE;
    jc->pair_state = JOYCON_PAIRING;
    jc->reply_pending = false;
    jc->imu_enabled = false;
    jc->vibration_enabled = false;
    jc->rumble = 0.0f;
}

void joycon_disconnected(Joycon *jc) {
    jc->pair_state = JOYCON_DISCONNECTED;
    jc->reply_pending = false;
    jc->player_leds = 0;
    jc->rumble = 0.0f;
}

// ---------- incoming reports ---------- //

// rumble block is 4 bytes per side, 00 01 40 40 is neutral. this only reads the
// low frequency amplitude so the value is approximate
static void parse_rumble(Joycon *jc, const uint8_t *block) {
    int low_amp = block[3] & 0x7f;
    if (low_amp <= 0x40) {
        jc->rumble = 0.0f;
    } else {
        jc->rumble = clamp_float((low_amp - 0x40) / 50.0f, 0.0f, 1.0f);
    }
}

void joycon_handle_output(Joycon *jc, uint8_t report_id, const uint8_t *data, int size) {
    if (report_id != JOYCON_OUTPUT_SUBCOMMAND && report_id != JOYCON_OUTPUT_RUMBLE) return;

    // data[0] is the timer, data[1..8] is rumble for left then right
    if (size >= 9) {
        parse_rumble(jc, data + (jc->side == JOYCON_LEFT ? 1 : 5));
    }

    if (report_id != JOYCON_OUTPUT_SUBCOMMAND || size < 10) return;

    jc->pending_command = data[9];
    jc->pending_size = (uint8_t)(size - 10);
    if (jc->pending_size > sizeof(jc->pending_data)) {
        jc->pending_size = sizeof(jc->pending_data);
    }
    memcpy(jc->pending_data, data + 10, jc->pending_size);
    jc->reply_pending = true;

    printf("Switch setup request: 0x%02x\n", jc->pending_command);
}

// ---------- spi flash ---------- //

static const uint8_t motion_calibration[] = {
    0xcc, 0x00, 0x40, 0x00, 0x91, 0x01,
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
    0xe7, 0xff, 0x0e, 0x00, 0xdc, 0xff,
    0x3b, 0x34, 0x3b, 0x34, 0x3b, 0x34
};
// each stick is 9 bytes of packed 12 bit pairs. left is above, center, below and right is
// center, above, below. center is 0x800 and the range is 0x708 (1800) so neutral reads as
// exactly 0 and full deflection matches STICK_RANGE
static const uint8_t stick_calibration[] = {
    0x08, 0x87, 0x70, 0x00, 0x08, 0x80, 0x08, 0x87, 0x70,
    0x00, 0x08, 0x80, 0x08, 0x87, 0x70, 0x08, 0x87, 0x70
};
static const uint8_t stick_header[] = {0x50, 0xfd, 0x00, 0x00, 0xc6, 0x0f};
static const uint8_t stick_parameters[] = {
    0x0f, 0x30, 0x61, 0x96, 0x30, 0xf3,
    0xd4, 0x14, 0x54, 0x41, 0x15, 0x54,
    0xc7, 0x79, 0x9c, 0x33, 0x36, 0x63
};

static const struct {
    uint16_t address;
    const uint8_t *data;
    uint8_t length;
} spi_regions[] = {
    {0x6020, motion_calibration, sizeof(motion_calibration)},
    {0x603d, stick_calibration, sizeof(stick_calibration)},
    {0x6080, stick_header, sizeof(stick_header)},
    {0x6086, stick_parameters, sizeof(stick_parameters)},
    {0x6098, stick_parameters, sizeof(stick_parameters)}
};

// returns one byte of the fake flash, anything unknown reads as 0xff (unset)
static uint8_t spi_byte(const Joycon *jc, uint16_t address) {
    if (address == 0x6012) return (uint8_t)jc->side;
    if (address == 0x601b) return 0x01; // tells the switch the colors below are valid
    if (address >= 0x6050 && address < 0x6053) return jc->body_color[address - 0x6050];
    if (address >= 0x6053 && address < 0x6056) return jc->button_color[address - 0x6053];
    // grips reuse the body color
    if (address >= 0x6056 && address < 0x605c) return jc->body_color[(address - 0x6056) % 3];

    for (size_t i = 0; i < sizeof(spi_regions) / sizeof(spi_regions[0]); i++) {
        uint16_t start = spi_regions[i].address;
        if (address >= start && address < start + spi_regions[i].length) {
            return spi_regions[i].data[address - start];
        }
    }
    return 0xff;
}

static void read_spi(const Joycon *jc, uint16_t address, uint8_t *destination, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        destination[i] = spi_byte(jc, address + i);
    }
}

// ---------- report building ---------- //

static uint8_t battery_byte(const Joycon *jc) {
    uint8_t level;
    if (jc->battery > 0.75f) level = 8;
    else if (jc->battery > 0.50f) level = 6;
    else if (jc->battery > 0.25f) level = 4;
    else if (jc->battery > 0.05f) level = 2;
    else level = 0;
    return (uint8_t)((level << 4) | 0x01); // low nibble is the connection info
}

// 12 bit x and y packed into 3 bytes
static void pack_stick(uint8_t *out, float x, float y) {
    int raw_x = STICK_CENTER + (int)lroundf(x * STICK_RANGE);
    int raw_y = STICK_CENTER + (int)lroundf(y * STICK_RANGE);
    out[0] = raw_x & 0xff;
    out[1] = ((raw_x >> 8) & 0x0f) | ((raw_y & 0x0f) << 4);
    out[2] = (raw_y >> 4) & 0xff;
}

static void write_int16(uint8_t *out, int value) {
    out[0] = value & 0xff;
    out[1] = (value >> 8) & 0xff;
}

// gravity from pitch and roll, yaw can't be seen by an accelerometer and gyro is left at 0.
// axis signs are a first guess and still need testing on a real switch
static void write_imu(const Joycon *jc, uint8_t *out) {
    float pitch = jc->imu[1] * PI_F / 180.0f;
    float roll = jc->imu[2] * PI_F / 180.0f;
    int accel_x = (int)lroundf(-sinf(pitch) * ACCEL_PER_G);
    int accel_y = (int)lroundf(sinf(roll) * cosf(pitch) * ACCEL_PER_G);
    int accel_z = (int)lroundf(cosf(roll) * cosf(pitch) * ACCEL_PER_G);

    // three samples per report
    for (int sample = 0; sample < 3; sample++) {
        uint8_t *slot = out + sample * 12;
        write_int16(slot, accel_x);
        write_int16(slot + 2, accel_y);
        write_int16(slot + 4, accel_z);
        // gyro bytes stay 0
    }
}

static uint32_t pairing_buttons(Joycon *jc, uint32_t now_ms) {
    uint32_t elapsed = now_ms - jc->connected_ms;
    uint32_t buttons = 0;
    uint8_t phase = PHASE_NEUTRAL;

    if (elapsed >= PAIRING_NEUTRAL_MS) {
        uint32_t sequence_ms = elapsed - PAIRING_NEUTRAL_MS;
        uint32_t pulse_period_ms = PAIRING_SL_SR_PRESS_MS + PAIRING_SL_SR_RELEASE_MS;
        uint32_t sl_sr_total_ms = pulse_period_ms * PAIRING_SL_SR_PULSES;

        if (sequence_ms < sl_sr_total_ms) {
            phase = sequence_ms % pulse_period_ms < PAIRING_SL_SR_PRESS_MS
                ? PHASE_SL_SR_PRESS : PHASE_SL_SR_RELEASE;
        } else if (sequence_ms < sl_sr_total_ms + PAIRING_A_PRESS_MS) {
            phase = PHASE_A_PRESS;
        } else {
            phase = PHASE_DONE;
        }
    }

    if (phase == PHASE_SL_SR_PRESS) buttons = JC_BTN_SL | JC_BTN_SR;
    if (phase == PHASE_A_PRESS) buttons = JC_BTN_A;

    if (phase != jc->pairing_phase) {
        if (phase == PHASE_SL_SR_PRESS && jc->pairing_phase == PHASE_NEUTRAL) {
            printf("Pairing input: sending %u SL+SR presses\n", PAIRING_SL_SR_PULSES);
        } else if (phase == PHASE_A_PRESS) {
            printf("Pairing input: A confirmation\n");
        } else if (phase == PHASE_DONE) {
            printf("Pairing input: sequence complete\n");
            jc->pair_state = JOYCON_CONNECTED;
        }
        jc->pairing_phase = phase;
    }
    return buttons;
}

static void write_buttons(const Joycon *jc, uint8_t *report, uint32_t buttons) {
    for (size_t i = 0; i < sizeof(button_bits) / sizeof(button_bits[0]); i++) {
        if (buttons & button_bits[i].mask) {
            report[button_bits[i].byte] |= button_bits[i].bit;
        }
    }

    // sl and sr share a byte with the shoulder buttons of their own side
    uint8_t side_byte = jc->side == JOYCON_LEFT ? REPORT_BUTTONS_LEFT : REPORT_BUTTONS_RIGHT;
    if (buttons & JC_BTN_SR) report[side_byte] |= 0x10;
    if (buttons & JC_BTN_SL) report[side_byte] |= 0x20;
}

static void build_input_report(Joycon *jc, uint8_t report_id, uint8_t report[JOYCON_REPORT_SIZE], uint32_t now_ms) {
    memset(report, 0, JOYCON_REPORT_SIZE);
    report[REPORT_HEADER] = 0xa1;
    report[REPORT_ID] = report_id;
    report[REPORT_TIMER] = jc->input_timer++;
    report[REPORT_BATTERY] = battery_byte(jc);

    uint32_t buttons = jc->buttons_held | jc->buttons_tapped;
    if (jc->tap_reports_left > 0 && --jc->tap_reports_left == 0) {
        jc->buttons_tapped = 0;
    }
    if (jc->pair_state == JOYCON_PAIRING) {
        buttons |= pairing_buttons(jc, now_ms);
    }
    write_buttons(jc, report, buttons);

    pack_stick(report + REPORT_LEFT_STICK, jc->sticks[0], jc->sticks[1]);
    pack_stick(report + REPORT_RIGHT_STICK, jc->sticks[2], jc->sticks[3]);
    report[REPORT_VIBRATION] = jc->vibration_enabled ? 0x80 : 0x00;

    if (report_id == JOYCON_INPUT_STANDARD && jc->imu_enabled) {
        write_imu(jc, report + REPORT_DATA);
    }
}

static void build_subcommand_reply(Joycon *jc, uint8_t report[JOYCON_REPORT_SIZE], uint32_t now_ms) {
    uint8_t command = jc->pending_command;
    const uint8_t *parameters = jc->pending_data;
    uint8_t parameter_count = jc->pending_size;

    build_input_report(jc, JOYCON_INPUT_SUBCOMMAND_REPLY, report, now_ms);
    // the reply is a standard report with an ack byte and the command echoed at the data offset
    report[REPORT_DATA] = 0x80;
    report[REPORT_DATA + 1] = command;

    switch (command) {
        case SUBCMD_PAIRING:
            report[REPORT_DATA] = 0x81;
            report[16] = 0x01;
            report[17] = 0x03;
            break;
        case SUBCMD_DEVICE_INFO:
            report[REPORT_DATA] = 0x82;
            report[16] = FIRMWARE_MAJOR;
            report[17] = FIRMWARE_MINOR;
            report[18] = (uint8_t)jc->side;
            report[19] = 0x02;
            memcpy(report + 20, jc->address, sizeof(jc->address));
            report[26] = 0x01;
            report[27] = 0x01; // use the colors from spi
            break;
        case SUBCMD_TRIGGER_TIME:
            report[REPORT_DATA] = 0x83;
            break;
        case SUBCMD_SPI_READ:
            if (parameter_count >= 5) {
                uint16_t address = (uint16_t)parameters[0] | ((uint16_t)parameters[1] << 8);
                uint8_t read_length = parameters[4];
                if (read_length > JOYCON_REPORT_SIZE - 21) {
                    read_length = JOYCON_REPORT_SIZE - 21;
                }
                report[REPORT_DATA] = 0x90;
                report[16] = parameters[0];
                report[17] = parameters[1];
                report[18] = parameters[2];
                report[19] = parameters[3];
                report[20] = read_length;
                read_spi(jc, address, report + 21, read_length);
            }
            break;
        case SUBCMD_MCU_CONFIG:
            report[REPORT_DATA] = 0xa0;
            report[16] = 0x01;
            report[17] = 0x00;
            report[18] = 0xff;
            report[19] = 0x00;
            report[20] = 0x08;
            report[21] = 0x00;
            report[22] = 0x1b;
            report[23] = 0x01;
            report[49] = 0xc8;
            break;
        case SUBCMD_PLAYER_LIGHTS:
            if (parameter_count > 0) jc->player_leds = parameters[0];
            break;
        case SUBCMD_IMU_ENABLE:
            jc->imu_enabled = parameter_count > 0 && parameters[0] != 0;
            break;
        case SUBCMD_VIBRATION_ENABLE:
            jc->vibration_enabled = parameter_count > 0 && parameters[0] != 0;
            break;
        // these only need the plain ack
        case SUBCMD_INPUT_MODE:
        case SUBCMD_SHIPMENT:
        case SUBCMD_MCU_STATE:
        case SUBCMD_IMU_SENSITIVITY:
            break;
        default:
            printf("Replying with generic ACK for subcommand 0x%02x\n", command);
            break;
    }
}

void joycon_next_report(Joycon *jc, uint8_t report[JOYCON_REPORT_SIZE], uint32_t now_ms) {
    jc->reports_sent++;
    if (jc->reply_pending) {
        build_subcommand_reply(jc, report, now_ms);
        printf("Joy-Con setup reply: 0x%02x\n", jc->pending_command);
        jc->reply_pending = false;
    } else {
        build_input_report(jc, JOYCON_INPUT_STANDARD, report, now_ms);
    }
}
