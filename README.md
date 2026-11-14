# Battle Orchestrator — Arena TopCom

O **Battle Orchestrator** é o motor da Arena: recebe os códigos das equipes, injeta os wrappers de I/O, compila, isola e executa os bots, roda a **Engine de física em C++20** e gera o replay `match_frames.json`. O BOCA transforma esse replay em experiência visual no pacote Unity.

O módulo é **autônomo**: roda igual na linha de comando (fora do BOCA) e dentro do contêiner `gamejam-boca-web-1`, quando um juiz ou admin clica em **Executar Engine**.

```text
códigos das equipes ──▶ wrappers + compilação ──▶ Engine C++ (até 1500 ticks) ──▶ match_frames.json ──▶ Unity (build Linux)
```

Documentos para as equipes: o enunciado do Desafio (fonte em `Boca-Game-Jam-Pet/desafio/enunciado/`) e o starter kit, que tem repositório próprio (`BartSOS/Starter-Kit-Game-Jam`, no workspace em `Starter-Kit-Game-Jam/`, ao lado deste).

---

## 1. Contrato de Entrada

- **Chamada:** `script.sh [DIR_ENTRADA]` — `submissoes/` (padrão) ou o diretório passado como 1º argumento.
- **Extensões aceitas na partida:** `.c` (C99), `.cpp` (C++20), `.java`, `.py`. O nome do arquivo sem extensão é o nome da equipe (no BOCA, o username).
- **Excluídos antes da partida** (vão para `excluidos[]` com o motivo): arquivo vazio, nome repetido (o primeiro em ordem alfabética entra), erro de compilação ou de sintaxe e excedentes do limite.
- O `build_bot.sh` é chamado via `bash`: a permissão `+x` se perde quando o BOCA extrai o `engine.zip`.
- **Capacidade:** de 2 a **20 bots** por partida (limite do evento). O limite é aplicado **antes** de compilar: os candidatos são compilados em ordem alfabética só até preencher as vagas; se algum falhar, o lote seguinte tenta os próximos. Os excedentes não chegam a ser compilados.
- **Dentro do BOCA:** o `EngineExecutionService.php` extrai o `engine.zip` numa pasta própria da partida (`private/runs/contest_N/<runId>/`), copia o código ativo de cada equipe como `submissoes/<username>.<ext>` e executa `bash ./script.sh <submissoes>`. Uma trava por contest impede duas partidas ao mesmo tempo ("Partida em andamento").

### Variáveis de ambiente

| Variável | Padrão | Efeito |
| :--- | :--- | :--- |
| `TOPCOM_SEED` | sorteada | Seed da partida (inteiro de 0 a 4294967295). Sem ela, ou com valor inválido, a engine sorteia uma e a registra no JSON. |
| `TOPCOM_MAX_BOTS` | 20 | Limite de bots na partida (`script.sh` e engine). O `--benchmark` usa para medir acima de 20. |
| `TOPCOM_COMPILE_JOBS` | automático | Compilações simultâneas. Automático: `min(nproc, memória disponível / 500 MB)`, mínimo 1 (em contêiner com limite de memória, vale o que sobra no cgroup). |
| `TOPCOM_TIMEOUT_PARTIDA` | calculado | Orçamento de tempo real da partida, em segundos (seção 3). Sobrepõe o cálculo; só para ensaios. |
| `TOPCOM_FASE` | — | Caminho do JSON de uma fase de treino (`fases/<id>.json`). Liga o **modo fase** (seção 1.2). |
| `TOPCOM_FASE_SEED` | sorteada | Seed do modo fase (0 a 4294967295): sorteia as faixas `[min, max]` do cenário e os hazards. |
| `TOPCOM_BOT_UID_BASE` e afins | — | Modo isolado do executor da arena (seção 1.3). |

O `EngineExecutionService.php` passa um ambiente fixo ao `proc_open`, que já inclui `TOPCOM_SEED` (sorteada pelo BOCA para cada partida e gravada no resultado), `TOPCOM_MAX_BOTS` (20, item 7.8) e `TOPCOM_COMPILE_JOBS` (se definido no servidor). Para reproduzir uma partida do BOCA, use a `seed` do `match_frames.json` com `gerenciar_engine.py --testar --seed X`.

---

## 1.1 Contrato de Saída (C1)

`match_frames.json` (com os frames) e `replays/partida_<partida_id>.json` (o mesmo resumo, sem `config` nem `frames`):

