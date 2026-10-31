#!/usr/bin/env python3
"""Teste do modo cenário da engine (fases de treino), da frente engine.

Copia o orquestrador para uma pasta de trabalho (padrão /tmp/frente_engine/modo_fase),
roda o script.sh em modo fase com cada bot desta pasta e confere o match_frames.json:
bloco "fase", alvos inertes com "alvo": true, artilharia nos ticks programados (e a
no_jogador perto do jogador), ausência de laser/itens/zona quando desligados, e a chave
"objetivo" chegando às 4 linguagens (o bot alcança o primeiro ponto).

Uso (WSL): python3 mock_bots/treino_engine/testar_modo_fase.py [--trabalho DIR] [--seed N]
Saída: uma linha OK/FALHA por verificação; código 1 se alguma falhar.
"""
import argparse
import json
import math
import os
import shutil
import subprocess
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
ORCH = os.path.dirname(os.path.dirname(AQUI))
IGNORAR = shutil.ignore_patterns("build", "submissoes", "replays", "match_frames.json", ".git")

falhas = 0


def conferir(cond, msg):
    global falhas
    print(("  OK    " if cond else "  FALHA ") + msg)
    if not cond:
        falhas += 1


def rodar(trab, bot, cenario, seed):
    sub = os.path.join(trab, "tentativa", "submissoes")
    shutil.rmtree(os.path.join(trab, "tentativa"), ignore_errors=True)
    os.makedirs(sub)
    shutil.copy(os.path.join(AQUI, bot), sub)
    amb = dict(os.environ, TOPCOM_FASE=os.path.join(AQUI, cenario), TOPCOM_FASE_SEED=str(seed))
    p = subprocess.run(["bash", os.path.join(trab, "script.sh"), os.path.join(trab, "tentativa")],
                       cwd=trab, env=amb, capture_output=True, text=True)
    caminho = os.path.join(trab, "match_frames.json")
    if p.returncode != 0 or not os.path.isfile(caminho):
        print(p.stdout[-1500:], p.stderr[-800:])
        return None, p
    with open(caminho, encoding="utf-8") as f:
        return json.load(f), p


def jogador(frame):
    return next(t for t in frame["tanks"] if t["id"] == 1)


