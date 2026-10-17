# Ajudantes do starter kit (contrato C-K1)

Material de apoio para quem escreve o README e o enunciado. **Não vai no zip do kit**: a equipe lê os ajudantes no próprio arquivo do kit (`bot.py`, `bot.cpp`, `MeuBot.java`, `bot.c`).

## Princípio (definido pelo usuário)

O maior desafio do participante é a lógica de decisão (máquina de estados, escolha de alvo, fuga etc.), não entender o sistema. Por isso o kit traz **ajudantes escritos no próprio arquivo**, numa seção marcada:

```
AJUDANTES (opcional: renomeie, altere ou apague)
```

- A equipe vê o código, edita, renomeia ou apaga. A IDE encontra tudo (autocompletar, ir para a definição), porque está no arquivo.
- Os ajudantes **só facilitam o acesso aos atributos**. Nenhum deles decide nada: não existe "inimigo mais próximo", fuga, mira, perigo iminente, BFS etc. Isso é a estratégia da equipe.
- O template (`pensar`) é mínimo: lê cada atributo pelos ajudantes, com um comentário por campo, e devolve uma ação válida e simples. Não há FSM, enum de estados nem estrutura de decisão pronta. FSM e arquiteturas aparecem só como conceito no enunciado.
- Quem preferir pode apagar a seção inteira e ler o estado bruto (JSON, `dict`, `json`, `JsonObject`). O que o `pensar` recebe e devolve não muda.

## Nomes

Os nomes seguem o `arena.h` do C (que já é tipado) e as chaves do JSON da engine: `angle`, `turret_angle`, `hp`, `energy`, `cooldown`, `shield`, `throttle`, `turn`, `shoot`. Os ajudantes são em português (`eu`, `inimigos`, `distancia`...). Os nomes são os **mesmos nas 4 linguagens**; em Java, os métodos e campos compostos usam camelCase (`anguloPara`, `turretAngle`).

Quem lê o JSON bruto e quem usa os ajudantes enxerga os mesmos nomes: `eu(estado).hp` é o mesmo valor de `estado["self"]["hp"]`.

## Acessores do estado

| Ajudante | Python | C++ | Java | C (já vem tipado) |
| :--- | :--- | :--- | :--- | :--- |
| `tick` | `tick(estado)` -> `int` | `int tick(const json& estado)` | `static int tick(JsonObject estado)` | `e->tick` |
| `eu` | `eu(estado)` -> `Tanque` | `Tanque eu(const json& estado)` | `static Tanque eu(JsonObject estado)` | `e->self` |
| `inimigos` | `inimigos(estado)` -> lista de `Inimigo` | `std::vector<Inimigo> inimigos(const json& estado)` | `static List<Inimigo> inimigos(JsonObject estado)` | `e->inimigos[i]`, com `i < e->n_inimigos` |
| `perigos` | `perigos(estado)` -> lista de `Hazard` | `std::vector<Hazard> perigos(const json& estado)` | `static List<Hazard> perigos(JsonObject estado)` | `e->hazards[i]`, com `i < e->n_hazards` |
| `itens` | `itens(estado)` -> lista de `Item` | `std::vector<Item> itens(const json& estado)` | `static List<Item> itens(JsonObject estado)` | `e->itens[i]`, com `i < e->n_itens` |

- As listas contêm só o que a engine enviou neste tick: `inimigos` traz apenas adversários **vivos** e sem a vida deles.
- Lista vazia quando não há nada (nunca `None`/`null`).
- Em C os tipos abaixo são as structs do `arena.h` (`Estado`, `Tanque`, `Inimigo`, `Hazard`, `Item`, `Acao`), que o kit já traz. A seção de ajudantes do `bot.c` tem só a matemática e comentários que explicam esse mapa.

## Tipos e campos

Python e C++ usam os nomes da coluna **Campo**; Java usa a coluna **Java** quando difere. Em Python os objetos são classes simples com esses atributos (`meu.x`). Em C++ são `struct`s. Em Java são classes `static` aninhadas dentro da classe principal, com campos públicos.

### `Tanque` (devolvido por `eu`)

