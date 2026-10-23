#ifndef JOYCON_DESCRIPTOR_H
#define JOYCON_DESCRIPTOR_H

#include <stdint.h>

enum joycon_report_id {
    JOYCON_OUTPUT_SUBCOMMAND = 0x01,
    JOYCON_OUTPUT_FIRMWARE_UPDATE = 0x03,
    JOYCON_OUTPUT_RUMBLE = 0x10,
    JOYCON_OUTPUT_MCU_DATA = 0x11,
    JOYCON_OUTPUT_UNKNOWN = 0x12,
    JOYCON_INPUT_SUBCOMMAND_REPLY = 0x21,
    JOYCON_INPUT_FIRMWARE_UPDATE = 0x23,
    JOYCON_INPUT_STANDARD = 0x30,
    JOYCON_INPUT_NFC = 0x31,
    JOYCON_INPUT_UNKNOWN_32 = 0x32,
    JOYCON_INPUT_UNKNOWN_33 = 0x33,
    JOYCON_INPUT_BUTTONS = 0x3f
};

extern const uint8_t joycon_hid_descriptor[];
extern const uint16_t joycon_hid_descriptor_size;

#endif