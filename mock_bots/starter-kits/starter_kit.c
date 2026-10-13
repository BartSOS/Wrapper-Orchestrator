/* =============================================================================
 * TOPCOM ARENA - STARTER KIT C (C99)
 * =============================================================================
 * Instruções para o competidor:
 * - Implemente apenas a função pensar(const Estado *e).
 * - Não crie main(), não leia stdin e não escreva em stdout.
 * - Os tipos Estado, Acao, Tanque, Inimigo, Hazard e Item, e as funções
 *   distancia(), angulo_para(), normalizar_angulo() e diferenca_angulo(),
 *   são injetados automaticamente (arena.h).
 * ============================================================================= */

Acao pensar(const Estado *e) {
    Acao a = {0};

    /* Exemplo inicial: andar para frente, girar suavemente
       e manter a torre na direção do chassi */
    a.throttle = 1.0;
    a.turn = 0.5;
    a.turret_angle = e->self.angle;
    a.shoot = false;
    a.shield = false;

    return a;
}
