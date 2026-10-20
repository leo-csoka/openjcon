// import sdk basics, radio functions, and bluetooth classic lib
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "joycon_hid.h"

// service storage buffer used by btstack
#define HID_SERVICE_BUFFER_SIZE 512
static uint8_t hid_service_buffer[HID_SERVICE_BUFFER_SIZE];
static uint8_t device_id_service_buffer[100];

static uint16_t hid_cid;
static btstack_timer_source_t input_report_timer;
static uint8_t pending_subcommand;
static uint8_t pending_subcommand_data[38];
static uint8_t pending_subcommand_size;
static bool subcommand_reply_pending;
static bool imu_enabled;
static bool vibration_enabled;
static uint8_t input_timer;
static uint32_t connection_started_ms;

#define INPUT_REPORT_INTERVAL_MS 15
#define PAIRING_NEUTRAL_MS 1000
#define PAIRING_SL_SR_PRESS_MS 400
#define PAIRING_SL_SR_RELEASE_MS 300
#define PAIRING_SL_SR_PULSES 4
#define PAIRING_A_PRESS_MS 700
#define JOYCON_REPORT_SIZE 50

enum pairing_input_phase {
    PAIRING_PHASE_NEUTRAL,
    PAIRING_PHASE_SL_SR_PRESS,
    PAIRING_PHASE_SL_SR_RELEASE,
    PAIRING_PHASE_A_PRESS,
    PAIRING_PHASE_DONE
};

static enum pairing_input_phase last_logged_phase = PAIRING_PHASE_DONE;

static void build_input_report(uint8_t report_id, uint8_t report[JOYCON_REPORT_SIZE]) {
    static const uint8_t imu_sample[36] = {
        0x75, 0xfd, 0xfd, 0xff, 0x09, 0x10, 0x21, 0x00, 0xd5, 0xff, 0xe0, 0xff,
        0x72, 0xfd, 0xf9, 0xff, 0x0a, 0x10, 0x22, 0x00, 0xd5, 0xff, 0xe0, 0xff,
        0x76, 0xfd, 0xfc, 0xff, 0x09, 0x10, 0x23, 0x00, 0xd5, 0xff, 0xe0, 0xff
    };

    memset(report, 0, JOYCON_REPORT_SIZE);
    report[0] = 0xa1;
    report[1] = report_id;
    report[2] = input_timer++;
    report[3] = 0x91;
    report[7] = 0x00;
    report[8] = 0x08;
    report[9] = 0x80;
    report[10] = 0x00;
    report[11] = 0x08;
    report[12] = 0x80;
    report[13] = vibration_enabled ? 0x80 : 0x00;
    if (report_id == JOYCON_INPUT_STANDARD && imu_enabled) {
        memcpy(report + 14, imu_sample, sizeof(imu_sample));
    }

    const uint32_t connection_elapsed_ms = connection_started_ms == 0 ? 0 :
        to_ms_since_boot(get_absolute_time()) - connection_started_ms;
    enum pairing_input_phase phase = PAIRING_PHASE_NEUTRAL;
    uint8_t pulse_number = 0;

    if (connection_elapsed_ms >= PAIRING_NEUTRAL_MS) {
        const uint32_t sequence_elapsed_ms = connection_elapsed_ms - PAIRING_NEUTRAL_MS;
        const uint32_t pulse_period_ms = PAIRING_SL_SR_PRESS_MS + PAIRING_SL_SR_RELEASE_MS;
        const uint32_t sl_sr_sequence_ms = pulse_period_ms * PAIRING_SL_SR_PULSES;

        if (sequence_elapsed_ms < sl_sr_sequence_ms) {
            pulse_number = (uint8_t)(sequence_elapsed_ms / pulse_period_ms + 1);
            phase = sequence_elapsed_ms % pulse_period_ms < PAIRING_SL_SR_PRESS_MS
                ? PAIRING_PHASE_SL_SR_PRESS
                : PAIRING_PHASE_SL_SR_RELEASE;
        } else if (sequence_elapsed_ms < sl_sr_sequence_ms + PAIRING_A_PRESS_MS) {
            phase = PAIRING_PHASE_A_PRESS;
        } else {
            phase = PAIRING_PHASE_DONE;
        }
    }

    if (phase == PAIRING_PHASE_SL_SR_PRESS) {
        report[6] = 0x30;
    } else if (phase == PAIRING_PHASE_A_PRESS) {
        report[4] = 0x08;
    }

    if (report_id == JOYCON_INPUT_STANDARD &&
        phase != last_logged_phase) {
        switch (phase) {
            case PAIRING_PHASE_SL_SR_PRESS:
                if (pulse_number == 1) {
                    printf("Pairing input: sending %u SL+SR presses\n",
                           PAIRING_SL_SR_PULSES);
                }
                break;
            case PAIRING_PHASE_A_PRESS:
                printf("Pairing input: A confirmation\n");
                break;
            case PAIRING_PHASE_DONE:
                printf("Pairing input: sequence complete\n");
                break;
            case PAIRING_PHASE_NEUTRAL:
            case PAIRING_PHASE_SL_SR_RELEASE:
                break;
        }
        last_logged_phase = phase;
    }
}

