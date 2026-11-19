# Plano de Testes — Semáforos de Pedestres e Veículos (Modelo V)

Dupla: ____________ (placa A, Rua A) / ____________ (placa B, Rua B) — Data: ____

## 1. Modelo V aplicado

![Modelo V](modelo-v.png)

| Fase de desenvolvimento | Atividade neste projeto | Nível de teste |
|---|---|---|
| 1. Requisitos de sistema | RF1–RF5, RNF (seção 2) | **Aceitação / validação (TA)** |
| 2. Arquitetura de sistema | 2 placas (Rua A coordenadora, Rua B seguidora), sinais PHASE e NIGHT, modos normal/noturno/travessia | **Sistema integrado (TS)** |
| 3. Projeto de componentes | `intersection.c` (FSM), `vehicle.c`, `pedestrian.c`, `io.c`; comunicação entre semáforos | **Integração de componentes (TI)** |
| 4. Implementação | código e threads | **Unidade (TU)** |

Cada teste: **pré-condição → etapas → pós-condição**. Instrumentação: log serial 115200 (`[ms] CROSS: ...`) com `tools/capture_log.py`; o log registra a decisão da máquina de estados e o LED muda até 10 ms depois (thread de LED a cada 10 ms) → incerteza ± 20 ms; tolerância adotada ± 60 ms. Referência independente: cronômetro/analisador lógico.

## 2. Requisitos e rastreabilidade

| Req. | Descrição | Testes |
|---|---|---|
| RF1 | Pedestres: LED verde 4 s / vermelho, 2 LEDs | TU-2, TU-3, TU-4, TA-1 |
| RF2 | Veículos: verde 3 s, amarelo 1 s, vermelho | TU-1, TI-1, TA-2 |
| RF3 | Sincronismo entre os semáforos (sem conflito) | TU-5, TI-1…TI-9, TA-4 |
| RF4 | Modo noturno (1 s aceso / 1 s apagado) | TU-6, TI-8, TS-5…TS-8, TA-5 |
| RF5 | Botão de travessia, bloqueio seguro dos carros | TU-3, TU-4, TI-5…TI-7, TS-2…TS-4, TA-6 |
| RNF | Debounce, falha segura, robustez, threads/mutex | TU-8…TU-10, TI-10, TI-11, TS-9…TS-11, TA-3 |

## 3. Testes de Unidade (Implementação → cada módulo isolado)

Preparação: **placa isolada** — só A ligada (TU-1 a TU-8, TU-10) ou só B ligada (TU-9), sem fios entre placas. Ruas: A = coordenadora (`ROLE_STREET_A`), B = seguidora (`ROLE_STREET_B`).
### TU-1 — Veículos: tempos e ordem do ciclo (RF2)
- **Pré-condição:** Só a placa A (`ROLE_STREET_A`) gravada e ligada; captura serial aberta; sem botão acionado.
- **Etapas:** 1. Reset da placa A. 2. Capturar 40 s. 3. `python tools/verify_log.py --a a.log --report`.
- **Pós-condição (esperado):** VERDE 3000, AMARELO 1000, VERMELHO 5000 (500 ms todos-vermelho + 4500 ms da outra rua), ±60 ms; ordem VERDE→AMARELO→VERMELHO→VERDE.
- **Obtido :** VERDE 3001–3008 ms · AMARELO 995–1008 ms · VERMELHO 5002–5009 ms; ordem correta; PASS=25 FAIL=0.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/tu_normal_A.log`

### TU-2 — Pedestres: estado padrão vermelho (RF1)
- **Pré-condição:** Idem TU-1.
- **Etapas:** 1. Observar os LEDs externos (PTC8 vermelho / PTC9 verde) por 40 s sem apertar o botão.
- **Pós-condição (esperado):** LED vermelho aceso fixo; verde apagado; nenhuma linha `PEDESTRES VERDE` no log.
- **Obtido :** Nenhuma linha de pedestres (estado inicial = VERMELHO e nunca muda).
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/tu_normal_A.log`