| Campo | Java | Tipo | Significado |
| :--- | :--- | :--- | :--- |
| `id` | `id` | inteiro | identificador do seu tanque |
| `x`, `y` | `x`, `y` | decimal | posição na arena (a arena tem 1000 x 1000) |
| `vx`, `vy` | `vx`, `vy` | decimal | velocidade, em unidades por segundo |
| `angle` | `angle` | decimal | direção do chassi, em graus [0, 360). 0 aponta para +x; `turn` positivo aumenta o ângulo |
| `turret_angle` | `turretAngle` | decimal | direção da torre, em graus [0, 360), absoluta (não relativa ao chassi) |
| `hp` | `hp` | decimal | vida, de 0 a 100 |
| `energy` | `energy` | decimal | energia, de 0 a 100 (tiro e escudo gastam energia) |
| `cooldown` | `cooldown` | decimal | segundos até poder atirar de novo (0 = pronto) |
| `shield` | `shield` | booleano | escudo ativo neste momento |

### `Inimigo` (elementos de `inimigos`)

| Campo | Tipo | Significado |
| :--- | :--- | :--- |
| `id` | inteiro | identificador do adversário |
| `x`, `y` | decimal | posição |
| `vx`, `vy` | decimal | velocidade, em unidades por segundo |

A engine não informa a vida, a energia nem o ângulo dos adversários.

### `Hazard` (elementos de `perigos`)

| Campo | Java | Tipo | Significado |
| :--- | :--- | :--- | :--- |
| `id` | `id` | inteiro | identificador do perigo |
| `tipo` | `tipo` | inteiro (constante) | `HAZARD_ARTILHARIA` ou `HAZARD_LASER` |
| `x`, `y` | `x`, `y` | decimal | posição do centro (artilharia) ou da origem do feixe (laser) |
| `radius` | `radius` | decimal | raio de efeito |
| `timer` | `timer` | decimal | segundos até o impacto (artilharia) ou até a ativação (laser) |
| `angle` | `angle` | decimal | direção atual do feixe, em graus (laser) |
| `duration` | `duration` | decimal | segundos de efeito (laser: quanto tempo queima depois de ativado) |

- `HAZARD_ARTILHARIA`: explode em (`x`, `y`) com raio `radius` quando `timer` chega a 0.
- `HAZARD_LASER`: o feixe gira a partir de (`x`, `y`) na direção `angle`; queima por `duration` segundos depois que `timer` chega a 0.

### `Item` (elementos de `itens`)

| Campo | Tipo | Significado |
| :--- | :--- | :--- |
| `id` | inteiro | identificador do item |
| `tipo` | inteiro (constante) | `ITEM_CURA` ou `ITEM_ENERGIA` |
| `x`, `y` | decimal | posição |

### Constantes de tipo

`HAZARD_ARTILHARIA = 0`, `HAZARD_LASER = 1`, `ITEM_CURA = 0`, `ITEM_ENERGIA = 1`. Mesmos nomes e valores nas 4 linguagens (em Python, constantes no topo do arquivo; em C++ e C, `enum`; em Java, `static final int` dentro da classe principal). O JSON bruto traz `"ARTILLERY"`, `"LASER"`, `"HEAL"`, `"ENERGY"` em `type`; os ajudantes convertem.

## Montador da ação

Recebe os comandos e devolve a ação no formato que a engine espera, com os **tipos certos**. Isso evita um erro silencioso: se `shoot` ou `shield` não forem booleanos de verdade (por exemplo, `1` em vez de `true`), a engine descarta o tick inteiro, e o tanque fica sem comando naquele tick.

| Linguagem | Assinatura |
| :--- | :--- |
| Python | `montar_acao(throttle=0.0, turn=0.0, turret_angle=None, shoot=False, shield=False)` -> `dict` |
| C++ | `json montar_acao(double throttle, double turn, double turret_angle, bool shoot, bool shield)` e a variante sem torre `json montar_acao(double throttle, double turn, bool shoot, bool shield)` |
| Java | `static JsonObject montarAcao(double throttle, double turn, double turretAngle, boolean shoot, boolean shield)` e a variante sem torre `montarAcao(double throttle, double turn, boolean shoot, boolean shield)` |
| C | não tem: `Acao` já é uma struct tipada (`throttle`, `turn`, `turret_angle`, `shoot`, `shield`). Atenção: em C a torre é sempre enviada; comece de `a.turret_angle = e->self.turret_angle` para mantê-la |

