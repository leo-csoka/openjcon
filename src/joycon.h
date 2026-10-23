#ifndef JOYCON_H
#define JOYCON_H

#include <stdbool.h>
#include <stdint.h>

#define JOYCON_REPORT_SIZE 50

// ---------- enums ---------- //

typedef enum {
    JOYCON_LEFT = 1,
    JOYCON_RIGHT = 2
} joycon_side;

// logical buttons, one bit each. sl/sr are mapped to the right byte based on side
typedef enum {
    JC_BTN_Y       = 1u << 0,
    JC_BTN_X       = 1u << 1,
    JC_BTN_B       = 1u << 2,
    JC_BTN_A       = 1u << 3,
    JC_BTN_R       = 1u << 4,
    JC_BTN_ZR      = 1u << 5,
    JC_BTN_MINUS   = 1u << 6,
    JC_BTN_PLUS    = 1u << 7,
    JC_BTN_LSTICK  = 1u << 8,
    JC_BTN_RSTICK  = 1u << 9,
    JC_BTN_HOME    = 1u << 10,
    JC_BTN_CAPTURE = 1u << 11,
    JC_BTN_DDOWN   = 1u << 12,
    JC_BTN_DUP     = 1u << 13,
    JC_BTN_DRIGHT  = 1u << 14,
    JC_BTN_DLEFT   = 1u << 15,
    JC_BTN_L       = 1u << 16,
    JC_BTN_ZL      = 1u << 17,
    JC_BTN_SL      = 1u << 18,
    JC_BTN_SR      = 1u << 19
} joycon_button;

// named colors, each one sets both the body and the button color
typedef enum {
    JC_COLOR_GRAY,
    JC_COLOR_NEON_RED,
    JC_COLOR_NEON_BLUE,
    JC_COLOR_NEON_YELLOW,
    JC_COLOR_NEON_GREEN,
    JC_COLOR_NEON_PINK,
    JC_COLOR_NEON_PURPLE,
    JC_COLOR_NEON_ORANGE,
    JC_COLOR_COUNT
} joycon_color;

typedef enum {
    JOYCON_DISCONNECTED = 0, // no host, leds should flash
    JOYCON_PAIRING = 1,      // host connected, still sending the pairing button sequence
    JOYCON_CONNECTED = 2     // pairing sequence finished
} joycon_pair_state;

// ---------- struct ---------- //

typedef struct Joycon Joycon;

struct Joycon {
    // identity
    char name[24];
    joycon_side side;
    uint8_t body_color[3];   // r, g, b
    uint8_t button_color[3]; // r, g, b

    // input state, only changes when a setter is called
    uint32_t buttons_held;
    uint32_t buttons_tapped;
    uint8_t tap_reports_left;
    float sticks[4];         // lx, ly, rx, ry in -1.0 to 1.0
    float imu[3];            // yaw, pitch, roll in degrees
    float battery;           // 0.0 to 1.0

    // state reported back from the switch
    joycon_pair_state pair_state;
    uint8_t player_leds;
    float rumble;
    bool imu_enabled;
    bool vibration_enabled;

    // protocol internals, the bluetooth layer should not touch these
    uint8_t address[6];
    uint8_t input_timer;
    uint32_t reports_sent;
    uint32_t connected_ms;
    uint8_t pairing_phase;
    bool reply_pending;
    uint8_t pending_command;
    uint8_t pending_data[38];
    uint8_t pending_size;

    // methods, set by joycon_init so they can be called like jc->press(jc, "A")
    bool (*press)(Joycon *jc, const char *button);
    bool (*release)(Joycon *jc, const char *button);
    bool (*tap)(Joycon *jc, const char *button);
    void (*btn_press_report)(Joycon *jc, const char *buttons[], int count);
    void (*release_all)(Joycon *jc);
    void (*imu_update)(Joycon *jc, const float positions[3]);
    void (*stick_update)(Joycon *jc, const float sticks[4]);
    void (*battery_update)(Joycon *jc, float level);
    void (*set_color)(Joycon *jc, joycon_color color);
    void (*set_color_rgb)(Joycon *jc, uint32_t body, uint32_t buttons);
    joycon_pair_state (*get_pair_state)(const Joycon *jc);
    float (*get_rumble_state)(const Joycon *jc);
    uint8_t (*get_player_leds)(const Joycon *jc);
};

// ---------- constructor ---------- //

void joycon_init(Joycon *jc, joycon_side side, joycon_color color);
Joycon *joycon_create(joycon_side side, joycon_color color);

// ---------- helpers ---------- //

// "A", "ZL", "DUP", "LSTICK" etc (case insensitive), returns 0 if unknown
uint32_t joycon_button_from_name(const char *name);

// 'R', 'B', 'Y', 'G', 'P', 'U', 'O', 'N' (gray), returns false if unknown
bool joycon_color_from_char(char letter, joycon_color *color);

// ---------- link functions, called by the bluetooth layer ---------- //

void joycon_connected(Joycon *jc, uint32_t now_ms);
void joycon_disconnected(Joycon *jc);

// data starts at the timer byte, right after the report id
void joycon_handle_output(Joycon *jc, uint8_t report_id, const uint8_t *data, int size);

// fills the next report to send, either a subcommand reply or a standard input report
void joycon_next_report(Joycon *jc, uint8_t report[JOYCON_REPORT_SIZE], uint32_t now_ms);

#endif