```json
{
  "config": {"arena_width": 1000, "arena_height": 1000, "max_ticks": 1500},
  "partida_id": "1791937982823285656",
  "data_hora": "13/10/2026 21:33:02",
  "status": "concluido",
  "seed": 12345,
  "vencedor": "base_radar",
  "turnos": 229,
  "duracao_ms": 22900,
  "bots": [
    {"id": 1, "nome": "Solution", "lang": "Java", "posicao": 6, "vivo": false, "kills": 0, "hp": 0.0, "vencedor": false, "tick_eliminacao": 30},
    {"id": 5, "nome": "base_radar", "lang": "C", "posicao": 1, "vivo": true, "kills": 3, "hp": 50.0, "vencedor": true, "tick_eliminacao": null}
  ],
  "excluidos": [{"nome": "bot_erro_c", "motivo": "Erro de compilacao modular (bot_erro_c.c:5:5: error: ...)"}],
  "frames": [{"tick": 0, "tanks": [{"id": 1, "x": 0, "y": 0, "angle": 0, "turret": 0, "hp": 100, "energy": 100, "shield": false, "kills": 0, "alive": true}], "projectiles": [], "hazards": [], "items": [], "events": []}]
}
```

- `bots[]` vem em ordem de `id` (`bots[id-1]`); `id` é o slot da arena (ordem alfabética dos nomes) e é o mesmo `id` dos `tanks` dos frames.
- `lang`: `"C"`, `"C++20"`, `"Java"` ou `"Python 3"`.
- `posicao` (1..N) segue o regulamento 8.4.1: vivo no fim > mais abates > mais vida > eliminado mais tarde > menor `id`. O último critério do regulamento é a ordem alfabética do nome de usuário da equipe no BOCA, que é exatamente o `id` do slot. A vida de um tanque destruído conta como 0.
- `vencedor` é `true` só para `posicao` 1; o `vencedor` do topo é o `nome` dele.
- `tick_eliminacao`: tick em que o tanque foi destruído, desconectou ou estourou o timeout (`null` se terminou vivo). Campo extra, além do C1.
- `excluidos[]`: bots que não entraram, na ordem em que foram descartados.
- Se a engine não gerar o `match_frames.json`, o `script.sh` termina com código diferente de 0.

**Reprodutibilidade.** A seed controla hazards (inclusive o ângulo inicial do laser) e itens; as posições iniciais são fixas. O sorteio do ângulo do laser (commit 78ca8d6) mudou a sequência do rng, e os temporizadores passaram a durar exatamente `T / TICK_DT` ticks (638c72c: recarga 10, aviso do laser 25, queima 50, aviso da artilharia 30). Por isso, seeds anteriores a esses commits não reproduzem as partidas antigas, mas os replays gravados continuam válidos. Com a mesma seed, os mesmos bots e bots determinísticos, a partida se repete (frames idênticos, verificado com 14 e com 12 bots mistos). Fontes de não determinismo que restam:
- **tempo real:** um bot que demora mais de 50 ms num tick perde aquele tick, e isso depende da carga da máquina;
- **o próprio bot:** `random` sem seed, relógio, `srand(time(NULL))`, ordem de `HashMap` em Java etc. Para Python, o `script.sh` fixa `PYTHONHASHSEED=0`;
- **compilador/biblioteca:** as distribuições de `<random>` são as da libstdc++; outra toolchain pode gerar outra sequência para a mesma seed.

---

## 1.2 Modo fase (fases de treino)

As fases de treino (aba **Treino** do BOCA) usam a engine real num cenário controlado, com 1 bot. Sem `TOPCOM_FASE`, nada disto se aplica e a engine é exatamente a do torneio: com a mesma `TOPCOM_SEED`, os replays são idênticos aos da versão sem o modo fase.

```bash
TOPCOM_FASE=/caminho/fases/01_andar.json TOPCOM_FASE_SEED=123 bash script.sh <pasta>
```

- **Bots:** se `<pasta>/submissoes/` existir, o bot vem dela e o `resultado_fase.json` é gravado em `<pasta>/`. Senão `<pasta>` é a própria pasta dos bots, e o resultado fica na raiz do orquestrador, ao lado do `match_frames.json`. Só 1 bot joga (o primeiro em ordem alfabética); os outros vão para `excluidos[]`.
- **Engine:** o `script.sh` chama `build/engine --fase <JSON absoluto> --fase-seed <N> "<bot>"`. A seed da fase também é a seed da partida (vai para `seed` e `fase.seed`). Cenário inválido: a engine sai com código 2 e uma mensagem `[ENGINE ERRO] Cenario da fase invalido (...): <campo>` sem iniciar o bot, e o `script.sh` termina sem `match_frames.json`.
- **Fim da partida:** em `cenario.max_ticks` ou quando o jogador é destruído. Alvos não encerram a partida.
- **Avaliação:** depois da consolidação, `build/avaliar_fase <fase.json> match_frames.json > resultado_fase.json` (fonte `engine/avaliar_fase.cpp`, da frente das fases). Ele compila junto com a engine, com as mesmas flags e a mesma assinatura em `build/`. Se o fonte não existir ou não compilar, uma linha de aviso e a fase roda sem avaliação.

