#!/usr/bin/env python3
"""Testes do avaliar_fase com replays sintéticos (sem a engine).

1. Casos de borda escritos à mão (entrada inválida, tanque que não entrou, timeout,
   chegada depois do limite, balão com mais de um critério).
2. Bots de referência (mock_bots/treino/<fase>__<rotulo>.py) rodados no mini-simulador
   (testes/simulador.py) com várias seeds; o veredito tem de bater com esperado.json.

Uso (WSL): python3 testar_avaliador.py [--bin CAMINHO] [--seeds N] [--tabela]
  --bin     avaliar_fase já compilado (padrão: compila em /tmp/frente_fases/avaliar_fase)
  --seeds   seeds por bot (padrão 20)
  --tabela  imprime, por fase e bot, os balões e a faixa de ticks/tiros/dano
Saída: uma linha por falha e o resumo; código 0 se tudo passou.
"""
import argparse
import copy
import json
import os
import subprocess
import sys
import tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
TREINO = os.path.dirname(AQUI)
ORCH = os.path.dirname(os.path.dirname(TREINO))
FASES = os.path.join(ORCH, "fases")
sys.path.insert(0, AQUI)
import simulador  # noqa: E402


def compilar(destino):
    os.makedirs(os.path.dirname(destino), exist_ok=True)
    fonte = os.path.join(ORCH, "engine", "avaliar_fase.cpp")
    r = subprocess.run(["g++", "-std=c++20", "-O2", "-o", destino, fonte], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr[-2000:])
        sys.exit(1)


def avaliar(binario, fase, replay, tmp):
    fp, rp = os.path.join(tmp, "fase.json"), os.path.join(tmp, "replay.json")
    with open(fp, "w", encoding="utf-8") as f:
        json.dump(fase, f, ensure_ascii=False)
    with open(rp, "w", encoding="utf-8") as f:
        json.dump(replay, f)
    r = subprocess.run([binario, fp, rp], capture_output=True, text=True)
    try:
        saida = json.loads(r.stdout)
    except ValueError:
        saida = {"_stdout": r.stdout[:300]}
    return r.returncode, saida


def carregar_fase(fid):
    with open(os.path.join(FASES, fid + ".json"), encoding="utf-8") as f:
        return json.load(f)


def bot_parado(_estado):
    return {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}


def casos_de_borda(binario, tmp):
    falhas = []

    def conferir(nome, cond, saida):
        if not cond:
            falhas.append(f"[borda] {nome}: {json.dumps(saida, ensure_ascii=False)[:400]}")

    f01 = carregar_fase("01_andar")
    rep = simulador.simular(f01, bot_parado, 1)
    # Entrada inválida
    ruim = copy.deepcopy(f01)
    ruim["objetivos"] = [{"tipo": "voar", "ate_tick": 10}]
    cod, s = avaliar(binario, ruim, rep, tmp)
    conferir("tipo desconhecido -> 2", cod == 2 and "erro" in s, s)
    ruim = copy.deepcopy(f01)
    ruim["baloes"] = [{"baloes": 2, "metrica": "pontos", "max": 1}]
    cod, s = avaliar(binario, ruim, rep, tmp)
    conferir("métrica desconhecida -> 2", cod == 2, s)
    outra = copy.deepcopy(rep)
    outra["fase"]["id"] = "02_parar"
    cod, s = avaliar(binario, f01, outra, tmp)
    conferir("replay de outra fase -> 2", cod == 2, s)
    sem_bloco = copy.deepcopy(rep)
    del sem_bloco["fase"]
    cod, s = avaliar(binario, f01, sem_bloco, tmp)
    conferir("faixa sem bloco fase -> 2", cod == 2, s)
    cod, s = avaliar(binario, f01, {"nada": 1}, tmp)
    conferir("replay sem frames -> 2", cod == 2, s)
    r = subprocess.run([binario, "/nao/existe.json", "/nao/existe2.json"], capture_output=True, text=True)
    conferir("arquivo ausente -> 2", r.returncode == 2, r.stdout)
    # Fase com pontos fixos, sem o bloco fase: usa o cenário
    fixo = copy.deepcopy(f01)
    fixo["cenario"]["pontos"] = [{"id": "B", "x": 800, "y": 500, "raio": 30}]
    fixo["cenario"]["jogador"]["y"] = 500
    fixo["cenario"]["jogador"]["angulo"] = 0
    rep_fixo = simulador.simular(fixo, simulador.carregar_bot(os.path.join(TREINO, "01_andar__3baloes.py")), 1)
    del rep_fixo["fase"]
    cod, s = avaliar(binario, fixo, rep_fixo, tmp)
    conferir("pontos fixos sem bloco fase -> avalia", cod == 0 and s.get("passou") is True, s)
    # Parado: não chegou e quase não se mexeu
    cod, s = avaliar(binario, f01, rep, tmp)
    conferir("parado -> não passou", cod == 0 and s["passou"] is False and "quase não se mexeu" in s["motivo"], s)
    conferir("parado -> proximo_balao", s.get("proximo_balao", "").startswith("1 balão"), s)
    # Tanque não entrou na arena
    vazio = copy.deepcopy(rep)
    for fr in vazio["frames"]:
        fr["tanks"] = []
    cod, s = avaliar(binario, f01, vazio, tmp)
    conferir("não entrou -> passou false", cod == 0 and s["passou"] is False and "não entrou" in s["motivo"], s)
    # Timeout no tick 50
    bot3 = simulador.carregar_bot(os.path.join(TREINO, "01_andar__3baloes.py"))
    rep_to = simulador.simular(f01, bot3, 1, morrer_no_tick=50)
    cod, s = avaliar(binario, f01, rep_to, tmp)
    conferir("timeout -> motivo 'deixou de responder'", s.get("passou") is False and "deixou de responder" in s.get("motivo", ""), s)
    # Chegada depois do limite
    tarde = copy.deepcopy(f01)
    tarde["objetivos"][0]["ate_tick"] = 50
    rep3 = simulador.simular(f01, bot3, 1)
    cod, s = avaliar(binario, tarde, rep3, tmp)
    conferir("chegou depois do limite", s["passou"] is False and "depois do limite" in s["motivo"], s)
    # Motivo de falha tem no máximo 3 frases, mesmo com dois objetivos falhos ou com a morte do tanque
    f05 = carregar_fase("05_mira_andando")
    cod, s = avaliar(binario, f05, simulador.simular(f05, bot_parado, 1), tmp)
    conferir("dois objetivos falhos -> 3 frases, cita os dois",
             frases(s["motivo"]) <= 3 and "ponto B" in s["motivo"] and "alvos" in s["motivo"], s)
    cod, s = avaliar(binario, f05, simulador.simular(f05, bot_parado, 1, morrer_no_tick=50), tmp)
    conferir("dois objetivos falhos e morte -> 3 frases, cita a morte",
             frases(s["motivo"]) <= 3 and "deixou de responder" in s["motivo"], s)
    conferir("timeout -> no máximo 3 frases", frases(avaliar(binario, f01, rep_to, tmp)[1]["motivo"]) <= 3, s)
    # Balão com dois critérios (o segundo inalcançável) e proximo_balao
    duas = copy.deepcopy(f01)
    duas["baloes"] = [{"baloes": 2, "metrica": "ticks", "max": 1000},
                        {"baloes": 3, "metrica": "ticks", "max": 1000},
                        {"baloes": 3, "metrica": "tiros", "max": -1}]
    cod, s = avaliar(binario, duas, rep3, tmp)
    conferir("3 balões exige os dois critérios", s["baloes"] == 2 and "tiros" in (s.get("proximo_balao") or ""), s)
    conferir("métricas completas", set(s.get("metricas", {})) == {"ticks", "tiros", "vida", "dano_recebido"}, s)
    return falhas


