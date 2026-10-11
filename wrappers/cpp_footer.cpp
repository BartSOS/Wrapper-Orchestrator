int main() {
    std::string linha;
    while (std::getline(std::cin, linha)) {
        if (linha.empty()) continue;
        try {
            json estado_arena = json::parse(linha);
            json comando_saida = pensar(estado_arena);
            std::cout << comando_saida.dump() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[ERRO NO BOT C++]: " << e.what() << std::endl;
        }
    }
    return 0;
}

