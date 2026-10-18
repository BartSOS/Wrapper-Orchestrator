#!/bin/bash
# Validador BOCA (Python 3): mesma montagem da partida (wrappers/build_bot.sh).
# A lógica está em compile_comum.sh (modos: <fonte> <saida> e --preparar).
LINGUAGEM=py
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/compile_comum.sh"
