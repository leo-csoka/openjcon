#ifndef JOYCON_BT_H
#define JOYCON_BT_H

#include "joycon.h"

// sets up bluetooth classic hid for this joycon, timers can be added after this
void joycon_bt_init(Joycon *jc);

// runs the btstack loop (does not return)
void joycon_bt_run(void);

#endif
