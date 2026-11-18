#!/usr/bin/env python3
"""Validador automatico de logs seriais (115200 8N1) das placas de semaforo.

Uso:
  python3 tools/verify_log.py --veh veh.log [--ped ped.log] [--tol 60]
        [--ped-standalone] [--offset MS]

Linhas esperadas: "[<ms>] VEH|PED: <msg>" (geradas por LOG/LOG_AT).
Verifica: duracao de cada LED, ordem do ciclo, exclusao mutua (1 LED por vez),
modo noturno (1000 ms aceso) e sincronismo PED verde x VEH vermelho.
Retorna 0 se tudo passar, 1 caso contrario.
"""
import argparse, re, sys

LINE = re.compile(r"\[(\d+)\]\s+(VEH|PED):\s+(.*)")
NOM = {("VEH", "VERDE"): 3000, ("VEH", "AMARELO"): 1000,
       ("VEH", "VERMELHO"): 4000, ("PED", "VERDE"): 4000}
NIGHT_ON = 1000
fails, passes = [], 0


def check(ok, msg):
    global passes
    if ok:
        passes += 1
    else:
        fails.append(msg)


def parse(path, tag, off=0):
    ev = []
    for ln in open(path, errors="replace"):
        m = LINE.search(ln)
        if m and m.group(2) == tag:
            ev.append((int(m.group(1)) + off, m.group(3).strip()))
    return ev


def analyse(ev, tag, tol, ped_standalone):
    """Retorna intervalos [(led, on, off, modo)] e verifica a placa isolada."""
    mode, lit, ivs, order = "NORMAL", None, [], []
    for t, msg in ev:
        m = re.match(r"modo (NOTURNO|NORMAL)", msg)
        if m:
            mode = "NOTURNO" if m.group(1) == "NOTURNO" else "NORMAL"
            if lit:  # LED interrompido por troca de modo: nao checa duracao
                lit[3] = None
            continue
        m = re.match(r"(VERDE|AMARELO|VERMELHO) ON", msg)
        if m:
            check(lit is None, f"{tag}@{t}: {m.group(1)} ON com {lit[0] if lit else '-'} aceso (violacao de exclusao mutua)")
            lit = [m.group(1), t, mode, mode]
            continue
        m = re.match(r"(VERDE|AMARELO|VERMELHO) OFF", msg)
        if m and lit:
            check(lit[0] == m.group(1), f"{tag}@{t}: OFF de {m.group(1)} mas aceso era {lit[0]}")
            ivs.append((lit[0], lit[1], t, lit[3]))
            lit = None
    for led, on, off, md in ivs:
        d = off - on
        if md == "NOTURNO":
            if led in ("AMARELO", "VERMELHO"):
                check(abs(d - NIGHT_ON) <= tol, f"{tag}@{on}: {led} noturno durou {d} ms (esperado {NIGHT_ON})")
        elif md == "NORMAL":
            if (tag, led) in NOM:
                exp = NOM[(tag, led)]
                if tag == "VEH" and led == "VERMELHO":  # pode estender p/ travessia
                    check(exp - tol <= d <= 4050 + tol, f"{tag}@{on}: VERMELHO durou {d} ms (4000..4050)")
                else:
                    check(abs(d - exp) <= tol, f"{tag}@{on}: {led} durou {d} ms (esperado {exp})")
            elif tag == "PED" and led == "VERMELHO" and ped_standalone:
                check(abs(d - 4000) <= tol, f"PED@{on}: VERMELHO durou {d} ms (esperado 4000)")
    # noturno: ciclo de 2 s (ON a ON)
    nt = [iv for iv in ivs if iv[3] == "NOTURNO"]
    for a, b in zip(nt, nt[1:]):
        if a[0] == b[0]:
            check(abs((b[1] - a[1]) - 2000) <= tol * 2, f"{tag}@{a[1]}: periodo noturno {b[1]-a[1]} ms (esperado 2000)")
    # ordem do ciclo no modo normal
    if tag == "VEH":
        nxt = {"VERDE": "AMARELO", "AMARELO": "VERMELHO", "VERMELHO": "VERDE"}
        nm = [iv for iv in ivs if iv[3] == "NORMAL"]
        for a, b in zip(nm, nm[1:]):
            if b[1] - a[2] < 200:  # consecutivos
                check(nxt[a[0]] == b[0], f"VEH@{b[1]}: ordem invalida {a[0]} -> {b[0]}")
    return ivs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--veh"); ap.add_argument("--ped")
    ap.add_argument("--tol", type=int, default=60)
    ap.add_argument("--ped-standalone", action="store_true")
    ap.add_argument("--offset", type=int, default=None,
                    help="ms somados aos tempos PED (default: alinha pelo 1o PED_GO=1)")
    a = ap.parse_args()
    if not a.veh and not a.ped:
        ap.error("informe --veh e/ou --ped")
    veh = ped = None
    if a.veh:
        veh = parse(a.veh, "VEH")
        vi = analyse(veh, "VEH", a.tol, False)
    if a.ped:
        off = 0
        ped = parse(a.ped, "PED")
        if veh and a.offset is None:
            v = next((t for t, m in veh if m == "PED_GO=1"), None)
            p = next((t for t, m in ped if m == "PED_GO=1"), None)
            if v is not None and p is not None:
                off = v - p
        elif a.offset is not None:
            off = a.offset
        ped = [(t + off, m) for t, m in ped]
        pi = analyse(ped, "PED", a.tol, a.ped_standalone)
    if veh and ped:  # sincronismo (margem: debounce do link ~30 ms)
        g = 100 + a.tol
        for led, on, off_, md in pi:
            if led != "VERDE" or md != "NORMAL":
                continue
            ok = any(r == "VERMELHO" and ron - g <= on and off_ <= roff + g
                     for r, ron, roff, _ in vi)
            check(ok, f"SYNC: PED VERDE [{on},{off_}] nao esta contido em um VEH VERMELHO")
            bad = [x for x in vi if x[0] != "VERMELHO" and x[1] < off_ - g and x[2] > on + g]
            check(not bad, f"SYNC: PED VERDE [{on},{off_}] sobrepoe VEH {bad[0][0] if bad else ''}")
    print(f"PASS={passes} FAIL={len(fails)}")
    for f in fails:
        print("  FALHA:", f)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
