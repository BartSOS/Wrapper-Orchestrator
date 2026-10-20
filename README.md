# Battle Orchestrator — Arena TopCom

O **Battle Orchestrator** é o motor da Arena: recebe os códigos das equipes, injeta os wrappers de I/O, compila, isola e executa os bots, roda a **Engine de física em C++20** e gera o replay `match_frames.json`. O BOCA transforma esse replay em experiência visual no pacote Unity.

O módulo é **autônomo**: roda igual na linha de comando (fora do BOCA) e dentro do contêiner `gamejam-boca-web-1`, quando um juiz ou admin clica em **Executar Engine**.

```text
códigos das equipes ──▶ wrappers + compilação ──▶ Engine C++ (até 1500 ticks) ──▶ match_frames.json ──▶ Unity (build Linux)
```

Documentos para as equipes: o enunciado do Desafio (fonte em `Boca-Game-Jam-Pet/desafio/enunciado/`) e o starter kit (`starter-kit/`, com o `README.md` que vai no zip).

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

## 2. O que o Competidor Escreve

Apenas a função `pensar(estado)`, chamada a cada tick. O competidor **não** escreve `main()`, não lê `stdin` e não faz parsing de JSON: os wrappers fazem isso.

O ponto de partida da equipe é o `starter-kit/` (`c/bot.c` + `arena.h`, `cpp/bot.cpp` + `nlohmann/json.hpp`, `java/MeuBot.java` + `gson.jar`, `python/bot.py` e o `README.md`). Cada arquivo traz uma seção de **ajudantes** editável (acessores do estado, montador da ação e matemática básica), descrita em `starter-kit/AJUDANTES.md` (material interno, fora do zip). O zip é gerado pela skill `boca-desafio` (`desafio.py --starter`).

O validador do BOCA monta o bot com o mesmo `wrappers/build_bot.sh` da partida, então só dá YES ao código que compila exatamente como na partida. `main()` próprio (C/C++), `package` em Java, import relativo, bloco `if __name__ == "__main__":` ou biblioteca fora da padrão em Python dão Compilation Error. O texto exato de cada mensagem (`ERRO DE VALIDACAO: ...` / `ERRO DE COMPILACAO: ...`) está em `starter-kit/ERROS_VALIDADOR.md` (contrato C-K3, fora do zip).

### Entrada — `estado`

```json
{
  "tick": 123,
  "self":    {"id", "x", "y", "vx", "vy", "angle", "turret_angle", "hp", "energy", "cooldown", "shield"},
  "enemies": [{"id", "x", "y", "vx", "vy"}],
  "hazards": [{"id", "type": "ARTILLERY|LASER", "x", "y", "radius", "timer", "angle", "duration"}],
  "items":   [{"id", "type": "HEAL|ENERGY", "x", "y"}]
}
```

### Saída — ação

| Campo | Tipo | Faixa | Efeito |
| :--- | :--- | :--- | :--- |
| `throttle` | float | -1.0 a 1.0 | Aceleração (ré / frente) |
| `turn` | float | -1.0 a 1.0 | Giro do chassi |
| `turret_angle` | float | 0 a 360 (graus) | Direção da torre |
| `shoot` | bool | — | Disparar |
| `shield` | bool | — | Escudo |

Campos ausentes assumem 0 ou `false`; a torre mantém o ângulo atual.

### Modelos por linguagem

**C (C99)**: o cabeçalho `wrappers/arena.h` é injetado automaticamente e define as structs `Estado`, `Tanque`, `Inimigo`, `Hazard`, `Item` e `Acao`. As funções de matemática (`distancia()`, `angulo_para()`, `normalizar_angulo()` e `diferenca_angulo()`) ficam no `bot.c` do kit, à vista da equipe. O código do aluno é compilado com `gcc -std=c99` e ligado ao wrapper interno `wrappers/c_wrapper.cpp` (C++20), que converte o JSON nas structs. `printf()` do aluno vai para o `stderr` e não corrompe a comunicação. Listas maiores que `MAX_INIMIGOS`/`MAX_HAZARDS`/`MAX_ITENS` são truncadas.
```c
Acao pensar(const Estado *e) {
    Acao a = {0};
    a.throttle = 1.0;
    if (e->n_inimigos > 0) {
        const Inimigo *alvo = &e->inimigos[0];
        a.turret_angle = angulo_para(e->self.x, e->self.y, alvo->x, alvo->y);
        a.shoot = e->self.cooldown <= 0;
    }
    return a;
}
```