static void copy_spi_data(uint16_t address, uint8_t *destination, uint8_t length) {
    static const uint8_t stick_parameters[] = {
        0x0f, 0x30, 0x61, 0x96, 0x30, 0xf3,
        0xd4, 0x14, 0x54, 0x41, 0x15, 0x54,
        0xc7, 0x79, 0x9c, 0x33, 0x36, 0x63
    };
    static const uint8_t stick_calibration[] = {
        0xd4, 0x75, 0x61, 0xe5, 0x87, 0x7c, 0xec, 0x55, 0x61,
        0x5d, 0xd8, 0x7f, 0x18, 0xe6, 0x61, 0x86, 0x65, 0x5d
    };
    static const uint8_t motion_calibration[] = {
        0xcc, 0x00, 0x40, 0x00, 0x91, 0x01,
        0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
        0xe7, 0xff, 0x0e, 0x00, 0xdc, 0xff,
        0x3b, 0x34, 0x3b, 0x34, 0x3b, 0x34
    };

    memset(destination, 0xff, length);
    switch (address) {
        case 0x6000:
            break;
        case 0x6050:
            memset(destination, 0x32, length < 3 ? length : 3);
            break;
        case 0x6080:
            if (length > 0) destination[0] = 0x50;
            if (length > 1) destination[1] = 0xfd;
            if (length > 2) destination[2] = 0x00;
            if (length > 3) destination[3] = 0x00;
            if (length > 4) destination[4] = 0xc6;
            if (length > 5) destination[5] = 0x0f;
            if (length > 6) {
                uint8_t copy_size = length - 6;
                if (copy_size > sizeof(stick_parameters)) copy_size = sizeof(stick_parameters);
                memcpy(destination + 6, stick_parameters, copy_size);
            }
            break;
        case 0x6098: {
            uint8_t copy_size = length;
            if (copy_size > sizeof(stick_parameters)) copy_size = sizeof(stick_parameters);
            memcpy(destination, stick_parameters, copy_size);
            break;
        }
        case 0x8010:
            memset(destination, 0xff, length < 3 ? length : 3);
            break;
        case 0x603d: {
            uint8_t copy_size = length;
            if (copy_size > sizeof(stick_calibration)) copy_size = sizeof(stick_calibration);
            memcpy(destination, stick_calibration, copy_size);
            break;
        }
        case 0x6020: {
            uint8_t copy_size = length;
            if (copy_size > sizeof(motion_calibration)) copy_size = sizeof(motion_calibration);
            memcpy(destination, motion_calibration, copy_size);
            break;
        }
        default:
            break;
    }
}

static void queue_subcommand(uint16_t report_id, const uint8_t *report, int report_size) {
    if (report_id != JOYCON_OUTPUT_SUBCOMMAND || report_size < 10) {
        return;
    }

    pending_subcommand = report[9];
    pending_subcommand_size = (uint8_t)(report_size - 10);
    if (pending_subcommand_size > sizeof(pending_subcommand_data)) {
        pending_subcommand_size = sizeof(pending_subcommand_data);
    }
    memcpy(pending_subcommand_data, report + 10, pending_subcommand_size);
    subcommand_reply_pending = true;

    printf("Switch setup request: 0x%02x\n", pending_subcommand);
}

