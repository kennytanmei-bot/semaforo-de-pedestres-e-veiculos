#!/usr/bin/env python3
"""Gera logs sinteticos (bom e defeituoso) para autoteste do verify_log.py."""
import sys
out = sys.argv[1]
bad = len(sys.argv) > 2 and sys.argv[2] == "bad"
veh, ped = [], []
def v(t, m): veh.append(f"[{t}] VEH: {m}")
def p(t, m): ped.append(f"[{t+5000}] PED: {m}")  # relogio da outra placa
t = 0
for cycle in range(4):
    req = cycle % 2 == 1
    v(t, "VERDE ON"); v(t+3000, "VERDE OFF | duracao=3000 ms"); t += 3000
    v(t, "AMARELO ON"); v(t+1000, "AMARELO OFF | duracao=1000 ms"); t += 1000
    v(t, "VERMELHO ON")
    if req:
        v(t, "PED_GO=1"); p(t+25, "PED_GO=1"); p(t+25, "VERDE ON")
        gd = 4600 if bad and cycle == 3 else 4000
        p(t+25+gd, f"VERDE OFF | duracao={gd} ms"); p(t+25+gd, "VERMELHO ON")
        v(t+4050, "PED_GO=0"); v(t+4050, "VERMELHO OFF | duracao=4050 ms"); t += 4050
        p(t+25, "PED_GO=0")
        p(t+3000+1000, "VERMELHO OFF | duracao=8000 ms")
    else:
        v(t+4000, "VERMELHO OFF | duracao=4000 ms"); t += 4000
veh.append(f"[{t}] VEH: modo NOTURNO")
for i in range(3):
    v(t+i*2000, "AMARELO ON"); v(t+i*2000+1000, "AMARELO OFF | duracao=1000 ms")
open(out + "_veh.log", "w").write("\n".join(veh) + "\n")
open(out + "_ped.log", "w").write("\n".join(ped) + "\n")
