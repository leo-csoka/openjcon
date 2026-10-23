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
├── pico_sdk_import.cmake
├── build.sh
├── push.sh
├── src/
│   ├── btstack_config.h
│   ├── btstack_config_common.h
│   ├── joycon.c
│   ├── joycon.h
│   ├── joycon_bt.c
│   ├── joycon_bt.h
│   ├── joycon_descriptor.c
│   ├── joycon_descriptor.h
│   └── main.c
```

**last updated**: Oct 22, 2026
