// =============================================================================
// GAME JAM PET 2026 - Arquivo de apoio para bots em C++ (C++20)
// =============================================================================
// Este arquivo vem no kit e no zip das fases (deixe-o na mesma pasta do seu
// bot.cpp) e já existe no servidor: você NÃO envia o arena.hpp, só o bot.cpp.
//
// O que você escreve é só a função:
//
//     Acao pensar(const Estado& e);
//
// Ela é chamada 10 vezes por segundo (uma vez por frame). Ela recebe o que o
// seu tanque enxerga (Estado) e devolve o que ele deve fazer (Acao).
// Não escreva main(), não leia o teclado (cin) e não use cout para responder:
// a arena cuida disso. Para depurar, use
//     cerr << "frame " << e.frame << ": x=" << e.eu.x << endl;
// (o modelo já traz "using namespace std;" logo depois do #include "arena.hpp").
//
// Unidades:
//   tempo ........ frames (1 frame = 0,1 segundo; 10 frames por segundo)
//   posição ...... unidades da arena (o mapa tem 1000 x 1000)
//   velocidade ... unidades por frame
//   ângulos ...... graus, de 0 a 360; 0 aponta para +x e o ângulo cresce de
//                  +x para +y (90 aponta para +y)
// =============================================================================
#ifndef GAMEJAM_ARENA_HPP
#define GAMEJAM_ARENA_HPP

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Valores para Acao.mover
const double FRENTE = 1.0;    // anda para a frente: 5 unidades por frame
const double PARADO = 0.0;    // fica parado (para no mesmo frame, sem inércia)
const double RE = -1.0;       // anda de ré: 5 unidades por frame para trás

// Seu tanque (no Estado ele se chama eu)
struct Tanque {
    int    id = 0;
    double x = 0, y = 0;            // posição do centro
    double vel_x = 0, vel_y = 0;    // velocidade, em unidades por frame
    double angulo_chassi = 0;       // para onde o chassi aponta (graus, 0 a 360)
    double angulo_torre = 0;        // para onde a torre aponta (graus, 0 a 360)
    double vida = 0;                // 0 a 100; em 0 o tanque é destruído
    double energia = 0;             // 0 a 100; atirar e o escudo gastam energia
    int    frames_para_atirar = 0;  // frames até poder atirar de novo (0 = pode)
    bool   escudo_ativo = false;    // o escudo está ligado neste frame
};

// Inimigo vivo
struct Inimigo {
    int    id = 0;
    double x = 0, y = 0;            // posição do centro
    double vel_x = 0, vel_y = 0;    // velocidade, em unidades por frame
};

enum TipoPerigo { PERIGO_ARTILHARIA = 0, PERIGO_LASER = 1 };

// Perigo da arena (artilharia ou laser)
struct Perigo {
    int        id = 0;
    TipoPerigo tipo = PERIGO_ARTILHARIA;  // PERIGO_ARTILHARIA ou PERIGO_LASER
    double     x = 0, y = 0;              // centro
    double     raio = 0;                  // alcance do efeito, em unidades
    int        frames_ate_impacto = 0;    // frames até cair/ligar (0 = já está ativo)
    double     angulo = 0;                // direção do feixe do laser (graus)
    int        frames_restantes = 0;      // frames que o laser ainda fica ligado
};

enum TipoItem { ITEM_CURA = 0, ITEM_ENERGIA = 1 };

// Item que o tanque coleta passando por cima
struct Item {
    int      id = 0;
    TipoItem tipo = ITEM_CURA;      // ITEM_CURA ou ITEM_ENERGIA
    double   x = 0, y = 0;
};

// Ponto do objetivo de uma fase de treino ("chegue ao ponto B").
// É só uma marca no chão: não bloqueia e não é inimigo.
struct Ponto {
    std::string id;                 // nome do ponto, igual ao do texto da fase
    double x = 0, y = 0;            // centro
    double raio = 0;                // o tanque está no ponto se o centro dele
                                    // estiver a até raio unidades deste centro
};

// O que o seu tanque enxerga neste frame
struct Estado {
    int                  frame = 0;      // número do frame (começa em 0)
    Tanque               eu;             // o seu tanque
    int                  n_inimigos = 0; // quantos inimigos há em inimigos
    std::vector<Inimigo> inimigos;
    int                  n_perigos = 0;  // quantos perigos há em perigos
    std::vector<Perigo>  perigos;
    int                  n_itens = 0;    // quantos itens há em itens
    std::vector<Item>    itens;
    int                  n_pontos = 0;   // fases de treino: quantos pontos há
    std::vector<Ponto>   pontos;         // no torneio, n_pontos é sempre 0
};

// O que o seu tanque faz neste frame. O que você não preencher fica como está
// aqui: parado, sem girar, torre onde está, sem atirar e sem escudo.
struct Acao {
    double mover = PARADO;          // FRENTE, PARADO ou RE (0.5 = metade da velocidade)
    double direcao = NAN;           // para onde o chassi deve apontar (graus). O tanque
                                    // gira até 9 graus por frame até chegar lá.
                                    // Sem valor (NAN) = mantém a direção atual.
    double angulo_torre = NAN;      // para onde a torre aponta (graus); muda na hora.
                                    // Sem valor (NAN) = mantém.
    bool   atirar = false;          // true (ou 1) atira; false (ou 0) não atira
    bool   ligar_escudo = false;    // true (ou 1) liga o escudo neste frame
};

// A função que você escreve
Acao pensar(const Estado& e);

#endif  // GAMEJAM_ARENA_HPP