static void build_subcommand_reply(uint8_t report[JOYCON_REPORT_SIZE]) {
    uint8_t command = pending_subcommand;
    uint8_t *parameters = pending_subcommand_data;
    uint8_t parameter_count = pending_subcommand_size;
    bd_addr_t local_address;

    build_input_report(JOYCON_INPUT_SUBCOMMAND_REPLY, report);
    report[14] = 0x80;
    report[15] = command;

    switch (command) {
        case 0x01:
            report[14] = 0x81;
            report[16] = 0x01;
            report[17] = 0x03;
            break;
        case 0x02:
            report[14] = 0x82;
            report[16] = 0x03;
            report[17] = 0x48;
            report[18] = 0x01;
            report[19] = 0x02;
            gap_local_bd_addr(local_address);
            memcpy(report + 20, local_address, sizeof(local_address));
            report[26] = 0x01;
            report[27] = 0x01;
            break;
        case 0x08:
            break;
        case 0x10:
            if (parameter_count >= 5) {
                uint16_t address = (uint16_t)parameters[0] |
                                   ((uint16_t)parameters[1] << 8);
                uint8_t read_length = parameters[4];
                if (read_length > JOYCON_REPORT_SIZE - 21) {
                    read_length = JOYCON_REPORT_SIZE - 21;
                }
                report[14] = 0x90;
                report[16] = parameters[0];
                report[17] = parameters[1];
                report[20] = read_length;
                copy_spi_data(address, report + 21, read_length);
            }
            break;
        case 0x03:
            break;
        case 0x04:
            report[14] = 0x83;
            break;
        case 0x21:
            report[14] = 0xa0;
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
        case 0x22:
        case 0x30:
        case 0x41:
            break;
        case 0x40:
            imu_enabled = parameter_count > 0 && parameters[0] != 0;
            break;
        case 0x48:
            vibration_enabled = parameter_count > 0 && parameters[0] != 0;
            break;
        default:
            printf("Replying with generic ACK for subcommand 0x%02x\n", command);
            break;
    }
}

static void report_data_handler(uint16_t cid, hid_report_type_t report_type, uint16_t report_id, int report_size, uint8_t *report) {
    UNUSED(cid);
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        queue_subcommand(report_id, report, report_size);
    }
}

static void set_report_handler(uint16_t cid, hid_report_type_t report_type, int report_size, uint8_t *report) {
    UNUSED(cid);
    if (report_type == HID_REPORT_TYPE_OUTPUT && report_size > 1) {
        queue_subcommand(report[0], report + 1, report_size - 1);
    }
}

