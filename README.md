# picon
**picon** is an effort to emulate a Joy-Con using the original Raspberry Pi Pico W. This includes Bluetooth Classic HID support and a wired connection to the Switch. More information will be available soon.

## repository structure
```text
.
├── .gitignore
├── CMakeLists.txt
├── LICENSE
├── README.md
├── TODO.txt
├── pico-switch-controller.uf2
├── pico_sdk_import.cmake
├── push.sh
├── src/
│   ├── btstack_config.h
│   ├── btstack_config_common.h
│   ├── gamepad_hid.c
│   ├── gamepad_hid.h
│   └── main.c
└── testing/
    └── input.py
```

**last updated**: Oct 19, 2026
