# Semáforos de Pedestres e Veículos (Zephyr / FRDM-KL25Z)

Duas placas FRDM-KL25Z, mesmo firmware, papel escolhido em `src/config.h`:

| Placa | Papel | Define | LEDs (RGB onboard) |
|---|---|---|---|
| A | Pedestres | `ROLE_PEDESTRIAN` | verde = PTB19, vermelho = PTB18 |
| B | Veículos | `ROLE_VEHICLE` | verde = PTB19, vermelho = PTB18, amarelo = R+G |

Threads: 2 (pedestres) + 3 (veículos), uma por LED; um mutex + `lit` garante 1 LED aceso por vez; esperas com `k_condvar_wait` (sem busy-wait).

## Ligações entre placas (todas com GND comum)

| Sinal | Origem → Destino | Pino |
|---|---|---|
| PED_GO (autoriza travessia) | Veículos → Pedestres | PTB3 → PTB3 |
| NIGHT (modo noturno) | Veículos → Pedestres | PTD4 → PTD4 |
| REQ (pedido do botão) | Pedestres → Veículos | PTD5 → PTD5 |
| GND | ↔ | GND ↔ GND |

Entradas locais (ativas em nível baixo, pull-up interno): chave noturna na placa de **veículos** PTA12→GND; botão de travessia na placa de **pedestres** PTD0→GND.

## Comportamento

- **Normal**: veículos verde 3 s → amarelo 1 s → vermelho 4 s. Pedestre fica vermelho; ao pressionar o botão, o pedido é guardado e atendido no **próximo vermelho dos veículos**: veículos ativam PED_GO, vermelho veicular se estende para 4,05 s, pedestre fica verde 4 s e depois volta ao vermelho. Sem sinal, pedestre permanece vermelho (falha segura).
- **Noturno** (chave na placa de veículos): veículos piscam amarelo, pedestres piscam vermelho, 1 s aceso / 1 s apagado. Pedidos de travessia são ignorados.
- Ao sair do noturno os ciclos reiniciam (veículo verde, pedestre vermelho).

## Compilar / gravar (PlatformIO)

1. `config.h`: deixe só `ROLE_VEHICLE` **ou** `ROLE_PEDESTRIAN`.
2. `pio run -t upload` em cada placa; `pio device monitor -b 115200` para o log.
3. Testes unitários isolados: ative `VEH_STANDALONE_TEST` ou `PED_STANDALONE_TEST` (ver `docs/PLANO_DE_TESTES.md`).

## Validação automática de logs

```
pio device monitor -b 115200 | tee veh.log      # placa de veículos
pio device monitor -b 115200 | tee ped.log      # placa de pedestres
python3 tools/verify_log.py --veh veh.log --ped ped.log
sh test/host/run_selftest.sh                    # autoteste do validador (sem placa)
```
O validador confere durações, ordem do ciclo, exclusão mutua, noturno e sincronismo PED×VEH.

## Testes
Plano completo em modelo V (unidade, integração, sistema, aceitação; pré-condição/etapas/pós-condição): [`docs/PLANO_DE_TESTES.md`](docs/PLANO_DE_TESTES.md).
