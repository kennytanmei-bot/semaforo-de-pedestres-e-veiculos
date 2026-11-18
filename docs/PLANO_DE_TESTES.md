# Plano de Testes — Semáforos de Pedestres e Veículos (Modelo V)

Dupla: ____________ (placa de pedestres) / ____________ (placa de veículos) — Data: ____

## 1. Modelo V

| Fase de projeto | Nível de teste | Escopo |
|---|---|---|
| Requisitos do sistema | **Sistema (TS)** | modo noturno, botão de travessia, ciclo completo |
| Arquitetura (2 placas, 3 sinais) | **Integração (TI)** | PED_GO, NIGHT, REQ, sincronismo |
| Projeto detalhado (módulos) | **Unitário (TU)** | `pedestrian.c`, `vehicle.c`, `io.c` (debounce) |

**Instrumentação:** log serial 115200 com timestamp (`[ms] TAG: msg`) gerado no instante da transição do LED; cronômetro/analisador lógico como referência independente. Tolerância de tempo: ±60 ms (ciclo de polling 10 ms + debounce 20–30 ms). Validação automática: `python3 tools/verify_log.py --veh veh.log --ped ped.log`.

**Preparação TU:** uma placa por vez, ative a flag em `config.h` (`PED_STANDALONE_TEST` ou `VEH_STANDALONE_TEST`); nesse modo não há entradas nem fios.
**Preparação TI/TS:** ambas flags comentadas, placas ligadas conforme README.

## 2. Testes Unitários

| ID | Req. | Procedimento | Resultado esperado | Obtido | OK |
|---|---|---|---|---|---|
| TU-P1 | RF1 | `PED_STANDALONE_TEST`; observar log por ≥ 3 ciclos | VERDE ON → OFF `duracao`=4000±60 ms | | |
| TU-P2 | RF1 | idem | VERMELHO `duracao`=4000±60 ms; alterna com verde | | |
| TU-P3 | RF1/Mutex | idem; observar LEDs/log | Nunca VERDE e VERMELHO acesos juntos; sem intervalo apagado >50 ms entre fases | | |
| TU-P4 | RF1 | idem; 2 threads (nomes VERDE/VERMELHO) | Cada LED comandado por thread própria (conferir `k_thread_name_set`/código) | | |
| TU-V1 | RF2 | `VEH_STANDALONE_TEST` | VERDE=3000, AMARELO=1000, VERMELHO=4000 (±60 ms) | | |
| TU-V2 | RF2 | idem | Ordem VERDE→AMARELO→VERMELHO→VERDE | | |
| TU-V3 | RF2/Mutex | idem | Exatamente 1 LED por vez (amarelo = R+G, nunca com verde ou vermelho isolado) | | |
| TU-V4 | RF2 | idem, 10 ciclos | Deriva acumulada < 100 ms (ciclo = 8000 ms) | | |
| TU-IO1 | RNF | Botão/chave: pressionar com bounce curto (<20 ms, tocar leve) | Nenhum evento registrado | | |
| TU-IO2 | RNF | Pressão firme ≥ 40 ms | Exatamente 1 evento (modo/`botao travessia pressionado`) | | |
| TU-IO3 | RNF | Segurar botão 3 s | 1 único pedido (REQ só na borda) | | |
| TU-IO4 | RNF | `verify_log.py` sobre log sintético: `sh test/host/run_selftest.sh` | Log bom = PASS; log defeituoso = FAIL | | |

## 3. Testes de Integração (duas placas)

