# Battle Orchestrator — Arena TopCom

O **Battle Orchestrator** é o motor da Arena: recebe os códigos das equipes, injeta os wrappers de I/O, compila, isola e executa os bots, roda a **Engine de física em C++20** e gera o replay `match_frames.json`. O BOCA transforma esse replay em experiência visual no pacote Unity.

O módulo é **autônomo**: roda igual na linha de comando (fora do BOCA) e dentro do contêiner `topcom-boca-web-1`, quando um juiz ou admin clica em **Executar Engine**.

```text
códigos das equipes ──▶ wrappers + compilação ──▶ Engine C++ (até 1500 ticks) ──▶ match_frames.json ──▶ Unity (GameJam.exe)
```

---

## 1. Contrato de Entrada

- **Diretório de entrada:** `submissoes/` (padrão) ou o diretório passado como 1º argumento de `script.sh`.
- **Extensões aceitas na partida:** `.c` (C99), `.cpp` (C++20), `.java`, `.py`. Arquivos vazios são descartados.
- O `build_bot.sh` é chamado via `bash`: a permissão `+x` se perde quando o BOCA extrai o `engine.zip`.
- **Capacidade:** de 2 a **20 bots** por partida. Os excedentes são listados como excluídos.
- **Dentro do BOCA:** o `EngineExecutionService.php` extrai o `engine.zip`, copia o código ativo de cada equipe como `submissoes/<username>.<ext>` e executa `./script.sh`.

---

## 2. O que o Competidor Escreve

Apenas a função `pensar(estado)`, chamada a cada tick. O competidor **não** escreve `main()`, não lê `stdin` e não faz parsing de JSON: os wrappers fazem isso.

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

**C (C99)**: o cabeçalho `wrappers/arena.h` é injetado automaticamente e define as structs `Estado`, `Tanque`, `Inimigo`, `Hazard`, `Item` e `Acao`, além das funções auxiliares `distancia()`, `angulo_para()`, `normalizar_angulo()` e `diferenca_angulo()`. O código do aluno é compilado com `gcc -std=c99` e ligado ao wrapper interno `wrappers/c_wrapper.cpp` (C++20), que converte o JSON nas structs. `printf()` do aluno vai para o `stderr` e não corrompe a comunicação. Listas maiores que `MAX_INIMIGOS`/`MAX_HAZARDS`/`MAX_ITENS` são truncadas.
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

**Python 3**: o wrapper é anexado ao final do código. `print()` do aluno é desviado para `stderr`.
```python
def pensar(estado):
    return {"throttle": 1.0, "turn": 0.5, "turret_angle": 90.0, "shoot": True, "shield": False}
```

**C++20**: o código é concatenado entre `cpp_header.hpp` (que já expõe `json`, da nlohmann) e `cpp_footer.cpp` (que contém o `main`).
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
| Arena | 1000 × 1000; tanques posicionados em círculo |
| Tick | 0,1 s (10 Hz) · máximo **1500 ticks** (150 s simulados) |
| Tanque | raio 20 · velocidade máx. 50/s · giro 90°/s · **100 HP** · **100 de energia** (regenera 5/s) |
| Tiro | custa 20 de energia · recarga de 1 s · projétil com velocidade 300 e **25 de dano** |
| Escudo | consome 50 de energia/s · **só ativa com mais de 50% de energia** · bloqueia projéteis, reduz artilharia a 20% e laser a 10% |
| Itens | `HEAL` (+30 HP) · `ENERGY` (+50 de energia) |
| Artilharia | aviso de 3 s · raio 80 · 40 de dano (fica maior e mais frequente após o tick 400) |
| Laser | feixe giratório (30°/s) · 30 de dano/s |
| Zona | a partir do **tick 600**, o raio seguro encolhe de 550 até 80. Fora dele, 2,5 HP a cada 10 ticks |
| Timeout | 20 ticks seguidos sem resposta → tanque desativado (`[TIMEOUT]`) |
| Processo encerrado | o bot que termina ou fecha o pipe é destruído (`[DESCONEXAO]`). **O bot precisa ficar vivo a partida inteira** |
| Fim | resta 1 sobrevivente ou o limite de ticks |
| Classificação | 1º vivo · 2º abates · 3º HP restante |