### Bloco `cenario` (lido pela engine)

Número = valor fixo; `[min, max]` = sorteado com a seed (inteiro nos ticks); chave ausente = padrão da arena.

| Chave | Padrão | Significado |
| :--- | :--- | :--- |
| `max_ticks` | 1500 | Duração máxima (1 a 1500). |
| `jogador` | `{x: 860, y: 500, angulo: 180, hp: 100, energia: 100}` | Início do tanque do bot (o padrão é o nascimento de 1 bot no torneio). |
| `pontos` | `[]` | Marcadores `{id, x, y, raio = 25}`, no máximo 16, `id` com até 15 caracteres. Não colidem nem aparecem em `enemies`; chegam ao bot em `objetivo`. |
| `alvos` | `[]` | Tanques inertes da engine `{id, x, y, hp = 100, angulo = 0}`, no máximo 19, com ids 2, 3... na ordem do arquivo. Aparecem em `enemies` como tanques comuns, não agem e podem ser destruídos. Ficam fora do ranking. |
| `artilharia` | `"aleatoria"` | `"aleatoria"` (como no torneio), `false` ou `[]` (nenhuma) ou a lista programada `{tick, x, y, raio = 80}`. Com `no_jogador: true` (sem x/y), a bomba cai na posição atual do jogador no tick, deslocada por `desvio` (número ou `[min, max]`, padrão 0) numa direção sorteada, limitada à arena. Aviso `ARTILLERY_WARN` no tick e impacto 3 s depois. |
| `laser` | `true` | `false` desliga o laser. |
| `itens` | `"aleatorios"` | `false` (nenhum), `"aleatorios"` (como no torneio) ou a lista fixa `{tipo: "HEAL" \| "ENERGY", x, y}`, posta no tick 0. |
| `zona` | `true` | `false` desliga a zona de radiação, que fecha depois do tick 600. |

Com artilharia programada, a aleatória fica desligada. Com só artilharia ou só laser aleatórios, o sorteio periódico sempre escolhe o que está ligado.

### Saídas do modo fase

- **Estado do bot:** ganha `"objetivo": {"pontos": [{"id": "B", "x": 800.0, "y": 512.3, "raio": 25.0}]}`, na ordem do arquivo. A equipe vê a lista `pontos` (`Ponto`: `id`, `x`, `y`, `raio`): `estado.pontos` em Python, `e.pontos` em C++ e Java, `e->pontos[i]` com `i < e->n_pontos` em C (`MAX_PONTOS` 16). No torneio a chave não existe e a lista vem vazia.
- **`match_frames.json`:** o bloco `fase` é a primeira chave, com o cenário sorteado: `{id, seed, max_ticks, jogador, pontos[{id, ordem, x, y, raio}], alvos[{id, tank_id, x, y, hp, angulo}], artilharia, laser, itens, zona}`. Na artilharia programada, cada item traz `{tick, x, y, raio, criada}` (e `no_jogador: true`, com x/y efetivos ou `null` se não chegou a cair). Os alvos aparecem em `frames[].tanks` como qualquer tanque e, no cabeçalho, em `tanques[]` com `"controle": "alvo"` (rótulo `A1`...); `bots[]` só tem o jogador. O que a Unity desenha: `Boca-Game-Jam-Pet/desafio/VISUALIZADOR_WEB.md`, seção "Bloco fase".
- **Teste:** `python3 mock_bots/treino_engine/testar_modo_fase.py` (WSL) roda os cenários de `mock_bots/treino_engine/` com bots nas 4 linguagens numa cópia em `/tmp/frente_engine/modo_fase` e confere tudo isso.

## 1.3 Isolamento (modo arena)

No evento, as partidas rodam no executor `boca-arena` (sem rede, sem `private/`), que chama o `script.sh` como root com `TOPCOM_BOT_UID_BASE`. Sem essa variável nada muda: uid único, `/tmp` comum, stderr dos bots descartado, mesmo `match_frames.json` para a mesma seed.

| Variável | Padrão | Efeito (só com `TOPCOM_BOT_UID_BASE`) |
| :--- | :--- | :--- |
| `TOPCOM_BOT_UID_BASE` | — | Liga o modo. O bot `i` (0-based na ordem dos slots, NPCs incluídos) usa uid = gid = `BASE + i`. Exige root e `setpriv`; valor inválido aborta a partida. |
| `TOPCOM_BOT_MAX_PROC` | 64 | `ulimit -u` de cada bot na partida (a JVM cabe). A compilação usa 256. |
| `TOPCOM_BOT_MAX_DISCO_MB` | 256 | Disco por bot (diretório próprio + `/tmp`, `/var/tmp`, `/dev/shm`...); também é o `ulimit -f`. |
| `TOPCOM_BOT_MAX_ARQUIVOS` | 2000 | Arquivos por bot. |