### TU-3 — Pedestres: botão gera travessia de 4 s (RF1/RF5)
- **Pré-condição:** Placa A isolada, modo normal.
- **Etapas:** 1. Apertar o botão (PTD0→GND) durante o verde de A. 2. Observar até o fim da travessia.
- **Pós-condição (esperado):** `pedido armazenado`; travessia só no início da fase da outra rua (A vermelho): PED verde 4000 ± 60 ms; volta a vermelho.
- **Obtido :** Pedido em 1027 ms; `Travessia autorizada` em 4503 ms (veículos A vermelho desde 4000); PED verde 4004 ms.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/tu_button_a_A.log`

### TU-4 — Pedido memorizado e único (RF5)
- **Pré-condição:** Idem TU-3.
- **Etapas:** 1. Apertar o botão 3× antes da próxima fase.
- **Pós-condição (esperado):** 3 linhas `pedido armazenado`, mas apenas 1 `Travessia autorizada`.
- **Obtido :** 3 pedidos (1027, 1529, 2524 ms) → 1 travessia (4503 ms, 4004 ms).
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/tu_button_dup_A.log`

### TU-5 — Exclusão: pedestre verde nunca com veículo verde/amarelo (mutex/estado)
- **Pré-condição:** Logs de TU-1 a TU-8.
- **Etapas:** 1. Rodar `verify_log.py` em todos os logs de unidade.
- **Pós-condição (esperado):** 0 ocorrências de `PED VERDE sobrepoe veiculo`.
- **Obtido :** 0 ocorrências em 7 cenários de unidade.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/RESULTADOS.txt`

### TU-6 — Modo noturno local (RF4)
- **Pré-condição:** Placa A isolada.
- **Etapas:** 1. Ligar chave noturna (PTA12→GND). 2. Observar 10 s.
- **Pós-condição (esperado):** Veículo AMARELO 1000 ms / APAGADO 1000 ms; pedestre VERMELHO 1000 / APAGADO 1000; verde apagado.
- **Obtido :** AMARELO 994–1007 ms; APAGADO 995–1004 ms; PED vermelho 994–1007 ms.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/tu_night_A.log`

### TU-7 — Botão ignorado no modo noturno
- **Pré-condição:** Placa A em modo noturno.
- **Etapas:** 1. Apertar o botão. 2. Desligar a chave noturna.
- **Pós-condição (esperado):** `Pedido ignorado durante o modo noturno`; ao voltar ao normal não há travessia pendente.
- **Obtido :** Log `Pedido ignorado…` em 6027 ms; após `MODO NORMAL retomado` nenhuma travessia.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/tu_night_btn_A.log`

### TU-8 — Debounce do botão (RNF)
- **Pré-condição:** Placa A isolada.
- **Etapas:** 1. Encostar o fio por < 30 ms (toque rápido). 2. Pressionar firme ≥ 40 ms.
- **Pós-condição (esperado):** Toque curto: nenhum evento. Pressão firme: 1 evento (3 amostras de 10 ms).
- **Obtido :** Pulso de 20 ms ignorado (nenhuma linha de botão); pulso de 200 ms (TU-3) aceito 27 ms após o toque (3 amostras de 10 ms).
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/tu_glitch_A.log`

### TU-9 — Placa B isolada: falha segura (RNF)
- **Pré-condição:** Só a placa B (`ROLE_STREET_B`), sem PHASE/NIGHT.
- **Etapas:** 1. Reset. 2. Apertar o botão de B. 3. Observar 20 s.
- **Pós-condição (esperado):** Veículos B e pedestres B ficam VERMELHO; log `aguardando sincronizacao`; pedido armazenado e não atendido.
- **Obtido :** B nunca acende verde; pedido em 3027 ms permanece pendente; sem aviso de timeout (ver D3).
- **Resultado :** ☑ Passou c/ ressalva (sim.) · **Evidência:** `docs/evidencias/tu_b_alone_B.log`

