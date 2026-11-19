#!/usr/bin/env python3
"""Validador de logs seriais das placas A e B (formato "[ms] CROSS: ...").

  python tools/verify_log.py --a a.log [--b b.log] [--tol 60] [--report]

Checa por placa (modo normal): VERDE 3000, AMARELO 1000, VERMELHO(apos amarelo)
5000 (=500 todos-vermelho + 4500 da outra rua), ordem do ciclo, travessia 4000,
pedestre verde nunca junto de veiculo verde/amarelo; modo noturno 1000/1000.
Com --b (duas placas): nunca as duas ruas em verde/amarelo ao mesmo tempo e
intervalo todos-vermelho >= 500 ms.  --report imprime as medidas.
Saida 0 = tudo OK; 1 = alguma falha.
"""
import argparse, re, statistics, sys

L = re.compile(r"\[(\d+)\]\s+CROSS:\s+(.*)")
V = re.compile(r"Rua (A|B): VEICULOS (VERDE|AMARELO|VERMELHO|APAGADO)")
P = re.compile(r"Rua (A|B): PEDESTRES (VERDE|VERMELHO|APAGADO)")
NIGHT_EV = ("MODO NOTURNO ATIVADO", "MODO NOTURNO recebido")
NORM_EV = ("MODO NORMAL retomado", "MODO NORMAL recebido")
fails, passes, rep = [], 0, []


def check(ok, msg):
    global passes
    if ok: passes += 1
    else: fails.append(msg)


def parse(path):
    ev = []
    for ln in open(path, errors="replace"):
        if ln.startswith("=== RESET"):
            ev.append((None, "RESET")); continue
        m = L.search(ln)
        if m: ev.append((int(m.group(1)), m.group(2).strip()))
    return ev


def segments(ev, rx):
    """[(estado, ini, fim, modo_ini, limpo)]; limpo = sem troca de modo/reset dentro."""
    mode, segs, cur, street, last_t, epoch = "N", [], None, None, 0, 0
    for t, m in ev:
        if t is None:  # reset: descarta segmento aberto
            cur = None; mode = "N"; epoch += 1; continue
        newmode = "T" if m.startswith(NIGHT_EV) else "N" if m.startswith(NORM_EV) else None
        if newmode:
            if cur:  # fecha o trecho no instante da troca de modo e continua (sujo)
                segs.append((cur[0], cur[1], t, cur[2], False, epoch))
                cur = [cur[0], t, newmode, "dirty"]
            mode = newmode
        x = rx.search(m)
        if x:
            street = x.group(1)
            if cur: segs.append((cur[0], cur[1], t, cur[2], "dirty" not in cur, epoch))
            cur = [x.group(2), t, mode]
    return segs, street


def board_checks(tag, ev, tol):
    vs, street = segments(ev, V); ps, _ = segments(ev, P)
    nm = [s for s in vs if s[3] == "N" and s[4]]
    for st, a, b, m, ok, _e in vs:
        if not ok: continue
        d = b - a
        if m == "N":
            if st == "VERDE": check(abs(d - 3000) <= tol, f"{tag}@{a}: VERDE {d} ms (3000)"); rep.append((tag, "veh_verde", d))
            if st == "AMARELO": check(abs(d - 1000) <= tol, f"{tag}@{a}: AMARELO {d} ms (1000)"); rep.append((tag, "veh_amarelo", d))
        else:
            if st in ("AMARELO", "APAGADO"): check(abs(d - 1000) <= tol, f"{tag}@{a}: noturno {st} {d} ms (1000)"); rep.append((tag, "night_veh_" + st, d))
    for i in range(1, len(vs)):  # vermelho apos amarelo
        p, c = vs[i - 1], vs[i]
        if p[5] == c[5] and p[0] == "AMARELO" and c[0] == "VERMELHO" and c[3] == "N" and c[4] and p[3] == "N" and p[4]:
            d = c[2] - c[1]; check(abs(d - 5000) <= tol, f"{tag}@{c[1]}: VERMELHO {d} ms (5000)"); rep.append((tag, "veh_vermelho", d))
    nxt = {"VERDE": "AMARELO", "AMARELO": "VERMELHO", "VERMELHO": "VERDE"}
    for a, b in zip(vs, vs[1:]):
        if a[5] == b[5] and a[3] == "N" and b[3] == "N" and a[4] and a[0] in nxt and b[0] != "APAGADO":
            check(nxt[a[0]] == b[0], f"{tag}@{b[1]}: ordem invalida {a[0]}->{b[0]}")
    for st, a, b, m, ok, _e in ps:
        if not ok: continue
        d = b - a
        if m == "N" and st == "VERDE": check(abs(d - 4000) <= tol, f"{tag}@{a}: travessia {d} ms (4000)"); rep.append((tag, "ped_verde", d))
        if m == "T": check(abs(d - 1000) <= tol, f"{tag}@{a}: noturno ped {st} {d} ms (1000)"); rep.append((tag, "night_ped_" + st, d))
    for st, a, b, m, ok, _e in ps:  # exclusao ped x veiculo (mesma placa)
        if st != "VERDE": continue
        bad = [v for v in vs if v[0] in ("VERDE", "AMARELO") and v[1] < b and v[2] > a]
        check(not bad, f"{tag}@{a}: PED VERDE sobrepoe veiculo {bad[0][0] if bad else ''}")
    return vs, ps, street


