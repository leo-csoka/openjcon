// import SDK Basics, Radio functions, and Bluetooth Classic Lib
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

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
}

int main(void) {
    // start I/O, radio
    stdio_init_all();

    if (cyw43_arch_init() != PICO_OK) {
        panic("Failed to initialize CYW43");
    }

    static btstack_packet_callback_registration_t hci_event_callback_registration;
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup BT stack loop -> run it
    hci_power_control(HCI_POWER_ON);
    btstack_run_loop_execute();
    return 0;
}