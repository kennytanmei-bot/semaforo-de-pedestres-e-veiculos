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

/* LEDs RGB integrados da FRDM-KL25Z:
 * PTB19 = verde; PTB18 = vermelho.
 * No semaforo veicular, amarelo = vermelho + verde.
 * Os LEDs RGB da placa sao ativos em nivel baixo.
 */
extern const struct pin PIN_LED0, PIN_LED1, PIN_LED2;
extern const struct pin PIN_LINK_PEDGO, PIN_LINK_NIGHT, PIN_LINK_REQ;
extern const struct pin PIN_LOCAL_IN;

void io_led_init(void);
void io_led_write(const struct pin *p, int on);
void io_init_out(const struct pin *p, int level);
void io_init_in(const struct pin *p, gpio_flags_t pull);
void io_write(const struct pin *p, int level);
int io_read(const struct pin *p);

/* Debounce por amostragem */
struct deb {
	const struct pin *pin;
	bool active_low;
	uint8_t limit;
	uint8_t cnt;
	bool state;
};
bool io_deb_update(struct deb *d);

/* Log com timestamp capturado no instante da transicao da saida. */
#define LOG_AT(ms, tag, fmt, ...) printk("[%u] %s: " fmt "\n", (unsigned int)(ms), tag, ##__VA_ARGS__)
#define LOG(tag, fmt, ...) LOG_AT(k_uptime_get_32(), tag, fmt, ##__VA_ARGS__)

#endif