def find(ev, prefix): return [t for t, m in ev if t is not None and m.startswith(prefix)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--a"); ap.add_argument("--b")
    ap.add_argument("--tol", type=int, default=60)
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    if not (a.a or a.b): ap.error("informe --a e/ou --b")
    res = {}
    for k in "ab":
        p = getattr(a, k)
        if p:
            ev = parse(p); res[k] = (ev, *board_checks(k.upper(), ev, a.tol))
    if "a" in res and "b" in res:
        ea, eb = res["a"][0], res["b"][0]
        def lab(ev, pre):
            return [(t, m[-7:] if pre != "MODO" else m) for t, m in ev if t is not None and m.startswith(pre)]
        pa, pb = lab(ea, "Fase liberada"), lab(eb, "Sincronizado")
        if not pa or not pb:
            pa, pb = lab(ea, "MODO NOTURNO ATIVADO"), lab(eb, "MODO NOTURNO recebido")
            pb = [(t, "MODO NOTURNO ATIVADO") for t, _ in pb]
        else:
            pb = [(t, "Rua " + m.split("Rua ")[-1][0]) if False else (t, m) for t, m in pb]
        off, best = None, 0
        resets = any(t is None for t, _ in ea + eb)
        pa2 = [(t, x.strip()[-1]) for t, x in pa]; pb2 = [(t, x.strip()[-1] if "liberada" not in x else x.split("Rua ")[-1][0]) for t, x in pb]
        if pa and pb and "Fase liberada" in "".join(m for _, m in ea):
            pa2 = [(t, m[-1]) for t, m in ((t, m) for t, m in ea if t is not None and m.startswith("Fase liberada"))]
            pb2 = [(t, m.split("Rua ")[1][0]) for t, m in eb if t is not None and m.startswith("Sincronizado")]
        for t0, l0 in pa2:
            for t1, l1 in pb2[:3]:
                if l0 != l1: continue
                cand = t0 - t1
                diffs = [x - y for x, lx in pa2 for y, ly in pb2 if lx == ly and -5 <= (x - (y + cand)) <= 40]
                if len(diffs) > best: best, off = len(diffs), int(statistics.median(diffs))
        if resets:
            print("  (aviso: houve RESET; cruzamento entre placas nao verificado automaticamente)")
            off = None
        if off is not None:
            iva = [(s, t0 - 0, t1 - 0) for s, t0, t1, m, ok, _e in res["a"][1] if s in ("VERDE", "AMARELO") and m == "N" and t1 > t0]
            ivb = [(s, t0 + off, t1 + off) for s, t0, t1, m, ok, _e in res["b"][1] if s in ("VERDE", "AMARELO") and m == "N" and t1 > t0]
            g = 40
            conf = [(x, y) for x in iva for y in ivb if x[1] < y[2] - g and y[1] < x[2] - g]
            check(not conf, f"CROSS: ruas A e B verde/amarelo juntas: {conf[:1]}")
            gaps = []
            for x in iva:
                for y in ivb:
                    if 0 <= y[1] - x[2] < 3000: gaps.append(y[1] - x[2])
                    if 0 <= x[1] - y[2] < 3000: gaps.append(x[1] - y[2])
            if gaps:
                check(min(gaps) >= 500 - g, f"CROSS: intervalo todos-vermelho {min(gaps)} ms (<500)"); rep.append(("A/B", "todos_vermelho_min", min(gaps)))
            # pedestres: nunca verde enquanto a rua cruzada... (ped A so com veiculo A vermelho: ja checado)
            la, lb = find(ea, "MODO NOTURNO ATIVADO"), find(eb, "MODO NOTURNO recebido")
            for x, y in zip(la, lb):
                rep.append(("A/B", "latencia_night_ms", y + off - x))
                check(abs(y + off - x) <= 40, f"NIGHT: B reagiu {y+off-x} ms depois de A (>40)")
            rep.append(("A/B", "offset_relogios_ms", off))
    print(f"PASS={passes} FAIL={len(fails)}")
    for f in fails: print("  FALHA:", f)
    if a.report:
        by = {}
        for tag, k, v in rep: by.setdefault((tag, k), []).append(v)
        for (tag, k), v in sorted(by.items()):
            print(f"  {tag:3} {k:22} n={len(v):3} min={min(v)} med={int(statistics.median(v))} max={max(v)}")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