| Parâmetro | Faixa | Significado |
| :--- | :--- | :--- |
| `throttle` | -1.0 a 1.0 | aceleração: -1 ré, 0 parado, 1 frente toda |
| `turn` | -1.0 a 1.0 | giro do chassi: negativo diminui o ângulo, positivo aumenta |
| `turret_angle` | graus (0 a 360) | direção **absoluta** da torre. Omitido: a torre mantém o ângulo atual (Python: `None`; C++ e Java: variante sem o parâmetro) |
| `shoot` | booleano | tentar disparar (precisa de `cooldown` zerado e energia) |
| `shield` | booleano | tentar ligar o escudo (precisa de energia) |

A engine limita `throttle` e `turn` a [-1, 1] e normaliza a torre; o montador não faz isso por você.

## Matemática básica

As quatro funções que antes ficavam escondidas no `arena.h` passam a ficar **no arquivo do kit**, nas 4 linguagens, para a equipe ver, copiar e trocar. Ângulos em graus, mesma convenção da arena (0 = +x, crescente de +x para +y).

| Ajudante | Python / C++ / C | Java | Devolve |
| :--- | :--- | :--- | :--- |
| distância entre dois pontos | `distancia(x1, y1, x2, y2)` | `distancia(x1, y1, x2, y2)` | decimal >= 0 |
| ângulo do ponto 1 para o ponto 2 | `angulo_para(x1, y1, x2, y2)` | `anguloPara(x1, y1, x2, y2)` | graus em [0, 360) |
| menor diferença entre dois ângulos | `diferenca_angulo(de, para)` | `diferencaAngulo(de, para)` | graus em (-180, 180]; positivo = girar no sentido em que o ângulo aumenta |
| ângulo para o intervalo padrão | `normalizar_angulo(graus)` | `normalizarAngulo(graus)` | graus em [0, 360) |

Em C++ e Java os parâmetros e retornos são `double`. Em C, `static inline double`.

## Onde cada ajudante fica no arquivo

| Linguagem | Arquivo do kit | Onde |
| :--- | :--- | :--- |
| Python | `bot.py` | classes simples e funções **no topo** do arquivo, depois o `pensar` |
| C++ | `bot.cpp` | `#include <nlohmann/json.hpp>`, alias `json`, `struct`s e funções **no topo**, depois o `pensar` |
| Java | `MeuBot.java` | classes `static` aninhadas e métodos `static` **dentro da classe principal** `MeuBot` (uma classe auxiliar no nível do arquivo quebra o executor), antes do `pensar` |
| C | `bot.c` | `#include "arena.h"`, as 4 funções de matemática (`static inline`) e comentários |

Para apagar os ajudantes: remova a seção inteira (entre as linhas marcadas `AJUDANTES`) e trabalhe com o estado bruto. Em Java, apague também os `import` que ficarem sem uso (opcional).

## Assinaturas de `pensar` (não mudam)

| Linguagem | Assinatura |
| :--- | :--- |
| Python | `def pensar(estado):` (`estado` é um `dict`; devolve um `dict`) |
| C++ | `json pensar(const json& estado)` |
| Java | `public static JsonObject pensar(JsonObject estado)` |
| C | `Acao pensar(const Estado *e)` |

## Estado bruto (para quem apagar os ajudantes)

Uma linha JSON por tick. Chaves: `tick`; `self` {`id`, `x`, `y`, `vx`, `vy`, `angle`, `turret_angle`, `hp`, `energy`, `cooldown`, `shield`}; `enemies[]` {`id`, `x`, `y`, `vx`, `vy`} (só vivos); `hazards[]` {`id`, `type` (`ARTILLERY` ou `LASER`), `x`, `y`, `radius`, `timer`, `angle`, `duration`}; `items[]` {`id`, `type` (`HEAL` ou `ENERGY`), `x`, `y`}. A zona segura não vem no estado.

Resposta: um objeto JSON com `throttle`, `turn` (decimais), `turret_angle` (decimal, opcional), `shoot`, `shield` (booleanos). Saída de `print`/`printf`/`System.out` é desviada para o log de erros e descartada; o canal com a engine é protegido pelo executor.
