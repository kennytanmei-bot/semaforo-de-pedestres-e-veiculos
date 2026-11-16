/*
 * Semaforo de VEICULOS (verde 3 s, amarelo 1 s, vermelho 4 s).
 *
 * - 3 threads independentes, uma por LED (verde, amarelo, vermelho).
 * - Exclusao mutua: um unico mutex protege o estado compartilhado e a variavel
 *   'lit' (qual LED esta aceso). Um LED so acende se 'lit' estiver livre.
 * - Espera por tempo/evento com k_condvar_wait() (libera o mutex enquanto espera).
 * - A thread principal le as entradas (chave noturna, pedido de travessia).
 */
#include "config.h"

#ifdef ROLE_VEHICLE

#include "app.h"
#include "io.h"

enum phase { PH_GREEN, PH_YELLOW, PH_RED, PH_COUNT };

struct led_cfg {
	enum phase ph;
	const struct pin *pin;
	const char *name;
};

static const struct led_cfg leds[PH_COUNT] = {
	[PH_GREEN]  = { PH_GREEN,  &PIN_LED0, "VERDE" },
	[PH_YELLOW] = { PH_YELLOW, &PIN_LED1, "AMARELO" },
	[PH_RED]    = { PH_RED,    &PIN_LED2, "VERMELHO" },
};

#define NO_LED (-1)
#define LED_STACK_SIZE 1024
#define LED_PRIO 5

static K_MUTEX_DEFINE(lock);
static K_CONDVAR_DEFINE(cv);

/* Estado compartilhado (protegido por 'lock') */
static enum mode cur_mode = MODE_NORMAL;
static enum phase turn = PH_GREEN;   /* de quem e a vez no modo normal */
static bool cross_req;               /* pedido de travessia pendente   */
static int lit = NO_LED;             /* LED aceso no momento           */

K_THREAD_STACK_ARRAY_DEFINE(stacks, PH_COUNT, LED_STACK_SIZE);
static struct k_thread tcb[PH_COUNT];

/* ---- helpers (chamar com 'lock' travado) ---- */

static bool led_take(const struct led_cfg *c, uint32_t *started)
{
	if (lit != NO_LED && lit != (int)c->ph) {
		return false;
	}
	lit = c->ph;
	io_led_write(c->pin, 1);
	*started = k_uptime_get_32(); /* instante proximo da transicao fisica */
	LOG_AT(*started, "VEH", "%s ON", c->name);
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
	LOG_AT(ended, "VEH", "%s OFF | duracao=%u ms",
	       c->name, (unsigned int)(ended - started));
	k_condvar_broadcast(&cv);
}

/* Espera ate um prazo absoluto contado a partir do instante em que o LED acendeu.
 * Assim, o tempo gasto imprimindo o log ON nao e somado aos 3 s da fase.
 */
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

/* Aguarda um intervalo contado a partir de um marco de tempo especifico. */
static void hold_from(uint32_t started, uint32_t duration, enum mode m)
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

/* ---- thread de LED ---- */

