/*
 * Semaforo de PEDESTRES (verde 4 s, vermelho 4 s) - escravo do semaforo de veiculos.
 *
 * - 2 threads independentes, uma por LED (verde e vermelho).
 * - Exclusao mutua: mesmo mutex + variavel 'lit' (so um LED aceso por vez).
 * - Sincronismo: o verde comeca na subida da linha PED_GO (carros em vermelho) e
 *   termina em 4 s ou na descida de PED_GO. O vermelho espera a proxima subida.
 *   Sem sinal de PED_GO o pedestre permanece em VERMELHO (falha segura).
 * - A thread principal le: PED_GO, NIGHT (da placa de veiculos) e o botao de travessia.
 */
#include "config.h"

#ifdef ROLE_PEDESTRIAN

#include "app.h"
#include "io.h"

enum phase { PP_GREEN, PP_RED, PP_COUNT };

struct led_cfg {
	enum phase ph;
	const struct pin *pin;
	const char *name;
};

static const struct led_cfg leds[PP_COUNT] = {
	[PP_GREEN] = { PP_GREEN, &PIN_LED0, "VERDE" },
	[PP_RED]   = { PP_RED,   &PIN_LED1, "VERMELHO" },
};

#define NO_LED (-1)
#define LED_STACK_SIZE 1024
#define LED_PRIO 5

static K_MUTEX_DEFINE(lock);
static K_CONDVAR_DEFINE(cv);

/* Estado compartilhado (protegido por 'lock') */
static enum mode cur_mode = MODE_NORMAL;
static enum phase turn = PP_RED;
static bool ped_go;       /* nivel (debounced) da linha PED_GO     */
static bool go_pending;   /* subida de PED_GO ainda nao consumida  */
static int lit = NO_LED;

K_THREAD_STACK_ARRAY_DEFINE(stacks, PP_COUNT, LED_STACK_SIZE);
static struct k_thread tcb[PP_COUNT];

static bool led_take(const struct led_cfg *c, uint32_t *started)
{
	if (lit != NO_LED && lit != (int)c->ph) {
		return false;
	}
	lit = c->ph;
	io_led_write(c->pin, 1);
	*started = k_uptime_get_32();
	LOG_AT(*started, "PED", "%s ON", c->name);
	return true;
}

static void led_release(const struct led_cfg *c, uint32_t started)
{
	uint32_t ended;

	io_led_write(c->pin, 0);
	ended = k_uptime_get_32();
	if (lit == (int)c->ph) {
		lit = NO_LED;
	}
	LOG_AT(ended, "PED", "%s OFF | duracao=%u ms",
	       c->name, (unsigned int)(ended - started));
	k_condvar_broadcast(&cv);
}

static void hold_until(uint32_t started, uint32_t duration, enum mode m)
{
	uint32_t deadline = started + duration;

	while (cur_mode == m) {
		int32_t left = (int32_t)(deadline - k_uptime_get_32());
		if (left <= 0) {
			break;
		}
		k_condvar_wait(&cv, &lock, K_MSEC(left));
	}
}

static void hold_night(uint32_t ms)
{
	int64_t end = k_uptime_get() + ms;

	while (cur_mode == MODE_NIGHT) {
		int64_t left = end - k_uptime_get();
		if (left <= 0) {
			break;
		}
		k_condvar_wait(&cv, &lock, K_MSEC(left));
	}
}

/* Verde: dura os 4 s completos. A queda de PED_GO nao encurta a travessia. */
static void hold_green(uint32_t started)
{
	hold_until(started, PED_GREEN_MS, MODE_NORMAL);
}

/* Vermelho: permanece aceso enquanto nao houver uma nova autorizacao PED_GO.
 * No ciclo normal, o intervalo entre travessias e formado pelo verde (3 s)
 * e amarelo (1 s) dos veiculos; assim, o vermelho dos pedestres fica aceso
 * durante esse intervalo e nunca libera a travessia por conta propria.
 */
static void hold_red(uint32_t started)
{
#if defined(PED_STANDALONE_TEST)
	/* Teste unitario: vermelho dura 4 s sem depender da placa de veiculos. */
	hold_until(started, PED_RED_MS, MODE_NORMAL);
#else
	bool warned = false;

	while (cur_mode == MODE_NORMAL && !(go_pending && ped_go)) {
		if (!warned) {
			int32_t left = (int32_t)(started + PED_SYNC_TIMEOUT_MS - k_uptime_get_32());
			if (left > 0) {
				k_condvar_wait(&cv, &lock, K_MSEC(left));
				continue;
			}
			LOG("PED", "AVISO sem autorizacao; permanece VERMELHO");
			warned = true;
		}
		k_condvar_wait(&cv, &lock, K_FOREVER);
	}

	if (cur_mode == MODE_NORMAL) {
		go_pending = false;
	}
#endif
}