O que muda:
- **Compilação e execução com o uid do bot:** `setpriv --reuid=U --regid=U --clear-groups --no-new-privs --inh-caps=-all --bounding-set=-all` (sem `CAP_SETPCAP`, sai o `--bounding-set`, com aviso). A engine, o `avaliar_fase` e o resto do script continuam como root.
- **Diretório próprio:** `build/bots/t<N>/` (dono do bot, 0700) com a cópia do fonte, o binário `bot`, `home/` e `tmp/` (`HOME` e `TMPDIR`), e `umask 077`. O cwd do bot é esse diretório. O `build_bot.sh` grava os temporários no `TMPDIR`.
- **Permissões:** os bots só leem `wrappers/`, `libs/`, `engine/nlohmann/` e `build/wrappers/`. Submissões, fontes da engine, fases, logs e os diretórios dos outros bots ficam fechados (0700/0711).
- **Limites:** `ulimit -u`, `-v` (o de sempre), `-f` e o `timeout` de sempre. Um vigia (a cada 0,25 s) encerra o bot que passar do disco ou de arquivos e apaga o que ele gravou (`[ISOLAMENTO] uid U: ... passou do limite` no log).
- **Limpeza por uid:** no início, depois da compilação e no fim, todo processo dos uids dos bots é encerrado (`kill -KILL -1` como o próprio uid, contra fork bomb e processos com `setsid`) e seus arquivos em `/tmp`, `/var/tmp`, `/dev/shm`... são apagados. `build/bots/` é apagado no fim.
- **stderr dos bots:** vai para `build/logs/<bot>.stderr` (primeiros 64 KB); linhas `ISOLAMENTO-OK|FALHOU <caso>` aparecem no log como `[ISOLAMENTO] <bot>: ...`.
- **Teste:** bots de ataque em `mock_bots/isolamento/` (ver `LEIA.md`); o BOCA os roda com a skill `boca-arena` (`--testar-isolamento`).

---

## 2. O que o Competidor Escreve

Apenas a função `pensar`, chamada a cada frame (= tick da engine, 0,1 s). O competidor **não** escreve `main()`, não lê `stdin` e não faz parsing de JSON: os wrappers fazem isso e entregam o estado e recebem a ação **em português**, por um arquivo de apoio por linguagem. A engine, o protocolo e o replay continuam em inglês; a tradução é toda dos wrappers.

| Linguagem | Arquivo da equipe | Assinatura | Arquivo de apoio (em `wrappers/`) |
| :--- | :--- | :--- | :--- |
| C | `bot.c` | `Acao pensar(const Estado *e)` | `arena.h` (injetado com `-include`) |
| C++ | `bot.cpp` | `Acao pensar(const Estado& e)` | `arena.hpp` (incluído pelo `cpp_header.hpp`, que usa a nlohmann) |
| Java | `MeuBot.java` | `public static Acao pensar(Estado e)` em `public class MeuBot extends Arena` | `Arena.java` (+ `gson.jar`); os nomes `Arena` e `JavaRunner` são reservados |
| Python | `bot.py` | `def pensar(estado):` com `from arena import *` | `arena.py` |

O ponto de partida da equipe é o starter kit, no repositório `Starter-Kit-Game-Jam` (`c/`, `cpp/`, `java/`, `python/`, cada pasta com o bot e o arquivo de apoio, mais o `README.md` e o `verificar.py`). No fim de cada bot há uma seção `AJUDANTES (opcional: use, mude ou apague)` com `distancia`, `angulo_para` e `diferenca_angulo` (mesmos nomes nas 4 linguagens), descrita no `AJUDANTES.md` do kit (material interno, fora do zip). O zip é gerado pela skill `boca-desafio` (`desafio.py --starter`). Os arquivos de apoio, o `json.hpp` e o `gson.jar` do kit são cópias dos daqui: `bash empacotar.sh --conferir-kit` / `--sincronizar-kit`.

Só existe a API em português: o `pensar` recebe o `Estado` e devolve uma `Acao`, nas 4 linguagens. Os nomes em inglês do protocolo não são aceitos no código da equipe.

O validador do BOCA monta o bot com o mesmo `wrappers/build_bot.sh` da partida, então só dá YES ao código que compila exatamente como na partida. `main()` próprio (C/C++), `package` em Java, import relativo, bloco `if __name__ == "__main__":` ou biblioteca fora da padrão em Python dão Compilation Error. O texto exato de cada mensagem (`ERRO DE VALIDACAO: ...` / `ERRO DE COMPILACAO: ...`) está no `ERROS_VALIDADOR.md` do kit (contrato C-K3, fora do zip).