| ID | Req. | Procedimento | Resultado esperado | Obtido | OK |
|---|---|---|---|---|---|
| TI-1 | Link PED_GO | Pressionar botão; observar `PED_GO=1` na placa VEH e na PED | PED recebe PED_GO≤ 50 ms após VEH enviar | | |
| TI-2 | Sincronismo | Pedido durante VERDE veicular | Atendido só no próximo VEH VERMELHO; PED fica VERMELHO até lá | | |
| TI-3 | Sincronismo | Log conjunto de ≥ 5 travessias → `verify_log.py` | PED VERDE contido em VEH VERMELHO; nunca sobrepõe VEH verde/amarelo | | |
| TI-4 | Margem | Medir fim do PED verde vs. VEH verde seguinte | PED VERMELHO antes do VEH VERDE (margem ≥ 20 ms) | | |
| TI-5 | Pedido durante VEH vermelho | Botão logo após VEH VERMELHO ON | Fica pendente para o próximo ciclo (não encurta nem inicia agora) | | |
| TI-6 | Pedido duplicado | Apertar 3× antes do vermelho | Log: 1 `REQ aceito`, demais `REQ duplicado`; 1 travessia | | |
| TI-7 | Sem pedido | Ciclo normal sem apertar | PED permanece VERMELHO, PED_GO=0; VEH vermelho = 4000 ms | | |
| TI-8 | Falha segura | Desconectar fio PED_GO (VEH→PED) | PED permanece VERMELHO; aviso após 12 s no log | | |
| TI-9 | Falha REQ | Desconectar fio REQ | Botão sem efeito; veículos seguem ciclo normal, sem travar | | |
| TI-10 | Link NIGHT | Ligar chave noturna | PED loga `modo NOTURNO` ≤ 50 ms após VEH | | |
| TI-11 | Exclusão mútua inter-placas | Rever todos os logs TI | Em nenhum instante PED verde e VEH verde/amarelo | | |

## 4. Testes de Sistema

| ID | Req. | Procedimento | Resultado esperado | Obtido | OK |
|---|---|---|---|---|---|
| TS-1 | Modo normal | Ligar placas, 2 min sem interação | Ciclo veículos 8 s estável; pedestre vermelho fixo | | |
| TS-2 | Botão | Apertar em qualquer fase | Travessia no próximo vermelho: PED verde 4000±60 ms, veículos em vermelho ≥ 4000 ms | | |
| TS-3 | Botão (única via) | Apertar repetidamente durante travessia | Pedido novo vira pendente p/ ciclo seguinte; sem conflito de LEDs | | |
| TS-4 | Noturno veículos | Ligar chave | Amarelo pisca 1 s ON / 1 s OFF; verde/vermelho apagados | | |
| TS-5 | Noturno pedestres | idem | Vermelho pisca 1 s/1 s; verde apagado | | |
| TS-6 | Botão no noturno | Apertar botão em noturno | Log `REQ ignorado: modo noturno`; nenhuma travessia | | |
| TS-7 | Entrada no noturno durante travessia | Ligar chave com PED verde aceso | Verde apaga imediatamente, passa a piscar vermelho; sem 2 LEDs | | |
| TS-8 | Saída do noturno | Desligar chave | Veículos voltam em VERDE; pedestre VERMELHO; ciclo normal retomado | | |
| TS-9 | Endurance | 30 min com travessias aleatórias → `verify_log.py` | FAIL=0; sem travamento/queda | | |
| TS-10 | Reset | Reiniciar uma placa com a outra ligada | Reinicia em estado seguro (PED vermelho); volta a sincronizar | | |

## 5. Rastreabilidade

| Requisito | Testes |
|---|---|
| RF1 Pedestres 4 s/4 s, 2 threads, mutex | TU-P1..P4, TI-11 |
| RF2 Veículos 3/1/4 s, 3 threads, mutex | TU-V1..V4, TI-7 |
| RF3 Sincronismo PED×VEH | TI-1..TI-6, TI-11 |
| RF4 Modo noturno | TI-10, TS-4..TS-8 |
| RF5 Botão de travessia / bloqueio seguro dos carros / via única | TI-2, TI-5, TI-6, TS-2, TS-3 |
| RNF Debounce, falha segura, robustez | TU-IO1..IO4, TI-8, TI-9, TS-9, TS-10 |

## 6. Registro de evidências
Anexar: trechos de log (`veh.log`, `ped.log`), saída do `verify_log.py`, fotos/vídeo, e anotar defeitos encontrados (ID, causa, correção, re-teste) abaixo.

| Defeito | Teste | Causa | Correção (commit) | Re-teste |
|---|---|---|---|---|
| | | | | |

## 7. Commits individuais
Cada integrante faz commit de sua placa/testes com seu próprio usuário Git (`git config user.name/email`) para avaliação individual.
