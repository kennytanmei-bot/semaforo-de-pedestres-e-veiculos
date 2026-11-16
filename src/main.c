#include <zephyr/kernel.h>
#include "app.h"
#include "io.h"

int main(void)
{
	printk("\n=== Semaforos Zephyr (FRDM-KL25Z) ===\n");
	app_run();
	return 0;
}
