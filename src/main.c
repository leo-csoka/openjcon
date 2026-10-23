#include "pico/stdlib.h"
#include "btstack.h"
#include "joycon.h"
#include "joycon_bt.h"

// pair the joycon, then press dpad down every 2 seconds
static Joycon *jc;
static btstack_timer_source_t demo_timer;

static void demo_tick(btstack_timer_source_t *timer) {
    if (jc->get_pair_state(jc) == JOYCON_CONNECTED) {
        jc->tap(jc, "DDOWN");
    }
    btstack_run_loop_set_timer(timer, 2000);
    btstack_run_loop_add_timer(timer);
}

int main(void) {
    stdio_init_all();

    jc = joycon_create(JOYCON_LEFT, JC_COLOR_NEON_BLUE);
    joycon_bt_init(jc);

    btstack_run_loop_set_timer_handler(&demo_timer, demo_tick);
    btstack_run_loop_set_timer(&demo_timer, 2000);
    btstack_run_loop_add_timer(&demo_timer);

    joycon_bt_run();
    return 0;
}