def chegou(dados, ponto_id):
    p = next(p for p in dados["fase"]["pontos"] if p["id"] == ponto_id)
    for fr in dados["frames"]:
        j = jogador(fr)
        if math.hypot(j["x"] - p["x"], j["y"] - p["y"]) <= p["raio"]:
            return fr["tick"]
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trabalho", default="/tmp/frente_engine/modo_fase")
    ap.add_argument("--seed", type=int, default=123)
    a = ap.parse_args()
    trab = a.trabalho
    if not trab.startswith("/tmp/"):
        sys.exit("--trabalho precisa ficar em /tmp/")
    shutil.rmtree(trab, ignore_errors=True)
    shutil.copytree(ORCH, trab, ignore=IGNORAR)

    # 1. Cenário completo com o bot Python
    print("[1] cenario_exemplo.json + visitante.py")
    d, p = rodar(trab, "visitante.py", "cenario_exemplo.json", a.seed)
    conferir(d is not None, "partida rodou e gerou match_frames.json")
    if d is None:
        sys.exit(1)
    fase = d.get("fase", {})
    conferir(list(d.keys())[0] == "fase", "bloco fase no topo do match_frames.json")
    conferir(fase.get("id") == "90_exemplo_engine" and fase.get("seed") == a.seed, "fase.id e fase.seed")
    conferir(d["config"]["max_ticks"] == 400 and len(d["frames"]) <= 400, "max_ticks do cenario (400)")
    conferir([pt["id"] for pt in fase.get("pontos", [])] == ["B", "C"] and fase["pontos"][1]["ordem"] == 2,
             "pontos B e C na ordem")
    conferir(200 <= fase["pontos"][0]["y"] <= 800 and 550 <= fase["pontos"][1]["x"] <= 650, "faixas [min,max] sorteadas")
    alvo = fase["alvos"][0]
    conferir(alvo["id"] == "A1" and alvo["tank_id"] == 2, "alvo A1 = tanque 2")
    f0 = d["frames"][0]
    t2 = next(t for t in f0["tanks"] if t["id"] == 2)
    conferir(t2.get("alvo") is True and "alvo" not in jogador(f0), "\"alvo\": true so no tanque-alvo")
    movel = any(abs(next(t for t in fr["tanks"] if t["id"] == 2)["x"] - alvo["x"]) > 1e-9 for fr in d["frames"])
    conferir(not movel, "alvo inerte (nao se move)")
    morto = next((fr["tick"] for fr in d["frames"] if not next(t for t in fr["tanks"] if t["id"] == 2)["alive"]), None)
    conferir(morto is not None, f"alvo destruido pelo bot (tick {morto})")
    conferir(len(d["bots"]) == 1 and d["bots"][0]["nome"] == "visitante", "ranking so com o jogador")
    warns = [(fr["tick"], e) for fr in d["frames"] for e in fr["events"] if e["type"] == "ARTILLERY_WARN"]
    art = fase["artilharia"]
    conferir([t for t, _ in warns] == [x["tick"] for x in art], f"artilharia nos ticks programados {[t for t, _ in warns]}")
    conferir(100 <= art[2]["tick"] <= 120 and art[2]["raio"] == 60, "artilharia com tick sorteado e raio proprio")
    nj = art[1]
    pos_j = jogador(d["frames"][nj["tick"] - 1])
    dist = math.hypot(nj["x"] - pos_j["x"], nj["y"] - pos_j["y"])
    conferir(nj.get("no_jogador") is True and dist <= 30 + 5 + 1e-6, f"artilharia no_jogador a {dist:.1f} do jogador (desvio <= 30)")
    tipos = {h["type"] for fr in d["frames"] for h in fr["hazards"]}
    conferir("LASER" not in tipos, "sem laser (laser: false)")
    conferir(all(not fr["items"] for fr in d["frames"]), "sem itens (itens: false)")
    conferir(chegou(d, "B") is not None and chegou(d, "C") is not None,
             f"objetivo chegou ao bot: B no tick {chegou(d, 'B')}, C no tick {chegou(d, 'C')}")
    res = os.path.join(trab, "tentativa", "resultado_fase.json")
    if os.path.isfile(os.path.join(trab, "engine", "avaliar_fase.cpp")):
        conferir(os.path.isfile(res), "resultado_fase.json gerado em <pasta>/")
    else:
        conferir("avaliar_fase.cpp ausente" in p.stdout and not os.path.isfile(res),
                 "sem avaliar_fase.cpp: uma linha de log e a partida segue")

    # 2. Mesma seed = mesmo cenário
    d2, _ = rodar(trab, "visitante.py", "cenario_exemplo.json", a.seed)
    conferir(d2 is not None and d2["fase"] == fase, "mesma TOPCOM_FASE_SEED = mesmo cenario")
    d3, _ = rodar(trab, "visitante.py", "cenario_exemplo.json", a.seed + 1)
    conferir(d3 is not None and d3["fase"]["pontos"] != fase["pontos"], "outra seed = outro sorteio")

    # 3. 700 ticks sem zona, laser nem artilharia; itens fixos
    print("[2] cenario_sem_zona.json + vai_ponto_c.c (sem pontos: fica parado)")
    d, _ = rodar(trab, "vai_ponto_c.c", "cenario_sem_zona.json", a.seed)
    conferir(d is not None and len(d["frames"]) == 700, "700 frames")
    if d:
        hps = {jogador(fr)["hp"] for fr in d["frames"]}
        conferir(hps == {80.0}, f"vida constante 80 depois do tick 600 (sem zona): {sorted(hps)[:3]}")
        conferir(all(not fr["hazards"] for fr in d["frames"]), "nenhum hazard (artilharia: [], laser: false)")
        conferir(len(d["frames"][0]["items"]) == 2 and d["fase"]["itens"][0]["tipo"] == "HEAL", "itens fixos no tick 0")

    # 4. A chave objetivo chega às 4 linguagens (C pelo arena.h)
    print("[3] objetivo nas 4 linguagens")
    for bot in ("vai_ponto_c.c", "vai_ponto_cpp.cpp", "VaiPontoJava.java", "visitante.py"):
        d, _ = rodar(trab, bot, "cenario_exemplo.json", a.seed)
        conferir(d is not None and chegou(d, "B") is not None, f"{bot}: chegou ao ponto B (tick {chegou(d, 'B') if d else None})")

    print("RESULTADO:", "OK" if falhas == 0 else f"{falhas} FALHA(S)")
    sys.exit(1 if falhas else 0)


if __name__ == "__main__":
    main()
