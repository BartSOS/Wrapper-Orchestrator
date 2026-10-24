# Mensagens de Compilation Error do validador (contrato C-K3)

Material de apoio para o enunciado e o README. **Não vai no zip do kit.** Versão 1 (provisória até a bateria de pegadinhas fechar).

O validador do BOCA monta o bot exatamente como a partida monta (`wrappers/build_bot.sh`, o mesmo programa). Quem recebe YES compila na arena; quem recebe Compilation Error vê o motivo em **[Ver Erro]**. As mensagens abaixo são o texto exato (sem acentos, de propósito: aparecem num arquivo de texto). `N` é o número da linha do código enviado.

Formato:
- Caso reconhecido: uma linha `ERRO DE VALIDACAO: <mensagem>`.
- Qualquer outro erro do compilador: `ERRO DE COMPILACAO: o compilador recusou o seu codigo.` seguido da saída do compilador (gcc/g++/javac) ou, em Python, da linha `erro de sintaxe na linha N: ...`.
- O validador espera no máximo 60 s; passando disso: `Timeout de compilação excedido (60s). Verifique macros, loops de pré-processamento ou includes.`

Comentários não contam: `system(` ou `class X` escritos dentro de comentário são ignorados.

## Todas as linguagens

| Quando | Mensagem |
| :--- | :--- |
| O compilador recusa o código por outro motivo (erro de sintaxe, tipo, símbolo não definido...) | `ERRO DE COMPILACAO: o compilador recusou o seu codigo.` + saída do compilador |

## C

| Quando | Mensagem | Como corrigir |
| :--- | :--- | :--- |
| Não há `pensar(` no código (ou só em comentário) | `ERRO DE VALIDACAO: a funcao 'Acao pensar(const Estado *e)' nao foi encontrada no seu bot.` | Defina `Acao pensar(const Estado *e)` |
| `pensar` só declarada, sem corpo | `ERRO DE VALIDACAO: a funcao 'Acao pensar(const Estado *e)' nao esta definida (ela so aparece em comentario ou foi apenas declarada).` | Escreva o corpo da função |
| Assinatura diferente (ex.: `Estado *e` sem `const`) | `ERRO DE VALIDACAO: a assinatura de pensar esta errada: use exatamente 'Acao pensar(const Estado *e)'.` | Use a assinatura do kit |
| Função `main` no código | `ERRO DE VALIDACAO: nao crie a funcao main(): a arena ja tem a dela. Apague a sua main().` | Apague a `main` |
| Chamada a `system(` | `ERRO DE VALIDACAO: uso de funcoes de sistema (system) nao e permitido.` | Remova |
| `#include` com caminho absoluto ou `..` | `ERRO DE VALIDACAO: o #include "<caminho>" (linha N) nao e permitido: use apenas as bibliotecas padrao e os arquivos do kit (arena.h, nlohmann/json.hpp).` | Use só bibliotecas padrão e `arena.h` |
| Usa `distancia`, `angulo_para`, `diferenca_angulo` ou `normalizar_angulo` sem defini-las | `ERRO DE COMPILACAO: ...` com `undefined reference to 'distancia'` (ou o nome da função) | Copie as funções da seção AJUDANTES do `bot.c` do kit (elas não vêm mais no `arena.h`) |

## C++

| Quando | Mensagem | Como corrigir |
| :--- | :--- | :--- |
| Não há `pensar(` no código (ou só em comentário) | `ERRO DE VALIDACAO: a funcao 'json pensar(const json& estado)' nao foi encontrada no seu bot.` | Defina `json pensar(const json& estado)` |
| Assinatura diferente (ex.: `json pensar(json estado)` ou `void pensar(...)`) | `ERRO DE VALIDACAO: a assinatura de pensar esta errada: use exatamente 'json pensar(const json& estado)'.` | Use a assinatura do kit |
| Função `main` no código | `ERRO DE VALIDACAO: nao crie a funcao main(): a arena ja tem a dela. Apague a sua main().` | Apague a `main` |
| Chamada a `system(` | `ERRO DE VALIDACAO: uso de funcoes de sistema (system) nao e permitido.` | Remova |
| `#include` com caminho absoluto ou `..` | mesma mensagem do C | Use só bibliotecas padrão e `<nlohmann/json.hpp>` |

`#include <nlohmann/json.hpp>`, `using json = nlohmann::json;` e `#include "cpp_header.hpp"` são aceitos.