### TU-10 — Inspeção de código: threads e mutex (RF1/RF2)
- **Pré-condição:** Código `src/*.c`.
- **Etapas:** 1. Conferir `vehicle_thread`, `pedestrian_thread`, `display_lock`, `intersection_set_lights`, `vehicle_show`.
- **Pós-condição (esperado):** Threads por semáforo com acesso ao estado sob mutex; amarelo = vermelho+verde do RGB; LEDs de pedestre em PTC8/PTC9.
- **Obtido :** 2 threads (`vehicle_led`, `pedestrian_led`) + laço principal (FSM); `display_lock` protege `display_vehicle/display_pedestrian`; set atômico dos dois. Não há uma thread por LED (ver D5).
- **Resultado :** ☑ Passou c/ ressalva (sim.) · **Evidência:** `docs/evidencias/código`

### TU-11 — Ferramenta de validação
- **Pré-condição:** PC com Python 3.
- **Etapas:** 1. `sh test/host/run_sim.sh`.
- **Pós-condição (esperado):** Autoteste: log adulterado (+400 ms) é rejeitado.
- **Obtido :** `autoteste: OK (log adulterado rejeitado)`.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/run_sim.sh`


## 4. Testes de Integração de Componentes (duas placas: PHASE, NIGHT, sincronismo)

Preparação: placas A e B ligadas; PTB3 A→B (PHASE), PTD4 A→B (NIGHT), GND comum. Capturar `a.log` e `b.log` (relógios independentes; o validador alinha pelos eventos de fase).
### TI-1 — Sincronismo de fase (RF3)
- **Pré-condição:** A e B ligadas juntas, sem botões.
- **Etapas:** 1. Reset de A e B. 2. Capturar 60 s. 3. `verify_log.py --a a.log --b b.log --report`.
- **Pós-condição (esperado):** B loga `Sincronizado` ≤ 10 ms após cada `Fase liberada` de A; B: verde 3000 / amarelo 1000 / vermelho 5000 (±60).
- **Obtido :** B: verde 3004–3010, amarelo 992–1001, vermelho 4995–5008 ms; PASS=76 FAIL=0.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_sync_*.log`

### TI-2 — Nunca duas ruas liberadas; todos-vermelho (RF3)
- **Pré-condição:** Idem TI-1.
- **Etapas:** 1. Verificar sobreposição verde/amarelo A×B e intervalo entre fim do amarelo de uma e verde da outra.
- **Pós-condição (esperado):** 0 conflitos; intervalo ≥ 500 ms (tolerância de relógio ±40 ms).
- **Obtido :** 0 conflitos; intervalo mínimo medido ≈ 490 ms (nominal 500).
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_sync_*.log`

### TI-3 — B ligada depois de A
- **Pré-condição:** A rodando há > 5 s.
- **Etapas:** 1. Ligar/resetar B com A em operação.
- **Pós-condição (esperado):** B permanece vermelho até a 1ª borda de PHASE e então segue A.
- **Obtido :** B vermelho por 2312 ms; 1ª borda (Rua A) e verde em 6817 ms; sem conflito.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_late_b_*.log`

### TI-4 — A ligada depois de B
- **Pré-condição:** B ligada primeiro.
- **Etapas:** 1. Ligar A com B já aguardando.
- **Pós-condição (esperado):** B vermelho até a 1ª borda de A.
- **Obtido :** B aguardou 9803 ms (até a 1ª borda de A) e passou a seguir A; sem conflito.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_late_a_*.log`

### TI-5 — Travessia da Rua A com duas placas (RF5)
- **Pré-condição:** Normal, ambas ligadas.
- **Etapas:** 1. Apertar botão de A durante A verde.
- **Pós-condição (esperado):** Ped A verde 4000 ± 60 ms no início da fase de B (A vermelho; B verde/amarelo).
- **Obtido :** Pedido em 6027 ms → travessia em 13507 ms (início da fase B); PED A verde 4005 ms; sem conflito.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_ped_a_*.log`

