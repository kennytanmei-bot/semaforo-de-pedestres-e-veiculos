#include "app.h"
#include "io.h"

/* Controla o par de LEDs externos do semaforo de pedestres. */
void pedestrian_show(enum pedestrian_light light)
{
    switch (light) {
    case PED_LIGHT_GREEN:
        io_ped_write(false, true);
        break;
    case PED_LIGHT_RED:
        io_ped_write(true, false);
        break;
    case PED_LIGHT_OFF:
    default:
        io_ped_write(false, false);
        break;
    }
}


/* Thread dedicada aos dois LEDs externos dos pedestres. */
#define PEDESTRIAN_STACK_SIZE 1024
K_THREAD_STACK_DEFINE(pedestrian_stack, PEDESTRIAN_STACK_SIZE);
static struct k_thread pedestrian_tcb;

void pedestrian_thread(void *p1, void *p2, void *p3)
{
    enum pedestrian_light last = PED_LIGHT_OFF;
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (;;) {
        enum pedestrian_light current = intersection_get_pedestrian_light();
        if (current != last) {
            pedestrian_show(current);
            last = current;
        }
        k_msleep(10);
    }
}

void pedestrian_thread_start(void)
{
    k_thread_create(&pedestrian_tcb, pedestrian_stack,
                    K_THREAD_STACK_SIZEOF(pedestrian_stack),
                    pedestrian_thread, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
    k_thread_name_set(&pedestrian_tcb, "pedestrian_led");
}