### Entrada — `estado`

O que a equipe vê (nomes iguais nas 4 linguagens; em C, as listas vêm com `n_inimigos`, `n_perigos`, `n_itens` e `n_pontos`):

| Campo | Conteúdo |
| :--- | :--- |
| `frame` | número do frame (começa em 0) |
| `eu` | `Tanque`: `id`, `x`, `y`, `vel_x`, `vel_y` (por frame), `angulo_chassi`, `angulo_torre`, `vida`, `energia`, `frames_para_atirar`, `escudo_ativo` |
| `inimigos[]` | `Inimigo`: `id`, `x`, `y`, `vel_x`, `vel_y` |
| `perigos[]` | `Perigo`: `id`, `tipo` (`PERIGO_ARTILHARIA`/`PERIGO_LASER`), `x`, `y`, `raio`, `frames_ate_impacto`, `angulo`, `frames_restantes` |
| `itens[]` | `Item`: `id`, `tipo` (`ITEM_CURA`/`ITEM_ENERGIA`), `x`, `y` |
| `pontos[]` | `Ponto`: `id`, `x`, `y`, `raio` (só nas fases de treino; no torneio vem vazia) |

As constantes são textos em Python (`"ARTILHARIA"`, `"LASER"`, `"CURA"`, `"ENERGIA"`) e números nas outras linguagens. O wrapper converte `vx`/`vy` (por segundo) e `cooldown`/`timer`/`duration` (segundos) para unidades por frame e frames. Listas maiores que `MAX_INIMIGOS`/`MAX_PERIGOS`/`MAX_ITENS`/`MAX_PONTOS` (C) são truncadas.

O JSON do protocolo (engine → wrapper), que o wrapper traduz para o `Estado`:

```json
{
  "tick": 123,
  "self":    {"id", "x", "y", "vx", "vy", "angle", "turret_angle", "hp", "energy", "cooldown", "shield"},
  "enemies": [{"id", "x", "y", "vx", "vy"}],
  "hazards": [{"id", "type": "ARTILLERY|LASER", "x", "y", "radius", "timer", "angle", "duration"}],
  "items":   [{"id", "type": "HEAL|ENERGY", "x", "y"}],
  "objetivo": {"pontos": [{"id", "x", "y", "raio"}]}
}
```

`objetivo` só existe nas fases de treino (seção 1.2); no torneio a chave não vem e `pontos` fica vazia.

### Saída — ação

| Campo (equipe) | Valores | Efeito | Vira no protocolo |
| :--- | :--- | :--- | :--- |
| `mover` | `FRENTE`, `PARADO`, `RE` ou de -1 a 1 | anda (5 unidades por frame no máximo) | `throttle` |
| `direcao` | graus absolutos; ausente (`None`/`NAN`) = mantém | o chassi gira até 9° por frame, pelo menor lado, até apontar para lá | `turn` = diferença / 9, limitada a [-1, 1] |
| `angulo_torre` | graus absolutos; ausente = mantém | a torre aponta na hora | `turret_angle` |
| `atirar` | 1/0 ou true/false (Java: `boolean`) | dispara, se puder | `shoot` |
| `ligar_escudo` | 1/0 ou true/false (Java: `boolean`) | escudo neste frame | `shield` |

Padrões: Python `Acao(mover=PARADO, direcao=None, angulo_torre=None, atirar=False, ligar_escudo=False)`; em C++ e Java os ângulos começam em NaN (= mantém); em C o modelo preenche os 5 campos. Valor que não é número é ignorado com `[AVISO]`. Em Python, atribuir um campo que não existe na `Acao` dá `AttributeError` (com o nome novo, se for um antigo).

### Modelos por linguagem

**C (C99)**: o `wrappers/arena.h` é injetado automaticamente (structs `Estado`, `Tanque`, `Inimigo`, `Perigo`, `Item`, `Ponto` e `Acao`, constantes `FRENTE`/`PARADO`/`RE`, `PERIGO_*`, `ITEM_*`). O código do aluno é compilado com `gcc -std=c99` e ligado ao wrapper interno `wrappers/c_wrapper.cpp` (C++20), que converte o JSON nas structs e a `Acao` no comando da engine. `printf()` do aluno vai para o `stderr` e não corrompe a comunicação.
```c
Acao pensar(const Estado *e) {
    Acao a;
    a.mover = FRENTE;
    a.direcao = e->eu.angulo_chassi;
    a.angulo_torre = e->eu.angulo_torre;
    a.atirar = 0;
    a.ligar_escudo = 0;
    if (e->n_inimigos > 0) {
        a.angulo_torre = angulo_para(e->eu.x, e->eu.y, e->inimigos[0].x, e->inimigos[0].y);
        a.atirar = e->eu.frames_para_atirar == 0;
    }
    return a;
}
```

