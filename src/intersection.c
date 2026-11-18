#include <zephyr/kernel.h>
#include "config.h"
#include "app.h"
#include "io.h"


static K_MUTEX_DEFINE(display_lock);
static enum vehicle_light display_vehicle = VEH_LIGHT_RED;
static enum pedestrian_light display_pedestrian = PED_LIGHT_RED;

void intersection_set_lights(enum vehicle_light vehicle,
                             enum pedestrian_light pedestrian)
{
    k_mutex_lock(&display_lock, K_FOREVER);
    display_vehicle = vehicle;
    display_pedestrian = pedestrian;
    k_mutex_unlock(&display_lock);
}

enum vehicle_light intersection_get_vehicle_light(void)
{
    enum vehicle_light value;
    k_mutex_lock(&display_lock, K_FOREVER);
    value = display_vehicle;
    k_mutex_unlock(&display_lock);
    return value;
}

enum pedestrian_light intersection_get_pedestrian_light(void)
{
    enum pedestrian_light value;
    k_mutex_lock(&display_lock, K_FOREVER);
    value = display_pedestrian;
    k_mutex_unlock(&display_lock);
    return value;
}

static void log_vehicle_change(enum vehicle_light *last,
                               enum vehicle_light next, const char *street)
{
    if (*last == next) {
        return;
    }
    *last = next;
    switch (next) {
    case VEH_LIGHT_GREEN:  LOG("CROSS", "Rua %s: VEICULOS VERDE", street); break;
    case VEH_LIGHT_YELLOW: LOG("CROSS", "Rua %s: VEICULOS AMARELO", street); break;
    case VEH_LIGHT_RED:    LOG("CROSS", "Rua %s: VEICULOS VERMELHO", street); break;
    default:               LOG("CROSS", "Rua %s: VEICULOS APAGADO", street); break;
    }
}

static void log_ped_change(enum pedestrian_light *last,
                           enum pedestrian_light next, const char *street)
{
    if (*last == next) {
        return;
    }
    *last = next;
    switch (next) {
    case PED_LIGHT_GREEN: LOG("CROSS", "Rua %s: PEDESTRES VERDE", street); break;
    case PED_LIGHT_RED:   LOG("CROSS", "Rua %s: PEDESTRES VERMELHO", street); break;
    default:              LOG("CROSS", "Rua %s: PEDESTRES APAGADO", street); break;
    }
}

