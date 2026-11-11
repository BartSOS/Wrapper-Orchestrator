"""
Corpus de "pegadinhas" do validador: bots que a equipe pode enviar por engano (ou de propósito) e o
resultado esperado. Usado por .claude/skills/boca-gerenciar-engine/scripts/testar_kits.py (--pegadinhas
e --paridade). Para virar arquivos soltos (ex.: submeter pelo BOCA): testar_kits.py --exportar-pegadinhas DIR.

Cada caso: (linguagem, nome do caso, esperado, código)
  linguagem  c | cpp | java | py
  esperado   "YES" (o validador deve aprovar) ou um trecho que deve aparecer na mensagem do
             Compilation Error (as mensagens estão no ERROS_VALIDADOR.md do repositório Starter-Kit-Game-Jam)
"""

# Os casos usam a API em português (Acao/Estado), a que a equipe escreve.

# ------------------------------------------------------------------------------ C
_C_PENSAR = "Acao pensar(const Estado *e) {\n    Acao a = {0};\n    a.mover = FRENTE;\n    return a;\n}\n"

# ------------------------------------------------------------------------------ C++
_CPP_PENSAR = "Acao pensar(const Estado& e) {\n    Acao a;\n    a.mover = FRENTE;\n    return a;\n}\n"

# ------------------------------------------------------------------------------ Java
_JAVA_CORPO = ("        Acao a = new Acao();\n        a.mover = FRENTE;\n        a.atirar = false;\n"
               "        return a;\n")
_JAVA_OK = ("public class Solution extends Arena {\n    public static Acao pensar(Estado e) {\n"
            + _JAVA_CORPO + "    }\n}\n")

# ------------------------------------------------------------------------------ Python
_PY_RETORNO = "    acao = Acao()\n    acao.mover = FRENTE\n    return acao\n"
_PY_OK = "from arena import *\n\ndef pensar(estado):\n" + _PY_RETORNO

