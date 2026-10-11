# Battle Orchestrator - Arena TopCom BOCA

O **Battle Orchestrator** é o módulo central responsável pela compilação, injeção de wrappers de comunicação, controle de isolamento (*sandboxing*) e execução da Engine de Batalha (Top-Down Game Jam) para a maratona TopCom.

O módulo é **100% autônomo e independente**: ele funciona de forma idêntica tanto em linha de comando fora do BOCA quanto invocado como juiz automatizado dentro do contêiner BOCA.

---

## 1. Contrato de Entrada (`submissoes/`)

O orquestrador segue um contrato estrito de I/O para garantir simplicidade e desacoplamento:
- **Diretório Canônico de Entrada:** Todos os bots candidatos a entrar na arena devem estar localizados no diretório `submissoes/` (extensões aceitas: `.py`, `.cpp`, `.java`).
- **Capacidade da Arena:** A arena suporta de **2 até 20 bots simultâneos** por partida.
- **Execução Fora do BOCA:** Copie até 20 códigos para a pasta `submissoes/` e execute `./script.sh`.
- **Execução Dentro do BOCA:** O serviço administrativo do BOCA (`EngineExecutionService`) sincroniza as submissões aprovadas dos times diretamente para a pasta `submissoes/` e invoca `./script.sh`.

---

## 2. Modelo de Wrappers Transparentes (I/O Blindado)

Para que os competidores foquem estritamente na lógica e raciocínio competitivo (e não em parsing de JSON ou leitura de buffers do sistema operacional), a comunicação via `stdin`/`stdout` é **totalmente encapsulada internamente por Wrappers**.

O competidor **não** cria funções `main()`, **não** lê `stdin` e **não** precisa se preocupar com includes de bibliotecas de JSON.

### Especificação por Linguagem:

#### A. Python 3
- O competidor implementa apenas a função `pensar(estado)`:
```python
import math

def pensar(estado):
    # estado é um dicionário nativo Python com 'self', 'enemies' e 'items'
    return {
        "throttle": 1.0,        # Aceleração (-1.0 a 1.0)
        "turn": 0.5,            # Rotação do chassi (-1.0 a 1.0)
        "turret_angle": 90.0,   # Ângulo da mira (0.0 a 360.0)
        "shoot": True,          # Disparar canhão (booleano)
        "shield": False         # Ativar escudo defensivo (booleano)
    }
```
*O wrapper redireciona `print()` acidentais para `stderr`, consome o stdin contínuo e envia os comandos formatados.*

#### B. C++17
- O competidor **não** inclui cabeçalhos de JSON nem cria a função `main()`. O tipo `json` já está disponível no escopo global:
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
*O orquestrador injeta `cpp_header.hpp` antes do código e `cpp_footer.cpp` após o código durante a compilação.*

#### C. Java
- O competidor implementa apenas o método estático `pensar(JsonObject estado)`:
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
*O orquestrador detecta o nome da classe do aluno, compila com o Runner e empacota um Fat JAR executável auto-contido.*

---

## 3. Estrutura de Diretórios
 
```
battle-orchestrator/
├── boca-plugin/           # Plugin de integração e empacotamento com o BOCA
│   ├── empacotar_pro_boca.sh # Gera os pacotes .tgz de cada linguagem
│   ├── out_boca_packages/    # Arquivos .tgz finais para upload no painel BOCA
│   └── linguagens/           # Scripts de validação e compilação do BOCA (compile.sh)
├── engine/                # Motor de simulação em C++17 (main.cpp e json.hpp)
├── libs/                  # Dependências locais (gson-2.10.1.jar)
├── wrappers/              # Templates de I/O e scripts de build protegidos
│   ├── build_bot.sh       # Construtor interno para C++, Python e Java (evita conflito com o BOCA)
│   ├── cpp_header.hpp     # Cabeçalho transparente C++
│   ├── cpp_footer.cpp     # Rodapé/main() transparente C++
│   ├── wrapper.py         # Executor transparente Python
│   └── wrapper.java       # Runner reflexivo transparente Java
├── starter-kits/          # Modelos simples e minimalistas para os competidores
├── mock_bots/             # Bots de teste (intermediários, estratégias completas e maliciosos)
├── submissoes/            # Pasta de entrada oficial para execução da partida
├── script.sh              # ÚNICO script .sh na raiz (ponto de entrada da Arena BOCA)
└── README.md              # Esta documentação canônica
```

---

## 4. Como Executar

### Teste Local Standalone:
1. Copie até 20 bots para a pasta `submissoes/`:
   ```bash
   cp mock_bots/intermediarios/python3/valido/bot_bfs.py submissoes/
   cp mock_bots/intermediarios/c++20/valido/bot_dijkstra.cpp submissoes/
   ```
2. Execute o orquestrador:
   ```bash
   ./script.sh
   ```
3. O orquestrador exibirá o inventário pré/pós compilação, transmitirá os eventos da partida no terminal e exportará o replay completo em `match_frames.json`.

### Build Manual de um Bot (Opcional):
Para compilar uma submissão individualmente fora da partida:
```bash
./wrappers/build_bot.sh submissoes/bot.py build/bots/bot
```

### Empacotamento para o BOCA:
Para gerar os pacotes `.tgz` que os administradores sobem na aba *Languages* do BOCA (que validam a presença da função `pensar()` e bloqueiam chamadas indevidas):
```bash
cd boca-plugin && ./empacotar_pro_boca.sh
```
Os arquivos gerados estarão em `boca-plugin/out_boca_packages/`.
