#ifndef APP_H
#define APP_H

#include <stdbool.h>

enum vehicle_light {
    VEH_LIGHT_OFF,
    VEH_LIGHT_RED,
    VEH_LIGHT_YELLOW,
    VEH_LIGHT_GREEN
};

enum pedestrian_light {
    PED_LIGHT_OFF,
    PED_LIGHT_RED,
    PED_LIGHT_GREEN
};

void app_run(void);
void vehicle_show(enum vehicle_light light);
void pedestrian_show(enum pedestrian_light light);
void vehicle_thread_start(void);
void pedestrian_thread_start(void);
void intersection_set_lights(enum vehicle_light vehicle,
                             enum pedestrian_light pedestrian);
enum vehicle_light intersection_get_vehicle_light(void);
enum pedestrian_light intersection_get_pedestrian_light(void);

#endif
