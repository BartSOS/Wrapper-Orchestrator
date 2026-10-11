#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Declaração da função que o competidor deve implementar
json pensar(const json& estado);