void app_run(void)
{
    struct deb button = { &PIN_BUTTON, true, DEBOUNCE_BTN, 0, false };
#if defined(ROLE_STREET_A)
    struct deb night_switch = { &PIN_NIGHT_SWITCH, true, DEBOUNCE_SWITCH, 0, false };
#endif

    bool night = false;
    bool phase_b = false;       /* false: Rua A; true: Rua B */
    bool phase_valid = false;
    bool request_pending = false;
    bool pedestrian_green = false;
    uint32_t phase_started = k_uptime_get_32();
    uint32_t pedestrian_deadline = 0;
    uint32_t night_started = 0;

    enum vehicle_light last_vehicle = VEH_LIGHT_OFF;
    enum pedestrian_light last_pedestrian = PED_LIGHT_OFF;

    io_led_init();
    io_init_in(&PIN_BUTTON, GPIO_PULL_UP);

#if defined(ROLE_STREET_A)
    /* Placa A coordena as fases e transmite o estado noturno. */
    io_init_out(&PIN_LINK_PHASE, 0);
    io_init_out(&PIN_LINK_NIGHT, 0);
    io_init_in(&PIN_NIGHT_SWITCH, GPIO_PULL_UP);
    phase_valid = true;
    LOG("CROSS", "Placa A: coordenadora do cruzamento");
#else
    /* Placa B recebe a fase e o modo noturno da Placa A. */
    io_init_in(&PIN_LINK_PHASE, GPIO_PULL_DOWN);
    io_init_in(&PIN_LINK_NIGHT, GPIO_PULL_DOWN);
    phase_b = io_read(&PIN_LINK_PHASE) > 0;
    phase_valid = false; /* espera a primeira borda para sincronizar com A */
    LOG("CROSS", "Placa B: aguardando sincronizacao da Placa A");
#endif

    intersection_set_lights(VEH_LIGHT_RED, PED_LIGHT_RED);
    vehicle_thread_start();
    pedestrian_thread_start();
    last_vehicle = VEH_LIGHT_RED;
    last_pedestrian = PED_LIGHT_RED;

    for (;;) {
        bool button_changed;
        uint32_t now;
        bool phase_edge = false;

        k_msleep(POLL_MS);
        now = k_uptime_get_32();
        button_changed = io_deb_update(&button);

        /* Solicitações são memorizadas até o início da próxima fase segura. */
        if (button_changed && button.state) {
            if (!night) {
                request_pending = true;
                LOG("CROSS", "Botao da Rua %s pressionado; pedido armazenado",
#if defined(ROLE_STREET_A)
                    "A"
#else
                    "B"
#endif
                );
            } else {
                LOG("CROSS", "Pedido ignorado durante o modo noturno");
            }
        }

#if defined(ROLE_STREET_A)
        {
            bool switch_changed = io_deb_update(&night_switch);
            if (switch_changed) {
                night = night_switch.state;
                io_write(&PIN_LINK_NIGHT, night ? 1 : 0);
                if (night) {
                    night_started = now;
                    phase_b = false;
                    io_write(&PIN_LINK_PHASE, 0);
                    pedestrian_green = false;
                    LOG("CROSS", "MODO NOTURNO ATIVADO nas duas ruas");
                } else {
                    phase_b = false;
                    io_write(&PIN_LINK_PHASE, 0);
                    phase_started = now;
                    phase_valid = true;
                    pedestrian_green = false;
                    request_pending = false;
                    LOG("CROSS", "MODO NORMAL retomado; iniciando pela Rua A");
                }
            }
        }
#else
        {
            bool new_night = io_read(&PIN_LINK_NIGHT) > 0;
            if (new_night != night) {
                night = new_night;
                pedestrian_green = false;
                if (night) {
                    night_started = now;
                    LOG("CROSS", "MODO NOTURNO recebido da Placa A");
                } else {
                    phase_valid = false;
                    phase_b = io_read(&PIN_LINK_PHASE) > 0;
                    request_pending = false;
                    LOG("CROSS", "MODO NORMAL recebido; aguardando nova sincronizacao");
                }
            }
        }
#endif

        if (!night) {
#if defined(ROLE_STREET_A)
            if ((uint32_t)(now - phase_started) >= PHASE_PERIOD_MS) {
                phase_b = !phase_b;
                phase_started = now;
                io_write(&PIN_LINK_PHASE, phase_b ? 1 : 0);
                phase_edge = true;
                LOG("CROSS", "Fase liberada: Rua %s",
                    phase_b ? "B" : "A");
            }
#else
            {
                bool incoming_phase = io_read(&PIN_LINK_PHASE) > 0;
                if (!phase_valid || incoming_phase != phase_b) {
                    if (incoming_phase != phase_b) {
                        phase_b = incoming_phase;
                        phase_started = now;
                        phase_valid = true;
                        phase_edge = true;
                        LOG("CROSS", "Sincronizado: Rua %s liberada",
                            phase_b ? "B" : "A");
                    }
                }
            }
#endif

            if (phase_edge) {
#if defined(ROLE_STREET_A)
                bool pedestrian_phase = phase_b;   /* pedestres A cruzam quando B passa */
#else
                bool pedestrian_phase = !phase_b;  /* pedestres B cruzam quando A passa */
#endif
                pedestrian_green = pedestrian_phase && request_pending;
                if (pedestrian_green) {
                    pedestrian_deadline = now + PED_GREEN_MS;
                    request_pending = false;
                    LOG("CROSS", "Travessia autorizada por %u ms", PED_GREEN_MS);
                }
            }

            if (pedestrian_green && (int32_t)(now - pedestrian_deadline) >= 0) {
                pedestrian_green = false;
                LOG("CROSS", "Tempo de travessia encerrado");
            }

            /* Veículos: a placa A libera A quando phase_b=0; a B libera B
             * quando phase_b=1. A outra rua permanece vermelha.
             */
            {
                bool own_street_active;
                uint32_t elapsed = now - phase_started;
                enum vehicle_light vehicle_state;
#if defined(ROLE_STREET_A)
                own_street_active = phase_valid && !phase_b;
#else
                own_street_active = phase_valid && phase_b;
#endif
                if (!own_street_active) {
                    vehicle_state = VEH_LIGHT_RED;
                } else if (elapsed < VEH_GREEN_MS) {
                    vehicle_state = VEH_LIGHT_GREEN;
                } else if (elapsed < (VEH_GREEN_MS + VEH_YELLOW_MS)) {
                    vehicle_state = VEH_LIGHT_YELLOW;
                } else {
                    vehicle_state = VEH_LIGHT_RED; /* intervalo de todos vermelhos */
                }
                intersection_set_lights(vehicle_state, pedestrian_green ? PED_LIGHT_GREEN : PED_LIGHT_RED);
                log_vehicle_change(&last_vehicle, vehicle_state,
#if defined(ROLE_STREET_A)
                                   "A"
#else
                                   "B"
#endif
                );
            }

            log_ped_change(&last_pedestrian,
                           pedestrian_green ? PED_LIGHT_GREEN : PED_LIGHT_RED,
#if defined(ROLE_STREET_A)
                           "A"
#else
                           "B"
#endif
            );
        } else {
            /* Modo noturno: amarelo veicular e vermelho de pedestres piscam
             * com período de 1 s ligado e 1 s desligado.
             */
            bool blink_on = ((uint32_t)(now - night_started) % NIGHT_PERIOD_MS) < NIGHT_ON_MS;
            pedestrian_green = false;
            intersection_set_lights(blink_on ? VEH_LIGHT_YELLOW : VEH_LIGHT_OFF,
                                    blink_on ? PED_LIGHT_RED : PED_LIGHT_OFF);

            log_vehicle_change(&last_vehicle,
                               blink_on ? VEH_LIGHT_YELLOW : VEH_LIGHT_OFF,
#if defined(ROLE_STREET_A)
                               "A"
#else
                               "B"
#endif
            );
            log_ped_change(&last_pedestrian,
                           blink_on ? PED_LIGHT_RED : PED_LIGHT_OFF,
#if defined(ROLE_STREET_A)
                           "A"
#else
                           "B"
#endif
            );
        }
    }
}
