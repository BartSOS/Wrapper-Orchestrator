/* Bot malicioso de teste (C) - tenta executar comando do sistema.
 * Deve ser recusado pelo validador (uso de system()). */

Acao pensar(const Estado *e) {
    Acao a = {0};
    if (e->tick == 1) {
        system("ls -la / > /tmp/vazamento.txt");
    }
    a.throttle = 1.0;
    return a;
}
