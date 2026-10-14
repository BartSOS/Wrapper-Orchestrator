// =============================================================================
// INFRAESTRUTURA DE I/O PARA BOTS EM C++ (WRAPPER OFICIAL DO BOCA)
// Pré-compilado uma vez por partida (cpp_footer.o) com o cabeçalho cpp_header.hpp
// e ligado ao código do competidor, que só implementa json pensar(const json&).
// =============================================================================
#include <cstdio>
#include <unistd.h>

int main() {
    // Protege o canal com a Engine: std::cout/printf do competidor passam a ir para o
    // stderr, e as respostas usam uma cópia privada do stdout original.
    int fd_canal = dup(STDOUT_FILENO);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    FILE* canal = fdopen(fd_canal, "w");
    if (!canal) return 1;

    std::string linha;
    while (std::getline(std::cin, linha)) {
        if (linha.empty()) continue;
        try {
            json estado_arena = json::parse(linha);
            json comando_saida = pensar(estado_arena);
            std::fprintf(canal, "%s\n", comando_saida.dump().c_str());
            std::fflush(canal);
        } catch (const std::exception& e) {
            std::cerr << "[ERRO NO BOT C++]: " << e.what() << std::endl;
        }
        std::cout.flush();
    }
    return 0;
}
