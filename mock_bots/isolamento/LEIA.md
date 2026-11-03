# Bots de isolamento (C-A4)

Bots Python de ataque para conferir o executor isolado da arena (`boca-arena`, `script.sh` com
`TOPCOM_BOT_UID_BASE`). **Não são bots de equipe e não entram no kit.**

| Caso | Tenta | Barrado por |
| :--- | :--- | :--- |
| `fork_bomb` | criar 256 processos | `ulimit -u` do bot (`TOPCOM_BOT_MAX_PROC`, 64) |
| `ler_private` | ler `/var/www/boca/src/private/conf.php` e qualquer arquivo sob `/var/www` | o executor não tem `/var/www` |
| `rede` | TCP em `boca-db:5432`, `127.0.0.1:80`, `8.8.8.8:53` | `network_mode: none` |
| `ler_outro_bot` | arquivos de outros bots (`/proc/<pid>/cmdline`, cwd/environ/fd, `..` até 5 níveis, `submissoes/`) | uid próprio, diretório 0700, permissões do `script.sh` |
| `matar_outro_bot` | `os.kill(pid, 0)` em todo processo que não é dele nem da engine (SIGKILL nos bots que aceitarem) | uid próprio (EPERM) |
| `ler_fila` | listar e gravar em `/fila` e `/var/www/boca/arena` | fila 0770 do uid 33 |
| `encher_disco` | gravar até 3 GB em HOME, TMPDIR e cwd (falha com 1 GB ou mais) | vigia de disco do `script.sh` (`TOPCOM_BOT_MAX_DISCO_MB`, 256) |

Cada bot ataca no tick 0, escreve no stderr `ISOLAMENTO-OK <caso>` (barrado) ou
`ISOLAMENTO-FALHOU <caso>` (conseguiu), mais uma linha `  detalhe: ...`, e depois joga parado.
`esperado.json` traz a descrição e o `sinal_de_falha` de cada caso.

- No modo isolado o `script.sh` copia o stderr de cada bot para `build/logs/<bot>.stderr` e
  repete as linhas no log da partida: `  [ISOLAMENTO] <bot>: ISOLAMENTO-OK <caso>`. Fora dele
  o stderr dos bots vai para `/dev/null`.
- `encher_disco` costuma ser encerrado pelo vigia antes de terminar: não sai linha no stderr, e o
  log mostra `[ISOLAMENTO] uid <U>: ... passou do limite`. O teste passa se nenhuma saída tem
  `ISOLAMENTO-FALHOU`, a partida termina e a réplica e o web seguem vivos.
- Eles passam pelo `checar_python` do `build_bot.sh` porque usam `sys.modules["os"]` (o wrapper
  já importou o `os`) e módulos permitidos (`socket`, `signal`): a análise estática não é barreira
  de segurança; o isolamento é.
- Rode só no modo isolado (arena ou contêiner descartável). Sem isolamento eles leem o código das
  outras equipes, matam bots e enchem o disco de quem estiver rodando.
- Num contêiner feito da imagem do web, `ler_private` falha de propósito (o `/var/www` existe e
  `doc/` é legível); na imagem do arena ele não existe.
