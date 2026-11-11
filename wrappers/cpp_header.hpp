#pragma once
// Cabeçalho injetado (-include, pré-compilado) em todo bot C++ pelo build_bot.sh.
// Traz o arquivo de apoio da equipe (arena.hpp: Estado, Acao, FRENTE...) e o
// nlohmann::json usado pelo rodapé (cpp_footer.cpp) para ler o estado da engine.
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "arena.hpp"

using json = nlohmann::json;

// A função que o competidor implementa (declarada em arena.hpp):
//   Acao pensar(const Estado& e);
