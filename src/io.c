#include "io.h"

#define PORT_A DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define PORT_B DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define PORT_C DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define PORT_D DEVICE_DT_GET(DT_NODELABEL(gpiod))

/* FRDM-KL25Z:
 * PTB18 = canal vermelho do RGB (ativo em nivel baixo).
 * PTB19 = canal verde do RGB (ativo em nivel baixo).
 * PTC8  = LED vermelho externo dos pedestres (J1 pino 14).
 * PTC9  = LED verde externo dos pedestres (J1 pino 16).
 * PTB3  = PHASE (J10 pino 8).
 * PTD4  = NIGHT (J1 pino 6).
 * PTD0  = botao local (J2 pino 6).
 * PTA12 = chave noturna da placa A (J1 pino 8).
 */
const struct pin PIN_PED_RED      = { PORT_C, 8 };
const struct pin PIN_PED_GREEN    = { PORT_C, 9 };
const struct pin PIN_LINK_PHASE   = { PORT_B, 3 };
const struct pin PIN_LINK_NIGHT   = { PORT_D, 4 };
const struct pin PIN_BUTTON       = { PORT_D, 0 };
const struct pin PIN_NIGHT_SWITCH = { PORT_A, 12 };

void io_led_init(void)
{
    if (!device_is_ready(PORT_B) || !device_is_ready(PORT_C)) {
        printk("ERRO: GPIOB ou GPIOC nao pronto\n");
        return;
    }

    /* RGB integrado: vermelho e verde, ambos ativos em nivel baixo. */
    gpio_pin_configure(PORT_B, 18, GPIO_OUTPUT);
    gpio_pin_configure(PORT_B, 19, GPIO_OUTPUT);
    gpio_pin_set_raw(PORT_B, 18, 1);
    gpio_pin_set_raw(PORT_B, 19, 1);

    /* LEDs externos comuns: ativos em nivel alto. */
    gpio_pin_configure(PORT_C, 8, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(PORT_C, 9, GPIO_OUTPUT_INACTIVE);
}

void io_rgb_write(bool red_on, bool green_on)
{
    gpio_pin_set_raw(PORT_B, 18, red_on ? 0 : 1);
    gpio_pin_set_raw(PORT_B, 19, green_on ? 0 : 1);
}

void io_ped_write(bool red_on, bool green_on)
{
    gpio_pin_set_raw(PORT_C, 8, red_on ? 1 : 0);
    gpio_pin_set_raw(PORT_C, 9, green_on ? 1 : 0);
}

void io_init_out(const struct pin *p, int level)
{
    if (!device_is_ready(p->port)) {
        printk("ERRO: controlador GPIO nao pronto\n");
        return;
    }
    gpio_pin_configure(p->port, p->num,
                       level ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);
}

void io_init_in(const struct pin *p, gpio_flags_t pull)
{
    if (!device_is_ready(p->port)) {
        printk("ERRO: controlador GPIO nao pronto\n");
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
