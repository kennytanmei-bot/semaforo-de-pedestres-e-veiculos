#ifndef CONFIG_H
#define CONFIG_H

/* ===================== PAPEL DA PLACA =====================
 * Placa 1 (integrante A): ROLE_PEDESTRIAN
 * Placa 2 (integrante B): ROLE_VEHICLE
 * Deixe SOMENTE uma das linhas abaixo ativa antes de gravar cada placa. */
/* #define ROLE_VEHICLE */
#define ROLE_PEDESTRIAN 

#if defined(ROLE_VEHICLE) && defined(ROLE_PEDESTRIAN)
#error "Defina apenas um papel (ROLE_VEHICLE ou ROLE_PEDESTRIAN)"
#endif
#if !defined(ROLE_VEHICLE) && !defined(ROLE_PEDESTRIAN)
#error "Defina ROLE_VEHICLE ou ROLE_PEDESTRIAN"
#endif

/* ===================== MODO DE EXECUCAO =====================
 * Descomente SOMENTE uma opcao para teste unitario isolado.
 * Com ambas comentadas, o firmware opera no modo INTEGRADO.
 * A selecao do papel (ROLE_VEHICLE/ROLE_PEDESTRIAN) continua separada.
 */
/* #define VEH_STANDALONE_TEST */
#define PED_STANDALONE_TEST

#if defined(VEH_STANDALONE_TEST) && defined(PED_STANDALONE_TEST)
#error "Ative apenas um modo de teste isolado por compilacao"
#endif
#if defined(VEH_STANDALONE_TEST) && !defined(ROLE_VEHICLE)
#error "VEH_STANDALONE_TEST exige ROLE_VEHICLE"
#endif
#if defined(PED_STANDALONE_TEST) && !defined(ROLE_PEDESTRIAN)
#error "PED_STANDALONE_TEST exige ROLE_PEDESTRIAN"
#endif

/* ===================== TEMPOS (ms) ===================== */
#define PED_GREEN_MS      4000
#define PED_RED_MS        4000   /* pedestre vermelho */
#define VEH_GREEN_MS      3000   /* veiculo verde    */
#define VEH_YELLOW_MS     1000   /* veiculo amarelo  */
#define VEH_RED_MS        4000   /* veiculo vermelho */
#define NIGHT_ON_MS       1000   /* modo noturno: aceso   */
#define NIGHT_OFF_MS      1000   /* modo noturno: apagado */

/* Sincronismo entre as placas */
#define PED_SYNC_GUARD_MS  50    /* margem para debounce/leitura entre placas */
#define PED_SYNC_TIMEOUT_MS 12000/* pedestre sem sinal do semaforo de veiculos: avisa no log (fica vermelho) */

/* Pedido e armazenado em qualquer fase normal; sera atendido no proximo vermelho. */

/* Entradas */
#define POLL_MS           10
#define DEBOUNCE_BTN      3      /* amostras (30 ms) para chave/botao local */
#define DEBOUNCE_LINK     2      /* amostras (20 ms) para fios entre placas */
#define REQ_PULSE_MS      300    /* largura do pulso de pedido de travessia */

#endif
