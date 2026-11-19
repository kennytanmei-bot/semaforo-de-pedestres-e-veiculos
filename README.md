# Semáforos de Pedestres e Veículos — cruzamento com 2 placas (Zephyr / FRDM-KL25Z)

Duas placas FRDM-KL25Z, mesmo firmware; o papel é escolhido em `src/config.h`:

| Placa | Define | Função |
|---|---|---|
| A | `ROLE_STREET_A` | coordena as fases (Rua A / Rua B), lê a chave noturna, envia PHASE e NIGHT |
| B | `ROLE_STREET_B` | segue PHASE/NIGHT enviados por A |

Cada rua tem: semáforo de **veículos** = LED RGB da placa (vermelho PTB18, verde PTB19, amarelo = R+G) e semáforo de **pedestres** = 2 LEDs externos (vermelho PTC8, verde PTC9, com resistor ~330 Ω), mais 1 botão de travessia (PTD0→GND).

## Ligações

| Sinal | De → Para | Pino |
|---|---|---|
| PHASE (fase liberada: 0 = Rua A, 1 = Rua B) | A → B | PTB3 → PTB3 (J10-8) |
| NIGHT (modo noturno) | A → B | PTD4 → PTD4 (J1-6) |
| GND comum | A ↔ B | GND ↔ GND |
| Chave noturna (só na placa A) | PTA12 → GND (J1-8) | |
| Botão de travessia (cada placa) | PTD0 → GND (J2-6) | |

## Comportamento
- Ciclo de 9 s: fase A (A verde 3 s, amarelo 1 s, todos-vermelho 0,5 s) e fase B (idem com B). Vermelho de cada rua = 5 s.
- Botão: o pedido é guardado e atendido no início da fase da outra rua (própria rua vermelha): pedestre verde 4 s.
- Noturno (chave em A): amarelo veicular e vermelho de pedestres piscam 1 s aceso / 1 s apagado nas duas ruas; botões ignorados.
- B só libera sua rua após receber a 1ª borda de PHASE (falha segura).

## Compilar / gravar
1. Em `config.h` deixe só `ROLE_STREET_A` **ou** `ROLE_STREET_B` e grave cada placa (`pio run -t upload`).
2. Descubra a porta: `pio device list`. Feche o monitor serial antes de capturar.

## Capturar e validar logs (Windows/Linux)
```
python tools/capture_log.py COM5 a.log --seconds 60      # placa A (aperte RESET ao iniciar)
python tools/capture_log.py COM6 b.log --seconds 60      # placa B (outro terminal)
python tools/verify_log.py --a a.log --b b.log --report
```

## Simulação (sem placas)
`sh test/host/run_sim.sh` — `test/host/simulate.py` espelha `intersection.c`, gera `docs/evidencias/*.log` e `RESULTADOS.txt` (validados por `verify_log.py`).

## Testes
Plano em modelo V com resultados simulados e campos para a bancada: [`docs/PLANO_DE_TESTES.md`](docs/PLANO_DE_TESTES.md).
