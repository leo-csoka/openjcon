#include "pico/stdlib.h"
#include "btstack.h"
#include <stdio.h>
#include <string.h>
#include "joycon.h"
#include "joycon_bt.h"

#define BUFFER_SIZE 128

static Joycon *jc;
static btstack_timer_source_t serial_timer;

static void serial_tick(btstack_timer_source_t *timer) {
    static char buffer[BUFFER_SIZE];
    static int buffer_index = 0;

    while (true) {
        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) break;

        if (c == '\n' || c == '\r') {
            buffer[buffer_index] = '\0';

            if (strcmp(buffer, "DDOWN") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "DDOWN");
                }
            } else if (strcmp(buffer, "DUP") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "DUP");
                }
            } else if (strcmp(buffer, "DLEFT") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "DLEFT");
                }
            } else if (strcmp(buffer, "DRIGHT") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "DRIGHT");
                }
            } else if (strcmp(buffer, "CAPTURE") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "CAPTURE");
                }
            } else if (strcmp(buffer, "MINUS") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "MINUS");
                }
            } else if (strcmp(buffer, "LSTICK") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "LSTICK");
                }
            } else if (strcmp(buffer, "SL") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "SL");
                }
            } else if (strcmp(buffer, "SR") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "SR");
                }
            } else if (strcmp(buffer, "ZL") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "ZL");
                }
            } else if (strcmp(buffer, "L") == 0) {
                if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
                    jc->tap(jc, "L");
                }
            }

            buffer_index = 0;

        } else if (buffer_index < BUFFER_SIZE - 1) {
            buffer[buffer_index++] = (char)c;
        }
    }

    btstack_run_loop_set_timer(timer, 20);
    btstack_run_loop_add_timer(timer);
}


int main(void) {
    stdio_init_all();

    jc = joycon_create(JOYCON_LEFT, JC_COLOR_NEON_ORANGE);
    joycon_bt_init(jc);

    btstack_run_loop_set_timer_handler(&serial_timer, serial_tick);
    btstack_run_loop_set_timer(&serial_timer, 20);
    btstack_run_loop_add_timer(&serial_timer);

    joycon_bt_run();
    return 0;
}
