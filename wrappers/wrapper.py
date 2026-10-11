#!/usr/bin/env python3
# =============================================================================
# INFRAESTRUTURA DE I/O E COMUNICAÇÃO (WRAPPER OFICIAL DO BOCA)
# Este código será anexado ao final do código submetido pelo competidor.
# Ele protege contra prints acidentais e gerencia o stdin/stdout com o Orquestrador.
# =============================================================================

import sys
import json
import math
import traceback

# 1. Proteção contra prints acidentais do usuário
# Todo 'print' convencional do aluno irá para o stderr, salvando a vida do orquestrador
_stdout_original = sys.stdout
sys.stdout = sys.stderr

def enviar_comando(acao):
    # Usamos o descritor original blindado para mandar o JSON real para a Engine
    _stdout_original.write(json.dumps(acao) + '\n')
    _stdout_original.flush()

def main_loop():
    for linha in sys.stdin:
        linha = linha.strip()
        if not linha:
            continue
        
        try:
            estado_arena = json.loads(linha)
            
            # A função pensar() deve ser declarada no código do aluno
            comando_saida = pensar(estado_arena)
            
            enviar_comando(comando_saida)
            
        except Exception as e:
            print(f"[ERRO NO BOT PYTHON]: {e}", file=sys.stderr)
            traceback.print_exc(file=sys.stderr)
            sys.stderr.flush()

if __name__ == "__main__":
    main_loop()