CASOS = [
    # ---- C: devem dar Compilation Error
    ("c", "main_extra", "nao crie a funcao main()", _C_PENSAR + "\nint main(void) { return 0; }\n"),
    ("c", "assinatura_sem_const", "assinatura de pensar esta errada",
     "Acao pensar(Estado *e) {\n    Acao a = {0};\n    return a;\n}\n"),
    ("c", "sem_pensar", "nao foi encontrada", "int x = 1;\n"),
    ("c", "pensar_so_em_comentario", "nao foi encontrada", "/* Acao pensar(const Estado *e) */\nint x = 1;\n"),
    ("c", "pensar_so_declarada", "nao esta definida", "Acao pensar(const Estado *e);\n"),
    ("c", "system", "funcoes de sistema",
     'Acao pensar(const Estado *e) {\n    Acao a = {0};\n    system("ls");\n    return a;\n}\n'),
    ("c", "include_absoluto", "#include", '#include "/etc/passwd"\n' + _C_PENSAR),
    ("c", "include_pai", "#include", '#include "../../conf.php"\n' + _C_PENSAR),
    ("c", "erro_de_sintaxe", "ERRO DE COMPILACAO",
     "Acao pensar(const Estado *e) {\n    Acao a = {0}\n    return a;\n}\n"),
    ("c", "usa_distancia_sem_definir", "distancia",
     "Acao pensar(const Estado *e) {\n    Acao a = {0};\n"
     "    if (distancia(0, 0, e->eu.x, e->eu.y) > 100) {\n        a.mover = FRENTE;\n    }\n"
     "    return a;\n}\n"),
    ("c", "campo_inexistente", "ERRO DE COMPILACAO",
     "Acao pensar(const Estado *e) {\n    Acao a = {0};\n    a.throttle = 1.0;\n    return a;\n}\n"),
    # ---- C: devem passar
    ("c", "system_em_comentario", "YES", _C_PENSAR + '/* nao use system("ls") */\n'),
    ("c", "include_arena_h", "YES", '#include "arena.h"\n\n' + _C_PENSAR),
    ("c", "funcao_propria_distancia", "YES",
     "double distancia(double x1, double y1, double x2, double y2);\n\n"
     "Acao pensar(const Estado *e) {\n    Acao a = {0};\n"
     "    if (distancia(0, 0, e->eu.x, e->eu.y) > 100) {\n        a.mover = FRENTE;\n    }\n"
     "    return a;\n}\n\n"
     "double distancia(double x1, double y1, double x2, double y2) {\n"
     "    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));\n}\n"),

    # ---- C++: devem dar Compilation Error
    ("cpp", "main_extra", "nao crie a funcao main()", _CPP_PENSAR + "\nint main() { return 0; }\n"),
    ("cpp", "assinatura_por_valor", "assinatura de pensar esta errada",
     "Acao pensar(Estado e) {\n    Acao a;\n    return a;\n}\n"),
    ("cpp", "retorno_void", "assinatura de pensar esta errada", "void pensar(const Estado& e) {\n}\n"),
    ("cpp", "sem_pensar", "nao foi encontrada", "int x = 1;\n"),
    ("cpp", "system", "funcoes de sistema",
     'Acao pensar(const Estado& e) {\n    system("ls");\n    Acao a;\n    return a;\n}\n'),
    ("cpp", "include_absoluto", "#include", '#include "/etc/passwd"\n' + _CPP_PENSAR),
    ("cpp", "include_pai", "#include", '#include "../../conf.php"\n' + _CPP_PENSAR),
    ("cpp", "erro_de_sintaxe", "ERRO DE COMPILACAO",
     "Acao pensar(const Estado& e) {\n    Acao a\n    return a;\n}\n"),
    ("cpp", "campo_inexistente", "ERRO DE COMPILACAO",
     "Acao pensar(const Estado& e) {\n    Acao a;\n    a.throttle = 1.0;\n    return a;\n}\n"),
    # ---- C++: devem passar
    ("cpp", "include_arena_hpp", "YES", '#include "arena.hpp"\n\n' + _CPP_PENSAR),
    ("cpp", "pensar_inline", "YES", "inline " + _CPP_PENSAR),
    ("cpp", "main_em_comentario", "YES", _CPP_PENSAR + "// int main() { }\n"),

    # ---- Java: devem dar Compilation Error
    ("java", "package", "package", "package meubot;\n\n" + _JAVA_OK),
    ("java", "pensar_nao_estatico", "public static Acao pensar",
     "public class Solution extends Arena {\n    public Acao pensar(Estado e) {\n"
     + _JAVA_CORPO + "    }\n}\n"),
    ("java", "assinatura_string", "public static Acao pensar",
     "public class Solution extends Arena {\n    public static Acao pensar(String e) {\n"
     + _JAVA_CORPO + "    }\n}\n"),
    ("java", "pensar_em_classe_aninhada", "public static Acao pensar",
     "public class Solution extends Arena {\n    public static class Ajuda {\n"
     "        public static Acao pensar(Estado e) {\n" + _JAVA_CORPO + "        }\n    }\n}\n"),
    ("java", "sem_pensar", "nao foi encontrado", "public class Solution {\n    int x = 1;\n}\n"),
    ("java", "runtime_exec", "execucao de sistema",
     "public class Solution extends Arena {\n    public static Acao pensar(Estado e) {\n"
     '        try { Runtime.getRuntime().exec("ls"); } catch (Exception ex) { }\n' + _JAVA_CORPO + "    }\n}\n"),
    ("java", "system_exit", "execucao de sistema",
     "public class Solution extends Arena {\n    public static Acao pensar(Estado e) {\n"
     "        System.exit(0);\n" + _JAVA_CORPO + "    }\n}\n"),
    ("java", "erro_de_sintaxe", "ERRO DE COMPILACAO",
     "public class Solution extends Arena {\n    public static Acao pensar(Estado e) {\n"
     "        Acao a = new Acao()\n        return a;\n    }\n}\n"),
    ("java", "import_inexistente", "ERRO DE COMPILACAO", "import foo.Bar;\n\n" + _JAVA_OK),
    ("java", "campo_inexistente", "ERRO DE COMPILACAO",
     "public class Solution extends Arena {\n    public static Acao pensar(Estado e) {\n"
     "        Acao a = new Acao();\n        a.throttle = 1.0;\n        return a;\n    }\n}\n"),
    # ---- Java: devem passar
    ("java", "classe_auxiliar_antes", "YES",
     "class Ajuda {\n    static double dobro(double x) { return 2 * x; }\n}\n\n" + _JAVA_OK),
    ("java", "comentario_class_principal", "YES", "// class principal do bot\n" + _JAVA_OK),
    ("java", "sem_classe_publica", "YES", _JAVA_OK.replace("public class Solution", "class Solution")),
    ("java", "main_extra", "YES",
     _JAVA_OK.replace("public class Solution extends Arena {\n",
                      "public class Solution extends Arena {\n    public static void main(String[] args) { }\n")),
    ("java", "nome_de_classe_diferente", "YES", _JAVA_OK.replace("Solution", "MeuBot")),

    # ---- Python: devem dar Compilation Error
    ("py", "numpy", "biblioteca externa 'numpy'", "import numpy as np\n\n" + _PY_OK),
    ("py", "import_os", "'os'", "import os\n\n" + _PY_OK),
    ("py", "from_os", "'os'", "from os import path\n\n" + _PY_OK),
    ("py", "subprocess", "'subprocess'", "import subprocess\n\n" + _PY_OK),
    ("py", "eval", "eval()", "from arena import *\n\ndef pensar(estado):\n    x = eval('1 + 1')\n" + _PY_RETORNO),
    ("py", "exec", "exec()", "from arena import *\n\ndef pensar(estado):\n    exec('x = 1')\n" + _PY_RETORNO),
    ("py", "dunder_import", "__import__", "m = __import__('math')\n\n" + _PY_OK),
    ("py", "importlib", "'importlib'", "import importlib\n\n" + _PY_OK),
    ("py", "import_relativo", "import relativo", "from . import ajuda\n\n" + _PY_OK),
    ("py", "input_no_arquivo", "input()", "nome = input()\n\n" + _PY_OK),
    ("py", "stdin_no_arquivo", "sys.stdin", "import sys\ndados = sys.stdin.read()\n\n" + _PY_OK),
    ("py", "main_guard", "__main__",
     _PY_OK + "\nif __name__ == '__main__':\n    print(pensar({}))\n"),
    ("py", "sem_pensar", "nao foi encontrada", "x = 1\n"),
    ("py", "pensar_dois_parametros", "exatamente 1 parametro",
     "from arena import *\n\ndef pensar(estado, extra):\n" + _PY_RETORNO),
    ("py", "pensar_async", "async", "from arena import *\n\nasync def pensar(estado):\n" + _PY_RETORNO),
    ("py", "pensar_dentro_de_classe", "nao foi encontrada",
     "class Bot:\n    def pensar(self, estado):\n        return None\n"),
    ("py", "indentacao", "erro de sintaxe", "def pensar(estado):\nreturn {}\n"),
    ("py", "return_fora_de_funcao", "erro de sintaxe", "return 1\n\n" + _PY_OK),
    # ---- Python: devem passar
    ("py", "parametro_com_padrao", "YES", "from arena import *\n\ndef pensar(estado, extra=None):\n" + _PY_RETORNO),
    ("py", "varias_bibliotecas_padrao", "YES",
     "import math, random, heapq\nfrom collections import deque\n" + _PY_OK),
    ("py", "input_dentro_de_funcao", "YES",
     "def ajuda():\n    return input()\n\n" + _PY_OK),
    ("py", "pensar_lambda", "YES", "from arena import *\n\npensar = lambda estado: Acao(mover=FRENTE)\n"),
    ("py", "from_future", "YES", "from __future__ import annotations\n" + _PY_OK),
    ("py", "main_guard_em_comentario", "YES", _PY_OK + "# if __name__ == '__main__': pensar({})\n"),
    ("py", "bom_com_classe", "YES",
     "from arena import *\n\nclass Memoria:\n    contador = 0\n\nmem = Memoria()\n\n"
     "def pensar(estado):\n    mem.contador += 1\n" + _PY_RETORNO),
]

EXTENSAO = {"c": "c", "cpp": "cpp", "java": "java", "py": "py"}


def nome_arquivo(linguagem, caso):
    """Nome do arquivo de um caso; Java usa o nome da classe pública (Solution, ou MeuBot no caso de nome)."""
    ext = EXTENSAO[linguagem]
    if linguagem == "java":
        return f"{caso}/MeuBot.java" if caso == "nome_de_classe_diferente" else f"{caso}/Solution.java"
    return f"{caso}.{ext}"
