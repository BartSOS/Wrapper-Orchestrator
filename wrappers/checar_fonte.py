#!/usr/bin/env python3
"""
Verificações estáticas dos bots em C, C++ e Java (usado pelo build_bot.sh).

Uso: python3 -I checar_fonte.py {c|cpp|java} <arquivo>

Não compila nem executa nada. Sai com 0 se o fonte passou nas verificações; senão
imprime UMA linha "ERRO DE VALIDACAO: ..." em stderr e sai com 1. Para Java, imprime
em stdout o nome da classe principal (a que recebe o arquivo <Classe>.java).

Os comentários são ignorados nas buscas (um "system(" ou "class X" dentro de comentário
não conta). Mensagens em ASCII: aparecem no [Ver Erro] do BOCA.
"""
import re
import sys


def falha(msg):
    sys.stderr.write("ERRO DE VALIDACAO: " + msg + "\n")
    sys.exit(1)


def sem_comentarios(texto, apagar_strings=False):
    """Troca comentários (e, opcionalmente, o conteúdo de strings) por espaços, mantendo as quebras de linha."""
    saida = []
    i, n = 0, len(texto)
    while i < n:
        c = texto[i]
        c2 = texto[i:i + 2]
        if c2 == "//":
            while i < n and texto[i] != "\n":
                saida.append(" ")
                i += 1
        elif c2 == "/*":
            fim = texto.find("*/", i + 2)
            fim = n if fim < 0 else fim + 2
            saida.extend(ch if ch == "\n" else " " for ch in texto[i:fim])
            i = fim
        elif c in "\"'":
            aspa, j = c, i + 1
            while j < n and texto[j] != aspa and texto[j] != "\n":
                j += 2 if texto[j] == "\\" else 1
            j = min(j, n)
            fechou = j < n and texto[j] == aspa
            fim = j + 1 if fechou else j
            if apagar_strings:
                saida.append(aspa + " " * (fim - i - (2 if fechou else 1)) + (aspa if fechou else ""))
            else:
                saida.append(texto[i:fim])
            i = fim
        else:
            saida.append(c)
            i += 1
    return "".join(saida)


def linha_de(texto, pos):
    return texto.count("\n", 0, pos) + 1


def checar_includes(texto):
    """#include com caminho absoluto ou '..' sai do kit (e expõe arquivos do servidor nas mensagens do compilador)."""
    for m in re.finditer(r'^[ \t]*#[ \t]*include[ \t]*[<"]([^>"\n]*)[>"]', texto, re.M):
        caminho = m.group(1).strip()
        if caminho.startswith("/") or ".." in caminho or "\\" in caminho:
            falha(f"o #include \"{caminho}\" (linha {linha_de(texto, m.start())}) nao e permitido: "
                  "use apenas as bibliotecas padrao e os arquivos do kit (arena.h, arena.hpp, nlohmann/json.hpp).")


def checar_c_cpp(linguagem, texto):
    puro = sem_comentarios(texto)
    sem_str = sem_comentarios(texto, apagar_strings=True)
    if not re.search(r"\bpensar\s*\(", sem_str):
        if linguagem == "c":
            falha("a funcao 'Acao pensar(const Estado *e)' nao foi encontrada no seu bot.")
        falha("a funcao 'Acao pensar(const Estado& e)' nao foi encontrada no seu bot.")
    if re.search(r"\bsystem\s*\(", sem_str):
        falha("uso de funcoes de sistema (system) nao e permitido.")
    checar_includes(puro)


def checar_java(texto):
    sem_str = sem_comentarios(texto, apagar_strings=True)
    if not re.search(r"\bpensar\s*\(", sem_str):
        falha("o metodo 'public static Acao pensar(Estado e)' nao foi encontrado no seu bot.")
    if re.search(r"Runtime\s*\.\s*getRuntime|ProcessBuilder|System\s*\.\s*exit", sem_str):
        falha("chamada de execucao de sistema nao e permitida (Runtime.getRuntime, ProcessBuilder, System.exit).")
    m = re.search(r"^[ \t]*package\s+[\w.]+\s*;", sem_str, re.M)
    if m:
        falha(f"remova a linha 'package' (linha {linha_de(sem_str, m.start())}): o bot deve ser um unico "
              "arquivo, sem package.")
    classe = classe_principal(sem_str)
    if classe in ("Arena", "JavaRunner"):
        falha(f"a classe do seu bot nao pode se chamar {classe} (nome usado pela arena): "
              "use, por exemplo, 'public class MeuBot extends Arena'.")
    return classe


def classe_principal(sem_str):
    """Classe pública (mesma regex do fvalidator.php) ou, sem ela, a classe de nível superior que contém pensar(."""
    m = re.search(r"public\s+(?:final\s+|abstract\s+)*class\s+([A-Za-z0-9_]+)", sem_str)
    if m:
        return m.group(1)
    pos_pensar = re.search(r"\bpensar\s*\(", sem_str).start()
    nivel, topo = 0, []   # classes de nível superior: (nome, início, fim)
    abertas = []
    for m in re.finditer(r"\bclass\s+([A-Za-z0-9_]+)|[{}]", sem_str):
        token = m.group(0)
        if token == "{":
            nivel += 1
        elif token == "}":
            nivel -= 1
            if abertas and nivel == abertas[-1][1]:
                nome, _, ini = abertas.pop()
                topo.append((nome, ini, m.end()))
        else:
            if nivel == 0:
                # a chave de abertura vem depois do nome (extends/implements); registra o nível 0
                abertas.append((m.group(1), 0, m.start()))
    for nome, ini, fim in topo:
        if ini <= pos_pensar < fim:
            return nome
    if topo:
        return topo[0][0]
    falha("nenhuma classe foi encontrada no codigo Java.")


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("c", "cpp", "java"):
        sys.stderr.write("Uso: checar_fonte.py {c|cpp|java} <arquivo>\n")
        sys.exit(2)
    linguagem, caminho = sys.argv[1], sys.argv[2]
    with open(caminho, "rb") as f:
        texto = f.read().decode("utf-8", errors="replace")
    if linguagem == "java":
        print(checar_java(texto))
    else:
        checar_c_cpp(linguagem, texto)


if __name__ == "__main__":
    main()
