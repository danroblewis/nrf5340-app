#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void) {
    printk("Hello from nRF5340!\n");
    while (1) { k_sleep(K_SECONDS(1)); }
}