### TI-6 — Travessia da Rua B com duas placas (RF5)
- **Pré-condição:** Idem.
- **Etapas:** 1. Apertar botão de B durante B verde.
- **Pós-condição (esperado):** Ped B verde 4000 ± 60 ms no início da fase de A.
- **Obtido :** Travessia em 7774 ms (B: `Sincronizado: Rua A liberada`); PED B verde 4004 ms.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_ped_b_*.log`

### TI-7 — Botões das duas ruas ao mesmo tempo
- **Pré-condição:** Idem.
- **Etapas:** 1. Apertar A e B juntos.
- **Pós-condição (esperado):** Cada pedido atendido na fase da outra rua, sem sobreposição.
- **Obtido :** PED A e PED B verdes por 4004 ms cada, em fases diferentes; PASS=52 FAIL=0.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_both_*.log`

### TI-8 — Enlace NIGHT (RF4)
- **Pré-condição:** Normal, ambas ligadas.
- **Etapas:** 1. Ligar chave noturna em A. 2. Observar B e comparar `MODO NOTURNO` nos logs.
- **Pós-condição (esperado):** B loga `MODO NOTURNO recebido` ≤ 20 ms após A; piscas em fase (±20 ms).
- **Obtido :** Latência medida −1…+1 ms (resolução 10 ms); piscas: A 991–1009 ms, B 991–1002 ms.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_night_*.log`

### TI-9 — Saída do modo noturno
- **Pré-condição:** Noturno ativo nas duas.
- **Etapas:** 1. Desligar a chave.
- **Pós-condição (esperado):** A retoma pela Rua A (verde); B fica vermelho até sincronizar na 1ª borda.
- **Obtido :** A verde em 20025 ms; B vermelho 18801→23301 ms (4500 ms) e então segue A.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_night_*.log`

### TI-10 — Falha: fio PHASE desconectado
- **Pré-condição:** Normal.
- **Etapas:** 1. Remover o fio PTB3.
- **Pós-condição (esperado):** Rua B não pode liberar (falha segura); A segue.
- **Obtido :** B permanece vermelho indefinidamente (nenhum verde em 30 s); sem aviso no log (D3).
- **Resultado :** ☑ Passou c/ ressalva (sim.) · **Evidência:** `docs/evidencias/ti_phase_cut_*.log`

### TI-11 — Falha: fio NIGHT desconectado
- **Pré-condição:** Normal.
- **Etapas:** 1. Remover o fio PTD4. 2. Ligar a chave noturna em A.
- **Pós-condição (esperado):** Ruas devem entrar em noturno coerente.
- **Obtido :** A pisca amarelo, **B não pisca**: amarelo de B cortado em 514 ms e vermelho fixo por 16,5 s. Validador: 2 falhas (D2).
- **Resultado :** ☒ Falhou (sim.) · **Evidência:** `docs/evidencias/ti_night_cut_*.log`


## 5. Testes de Sistema Integrado (comportamento completo: modos e botão)
### TS-1 — Modo normal contínuo
- **Pré-condição:** Placas A e B recém-ligadas.
- **Etapas:** 1. 60 s sem interação.
- **Pós-condição (esperado):** Ciclo de 9000 ms estável, ruas alternadas, PED vermelho fixo.
- **Obtido :** PASS=76 FAIL=0; períodos de fase 4500 ms; deriva < 10 ms por fase.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_sync_*.log`

### TS-2 — Travessia por botão – Rua A
- **Pré-condição:** Normal.
- **Etapas:** 1. Apertar botão de A.
- **Pós-condição (esperado):** PED A verde 4000 ± 60 ms com A vermelho.
- **Obtido :** PED A verde 4001–4005 ms.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ts_cross_a_*.log`

### TS-3 — Travessia por botão – Rua B
- **Pré-condição:** Normal.
- **Etapas:** 1. Apertar botão de B.
- **Pós-condição (esperado):** PED B verde 4000 ± 60 ms com B vermelho.
- **Obtido :** PED B verde 4004–4006 ms.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ts_cross_b_*.log`

### TS-4 — Via única: pedido durante a travessia
- **Pré-condição:** Travessia em andamento.
- **Etapas:** 1. Apertar o botão durante o PED verde.
- **Pós-condição (esperado):** Novo pedido fica pendente; nova travessia só no ciclo seguinte; sem conflito.
- **Obtido :** A: pedido em 14526 ms (dentro da travessia 13507–17512) → 2ª travessia em 22520 ms. B: idem (9290 → 16783 ms).
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ts_cross_*.log`

