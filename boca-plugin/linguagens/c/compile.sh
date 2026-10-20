#!/bin/bash
# Validador BOCA (C99): mesma montagem da partida (wrappers/build_bot.sh).
# A lógica está em compile_comum.sh (modos: <fonte> <saida> e --preparar).
LINGUAGEM=c
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/compile_comum.sh"
