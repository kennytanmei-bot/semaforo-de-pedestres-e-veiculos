#ifndef CONFIG_H
#define CONFIG_H

/* Selecione UMA funcao por compilacao:
 * ROLE_STREET_A: placa coordenadora (Rua A)
 * ROLE_STREET_B: placa seguidora (Rua B)
 */
//#define ROLE_STREET_A
#define ROLE_STREET_B

#if defined(ROLE_STREET_A) && defined(ROLE_STREET_B)
#error "Defina apenas ROLE_STREET_A ou ROLE_STREET_B"
#endif
#if !defined(ROLE_STREET_A) && !defined(ROLE_STREET_B)
#error "Defina ROLE_STREET_A ou ROLE_STREET_B"
#endif

/* Tempos do cruzamento (ms). Cada fase dura 3 s verde + 1 s amarelo +
 * 500 ms de seguranca com os dois semaforos veiculares vermelhos.
 */
#define VEH_GREEN_MS       3000U
#define VEH_YELLOW_MS      1000U
#define ALL_RED_MS          500U
#define PHASE_PERIOD_MS    (VEH_GREEN_MS + VEH_YELLOW_MS + ALL_RED_MS)

#define PED_GREEN_MS       4000U
#define NIGHT_ON_MS        1000U
#define NIGHT_OFF_MS       1000U
#define NIGHT_PERIOD_MS    (NIGHT_ON_MS + NIGHT_OFF_MS)

#define POLL_MS              10U
#define DEBOUNCE_BTN          3U  /* 3 amostras de 10 ms */
#define DEBOUNCE_SWITCH       3U

#endif