### TS-5 — Noturno nas duas ruas (RF4)
- **Pré-condição:** Normal.
- **Etapas:** 1. Ligar a chave noturna (A).
- **Pós-condição (esperado):** Amarelo veicular e vermelho de pedestres piscam 1 s/1 s nas duas ruas; verde apagado.
- **Obtido :** A e B: AMARELO/PED vermelho 991–1009 ms ligado, 991–1009 ms apagado.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ti_night_*.log`

### TS-6 — Botão no noturno
- **Pré-condição:** Noturno ativo.
- **Etapas:** 1. Apertar botão em A e em B.
- **Pós-condição (esperado):** `Pedido ignorado durante o modo noturno` em ambas; sem travessia ao voltar.
- **Obtido :** A em 12021 ms e B em 11788 ms: ignorado; nenhuma travessia após `MODO NORMAL`.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ts_night_btn_*.log`

### TS-7 — Entrar no noturno durante a travessia (concorrência)
- **Pré-condição:** PED A verde aceso.
- **Etapas:** 1. Ligar a chave noturna.
- **Pós-condição (esperado):** PED verde apaga de imediato e vermelho passa a piscar; nunca verde+vermelho juntos.
- **Obtido :** Em 14526 ms: PED verde→VERMELHO na mesma iteração, `MODO NOTURNO ATIVADO`; B acompanha em ≤ 10 ms.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ts_night_cross_*.log`

### TS-8 — Saída do noturno e pedidos residuais
- **Pré-condição:** Noturno ativo.
- **Etapas:** 1. Desligar a chave.
- **Pós-condição (esperado):** A retoma pela Rua A; pedidos antigos descartados.
- **Obtido :** `MODO NORMAL retomado` em 20025 ms, sem travessia imediata.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ts_night_btn_*.log`

### TS-9 — Estresse (30 min), race conditions
- **Pré-condição:** Normal.
- **Etapas:** 1. 30 min com botões e chave em instantes aleatórios. 2. `verify_log.py`.
- **Pós-condição (esperado):** FAIL=0; sem conflito entre ruas; sem PED verde com veículo liberado.
- **Obtido :** Simulado: 105 travessias e 6 noturnos; PASS=3066 FAIL=0; intervalo todos-vermelho mín. 485 ms.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ts_stress_*.log`

### TS-10 — Reset da placa A em operação
- **Pré-condição:** B em verde.
- **Etapas:** 1. Pressionar RESET em A com B verde.
- **Pós-condição (esperado):** Transição segura, sem conflito.
- **Obtido :** Ao cair PHASE, B vai a VERMELHO em ≤ 10 ms **sem amarelo** (verde de 493 ms); sem conflito, pois A reinicia depois (D1).
- **Resultado :** ☒ Falhou (sim.) · **Evidência:** `docs/evidencias/ts_reset_a_*.log`

### TS-11 — Reset da placa B em operação
- **Pré-condição:** A em operação.
- **Etapas:** 1. Pressionar RESET em B.
- **Pós-condição (esperado):** B volta em estado seguro e re-sincroniza.
- **Obtido :** B vermelho; sincroniza na 1ª borda (3615 ms) e libera em 8129 ms; sem conflito.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/ts_reset_b_*.log`


## 6. Testes de Aceitação / Validação (requisitos do enunciado)

Preparação: bancada completa; avaliador diferente do autor (par).
### TA-1 — Pedestres: 2 LEDs, verde 4 s
- **Pré-condição:** TU-3, TS-2, TS-3 aprovados.
- **Etapas:** 1. Conferir resultados.
- **Pós-condição (esperado):** Verde 4000 ms; vermelho como estado padrão.
- **Obtido :** Verde 4000 ± 10 ms. O vermelho de pedestre não tem duração fixa de 4 s (depende do ciclo de 9 s).
- **Resultado :** ☑ Passou c/ ressalva (sim.) · **Evidência:** `docs/evidencias/TU-3/TS-2/TS-3`