def frases(motivo):
    """Quantas frases tem o motivo (decimal usa vírgula: ". " só separa frases)."""
    return motivo.count(". ") + 1


def bots_de_referencia(binario, tmp, seeds):
    with open(os.path.join(TREINO, "esperado.json"), encoding="utf-8") as f:
        esperado = json.load(f)
    falhas, tabela = [], {}
    for nome, esp in sorted(esperado.items()):
        if nome.startswith("_"):
            continue
        fid = nome.split("__")[0]
        fase = carregar_fase(fid)
        caminho = os.path.join(TREINO, nome + ".py")
        linhas = tabela.setdefault(nome, [])
        for seed in range(1, seeds + 1):
            pensar = simulador.carregar_bot(caminho)   # recarrega: zera as variáveis globais do bot
            cod, s = avaliar(binario, fase, simulador.simular(fase, pensar, seed), tmp)
            linhas.append(s)
            ok = (cod == 0 and s.get("passou") == esp["passou"] and s.get("baloes") == esp["baloes"]
                  and esp.get("motivo_contem", "") in s.get("motivo", "")
                  and (s.get("passou") or frases(s.get("motivo", "")) <= 3))
            if not ok:
                falhas.append(f"[bot] {nome} seed {seed}: esperado passou={esp['passou']} baloes={esp['baloes']}; "
                              f"veio passou={s.get('passou')} baloes={s.get('baloes')} metricas={s.get('metricas')} "
                              f"motivo={s.get('motivo')}")
    return falhas, tabela


def faixa(valores):
    return f"{min(valores)}-{max(valores)}" if valores else "-"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin")
    ap.add_argument("--seeds", type=int, default=20)
    ap.add_argument("--tabela", action="store_true")
    a = ap.parse_args()
    binario = a.bin or "/tmp/frente_fases/avaliar_fase"
    if not a.bin:
        compilar(binario)
    with tempfile.TemporaryDirectory(prefix="avaliar_fase_") as tmp:
        falhas = casos_de_borda(binario, tmp)
        f2, tabela = bots_de_referencia(binario, tmp, a.seeds)
        falhas += f2
    if a.tabela:
        print(f"{'bot':32} {'balões':>12} {'ticks':>9} {'tiros':>7} {'dano':>7}")
        for nome, linhas in tabela.items():
            est = sorted({s.get('baloes') for s in linhas})
            m = [s.get("metricas", {}) for s in linhas]
            print(f"{nome:32} {str(est):>12} {faixa([x.get('ticks', 0) for x in m]):>9} "
                  f"{faixa([x.get('tiros', 0) for x in m]):>7} {faixa([x.get('dano_recebido', 0) for x in m]):>7}")
    for f in falhas[:30]:
        print(f)
    total = len(tabela) * a.seeds
    print(f"Resumo: {len(falhas)} falha(s); {total} tentativas de bots de referência ({a.seeds} seeds) + casos de borda.")
    sys.exit(1 if falhas else 0)


if __name__ == "__main__":
    main()
