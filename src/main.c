#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void) {
    while (1) {
        printk("Hello from nRF5340!\n");
        k_sleep(K_SECONDS(1));
    }
}