static void input_report_timer_handler(btstack_timer_source_t *timer) {
    UNUSED(timer);
    if (hid_cid != 0) {
        hid_device_request_can_send_now_event(hid_cid);
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    const uint8_t event_type = hci_event_packet_get_type(packet);
    if (event_type == HCI_EVENT_DISCONNECTION_COMPLETE) {
        printf("Bluetooth link disconnected: status=0x%02x handle=0x%04x reason=0x%02x\n",
               hci_event_disconnection_complete_get_status(packet),
               hci_event_disconnection_complete_get_connection_handle(packet),
               hci_event_disconnection_complete_get_reason(packet));
    }

    if (event_type == BTSTACK_EVENT_STATE &&
        btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
        printf("BTstack Classic is ready\n");
    }

    if (event_type == HCI_EVENT_USER_CONFIRMATION_REQUEST) {
        bd_addr_t remote_addr;
        hci_event_user_confirmation_request_get_bd_addr(packet, remote_addr);
        printf("Accepting Bluetooth SSP confirmation request\n");
        gap_ssp_confirmation_response(remote_addr);
    }

    if (event_type == HCI_EVENT_HID_META) {
        switch (hci_event_hid_meta_get_subevent_code(packet)) {
            case HID_SUBEVENT_CONNECTION_OPENED: {
                uint8_t status = hid_subevent_connection_opened_get_status(packet);

                if (status != ERROR_CODE_SUCCESS) {
                    printf("HID connection failed: 0x%02x\n", status);
                    break;
                }

                hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                input_timer = 0;
                connection_started_ms = to_ms_since_boot(get_absolute_time());
                subcommand_reply_pending = false;
                last_logged_phase = PAIRING_PHASE_DONE;
                imu_enabled = false;
                vibration_enabled = false;
                printf("HID host connected (cid %u)\n", hid_cid);
                hid_device_request_can_send_now_event(hid_cid);
                break;
            }

            case HID_SUBEVENT_CAN_SEND_NOW: {
                if (hid_subevent_can_send_now_get_hid_cid(packet) != hid_cid ||
                    hid_cid == 0) {
                    break;
                }

                uint8_t report[JOYCON_REPORT_SIZE];
                if (subcommand_reply_pending) {
                    build_subcommand_reply(report);
                    printf("Joy-Con setup reply: 0x%02x\n", pending_subcommand);
                    subcommand_reply_pending = false;
                } else {
                    build_input_report(JOYCON_INPUT_STANDARD, report);
                }
                hid_device_send_interrupt_message(hid_cid, report, sizeof(report));
                btstack_run_loop_set_timer(&input_report_timer, INPUT_REPORT_INTERVAL_MS);
                btstack_run_loop_add_timer(&input_report_timer);
                break;
            }

            case HID_SUBEVENT_CONNECTION_CLOSED:
                printf("HID host disconnected (active cid %u)\n", hid_cid);
                btstack_run_loop_remove_timer(&input_report_timer);
                hid_cid = 0;
                connection_started_ms = 0;
                subcommand_reply_pending = false;
                break;

            default:
                break;
        }
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500);

    int cyw43_status = cyw43_arch_init();
    if (cyw43_status != PICO_OK) {
        panic("Failed to initialize CYW43");
    }

    gap_set_class_of_device(0x2508);
    gap_set_local_name("Joy-Con (L)");
    gap_connectable_control(1);
    gap_discoverable_control(1);
    gap_set_default_link_policy_settings(
        LM_LINK_POLICY_ENABLE_ROLE_SWITCH | LM_LINK_POLICY_ENABLE_SNIFF_MODE);
    gap_set_allow_role_switch(true);

    l2cap_init();
    sdp_init();

    const hid_sdp_record_t hid_params = {
        .hid_device_subclass = 0x08,
        .hid_country_code = 33,
        .hid_virtual_cable = 1,
        .hid_remote_wake = 1,
        .hid_reconnect_initiate = 1,
        .hid_normally_connectable = false,
        .hid_boot_device = false, // not a keyboard/mouse
        .hid_ssr_host_max_latency = 0xffff,
        .hid_ssr_host_min_timeout = 0xffff,
        .hid_supervision_timeout = 3200,
        .hid_descriptor = joycon_hid_descriptor,
        .hid_descriptor_size = joycon_hid_descriptor_size,
        .device_name = "Wireless Gamepad"
    };

    hid_create_sdp_record(
        hid_service_buffer,
        sdp_create_service_record_handle(),
        &hid_params
    );
    btstack_assert(de_get_len(hid_service_buffer) <= sizeof(hid_service_buffer));
    sdp_register_service(hid_service_buffer);

    device_id_create_sdp_record(
        device_id_service_buffer,
        sdp_create_service_record_handle(),
        DEVICE_ID_VENDOR_ID_SOURCE_USB,
        0x057e,
        0x2006,
        0x0001
    );
    btstack_assert(de_get_len(device_id_service_buffer) <= sizeof(device_id_service_buffer));
    sdp_register_service(device_id_service_buffer);

    hid_device_init(
        true,
        joycon_hid_descriptor_size,
        joycon_hid_descriptor
    );

    hid_device_register_report_data_callback(&report_data_handler);
    hid_device_register_set_report_callback(&set_report_handler);
    btstack_run_loop_set_timer_handler(&input_report_timer, input_report_timer_handler);
    hid_device_register_packet_handler(&packet_handler);

    static btstack_packet_callback_registration_t hci_event_callback_registration;
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    printf("Ready: discoverable as Joy-Con (L); open Change Grip/Order on the Switch\n");
    hci_power_control(HCI_POWER_ON);
    btstack_run_loop_execute();
    return 0;
}