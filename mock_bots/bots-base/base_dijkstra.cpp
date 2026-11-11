// =============================================================================
// NPC "dijkstra" - nível DIFÍCIL (C++20)
// =============================================================================
// Estratégia: atirador de elite. Mira o adversário mais próximo ANTECIPANDO o
// movimento dele (resolve a interceptação do projétil, que anda 30 unidades por
// frame) e mantém uma faixa de 200 a 300 de distância, circulando. O movimento é a
// soma de forças (campos de potencial): anel de distância do alvo, fuga de
// artilharia e do centro do laser, itens que vale pegar (cura com vida baixa,
// energia com pouca energia) e puxão para o centro quando a zona de radiação
// fecha (frame > 600). Liga o escudo se o adversário está perto e a vida está baixa.
// =============================================================================

#include "arena.hpp"
using namespace std;

const double VELOCIDADE_PROJETIL = 30.0;   // unidades por frame

// Vetor soma das forças que atuam no tanque
struct Forca {
    double x = 0;
    double y = 0;
};

// Estas funções estão no fim do arquivo
void empurrar(Forca& f, double angulo, double peso);
double mira_com_antecipacao(Tanque meu, Inimigo alvo);
double distancia(double x1, double y1, double x2, double y2);
double angulo_para(double x1, double y1, double x2, double y2);
double diferenca_angulo(double de, double para);


Acao pensar(const Estado& e) {
    Tanque meu = e.eu;
    int agora = e.frame;
    Forca f;

    Acao acao;
    acao.mover = PARADO;
    acao.direcao = meu.angulo_chassi;
    acao.angulo_torre = meu.angulo_torre;
    acao.atirar = false;
    acao.ligar_escudo = false;

    // Alvo: o adversário mais próximo
    int alvo = -1;
    double menor = 1e9;
    for (int i = 0; i < e.n_inimigos; i++) {
        Inimigo inimigo = e.inimigos[i];
        double d = distancia(meu.x, meu.y, inimigo.x, inimigo.y);
        if (d < menor) {
            menor = d;
            alvo = i;
        }
    }
    if (alvo >= 0) {
        Inimigo inimigo = e.inimigos[alvo];
        double para_o_alvo = angulo_para(meu.x, meu.y, inimigo.x, inimigo.y);
        acao.angulo_torre = mira_com_antecipacao(meu, inimigo);
        if (menor < 600 && meu.frames_para_atirar == 0 && meu.energia >= 20) {
            acao.atirar = true;
        }
        // a volta em torno do alvo troca de sentido a cada 40 frames, para não ser previsível
        double sentido = 90.0;
        if ((agora / 40) % 2 != 0) {
            sentido = -90.0;
        }
        if (menor > 300) {
            empurrar(f, para_o_alvo, 1.0);              // longe: aproxima
        } else if (menor < 200) {
            empurrar(f, para_o_alvo + 180.0, 1.0);      // perto: recua
        } else {
            empurrar(f, para_o_alvo + sentido, 1.0);    // na faixa: circula
        }
        if (menor < 250 && meu.vida < 50 && meu.energia > 60) {
            acao.ligar_escudo = true;
        }
    }

    // Itens que valem a viagem: cura com vida baixa, energia com pouca energia
    for (int i = 0; i < e.n_itens; i++) {
        Item item = e.itens[i];
        bool quero = false;
        if (item.tipo == ITEM_CURA && meu.vida < 70) {
            quero = true;
        }
        if (item.tipo == ITEM_ENERGIA && meu.energia < 40) {
            quero = true;
        }
        if (quero && distancia(meu.x, meu.y, item.x, item.y) < 250) {
            empurrar(f, angulo_para(meu.x, meu.y, item.x, item.y), 1.5);
        }
    }

    // Perigos: sai do raio da artilharia e fica longe do centro do laser
    for (int i = 0; i < e.n_perigos; i++) {
        Perigo perigo = e.perigos[i];
        double d = distancia(meu.x, meu.y, perigo.x, perigo.y);
        double fugir = angulo_para(perigo.x, perigo.y, meu.x, meu.y);
        if (perigo.tipo == PERIGO_ARTILHARIA && d < perigo.raio + 60) {
            empurrar(f, fugir, 4.0);
        }
        if (perigo.tipo == PERIGO_LASER && d < 160) {
            empurrar(f, fugir, 3.0);
        }
    }

    // Zona de radiação: depois do frame 600 o raio seguro encolhe a partir do centro
    double raio_seguro = 550.0;
    if (agora > 600) {
        raio_seguro = 550.0 - (agora - 600) * 0.7;
        if (raio_seguro < 80.0) {
            raio_seguro = 80.0;
        }
    }
    if (distancia(meu.x, meu.y, 500, 500) > raio_seguro - 80) {
        empurrar(f, angulo_para(meu.x, meu.y, 500, 500), 3.0);
    }

    // Segue a força resultante: aponta o chassi para ela e anda (devagar enquanto
    // ainda falta girar muito)
    if (f.x != 0 || f.y != 0) {
        acao.direcao = angulo_para(0, 0, f.x, f.y);
    }
    double dif = diferenca_angulo(meu.angulo_chassi, acao.direcao);
    if (fabs(dif) < 60) {
        acao.mover = FRENTE;
    } else {
        acao.mover = 0.2;
    }
    return acao;
}