**Python 3**: a sintaxe é conferida na compilação; o `arena.py` e o wrapper vão junto. `print()` do aluno é desviado para `stderr`.
```python
from arena import *

def pensar(estado):
    acao = Acao(mover=FRENTE)
    if estado.inimigos:
        alvo = estado.inimigos[0]
        acao.angulo_torre = angulo_para(estado.eu.x, estado.eu.y, alvo.x, alvo.y)
        acao.atirar = estado.eu.frames_para_atirar == 0
    return acao
```

**C++20**: o código é compilado com `-include cpp_header.hpp` (pré-compilado; inclui a nlohmann e o `arena.hpp`) e ligado ao `cpp_footer.o` (o `main`, também pré-compilado). `std::cout`/`printf` do aluno vão para o `stderr`. Se a ligação falhar (ex.: `pensar` declarada `inline`), o bot é recompilado como antes, numa única unidade com cabeçalho + código + rodapé.
```cpp
Acao pensar(const Estado& e) {
    Acao a;
    a.mover = FRENTE;
    if (!e.inimigos.empty()) {
        a.angulo_torre = angulo_para(e.eu.x, e.eu.y, e.inimigos[0].x, e.inimigos[0].y);
        a.atirar = e.eu.frames_para_atirar == 0;
    }
    return a;
}
```

**Java**: o nome da classe é detectado automaticamente; o bot é compilado com o `Arena.java` e o runner reflexivo e empacotado em um JAR com Gson 2.10.1.
```java
public class MeuBot extends Arena {
    public static Acao pensar(Estado e) {
        Acao a = new Acao();
        a.mover = FRENTE;
        if (e.inimigos.length > 0) {
            a.angulo_torre = angulo_para(e.eu.x, e.eu.y, e.inimigos[0].x, e.inimigos[0].y);
            a.atirar = e.eu.frames_para_atirar == 0;
        }
        return a;
    }
}
```

Nos exemplos, `angulo_para` é o ajudante do kit (fica no fim do arquivo da equipe, não no apoio).

---

## 3. Regras da Arena (`engine/main.cpp`)

| Parâmetro | Valor |
| :--- | :--- |
| Arena | 1000 × 1000; tanques em círculo de raio 360 até 20 bots (igual a antes). Acima disso o raio cresce para manter 50 entre centros (até 480); acima de 60 bots, anéis concêntricos |
| Tick | 0,1 s (10 Hz) · máximo **1500 ticks** (150 s simulados) |
| Tanque | raio 20 · velocidade máx. 50/s · giro 90°/s · **100 HP** · **100 de energia** (regenera 5/s) |
| Tiro | custa 20 de energia · recarga de 1 s · projétil com velocidade 300 e **25 de dano** |
| Escudo | consome 50 de energia/s · **só ativa com mais de 50% de energia** · bloqueia projéteis, reduz artilharia a 20% e laser a 10% |
| Itens | `HEAL` (+30 HP) · `ENERGY` (+50 de energia) · um a cada 150 ticks, se houver menos de 4 no mapa |
| Perigos | ticks 0, 200 e 400 (máx. 3 ativos); depois do tick 400, a cada 90 ticks (máx. 5 ativos); tipo sorteado |
| Artilharia | aviso de 3 s · raio 80 (104 se surgir depois do tick 400) · 40 de dano a quem estiver a até raio + 20 |
| Laser | semirreta a partir do centro (500, 500) · ângulo inicial sorteado pela seed · gira 30°/s · aviso de 2,5 s · queima 5 s · 30 de dano/s a até 26 do feixe |
| Zona | a partir do **tick 600**, o raio seguro encolhe de 550 até 80. Fora dele, 2,5 HP a cada 10 ticks |
| Timeout | 50 ms por tick para responder; 20 ticks seguidos sem resposta → tanque desativado (`[TIMEOUT]`). No tick 0 o prazo é de até 5 s, para os processos terminarem de iniciar (a JVM leva centenas de ms) |
| Processo encerrado | o bot que termina ou fecha o pipe é destruído (`[DESCONEXAO]`). **O bot precisa ficar vivo a partida inteira** |
| Fim | resta 1 sobrevivente ou o limite de ticks |
| Classificação (8.4.1) | 1º vivo · 2º abates · 3º HP restante · 4º eliminado mais tarde · 5º menor id (ordem do slot) |

