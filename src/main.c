// import sdk basics, radio functions, and bluetooth classic lib
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "joycon_hid.h"

// service storage buffer used by btstack
static uint8_t hid_service_buffer[270];

// active hid connection id, or zero when disconnected
static uint16_t hid_cid;

// ------------------------------ //
// FUNCTIONS:

// handle hci and hid connection events
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    if (hci_event_packet_get_type(packet) == BTSTACK_EVENT_STATE &&
        btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
        printf("BTstack Classic is ready\n");
        gap_set_local_name("Pico Joy-Con");
        gap_discoverable_control(1);
        printf("Device is discoverable\n");
    }

    if (hci_event_packet_get_type(packet) == HCI_EVENT_HID_META) {
        switch (hci_event_hid_meta_get_subevent_code(packet)) {
            case HID_SUBEVENT_CONNECTION_OPENED: {
                uint8_t status = hid_subevent_connection_opened_get_status(packet);

                if (status != ERROR_CODE_SUCCESS) {
                    printf("HID connection failed: 0x%02x\n", status);
                    break;
                }

                hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                printf("HID host connected (cid %u)\n", hid_cid);
                break;
            }

            case HID_SUBEVENT_CONNECTION_CLOSED:
                if (hid_subevent_connection_closed_get_hid_cid(packet) == hid_cid) {
                    printf("HID host disconnected\n");
                    hid_cid = 0;
                }
                break;

            default:
                break;
        }
    }
}

int main(void) {
    // start io and radio
    stdio_init_all();

    if (cyw43_arch_init() != PICO_OK) {
        panic("Failed to initialize CYW43");
    }

    // start bluetooth classic protocols
    l2cap_init();
    sdp_init();

    // describe the hid service
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
        .hid_descriptor = joycon_hid_descriptor,
        .hid_descriptor_size = joycon_hid_descriptor_size,
        .device_name = "Pico Joy-Con"
    };

    // make the hid service discoverable to bluetooth hosts
    hid_create_sdp_record(
        hid_service_buffer,
        sdp_create_service_record_handle(),
        &hid_params
    );

    sdp_register_service(hid_service_buffer);

    // initialize the hid device using the joy-con report descriptor
    hid_device_init(
        false,
        joycon_hid_descriptor_size,
        joycon_hid_descriptor
    );

    // handle hid connection events and controller state changes
    hid_device_register_packet_handler(&packet_handler);

    static btstack_packet_callback_registration_t hci_event_callback_registration;
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // power on bluetooth and run the btstack event loop
    hci_power_control(HCI_POWER_ON);
    btstack_run_loop_execute();
    return 0;
}