Isolamento de cada bot (`script.sh`): `timeout` de **30 s de tempo real** e `ulimit -v` de ~2 GB. Os validadores do BOCA bloqueiam, já no envio, `os`, `subprocess`, `eval` e `exec` (Python), `system(` (C/C++) e `Runtime`, `ProcessBuilder` e `System.exit` (Java).

---

## 4. Etapas do `script.sh`

1. Verifica as dependências (`g++`, `python3`, `timeout`).
2. Verifica as bibliotecas locais (`libs/gson-2.10.1.jar`, `engine/nlohmann/json.hpp`).
3. Compila a Engine.
4. Faz o **inventário pré-compilação** (equipe, arquivo, tamanho, linguagem), compila cada bot com `wrappers/build_bot.sh` e faz o **inventário pós-compilação** (bots aprovados com slot e comando; excluídos com motivo). A falha de um bot não aborta a partida.
5. Executa a partida e transmite os eventos (`[ABATE]`, `[CURA]`, `[BOMBA]`, `[ZONA]`, `[LASER]`, `[TIMEOUT]`, `[DESCONEXAO]`, `[FIM]`).
6. Consolida o `match_frames.json`: `config`, `partida_id`, `vencedor`, `turnos`, `duracao_ms`, `destaques` e todos os `frames` (tanques, projéteis, hazards, itens, eventos), com os nomes reais das equipes.

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
│   ├── starter-kits/      # starter_kit.py / .cpp / .java / .c (modelos entregues às equipes)
│   ├── bots-base/         # Conjunto canônico de bots de exemplo (testes, benchmark e checkpoint inicial)
│   ├── intermediarios/    # Bots de teste válidos e com erro (python3, c++20, java, c, starter_completo)
│   └── malicious_bots/    # fork_bomb, infinite_memory, file_system_hacker, system_call.c
├── submissoes/            # Entrada da partida (conteúdo ignorado pelo git)
├── build/, replays/       # Saídas geradas (ignoradas pelo git)
└── build_bot.sh           # Cópia antiga; o script em uso é wrappers/build_bot.sh
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
Roda o `script.sh` dos fontes atuais dentro do contêiner `topcom-boca-web-1`.

**Medições (12/10/2026):**
- a partida em si leva 0,3 a 0,9 s de tempo real, mesmo com 20 bots;
- o custo é a compilação, cerca de 1 s por bot (~23 s com 20 bots);
- nenhuma desconexão ou timeout;
- memória do contêiner ~450 MB;
- acima de 20, os excedentes são excluídos, mas **ainda assim compilados**.

### Compilar um único bot
```bash
./wrappers/build_bot.sh submissoes/bot.py build/bots/bot
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

A build está em `../unity_engine_v1/` (`GameJam.exe`, `GameJam_Data/`, ...). O replay é lido de `GameJam_Data/StreamingAssets/match_frames.json`.

- Compacte a build em `.zip` **dentro de uma pasta raiz `TopCom_Arena/`** (`TopCom_Arena/GameJam.exe`, `TopCom_Arena/GameJam_Data/...`) e envie-a pela aba **Desafio → Visualizador de Partida**. O BOCA valida que o zip contém `GameJam.exe`.
- A cada download, o BOCA copia o pacote, grava o replay da partida em `TopCom_Arena/GameJam_Data/StreamingAssets/match_frames.json` e transmite o `.zip`, sem guardar cópias por partida. **Sem a pasta raiz `TopCom_Arena/`, o replay é gravado num caminho que o jogo não lê.**
- A build é **somente para Windows**.
