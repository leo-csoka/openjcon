// bluetooth glue between btstack and a Joycon object
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "joycon_bt.h"
#include "joycon_descriptor.h"

// ---------- constants and state ---------- //

#define HID_SERVICE_BUFFER_SIZE 512
#define INPUT_REPORT_INTERVAL_MS 15

static uint8_t hid_service_buffer[HID_SERVICE_BUFFER_SIZE];
static uint8_t device_id_service_buffer[100];

static Joycon *active_joycon;
static uint16_t hid_cid;
static btstack_timer_source_t input_report_timer;
static btstack_packet_callback_registration_t hci_event_callback_registration;

// ---------- hid callbacks ---------- //

static void report_data_handler(uint16_t cid, hid_report_type_t report_type, uint16_t report_id, int report_size, uint8_t *report) {
    UNUSED(cid);
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        joycon_handle_output(active_joycon, (uint8_t)report_id, report, report_size);
    }
}

static void set_report_handler(uint16_t cid, hid_report_type_t report_type, int report_size, uint8_t *report) {
    UNUSED(cid);
    if (report_type == HID_REPORT_TYPE_OUTPUT && report_size > 1) {
        joycon_handle_output(active_joycon, report[0], report + 1, report_size - 1);
    }
}

static void input_report_timer_handler(btstack_timer_source_t *timer) {
    UNUSED(timer);
    if (hid_cid != 0) {
        hid_device_request_can_send_now_event(hid_cid);
    }
}

// ---------- event handler ---------- //

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
        gap_local_bd_addr(active_joycon->address);
        printf("BTstack Classic is ready\n");
    }

    if (event_type == HCI_EVENT_USER_CONFIRMATION_REQUEST) {
        bd_addr_t remote_addr;
        hci_event_user_confirmation_request_get_bd_addr(packet, remote_addr);
        printf("Accepting Bluetooth SSP confirmation request\n");
        gap_ssp_confirmation_response(remote_addr);
    }

    if (event_type != HCI_EVENT_HID_META) {
        return;
    }

    switch (hci_event_hid_meta_get_subevent_code(packet)) {
        case HID_SUBEVENT_CONNECTION_OPENED: {
            uint8_t status = hid_subevent_connection_opened_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                printf("HID connection failed: 0x%02x\n", status);
                break;
            }

            hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
            joycon_connected(active_joycon, to_ms_since_boot(get_absolute_time()));
            printf("HID host connected (cid %u)\n", hid_cid);
            hid_device_request_can_send_now_event(hid_cid);
            break;
        }

        case HID_SUBEVENT_CAN_SEND_NOW: {
            if (hid_cid == 0 || hid_subevent_can_send_now_get_hid_cid(packet) != hid_cid) {
                break;
            }

            uint8_t report[JOYCON_REPORT_SIZE];
            joycon_next_report(active_joycon, report, to_ms_since_boot(get_absolute_time()));
            hid_device_send_interrupt_message(hid_cid, report, sizeof(report));
            btstack_run_loop_set_timer(&input_report_timer, INPUT_REPORT_INTERVAL_MS);
            btstack_run_loop_add_timer(&input_report_timer);
            break;
        }

        case HID_SUBEVENT_CONNECTION_CLOSED:
            printf("HID host disconnected (active cid %u)\n", hid_cid);
            btstack_run_loop_remove_timer(&input_report_timer);
            hid_cid = 0;
            joycon_disconnected(active_joycon);
            break;

        default:
            break;
    }
}

// ---------- setup ---------- //

void joycon_bt_init(Joycon *jc) {
    active_joycon = jc;

    if (cyw43_arch_init() != PICO_OK) {
        panic("Failed to initialize CYW43");
    }

    gap_set_class_of_device(0x2508);
    gap_set_local_name(jc->name);
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
        .device_name = jc->side == JOYCON_LEFT ? "Joycon (L)" : "Joycon (R)"
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
        jc->side == JOYCON_LEFT ? 0x2006 : 0x2007,
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

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    printf("ready\n");
    hci_power_control(HCI_POWER_ON);
}

void joycon_bt_run(void) {
    btstack_run_loop_execute();
}
