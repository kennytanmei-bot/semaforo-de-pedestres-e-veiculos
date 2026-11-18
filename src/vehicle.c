#include "app.h"
#include "io.h"

/* Converte o estado logico do semaforo veicular para os canais vermelho e
 * verde do LED RGB integrado. Amarelo = vermelho + verde.
 */
void vehicle_show(enum vehicle_light light)
{
    switch (light) {
    case VEH_LIGHT_RED:
        io_rgb_write(true, false);
        break;
    case VEH_LIGHT_YELLOW:
        io_rgb_write(true, true);
        break;
    case VEH_LIGHT_GREEN:
        io_rgb_write(false, true);
        break;
    case VEH_LIGHT_OFF:
    default:
        io_rgb_write(false, false);
        break;
    }
}


/* Thread dedicada a atualizar o RGB dos veiculos a partir do estado
 * calculado pela maquina de estados do cruzamento.
 */
#define VEHICLE_STACK_SIZE 1024
K_THREAD_STACK_DEFINE(vehicle_stack, VEHICLE_STACK_SIZE);
static struct k_thread vehicle_tcb;

void vehicle_thread(void *p1, void *p2, void *p3)
{
    enum vehicle_light last = VEH_LIGHT_OFF;
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (;;) {
        enum vehicle_light current = intersection_get_vehicle_light();
        if (current != last) {
            vehicle_show(current);
            last = current;
        }
        k_msleep(10);
    }
}

void vehicle_thread_start(void)
{
    k_thread_create(&vehicle_tcb, vehicle_stack,
                    K_THREAD_STACK_SIZEOF(vehicle_stack),
                    vehicle_thread, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
    k_thread_name_set(&vehicle_tcb, "vehicle_led");
}
