// =============================================================================
// INFRAESTRUTURA DE I/O E COMUNICAÇÃO (WRAPPER OFICIAL DO BOCA)
// Este código será anexado ao final ou compilado junto com o código do competidor.
// =============================================================================
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// A declaração da função do aluno, que deve ser implementada no código dele
extern json pensar(const json& estado);

int main() {
    std::string linha;
    
    // Ler o STDIN continuamente
    while (std::getline(std::cin, linha)) {
        if (linha.empty()) continue;
        
        try {
            json estado_arena = json::parse(linha);
            
            // Chama a lógica do aluno
            json comando_saida = pensar(estado_arena);
            
            // Retorna o resultado JSON pelo STDOUT de forma segura
            std::cout << comando_saida.dump() << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERRO NO BOT C++]: " << e.what() << std::endl;
        }
    }
    return 0;
}

