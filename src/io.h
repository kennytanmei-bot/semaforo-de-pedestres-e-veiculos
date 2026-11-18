#ifndef IO_H
#define IO_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <stdbool.h>
#include <stdint.h>

struct pin {
    const struct device *port;
    gpio_pin_t num;
};

/* LEDs externos dos pedestres. */
extern const struct pin PIN_PED_RED;
extern const struct pin PIN_PED_GREEN;

/* Comunicacao entre as placas. */
extern const struct pin PIN_LINK_PHASE;
extern const struct pin PIN_LINK_NIGHT;

/* Entradas locais. */
extern const struct pin PIN_BUTTON;
extern const struct pin PIN_NIGHT_SWITCH;

void io_led_init(void);
void io_rgb_write(bool red_on, bool green_on);
void io_ped_write(bool red_on, bool green_on);
void io_init_out(const struct pin *p, int level);
void io_init_in(const struct pin *p, gpio_flags_t pull);
void io_write(const struct pin *p, int level);
int io_read(const struct pin *p);

struct deb {
    const struct pin *pin;
    bool active_low;
    uint8_t limit;
    uint8_t cnt;
    bool state;
};
bool io_deb_update(struct deb *d);

#define LOG_AT(ms, tag, fmt, ...) printk("[%u] %s: " fmt "\n", (unsigned int)(ms), tag, ##__VA_ARGS__)
#define LOG(tag, fmt, ...) LOG_AT(k_uptime_get_32(), tag, fmt, ##__VA_ARGS__)

#endif
