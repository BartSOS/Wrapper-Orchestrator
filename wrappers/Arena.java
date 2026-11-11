// =============================================================================
// GAME JAM PET 2026 - Arquivo de apoio para bots em Java
// =============================================================================
// Este arquivo vem no kit e no zip das fases (deixe-o na mesma pasta do seu
// MeuBot.java) e já existe no servidor: você NÃO envia o Arena.java, só o seu bot.
//
// No seu bot, a classe estende Arena (é isso que dá acesso a Estado, Acao,
// FRENTE, PARADO, RE...) e você escreve só o método pensar:
//
//     public class MeuBot extends Arena {
//         public static Acao pensar(Estado e) {
//             Acao a = new Acao();
//             a.mover = FRENTE;
//             return a;
//         }
//     }
//
// Ele é chamado 10 vezes por segundo (uma vez por frame). Não escreva main(),
// não leia o teclado (Scanner) e não use System.out para responder: a arena
// cuida disso. Para depurar, use
//     System.err.println("frame " + e.frame + ": x=" + e.eu.x);
//
// Unidades:
//   tempo ........ frames (1 frame = 0,1 segundo; 10 frames por segundo)
//   posição ...... unidades da arena (o mapa tem 1000 x 1000)
//   velocidade ... unidades por frame
//   ângulos ...... graus, de 0 a 360; 0 aponta para +x e o ângulo cresce de
//                  +x para +y (90 aponta para +y)
// =============================================================================
public class Arena {

    // Valores para Acao.mover
    public static final double FRENTE = 1.0;   // anda para a frente: 5 unidades por frame
    public static final double PARADO = 0.0;   // fica parado (para no mesmo frame, sem inércia)
    public static final double RE = -1.0;      // anda de ré: 5 unidades por frame para trás

    // Tipos de perigo (Perigo.tipo) e de item (Item.tipo)
    public static final int PERIGO_ARTILHARIA = 0;
    public static final int PERIGO_LASER = 1;
    public static final int ITEM_CURA = 0;
    public static final int ITEM_ENERGIA = 1;

    // Seu tanque (no Estado ele se chama eu)
    public static class Tanque {
        public int id;
        public double x, y;               // posição do centro
        public double vel_x, vel_y;       // velocidade, em unidades por frame
        public double angulo_chassi;      // para onde o chassi aponta (graus, 0 a 360)
        public double angulo_torre;       // para onde a torre aponta (graus, 0 a 360)
        public double vida;               // 0 a 100; em 0 o tanque é destruído
        public double energia;            // 0 a 100; atirar e o escudo gastam energia
        public int frames_para_atirar;    // frames até poder atirar de novo (0 = pode)
        public boolean escudo_ativo;      // o escudo está ligado neste frame
    }

    // Inimigo vivo
    public static class Inimigo {
        public int id;
        public double x, y;               // posição do centro
        public double vel_x, vel_y;       // velocidade, em unidades por frame
    }

    // Perigo da arena (artilharia ou laser)
    public static class Perigo {
        public int id;
        public int tipo;                  // PERIGO_ARTILHARIA ou PERIGO_LASER
        public double x, y;               // centro
        public double raio;               // alcance do efeito, em unidades
        public int frames_ate_impacto;    // frames até cair/ligar (0 = já está ativo)
        public double angulo;             // direção do feixe do laser (graus)
        public int frames_restantes;      // frames que o laser ainda fica ligado
    }

    // Item que o tanque coleta passando por cima
    public static class Item {
        public int id;
        public int tipo;                  // ITEM_CURA ou ITEM_ENERGIA
        public double x, y;
    }

    // Ponto do objetivo de uma fase de treino ("chegue ao ponto B").
    // É só uma marca no chão: não bloqueia e não é inimigo.
    public static class Ponto {
        public String id = "";            // nome do ponto; compare com .equals("B")
        public double x, y;               // centro
        public double raio;               // o tanque está no ponto se o centro dele
                                          // estiver a até raio unidades deste centro
    }

    // O que o seu tanque enxerga neste frame
    public static class Estado {
        public int frame;                         // número do frame (começa em 0)
        public Tanque eu = new Tanque();          // o seu tanque
        public int n_inimigos;                    // quantos inimigos há em inimigos
        public Inimigo[] inimigos = new Inimigo[0];
        public int n_perigos;                     // quantos perigos há em perigos
        public Perigo[] perigos = new Perigo[0];
        public int n_itens;                       // quantos itens há em itens
        public Item[] itens = new Item[0];
        public int n_pontos;                      // fases de treino: quantos pontos há
        public Ponto[] pontos = new Ponto[0];     // no torneio, n_pontos é sempre 0
    }

    // O que o seu tanque faz neste frame. O que você não preencher fica como está
    // aqui: parado, sem girar, torre onde está, sem atirar e sem escudo.
    public static class Acao {
        public double mover = PARADO;             // FRENTE, PARADO ou RE (0.5 = metade da velocidade)
        public double direcao = Double.NaN;       // para onde o chassi deve apontar (graus). O tanque
                                                  // gira até 9 graus por frame até chegar lá.
                                                  // Sem valor (NaN) = mantém a direção atual.
        public double angulo_torre = Double.NaN;  // para onde a torre aponta (graus); muda na hora.
                                                  // Sem valor (NaN) = mantém.
        public boolean atirar = false;            // true atira; false não atira
        public boolean ligar_escudo = false;      // true liga o escudo neste frame
    }
}
