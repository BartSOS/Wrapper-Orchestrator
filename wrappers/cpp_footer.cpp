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

    // Toda linha de estado recebe exatamente uma resposta: se o pensar() lançar qualquer
    // exceção, a resposta é a ação vazia {} (sem isso a engine esperaria a resposta que
    // não veio, e o bot seria eliminado 20 ticks depois).
    std::string linha;
    while (std::getline(std::cin, linha)) {
        if (linha.empty()) continue;
        std::string resposta = "{}";
        try {
            json estado_arena = json::parse(linha);
            json comando_saida = pensar(estado_arena);
            resposta = comando_saida.dump();
        } catch (const std::exception& e) {
            std::cerr << "[ERRO NO BOT C++]: " << e.what() << " (acao deste tick: {})" << std::endl;
        } catch (...) {
            std::cerr << "[ERRO NO BOT C++]: excecao desconhecida (acao deste tick: {})" << std::endl;
        }
        std::fprintf(canal, "%s\n", resposta.c_str());
        std::fflush(canal);
        std::cout.flush();
    }
    return 0;
}