**Protocolo com os bots** (uma linha JSON por mensagem): cada bot tem no máximo um estado pendente. A resposta de um tick só vale para aquele tick: se chegar atrasada, é descartada, e o estado seguinte só é enviado depois dela (um bot travado não enche o pipe nem trava a engine). Linhas que não são objeto JSON são ignoradas, e o `stderr` dos bots vai para `/dev/null`. Cada bot roda num grupo de processos próprio, encerrado inteiro quando o tanque é eliminado e no fim da partida.

**Tempo da partida** (`script.sh`): não há mais limite fixo de 30 s por bot, que matava todos os bots de partidas longas. Vale o prazo de 50 ms por tick da engine, e o script calcula um **orçamento** para a partida inteira: `PARTIDA_INICIO_MS + MAX_TICKS × nº de bots × RESPOSTA_MS + 30 s` de folga. Um **vigia global** (`timeout`) encerra a engine se o orçamento estourar. Cada bot recebe o orçamento + 15 s, só como rede de segurança. `TOPCOM_TIMEOUT_PARTIDA` sobrepõe o cálculo em ensaios.

Isolamento de cada bot: `ulimit -v` de ~2 GB. Bots Java rodam com `-Xmx512m -XX:CompressedClassSpaceSize=64m -XX:+UseSerialGC`: sem isso a JVM reserva mais memória virtual que o `ulimit` e nem inicia. Os validadores do BOCA bloqueiam, já no envio, `os`, `subprocess`, `eval` e `exec` (Python), `system(` (C/C++) e `Runtime`, `ProcessBuilder` e `System.exit` (Java).

---

## 4. Etapas do `script.sh`

1. Verifica as dependências (`g++`, `python3`, `timeout`).
2. Verifica as bibliotecas locais (`libs/gson-2.10.1.jar`, `engine/nlohmann/json.hpp`).
3. Compila a Engine e, em paralelo, os wrappers pré-compilados de C (`c_wrapper.o`) e C++ (`cpp_header.hpp.gch` + `cpp_footer.o`) das linguagens presentes. Engine e wrappers são reaproveitados enquanto fonte, compilador e flags não mudam (assinatura SHA-256 em `build/`); no BOCA, o diretório de execução persiste entre partidas, então só a primeira paga essa etapa.
4. Faz o **inventário pré-compilação** (equipe, arquivo, tamanho, linguagem), compila os bots em paralelo (`TOPCOM_COMPILE_JOBS`) com `wrappers/build_bot.sh` e faz o **inventário pós-compilação** (bots aprovados com slot e comando; excluídos com motivo). O log de cada bot fica em `build/logs/<equipe>.log` e o resultado é impresso na ordem dos nomes; a ordem dos slots é sempre a alfabética. A falha de um bot não aborta a partida.
5. Executa a partida e transmite os eventos (`[ABATE]`, `[CURA]`, `[BOMBA]`, `[ZONA]`, `[LASER]`, `[TIMEOUT]`, `[DESCONEXAO]`, `[FIM]`).
6. Consolida o `match_frames.json` (contrato C1 acima) com os nomes reais das equipes, mapeados pelo `id` do slot.
7. Só no modo fase (seção 1.2): roda o `build/avaliar_fase` e grava o `resultado_fase.json`.

---

## 5. Estrutura

```text
battle-orchestrator/
├── script.sh              # Ponto de entrada da Arena
├── engine/                # main.cpp (física/juiz) + nlohmann/json.hpp
├── wrappers/              # build_bot.sh, wrapper.py, wrapper.java, cpp_header.hpp, cpp_footer.cpp, wrapper.cpp, arena.h (contrato C), c_wrapper.cpp (I/O dos bots C)
├── libs/                  # gson-2.10.1.jar
├── empacotar.sh           # Fonte única do engine.zip e dos pacotes de linguagem; copia os arquivos do kit (KIT_COPIAS)
├── boca-plugin/           # Validadores de linguagem para o BOCA
│   └── linguagens/{c,cpp,java,python3}/compile.sh
├── mock_bots/
│   ├── bots-base/         # Conjunto canônico de bots de exemplo (testes, benchmark e checkpoint inicial)
│   ├── intermediarios/    # Bots de teste válidos e com erro (python3, c++20, java, c)
│   ├── treino_engine/     # Cenários e bots de teste do modo fase + testar_modo_fase.py
│   ├── isolamento/        # Bots de ataque do teste do executor isolado (LEIA.md, esperado.json)
│   └── malicious_bots/    # fork_bomb, infinite_memory, file_system_hacker, system_call.c
├── submissoes/            # Entrada da partida (conteúdo ignorado pelo git)
└── build/, replays/       # Saídas geradas (ignoradas pelo git): build/engine, build/wrappers/, build/bots/, build/logs/
```

---

## 6. Como Usar

