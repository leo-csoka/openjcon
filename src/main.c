// import SDK Basics, Radio functions, and Bluetooth Classic Lib
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

// controller data profile
static const uint8_t gamepad_hid_descriptor[] = {
    0x05, 0x01,       // Usage Page: Generic Desktop
    0x09, 0x05,       // Usage: Game Pad
    0xa1, 0x01,       // Begin Application Collection

    0x05, 0x09,       //   Usage Page: Buttons
    0x19, 0x01,       //   First button: 1
    0x29, 0x08,       //   Last button: 8
    0x15, 0x00,       //   Button value minimum: 0
    0x25, 0x01,       //   Button value maximum: 1
    0x95, 0x08,       //   Number of button values: 8
    0x75, 0x01,       //   Bits per button value: 1
    0x81, 0x02,       //   Input: data, variable, absolute

    0x05, 0x01,       //   Usage Page: Generic Desktop
    0x09, 0x30,       //   Axis: X
    0x09, 0x31,       //   Axis: Y
    0x09, 0x33,       //   Axis: Rx
    0x09, 0x34,       //   Axis: Ry
    0x15, 0x81,       //   Axis value minimum: -127
    0x25, 0x7f,       //   Axis value maximum: 127
    0x75, 0x08,       //   Bits per axis value: 8
    0x95, 0x04,       //   Number of axis values: 4
    0x81, 0x02,       //   Input: data, variable, absolute

    0xc0              // End Application Collection
};

// service storage buffer used by BTStack
static uint8_t hid_service_buffer[270];

// connection id (cid)
static uint16_t hid_cid;
static uint8_t test_button_state;
static btstack_timer_source_t report_timer;

// ------------------------------ //
// FUNCTIONS:

// send a test button state twice per second
static void report_timer_handler(btstack_timer_source_t *timer) {
    if (hid_cid == 0) {
        return;
    }

    test_button_state ^= 0x01;
    hid_device_request_can_send_now_event(hid_cid);
    btstack_run_loop_set_timer(timer, 500);
    btstack_run_loop_add_timer(timer);
}

// per packet recieved 
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel); // keeps compiler from erring
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) { // only HCI packets allowed
        return;
    }

    if (hci_event_packet_get_type(packet) == BTSTACK_EVENT_STATE && btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
        // controller is discoverable
        printf("BTstack Classic is ready\n");
        gap_set_local_name("Pico Joy-Con Test"); // gap is device discovery/connection
        gap_discoverable_control(1);
        printf("Device is discoverable\n");
    }

    if (hci_event_packet_get_type(packet) == HCI_EVENT_HID_META) { // incase we dont get a connection, handle errors
        switch (hci_event_hid_meta_get_subevent_code(packet)) {
            case HID_SUBEVENT_CONNECTION_OPENED: {
                uint8_t status = hid_subevent_connection_opened_get_status(packet);

                if (status != ERROR_CODE_SUCCESS) {
                    printf("HID connection failed: 0x%02x\n", status);
                    break;
                }

                hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                test_button_state = 0;
                printf("HID host connected (cid %u)\n", hid_cid);
                hid_device_request_can_send_now_event(hid_cid);
                btstack_run_loop_set_timer_handler(&report_timer, report_timer_handler);
                btstack_run_loop_set_timer(&report_timer, 500);
                btstack_run_loop_add_timer(&report_timer);
                break;
            }

            case HID_SUBEVENT_CAN_SEND_NOW: {
                if (hid_subevent_can_send_now_get_hid_cid(packet) != hid_cid || hid_cid == 0) {
                    break;
                }

                const uint8_t report[] = {
                    0xa1, test_button_state, 0, 0, 0, 0
                };
                hid_device_send_interrupt_message(hid_cid, report, sizeof(report));
                break;
            }

            case HID_SUBEVENT_CONNECTION_CLOSED:
                if (hid_subevent_connection_closed_get_hid_cid(packet) == hid_cid) {
                    btstack_run_loop_remove_timer(&report_timer);
                    printf("HID host disconnected\n");
                    hid_cid = 0;
                    test_button_state = 0;
                }
                break;

            default:
                break;
        }
    }
}

// ------------------------------ //
// MAIN EXECUTION:


int main(void) {
    // start I/O, radio
    stdio_init_all();

    if (cyw43_arch_init() != PICO_OK) {
        panic("Failed to initialize CYW43");
    }

    // start core protocols
    l2cap_init();
    sdp_init();

    // HId service description
    const hid_sdp_record_t hid_params = {
        .hid_device_subclass = 0x08,
        .hid_country_code = 0,
        .hid_virtual_cable = 0,
        .hid_remote_wake = 1,
        .hid_reconnect_initiate = 1,
        .hid_normally_connectable = true,
        .hid_boot_device = false, // not a keyboard/mouse
        .hid_ssr_host_max_latency = 0xffff,
        .hid_ssr_host_min_timeout = 0xffff,
        .hid_supervision_timeout = 3200,
        .hid_descriptor = gamepad_hid_descriptor,
        .hid_descriptor_size = sizeof(gamepad_hid_descriptor),
        .device_name = "Pico HID Gamepad Test"
    };

    // buffer/bytes for the service to query
    hid_create_sdp_record(
        hid_service_buffer,
        sdp_create_service_record_handle(),
        &hid_params
    );

    sdp_register_service(hid_service_buffer);

    // start HID device handling
    hid_device_init(
        false,
        sizeof(gamepad_hid_descriptor),
        gamepad_hid_descriptor
    );

    // handle HCI connection and HCI events
    hid_device_register_packet_handler(&packet_handler);

    static btstack_packet_callback_registration_t hci_event_callback_registration;
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup BT stack loop -> run it
    hci_power_control(HCI_POWER_ON);
    btstack_run_loop_execute();
    return 0;
}