static void led_thread(void *p1, void *p2, void *p3)
{
	const struct led_cfg *c = p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_mutex_lock(&lock, K_FOREVER);

	for (;;) {
		uint32_t led_started;

		/* ---------- MODO NOTURNO: so o vermelho pisca ---------- */
		if (cur_mode == MODE_NIGHT) {
			if (c->ph != PP_RED || !led_take(c, &led_started)) {
				k_condvar_wait(&cv, &lock, K_FOREVER);
				continue;
			}
			hold_until(led_started, NIGHT_ON_MS, MODE_NIGHT);
			led_release(c, led_started);
			hold_night(NIGHT_OFF_MS);
			continue;
		}

		/* ---------- MODO NORMAL ---------- */
		if (turn != c->ph || !led_take(c, &led_started)) {
			k_condvar_wait(&cv, &lock, K_FOREVER);
			continue;
		}

		if (c->ph == PP_GREEN) {
			hold_green(led_started);
		} else {
			hold_red(led_started);
		}

		led_release(c, led_started);

		if (cur_mode == MODE_NORMAL) {
			turn = (enum phase)((c->ph + 1) % PP_COUNT);
			k_condvar_broadcast(&cv);
		}
	}
}

void app_run(void)
{
	io_led_init();

	LOG("PED", "%s",
#if defined(PED_STANDALONE_TEST)
	    "TESTE UNITARIO ISOLADO"
#else
	    "INTEGRADO (aguardando PED_GO)"
#endif
	);

	for (int i = 0; i < PP_COUNT; i++) {
		k_thread_create(&tcb[i], stacks[i], K_THREAD_STACK_SIZEOF(stacks[i]),
				led_thread, (void *)&leds[i], NULL, NULL,
				LED_PRIO, 0, K_NO_WAIT);
		k_thread_name_set(&tcb[i], leds[i].name);
	}

#if defined(PED_STANDALONE_TEST)
	/* Teste isolado: ciclo autonomo verde 4 s / vermelho 4 s.
	 * Nao configura nem consulta PED_GO, NIGHT ou botao/REQ.
	 */
	for (;;) {
		k_sleep(K_SECONDS(1));
	}
#else
	/* Modo integrado: sinais de autorizacao, modo noturno e botao. */
	{
		struct deb go = { &PIN_LINK_PEDGO, false, DEBOUNCE_LINK, 0, false };
		struct deb nt = { &PIN_LINK_NIGHT, false, DEBOUNCE_LINK, 0, false };
		struct deb bt = { &PIN_LOCAL_IN, true, DEBOUNCE_BTN, 0, false };
		int64_t req_off = 0;

		io_init_out(&PIN_LINK_REQ, 0);
		io_init_in(&PIN_LINK_PEDGO, GPIO_PULL_DOWN);
		io_init_in(&PIN_LINK_NIGHT, GPIO_PULL_DOWN);
		io_init_in(&PIN_LOCAL_IN, GPIO_PULL_UP);

		for (;;) {
			bool go_ch, nt_ch, bt_ch;
			int64_t now;

			k_msleep(POLL_MS);
			go_ch = io_deb_update(&go);
			nt_ch = io_deb_update(&nt);
			bt_ch = io_deb_update(&bt);
			now = k_uptime_get();

			if (req_off != 0 && now >= req_off) {
				io_write(&PIN_LINK_REQ, 0);
				req_off = 0;
			}

			if (bt_ch && bt.state) {
				io_write(&PIN_LINK_REQ, 1);
				req_off = now + REQ_PULSE_MS;
				LOG("PED", "botao travessia pressionado");
			}

			if (!go_ch && !nt_ch) {
				continue;
			}

			k_mutex_lock(&lock, K_FOREVER);

			if (nt_ch) {
				cur_mode = nt.state ? MODE_NIGHT : MODE_NORMAL;
				if (cur_mode == MODE_NORMAL) {
					turn = PP_RED;
					go_pending = false;
				}
				LOG("PED", "modo %s", nt.state ? "NOTURNO" : "NORMAL");
			}

			if (go_ch) {
				ped_go = go.state;
				if (ped_go && cur_mode == MODE_NORMAL) {
					go_pending = true;
				}
				LOG("PED", "PED_GO=%d", ped_go ? 1 : 0);
			}

			k_condvar_broadcast(&cv);
			k_mutex_unlock(&lock);
		}
	}
#endif
}

#endif /* ROLE_PEDESTRIAN */
