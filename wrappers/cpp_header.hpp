#pragma once
// Cabeçalho injetado (-include, pré-compilado) em todo bot C++ pelo build_bot.sh.
// Traz o arquivo de apoio da equipe (arena.hpp: Estado, Acao, FRENTE...) e,
// para os bots escritos antes dos nomes em português, o nlohmann::json.
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "arena.hpp"

using json = nlohmann::json;

// A função que o competidor implementa (uma das duas):
//   Acao pensar(const Estado& e);        // a atual, declarada em arena.hpp
//   json pensar(const json& estado);     // a antiga (JSON cru, nomes em inglês)
json pensar(const json& estado);
