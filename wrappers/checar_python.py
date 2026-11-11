#!/usr/bin/env python3
"""
Verificações estáticas dos bots em Python (usado pelo build_bot.sh, que o chama com python3 -I).

Uso: python3 -I checar_python.py <arquivo>

Analisa a AST: NÃO executa nem importa o código da equipe. Sai com 0 se o bot passou; senão
imprime UMA linha "ERRO DE VALIDACAO: ..." em stderr e sai com 1. Mensagens em ASCII: aparecem
no [Ver Erro] do BOCA.

Regras:
- erro de sintaxe (inclui 'return' fora de função e afins);
- def pensar(estado) no nível do arquivo, com 1 parâmetro (chamável com 1 argumento);
- imports só da biblioteca padrão (sys.stdlib_module_names) e do arquivo de apoio "arena"
  (from arena import *): sem bibliotecas externas e sem import relativo; os, subprocess e
  importlib são proibidos;
- sem eval(), exec(), __import__;
- sem input() nem sys.stdin no nível do arquivo (a arena entrega o estado pelo argumento de pensar);
- sem o bloco "if __name__ == '__main__':" (o arquivo roda como script, junto com o executor da
  arena: o bloco rodaria antes do laço da arena e poluiria o canal com a engine).
"""
import ast
import sys

PROIBIDOS = {"os", "subprocess", "importlib"}


def falha(msg):
    sys.stderr.write("ERRO DE VALIDACAO: " + msg + "\n")
    sys.exit(1)


def checar_import(topo, linha):
    if topo == "arena":
        return  # arquivo de apoio da arena (wrappers/arena.py), já existe no servidor
    if topo in PROIBIDOS:
        falha(f"o modulo '{topo}' nao e permitido (linha {linha}).")
    if topo not in sys.stdlib_module_names:
        falha(f"a biblioteca externa '{topo}' nao e permitida (linha {linha}): use apenas a "
              "biblioteca padrao do Python 3 (math, random, collections, heapq...).")


def eh_main_guard(no):
    """if __name__ == "__main__":"""
    if not isinstance(no, ast.If) or not isinstance(no.test, ast.Compare) or len(no.test.ops) != 1:
        return False
    partes = [no.test.left] + no.test.comparators
    nomes = [p.id for p in partes if isinstance(p, ast.Name)]
    textos = [p.value for p in partes if isinstance(p, ast.Constant)]
    return "__name__" in nomes and "__main__" in textos


class NivelDoArquivo(ast.NodeVisitor):
    """Percorre só o que roda ao carregar o arquivo (não entra em corpos de função nem lambda)."""

    def visit_FunctionDef(self, no):
        for d in no.decorator_list + no.args.defaults + [x for x in no.args.kw_defaults if x]:
            self.visit(d)

    visit_AsyncFunctionDef = visit_FunctionDef

    def visit_Lambda(self, no):
        for d in no.args.defaults + [x for x in no.args.kw_defaults if x]:
            self.visit(d)

    def visit_Call(self, no):
        if isinstance(no.func, ast.Name) and no.func.id == "input":
            falha(f"input() no nivel do arquivo (linha {no.lineno}) trava o bot: a arena entrega o "
                  "estado como argumento de pensar(estado).")
        self.generic_visit(no)

    def visit_Attribute(self, no):
        if isinstance(no.value, ast.Name) and no.value.id == "sys" and no.attr in ("stdin", "__stdin__"):
            falha(f"sys.stdin (linha {no.lineno}) nao pode ser usado: a arena entrega o estado como "
                  "argumento de pensar(estado).")
        self.generic_visit(no)


def checar_pensar(arvore):
    for no in arvore.body:
        if isinstance(no, ast.AsyncFunctionDef) and no.name == "pensar":
            falha("pensar nao pode ser 'async': use 'def pensar(estado):'.")
        if isinstance(no, ast.FunctionDef) and no.name == "pensar":
            a = no.args
            posicionais = len(a.posonlyargs) + len(a.args)
            obrigatorios = posicionais - len(a.defaults)
            kw_obrigatorios = [k for k, d in zip(a.kwonlyargs, a.kw_defaults) if d is None]
            if obrigatorios > 1 or kw_obrigatorios or (posicionais == 0 and a.vararg is None):
                falha(f"a funcao pensar (linha {no.lineno}) deve ter exatamente 1 parametro: "
                      "'def pensar(estado):'.")
            return
        if isinstance(no, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "pensar" for t in no.targets):
            return
    falha("a funcao 'def pensar(estado):' nao foi encontrada no seu bot (defina-a no nivel do "
          "arquivo, sem recuo).")


def main():
    if len(sys.argv) != 2:
        sys.stderr.write("Uso: checar_python.py <arquivo>\n")
        sys.exit(2)
    nome = sys.argv[1]
    with open(nome, "rb") as f:
        dados = f.read()
    try:
        compile(dados, "bot.py", "exec", dont_inherit=True)   # só compila; nada é executado
        arvore = ast.parse(dados, "bot.py")
    except SyntaxError as e:
        trecho = (e.text or "").strip()
        falha(f"erro de sintaxe na linha {e.lineno}: {e.msg}" + (f"\n    {trecho}" if trecho else ""))
    except ValueError as e:
        falha(f"o arquivo nao pode ser lido como Python: {e}")

    checar_pensar(arvore)

    for no in ast.walk(arvore):
        if isinstance(no, ast.Import):
            for alias in no.names:
                checar_import(alias.name.split(".")[0], no.lineno)
        elif isinstance(no, ast.ImportFrom):
            if no.level:
                falha(f"import relativo (linha {no.lineno}) nao e permitido: mantenha todo o codigo em "
                      "um unico arquivo.")
            checar_import((no.module or "").split(".")[0], no.lineno)
            if no.module == "sys" and any(a.name == "stdin" for a in no.names):
                falha(f"sys.stdin (linha {no.lineno}) nao pode ser usado: a arena entrega o estado como "
                      "argumento de pensar(estado).")
        elif isinstance(no, ast.Call) and isinstance(no.func, ast.Name) and no.func.id in ("eval", "exec"):
            falha(f"o uso de {no.func.id}() (linha {no.lineno}) nao e permitido.")
        elif isinstance(no, ast.Name) and no.id == "__import__":
            falha(f"o uso de __import__ (linha {no.lineno}) nao e permitido.")

    for no in arvore.body:
        if eh_main_guard(no):
            falha(f"remova o bloco \"if __name__ == '__main__':\" (linha {no.lineno}): a arena ja chama "
                  "pensar(estado) por voce, e esse bloco rodaria antes e atrapalharia a comunicacao.")
    NivelDoArquivo().visit(arvore)


if __name__ == "__main__":
    main()