### TA-2 — Veículos: verde 3 s, amarelo 1 s, vermelho 4 s
- **Pré-condição:** TU-1 e TI-1 aprovados.
- **Etapas:** 1. Conferir resultados.
- **Pós-condição (esperado):** 3 / 1 / 4 s.
- **Obtido :** 3 s e 1 s conformes; **vermelho = 5 s** (0,5 s todos-vermelho + 4,5 s da outra rua) — desvio D4.
- **Resultado :** ☑ Passou c/ ressalva (sim.) · **Evidência:** `docs/evidencias/TU-1/TI-1`

### TA-3 — Threads por LED e mutex
- **Pré-condição:** TU-10 aprovado.
- **Etapas:** 1. Revisão de código com o par.
- **Pós-condição (esperado):** 2 threads (pedestres) e 3 threads (veículos), 1 por LED, com mutex.
- **Obtido :** 1 thread por semáforo (2 no total) + mutex de estado — desvio D5.
- **Resultado :** ☒ Falhou (sim.) · **Evidência:** `docs/evidencias/TU-10`

### TA-4 — Sincronismo entre os semáforos
- **Pré-condição:** TI-1…TI-9 aprovados.
- **Etapas:** 1. Conferir logs.
- **Pós-condição (esperado):** Sem conflito entre ruas; PED só com a própria rua em vermelho.
- **Obtido :** 0 conflitos em todos os cenários simulados.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/TI-1…TI-9`

### TA-5 — Modo noturno
- **Pré-condição:** TS-5…TS-8 aprovados.
- **Etapas:** 1. Demonstrar ao avaliador.
- **Pós-condição (esperado):** Amarelo/vermelho piscando 1 s/1 s.
- **Obtido :** Conforme.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/TS-5…TS-8`

### TA-6 — Botão de travessia com bloqueio seguro
- **Pré-condição:** TS-2…TS-4 aprovados.
- **Etapas:** 1. Demonstrar ao avaliador.
- **Pós-condição (esperado):** Carros bloqueados antes do verde do pedestre; sem conflito de LEDs.
- **Obtido :** Conforme: PED verde só em fase com a própria rua vermelha.
- **Resultado :** ☑ Passou (sim.) · **Evidência:** `docs/evidencias/TS-2…TS-4`


## 7. Defeitos e riscos encontrados (na simulação do código)

| ID | Teste | Descrição | Sugestão de correção |
|---|---|---|---|
| D1 | TS-10 | Reset/queda de A durante o verde de B: B vai a vermelho em ≤ 10 ms sem amarelo | Em B, ao perder PHASE passar por amarelo antes do vermelho |
| D2 | TI-11 | Sem fio NIGHT, B não entra em noturno e fica vermelho fixo enquanto A pisca | Timeout de PHASE em B → modo de falha piscante; ou debounce/redundância |
| D3 | TU-9, TI-10 | Sem PHASE, B fica vermelho para sempre e sem log de aviso | Timeout (ex. 2 ciclos) + `LOG` de aviso |
| D4 | TA-2 | Vermelho dos veículos = 5 s (enunciado: 4 s) por causa do todos-vermelho de 0,5 s | Ajustar `ALL_RED_MS`/fases ou justificar no relatório |
| D5 | TA-3 / TU-10 | 1 thread por semáforo, não por LED (enunciado: 2+3 threads) | Dividir `vehicle_thread` em 3 e `pedestrian_thread` em 2, com o mutex |
| D6 | TI | Leitura de PHASE/NIGHT em B sem debounce (ruído no fio gera borda falsa) | Usar `struct deb` também nas linhas de enlace |

## 8. Avaliação por pares
| Testes revisados | Autor do código | Revisor | Observações | Data |
|---|---|---|---|---|
| Sim | Kenny e Davi | Kenny e Davi | 3 falhas registradas e ainda não solucionadas | 18/11 |

## 9. Versionamento
Marcos de commit sugeridos (cada integrante com o próprio usuário Git): (1) código original; (2) 1ª falha detectada (anexar log: ex. TI-11 ou TS-10); (3) correção; (4) validação final com os testes aprovados na bancada.

Anexar: `a.log`, `b.log`, saída do `verify_log.py`, fotos/vídeo.
