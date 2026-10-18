#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

int main(void) {
    stdio_init_all();
    sleep_ms(2000);  // give the USB serial connection time to enumerate

    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n");
        return -1;
    }
    printf("cyw43 init ok\n");

    int n = 0;
    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, n & 1);
        printf("tick %d\n", n++);
        sleep_ms(500);
    }
} 