### Partida local (fora do BOCA)
```bash
cp mock_bots/intermediarios/python3/valido/bot_bfs.py submissoes/
cp mock_bots/intermediarios/c++20/valido/bot_dijkstra.cpp submissoes/
./script.sh                    # ou ./script.sh /caminho/para/pasta
```
Resultado: log no terminal e `match_frames.json` na raiz.

### Testar no runtime do evento (recomendado)
```bash
python3 ../.claude/skills/boca-gerenciar-engine/scripts/gerenciar_engine.py --testar             # partida com os bots-base
python3 ../.claude/skills/boca-gerenciar-engine/scripts/gerenciar_engine.py --benchmark 4,8,20   # capacidade/tempo
```
Roda o `script.sh` dos fontes atuais dentro do contêiner `gamejam-boca-web-1`.

**Medições no contêiner web (`--benchmark 20,40,60`, 14/10/2026, VM de 12 núcleos e 3,7 GB, rotação dos 6 bots-base, 4 jobs de compilação escolhidos pela memória):**

| Bots | Compilação antes | Compilação agora | Engine + wrappers | Partida | Total antes | Total agora | Pico de memória |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 20 | 49,6 s | 3,7 s | 11,2 s | 1,3 s | 59,5 s | 16,7 s | ~1,0 GB |
| 40 | 98,7 s | 7,3 s | 11,1 s | 1,9 s | 108,9 s | 21,1 s | ~1,0 GB |
| 60 | 153,3 s | 10,5 s | 10,4 s | 3,8 s | 163,8 s | 25,7 s | ~1,0 GB |

- "Engine + wrappers" é a etapa 3 numa pasta nova. Com a engine e os wrappers já compilados (próximas partidas no mesmo diretório, como no BOCA), cai para ~0,1 s: no WSL, 60 bots levaram 18,7 s no total;
- por bot: C ~0,4 s (era ~5 s), C++ ~2 s (era ~4,3 s; o cabeçalho pré-compilado tem 100 MB), Java ~1,5 s, Python ~0,1 s;
- com `TOPCOM_COMPILE_JOBS=1` (sequencial), 60 bots compilam em ~42 s no WSL;
- nenhuma desconexão ou timeout até 60 bots; o JSON cresce com frames × bots e comprime ~20×;
- acima do limite, os excedentes são excluídos **sem** compilar.

### Compilar um único bot
```bash
bash wrappers/build_bot.sh submissoes/bot.py build/bots/bot   # prepara os wrappers C/C++ se faltarem
bash wrappers/build_bot.sh --preparar c cpp                    # só pré-compila os wrappers (build/wrappers/)
```

### Gerar o `engine.zip` para o BOCA
```bash
python3 ../.claude/skills/boca-gerenciar-engine/scripts/gerenciar_engine.py --empacotar
```
O pacote leva `script.sh`, `engine/`, `wrappers/` e `libs/` e é gravado em `Boca-Game-Jam-Pet/boca-master/src/private/engine/contest_1/engine.zip`. Depois, envie-o pela aba **Desafio** ou sincronize com `sync_and_test.py`.

### Gerar os validadores de linguagem (`.tgz`) para o BOCA
```bash
python3 ../.claude/skills/boca-gerenciar-engine/scripts/gerenciar_engine.py --empacotar-linguagens
```
Gera os pacotes com o `empacotar.sh` e grava em `Boca-Game-Jam-Pet/boca-master/src/private/languages/` (o `.tgz` e a pasta extraída de cada linguagem). Depois, rode `sync_and_test.py`. Na imagem, o build faz o mesmo (`empacotar.sh --saida`).

---

## 7. Visualizador Unity

As builds oficiais ficam no repositório `BartSOS/Build-Unity-Game-Jam`, para Linux e Windows.

- **Competição: Linux.** É a build que vai para as equipes (as máquinas do laboratório são Linux): pasta `Unity-Build-Linux/`, executável `Unity-Build-Linux.x86_64`, dados em `Unity-Build-Linux_Data/`. A build Windows (`GameJam.exe`) é só para os testes da organização.
- O pacote é um `.zip` **com a pasta raiz `GameJam_Arena/`**, gerado por `gerenciar_engine.py --empacotar-visualizer --plataforma linux|windows`, e enviado pela aba **Desafio → Visualizador de Partida**.
- A cada download, o BOCA copia o pacote, grava o replay da partida em `StreamingAssets/match_frames.json` dentro da pasta de dados da build e transmite o `.zip`, sem guardar cópias por partida. **Sem a pasta raiz `GameJam_Arena/`, o replay é gravado num caminho que o jogo não lê.**
- Para assistir no Linux: `unzip <arquivo>.zip && cd GameJam_Arena && chmod +x Unity-Build-Linux.x86_64 && ./Unity-Build-Linux.x86_64`.