static void led_thread(void *p1, void *p2, void *p3)
{
	const struct led_cfg *c = p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_mutex_lock(&lock, K_FOREVER);

	for (;;) {
		uint32_t led_started;

		/* ---------- MODO NOTURNO: so o amarelo pisca ---------- */
		if (cur_mode == MODE_NIGHT) {
			if (c->ph != PH_YELLOW || !led_take(c, &led_started)) {
				k_condvar_wait(&cv, &lock, K_FOREVER);
				continue;
			}
			hold_until(led_started, NIGHT_ON_MS, MODE_NIGHT);
			led_release(c, led_started);
			hold_from(k_uptime_get_32(), NIGHT_OFF_MS, MODE_NIGHT);
			continue;
		}

		/* ---------- MODO NORMAL ---------- */
		if (turn != c->ph || !led_take(c, &led_started)) {
			k_condvar_wait(&cv, &lock, K_FOREVER);
			continue;
		}

		switch (c->ph) {
		case PH_GREEN:
			/* Verde permanece aceso por 3 s completos, mesmo com REQ. */
			hold_until(led_started, VEH_GREEN_MS, MODE_NORMAL);
			break;
		case PH_YELLOW:
			hold_until(led_started, VEH_YELLOW_MS, MODE_NORMAL);
			break;
		case PH_RED: {
			/* Consome apenas o pedido que ja estava pendente ao iniciar o vermelho.
			 * Um novo pedido durante esta fase fica guardado para o proximo ciclo.
			 */
			bool serve_request = cross_req;
			uint32_t red_duration = VEH_RED_MS;

			if (serve_request) {
				uint32_t ped_go_started;
				cross_req = false;

				io_write(&PIN_LINK_PEDGO, 1);
				ped_go_started = k_uptime_get_32();
				LOG_AT(ped_go_started, "VEH", "PED_GO=1");

				/* Mantem PED_GO alto por 4 s completos mais a margem de
				 * leitura/debounce da outra placa. O vermelho dos veiculos
				 * acompanha esse prazo para nunca liberar carros antes do pedestre.
				 */
				if (PED_GREEN_MS + PED_SYNC_GUARD_MS > red_duration) {
					red_duration = PED_GREEN_MS + PED_SYNC_GUARD_MS;
				}
				hold_until(led_started, red_duration, MODE_NORMAL);

				io_write(&PIN_LINK_PEDGO, 0);
				LOG("VEH", "PED_GO=0");
			} else {
				/* Sem pedido, vermelho veicular dura exatamente o tempo nominal. */
				hold_until(led_started, red_duration, MODE_NORMAL);
			}
			break;
		}
		default:
			break;
		}

		led_release(c, led_started);

		if (cur_mode == MODE_NORMAL) {
			turn = (enum phase)((c->ph + 1) % PH_COUNT);
			k_condvar_broadcast(&cv);
		}
	}
}

/* ---- thread principal: entradas ---- */

void app_run(void)
{
	io_led_init();
	io_init_out(&PIN_LINK_PEDGO, 0);
	io_init_out(&PIN_LINK_NIGHT, 0);

	LOG("VEH", "modo %s", 
#if defined(VEH_STANDALONE_TEST)
	    "TESTE UNITARIO ISOLADO"
#else
	    "INTEGRADO (aguardando sinais/botao)"
#endif
	);

	for (int i = 0; i < PH_COUNT; i++) {
		k_thread_create(&tcb[i], stacks[i], K_THREAD_STACK_SIZEOF(stacks[i]),
				led_thread, (void *)&leds[i], NULL, NULL,
				LED_PRIO, 0, K_NO_WAIT);
		k_thread_name_set(&tcb[i], leds[i].name);
	}

#if defined(VEH_STANDALONE_TEST)
	/* Teste isolado: nao le REQ nem a chave NIGHT; executa o ciclo local. */
	for (;;) {
		k_sleep(K_SECONDS(1));
	}
#else
	/* Modo integrado: recebe pedido da placa de pedestres e chave noturna. */
	{
		struct deb sw  = { &PIN_LOCAL_IN, true,  DEBOUNCE_BTN,  0, false };
		struct deb req = { &PIN_LINK_REQ, false, DEBOUNCE_LINK, 0, false };

		io_init_in(&PIN_LINK_REQ, GPIO_PULL_DOWN);
		io_init_in(&PIN_LOCAL_IN, GPIO_PULL_UP);

		for (;;) {
			bool sw_ch, req_rise;

			k_msleep(POLL_MS);
			sw_ch = io_deb_update(&sw);
			req_rise = io_deb_update(&req) && req.state;

			if (!sw_ch && !req_rise) {
				continue;
			}

			k_mutex_lock(&lock, K_FOREVER);

			if (sw_ch) {
				cur_mode = sw.state ? MODE_NIGHT : MODE_NORMAL;
				io_write(&PIN_LINK_NIGHT, sw.state);
				if (cur_mode == MODE_NORMAL) {
					turn = PH_GREEN;
					cross_req = false;
				}
				LOG("VEH", "modo %s", sw.state ? "NOTURNO" : "NORMAL");
			}

			if (req_rise) {
				if (cur_mode == MODE_NORMAL) {
					if (!cross_req) {
						cross_req = true;
						LOG("VEH", "REQ aceito e aguardando fase vermelha");
					} else {
						LOG("VEH", "REQ duplicado; ja existe pedido pendente");
					}
				} else {
					LOG("VEH", "REQ ignorado: modo noturno");
				}
			}

			k_condvar_broadcast(&cv);
			k_mutex_unlock(&lock);
		}
	}
#endif
}

#endif /* ROLE_VEHICLE */
