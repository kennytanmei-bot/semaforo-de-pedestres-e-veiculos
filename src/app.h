#ifndef APP_H
#define APP_H

enum mode { MODE_NORMAL, MODE_NIGHT };

/* Implementada em vehicle.c ou pedestrian.c, conforme o papel da placa. Nao retorna. */
void app_run(void);

#endif