## Java

| Quando | Mensagem | Como corrigir |
| :--- | :--- | :--- |
| Não há `pensar(` no código (ou só em comentário) | `ERRO DE VALIDACAO: o metodo 'public static JsonObject pensar(JsonObject estado)' nao foi encontrado no seu bot.` | Defina o método |
| Linha `package ...;` | `ERRO DE VALIDACAO: remova a linha 'package' (linha N): o bot deve ser um unico arquivo, sem package.` | Apague o `package` |
| `Runtime.getRuntime`, `ProcessBuilder` ou `System.exit` | `ERRO DE VALIDACAO: chamada de execucao de sistema nao e permitida (Runtime.getRuntime, ProcessBuilder, System.exit).` | Remova |
| Nenhuma classe no arquivo | `ERRO DE VALIDACAO: nenhuma classe foi encontrada no codigo Java.` | Escreva a classe |
| `pensar` não estática, com outra assinatura ou fora da classe principal (em classe aninhada, por exemplo) | `ERRO DE VALIDACAO: o metodo pensar esta errado ou fora da classe principal: use exatamente 'public static JsonObject pensar(JsonObject estado)', dentro da classe publica do arquivo.` | Use a assinatura do kit, na classe pública |

A classe principal é a pública do arquivo (ou, se não houver, a que contém `pensar`). Classes auxiliares no nível do arquivo (sem `public`) funcionam, mas o mais simples é deixá-las dentro da classe principal, como o kit faz. Um `main` extra é ignorado.

## Python

Todas as mensagens abaixo começam com `ERRO DE VALIDACAO: `.

| Quando | Mensagem |
| :--- | :--- |
| Erro de sintaxe (inclui indentação e `return` fora de função) | `erro de sintaxe na linha N: <detalhe do Python>` e, na linha seguinte, o trecho |
| Não há `def pensar` no nível do arquivo | `a funcao 'def pensar(estado):' nao foi encontrada no seu bot (defina-a no nivel do arquivo, sem recuo).` |
| `pensar` exige mais de 1 argumento | `a funcao pensar (linha N) deve ter exatamente 1 parametro: 'def pensar(estado):'.` |
| `async def pensar` | `pensar nao pode ser 'async': use 'def pensar(estado):'.` |
| `import os`, `import subprocess` ou `import importlib` (inclusive `from os import ...`) | `o modulo 'os' nao e permitido (linha N).` (com o nome do módulo) |
| Biblioteca fora da biblioteca padrão (ex.: `numpy`) | `a biblioteca externa 'numpy' nao e permitida (linha N): use apenas a biblioteca padrao do Python 3 (math, random, collections, heapq...).` |
| Import relativo (`from . import x`) | `import relativo (linha N) nao e permitido: mantenha todo o codigo em um unico arquivo.` |
| Chamada a `eval(` ou `exec(` | `o uso de eval() (linha N) nao e permitido.` (ou `exec()`) |
| Uso de `__import__` | `o uso de __import__ (linha N) nao e permitido.` |
| `input()` no nível do arquivo | `input() no nivel do arquivo (linha N) trava o bot: a arena entrega o estado como argumento de pensar(estado).` |
| `sys.stdin` no nível do arquivo | `sys.stdin (linha N) nao pode ser usado: a arena entrega o estado como argumento de pensar(estado).` |
| Bloco `if __name__ == "__main__":` | `remova o bloco "if __name__ == '__main__':" (linha N): a arena ja chama pensar(estado) por voce, e esse bloco rodaria antes e atrapalharia a comunicacao.` |

Aceitos: `def pensar(estado, extra=None)`, `pensar = lambda estado: ...`, `from __future__ import ...`, `input()` dentro de uma função, qualquer módulo da biblioteca padrão (exceto `os`, `subprocess` e `importlib`).

## Decisões que a organização deve confirmar

- Python: o bloco `if __name__ == "__main__":` é recusado (o arquivo da equipe roda como script junto com o executor da arena, então o bloco rodaria antes do laço da arena e poderia escrever no canal com a engine).
- Python: `import os` e `import subprocess` continuam proibidos, como no validador antigo. `importlib` e `__import__` entram na lista.
- C/C++/Java: continuam recusados só `system(`, `Runtime.getRuntime`/`ProcessBuilder`/`System.exit`, como antes (o isolamento real é do sistema operacional: usuário sem privilégio, limites do `script.sh`).
