#include "gamepad_hid.h"

// controller data profile
const uint8_t gamepad_hid_descriptor[] = {
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

const uint16_t gamepad_hid_descriptor_size = sizeof(gamepad_hid_descriptor);