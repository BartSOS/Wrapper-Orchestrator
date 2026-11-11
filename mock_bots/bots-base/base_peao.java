// =============================================================================
// NPC "peao" - nível FÁCIL (Java)
// =============================================================================
// Estratégia: anda de ré girando em círculos, com a torre varrendo a arena, e
// atira sempre que pode, sem mirar em ninguém. Não olha o estado da arena.
// Serve de adversário para quem está começando e de exemplo de memória entre
// frames (variável static da classe) e de uso dos ajudantes em Java.
// =============================================================================

public class Solution extends Arena {

    // Memória entre frames: a direção da torre, que avança 25 graus a cada frame
    static double torre = 0.0;

    public static Acao pensar(Estado e) {
        Tanque meu = e.eu;

        torre = torre + 25.0;
        if (torre >= 360.0) {
            torre = torre - 360.0;
        }

        Acao acao = new Acao();
        acao.mover = RE;                            // de ré...
        acao.direcao = meu.angulo_chassi + 90;      // ...girando o máximo (9 graus por frame)
        acao.angulo_torre = torre;
        acao.atirar = false;
        acao.ligar_escudo = false;
        if (meu.frames_para_atirar == 0 && meu.energia >= 20) {
            acao.atirar = true;
        }
        return acao;
    }


    // -------------------------------------------------------------------------
    // AJUDANTES (opcional: use, mude ou apague)
    // -------------------------------------------------------------------------
    // Matemática básica, com ângulos em graus (0 aponta para +x; 90, para +y).
    // Elas só calculam: não decidem nada por você.
    // -------------------------------------------------------------------------

    // Distância entre o ponto (x1, y1) e o ponto (x2, y2)
    public static double distancia(double x1, double y1, double x2, double y2) {
        double dx = x2 - x1;                    // quanto falta em x
        double dy = y2 - y1;                    // quanto falta em y
        return Math.sqrt(dx * dx + dy * dy);    // Pitágoras
    }

    // Ângulo, em graus (de 0 a 360), de quem está em (x1, y1) olhando para (x2, y2)
    public static double angulo_para(double x1, double y1, double x2, double y2) {
        double radianos = Math.atan2(y2 - y1, x2 - x1);  // atan2 devolve radianos, de -pi a pi
        double graus = radianos * 180.0 / Math.PI;       // em graus: de -180 a 180
        if (graus < 0) {
            graus = graus + 360.0;                       // passa para 0 a 360
        }
        return graus;
    }

    // Quanto girar para ir do ângulo "de" ao ângulo "para" pelo caminho mais
    // curto: de -180 a 180 graus. Positivo = o ângulo aumenta; negativo = diminui.
    public static double diferenca_angulo(double de, double para) {
        double d = para - de;
        while (d > 180.0) {
            d = d - 360.0;                      // mais de meia volta: vai pelo outro lado
        }
        while (d <= -180.0) {
            d = d + 360.0;
        }
        return d;
    }

    // -------------------------------------------------------------------------
    // FIM DOS AJUDANTES
    // -------------------------------------------------------------------------
}
