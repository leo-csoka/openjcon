#include "joycon_descriptor.h"

// joycon data profile
const uint8_t joycon_hid_descriptor[] = {
    0x05, 0x01,                       // usage page: generic desktop
    0x09, 0x05,                       // usage: game pad
    0xa1, 0x01,                       // begin application collection
    0x06, 0x01, 0xff,                 // usage page: vendor-defined

    0x85, JOYCON_INPUT_SUBCOMMAND_REPLY, // report id: subcommand reply
    0x09, 0x21,                         // usage
    0x75, 0x08,                         // 8 bits per element
    0x95, 0x30,                         // 48 elements
    0x81, 0x02,                         // input data, variable, absolute

    0x85, JOYCON_INPUT_STANDARD,      // report id: standard input
    0x09, 0x30,                       // usage
    0x75, 0x08,                       // 8 bits per element
    0x95, 0x30,                       // 48 elements
    0x81, 0x02,                       // input data, variable, absolute

    0x85, JOYCON_INPUT_NFC,           // report id: input with nfc data
    0x09, 0x31,                       // usage
    0x75, 0x08,                       // 8 bits per element
    0x96, 0x69, 0x01,                 // 361 elements
    0x81, 0x02,                       // input data, variable, absolute

    0x85, JOYCON_INPUT_UNKNOWN_32,    // report id: 0x32
    0x09, 0x32,                       // usage
    0x75, 0x08,                       // 8 bits per element
    0x96, 0x69, 0x01,                 // 361 elements
    0x81, 0x02,                       // input data, variable, absolute

    0x85, JOYCON_INPUT_UNKNOWN_33,    // report id: 0x33
    0x09, 0x33,                       // usage
    0x75, 0x08,                       // 8 bits per element
    0x96, 0x69, 0x01,                 // 361 elements
    0x81, 0x02,                       // input data, variable, absolute

    0x85, JOYCON_INPUT_BUTTONS,       // report id: buttons
    0x05, 0x09,                       // usage page: buttons
    0x19, 0x01,                       // usage minimum: button 1
    0x29, 0x10,                       // usage maximum: button 16
    0x15, 0x00,                       // logical minimum: 0
    0x25, 0x01,                       // logical maximum: 1
    0x75, 0x01,                       // 1 bit per button
    0x95, 0x10,                       // 16 buttons
    0x81, 0x02,                       // input data, variable, absolute
    0x05, 0x01,                       // usage page: generic desktop
    0x09, 0x39,                       // usage: hat switch
    0x15, 0x00,                       // logical minimum: 0
    0x25, 0x07,                       // logical maximum: 7
    0x75, 0x04,                       // 4 bits
    0x95, 0x01,                       // 1 hat switch
    0x81, 0x42,                       // input data, variable, absolute, null state
    0x05, 0x09,                       // usage page: buttons
    0x75, 0x04,                       // 4 bits
    0x95, 0x01,                       // 1 padding field
    0x81, 0x01,                       // input constant, array, absolute
    0x05, 0x01,                       // usage page: generic desktop
    0x09, 0x30,                       // usage: x axis
    0x09, 0x31,                       // usage: y axis
    0x09, 0x33,                       // usage: rx axis
    0x09, 0x34,                       // usage: ry axis
    0x16, 0x00, 0x00,                 // logical minimum: 0
    0x27, 0xff, 0xff, 0x00, 0x00,     // logical maximum: 65535
    0x75, 0x10,                       // 16 bits per axis
    0x95, 0x04,                       // 4 axes
    0x81, 0x02,                       // input data, variable, absolute

    0x06, 0x01, 0xff,                 // usage page: vendor-defined
    0x85, JOYCON_OUTPUT_SUBCOMMAND,   // report id: subcommand output
    0x09, 0x01,                       // usage
    0x75, 0x08,                       // 8 bits per element
    0x95, 0x30,                       // 48 elements
    0x91, 0x02,                       // output data, variable, absolute

    0x85, JOYCON_OUTPUT_RUMBLE,       // report id: rumble output
    0x09, 0x10,                       // usage
    0x75, 0x08,                       // 8 bits per element
    0x95, 0x09,                       // 9 rumble bytes
    0x91, 0x02,                       // output data, variable, absolute

    0x85, JOYCON_OUTPUT_MCU_DATA,     // report id: mcu output
    0x09, 0x11,                       // usage
    0x75, 0x08,                       // 8 bits per element
    0x95, 0x30,                       // 48 elements
    0x91, 0x02,                       // output data, variable, absolute

    0x85, JOYCON_OUTPUT_UNKNOWN,      // report id: 0x12
    0x09, 0x12,                       // usage
    0x75, 0x08,                       // 8 bits per element
    0x95, 0x30,                       // 48 elements
    0x91, 0x02,                       // output data, variable, absolute

    0xc0                              // end application collection
};

const uint16_t joycon_hid_descriptor_size = sizeof(joycon_hid_descriptor);