**Python 3**: a sintaxe é conferida na compilação e o wrapper é anexado ao final do código. `print()` do aluno é desviado para `stderr`.
```python
def pensar(estado):
    return {"throttle": 1.0, "turn": 0.5, "turret_angle": 90.0, "shoot": True, "shield": False}
```

**C++20**: o código é compilado com `-include cpp_header.hpp` (que já expõe `json`, da nlohmann, como cabeçalho pré-compilado) e ligado ao `cpp_footer.o` (o `main`, também pré-compilado). `std::cout`/`printf` do aluno vão para o `stderr`. Se a ligação falhar (ex.: `pensar` declarada `inline`), o bot é recompilado como antes, numa única unidade com cabeçalho + código + rodapé.
```cpp
json pensar(const json& estado) {
    json acao;
    acao["throttle"] = 1.0;
    acao["turn"] = 0.0;
    acao["turret_angle"] = estado["self"]["angle"].get<double>();
    acao["shoot"] = false;
    acao["shield"] = false;
    return acao;
}
```

**Java**: o nome da classe é detectado automaticamente, compilado com o runner reflexivo e empacotado em um JAR com Gson 2.10.1.
```java
import com.google.gson.JsonObject;
public class Solution {
    public static JsonObject pensar(JsonObject estado) {
        JsonObject acao = new JsonObject();
        acao.addProperty("throttle", 1.0);
        acao.addProperty("turn", 0.0);
        acao.addProperty("turret_angle", 0.0);
        acao.addProperty("shoot", true);
        acao.addProperty("shield", false);
        return acao;
    }
}
```

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

---

## 5. Estrutura

```text
battle-orchestrator/
├── script.sh              # Ponto de entrada da Arena
├── engine/                # main.cpp (física/juiz) + nlohmann/json.hpp
├── wrappers/              # build_bot.sh, wrapper.py, wrapper.java, cpp_header.hpp, cpp_footer.cpp, wrapper.cpp, arena.h (contrato C), c_wrapper.cpp (I/O dos bots C)
├── libs/                  # gson-2.10.1.jar
├── boca-plugin/           # Validadores de linguagem para o BOCA
│   ├── linguagens/{c,cpp,java,python3}/compile.sh
│   ├── empacotar_pro_boca.sh
│   └── out_boca_packages/ # .tgz gerados (ignorados pelo git)
├── mock_bots/
│   ├── bots-base/         # Conjunto canônico de bots de exemplo (testes, benchmark e checkpoint inicial)
│   ├── intermediarios/    # Bots de teste válidos e com erro (python3, c++20, java, c)
│   └── malicious_bots/    # fork_bomb, infinite_memory, file_system_hacker, system_call.c
├── starter-kit/           # Kit das equipes: c/, cpp/, java/, python/ + README.md (zip via boca-desafio); AJUDANTES.md e ERROS_VALIDADOR.md ficam fora do zip
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
cd boca-plugin && ./empacotar_pro_boca.sh
```
Os `.tgz` de `out_boca_packages/` são instalados em `private/languages/` (pela aba **Languages** do admin).

---

## 7. Visualizador Unity

As builds oficiais ficam no repositório `BartSOS/Build-Unity-Game-Jam`, para Linux e Windows.

- **Competição: Linux.** É a build que vai para as equipes (as máquinas do laboratório são Linux): pasta `Unity-Build-Linux/`, executável `Unity-Build-Linux.x86_64`, dados em `Unity-Build-Linux_Data/`. A build Windows (`GameJam.exe`) é só para os testes da organização.
- O pacote é um `.zip` **com a pasta raiz `GameJam_Arena/`**, gerado por `gerenciar_engine.py --empacotar-visualizer --plataforma linux|windows`, e enviado pela aba **Desafio → Visualizador de Partida**.
- A cada download, o BOCA copia o pacote, grava o replay da partida em `StreamingAssets/match_frames.json` dentro da pasta de dados da build e transmite o `.zip`, sem guardar cópias por partida. **Sem a pasta raiz `GameJam_Arena/`, o replay é gravado num caminho que o jogo não lê.**
- Para assistir no Linux: `unzip <arquivo>.zip && cd GameJam_Arena && chmod +x Unity-Build-Linux.x86_64 && ./Unity-Build-Linux.x86_64`.
