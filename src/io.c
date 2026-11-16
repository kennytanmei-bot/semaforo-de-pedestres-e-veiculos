#include "io.h"
#include "config.h"

#define PORT_B DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define PORT_D DEVICE_DT_GET(DT_NODELABEL(gpiod))

/* RGB onboard: PTB19 verde e PTB18 vermelho (ativos em nivel baixo). */
const struct pin PIN_LED0 = { PORT_B, 19 };
const struct pin PIN_LED1 = { PORT_B, 18 };
const struct pin PIN_LED2 = { PORT_B, 18 };

const struct pin PIN_LINK_PEDGO = { PORT_B, 3 };
const struct pin PIN_LINK_NIGHT = { PORT_D, 4 };
const struct pin PIN_LINK_REQ = { PORT_D, 5 };
const struct pin PIN_LOCAL_IN = { PORT_D, 0 };

void io_led_init(void)
{
	if (!device_is_ready(PORT_B)) {
		printk("ERRO: GPIOB nao pronto\n");
		return;
	}

	/* Configura os dois canais usados e inicia ambos apagados (nivel alto). */
	gpio_pin_configure(PORT_B, 19, GPIO_OUTPUT);
	gpio_pin_configure(PORT_B, 18, GPIO_OUTPUT);
	gpio_pin_set_raw(PORT_B, 19, 1);
	gpio_pin_set_raw(PORT_B, 18, 1);
}

void io_led_write(const struct pin *p, int on)
{
	/* RGB onboard e ativo em nivel baixo: 0 acende, 1 apaga. */
	int raw = on ? 0 : 1;

	if (p == &PIN_LED0) {
		gpio_pin_set_raw(PORT_B, 19, raw); /* verde */
		return;
	}

#if defined(ROLE_VEHICLE)
	if (p == &PIN_LED1) {
		/* Amarelo: canais vermelho e verde ligados simultaneamente. */
		gpio_pin_set_raw(PORT_B, 18, raw);
		gpio_pin_set_raw(PORT_B, 19, raw);
		return;
	}
#endif

	if (p == &PIN_LED1 || p == &PIN_LED2) {
		gpio_pin_set_raw(PORT_B, 18, raw); /* vermelho */
	}
}

void io_init_out(const struct pin *p, int level)
{
	if (!device_is_ready(p->port)) {
		printk("ERRO: GPIO nao pronto\n");
		return;
	}
	gpio_pin_configure(p->port, p->num,
			   level ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);
}

void io_init_in(const struct pin *p, gpio_flags_t pull)
{
	if (!device_is_ready(p->port)) {
		printk("ERRO: GPIO nao pronto\n");
		return;
	}
	gpio_pin_configure(p->port, p->num, GPIO_INPUT | pull);
}

void io_write(const struct pin *p, int level)
{
	gpio_pin_set_raw(p->port, p->num, level);
}

int io_read(const struct pin *p)
{
	return gpio_pin_get_raw(p->port, p->num);
}

bool io_deb_update(struct deb *d)
{
	bool raw = io_read(d->pin) > 0;
	bool active = d->active_low ? !raw : raw;

	if (active == d->state) {
		d->cnt = 0;
		return false;
	}
	if (++d->cnt >= d->limit) {
		d->state = active;
		d->cnt = 0;
		return true;
	}
	return false;
}