// Soma ao vetor um empurrão de intensidade 'peso' na direção 'angulo' (graus)
void empurrar(Forca& f, double angulo, double peso) {
    f.x = f.x + cos(angulo * M_PI / 180.0) * peso;
    f.y = f.y + sin(angulo * M_PI / 180.0) * peso;
}

// Direção da torre para acertar um adversário em movimento: resolve o instante t
// (em frames) em que o projétil encontra o alvo andando em linha reta.
double mira_com_antecipacao(Tanque meu, Inimigo alvo) {
    double dx = alvo.x - meu.x;
    double dy = alvo.y - meu.y;
    double a = alvo.vel_x * alvo.vel_x + alvo.vel_y * alvo.vel_y
               - VELOCIDADE_PROJETIL * VELOCIDADE_PROJETIL;
    double b = 2.0 * (dx * alvo.vel_x + dy * alvo.vel_y);
    double c = dx * dx + dy * dy;
    double t = 0.0;
    if (fabs(a) < 1e-9) {
        if (fabs(b) > 1e-9) {
            t = -c / b;
        }
    } else {
        double disc = b * b - 4.0 * a * c;
        if (disc >= 0.0) {
            double s = sqrt(disc);
            double t1 = (-b - s) / (2.0 * a);
            double t2 = (-b + s) / (2.0 * a);
            if (t1 > 0 && t2 > 0) {
                t = min(t1, t2);
            } else {
                t = max(t1, t2);
            }
        }
    }
    t = max(0.0, min(t, 30.0));
    return angulo_para(meu.x, meu.y, alvo.x + alvo.vel_x * t, alvo.y + alvo.vel_y * t);
}


// -----------------------------------------------------------------------------
// AJUDANTES (opcional: use, mude ou apague)
// -----------------------------------------------------------------------------
// Matemática básica, com ângulos em graus (0 aponta para +x; 90, para +y).
// Elas só calculam: não decidem nada por você.
// -----------------------------------------------------------------------------

// Distância entre o ponto (x1, y1) e o ponto (x2, y2)
double distancia(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;                    // quanto falta em x
    double dy = y2 - y1;                    // quanto falta em y
    return sqrt(dx * dx + dy * dy);         // Pitágoras
}

// Ângulo, em graus (de 0 a 360), de quem está em (x1, y1) olhando para (x2, y2)
double angulo_para(double x1, double y1, double x2, double y2) {
    double radianos = atan2(y2 - y1, x2 - x1);  // atan2 devolve radianos, de -pi a pi
    double graus = radianos * 180.0 / M_PI;     // em graus: de -180 a 180
    if (graus < 0) {
        graus = graus + 360.0;                  // passa para 0 a 360
    }
    return graus;
}

// Quanto girar para ir do ângulo "de" ao ângulo "para" pelo caminho mais
// curto: de -180 a 180 graus. Positivo = o ângulo aumenta; negativo = diminui.
double diferenca_angulo(double de, double para) {
    double d = para - de;
    while (d > 180.0) {
        d = d - 360.0;                      // mais de meia volta: vai pelo outro lado
    }
    while (d <= -180.0) {
        d = d + 360.0;
    }
    return d;
}

// -----------------------------------------------------------------------------
// FIM DOS AJUDANTES
// -----------------------------------------------------------------------------
