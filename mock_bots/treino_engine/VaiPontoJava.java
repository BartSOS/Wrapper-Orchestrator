// Bot de teste do modo cenário (frente engine), em Java: vai até o primeiro ponto da
// fase (e.pontos[0], Arena.java) e para dentro de metade do raio. Sem pontos, fica parado.
public class VaiPontoJava extends Arena {

    public static Acao pensar(Estado e) {
        Tanque meu = e.eu;
        Acao acao = new Acao();
        acao.mover = PARADO;
        acao.direcao = meu.angulo_chassi;
        acao.angulo_torre = meu.angulo_torre;
        acao.atirar = false;
        acao.ligar_escudo = false;
        if (e.n_pontos < 1) {
            return acao;
        }
        Ponto p = e.pontos[0];
        if (distancia(meu.x, meu.y, p.x, p.y) <= p.raio * 0.5) {
            return acao;
        }
        double para_o_ponto = angulo_para(meu.x, meu.y, p.x, p.y);
        acao.direcao = para_o_ponto;
        if (Math.abs(diferenca_angulo(meu.angulo_chassi, para_o_ponto)) < 30.0) {
            acao.mover = FRENTE;
        } else {
            acao.mover = 0.2;
        }
        return acao;
    }

    public static double distancia(double x1, double y1, double x2, double y2) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        return Math.sqrt(dx * dx + dy * dy);
    }

    public static double angulo_para(double x1, double y1, double x2, double y2) {
        double graus = Math.atan2(y2 - y1, x2 - x1) * 180.0 / Math.PI;
        if (graus < 0) {
            graus = graus + 360.0;
        }
        return graus;
    }

    public static double diferenca_angulo(double de, double para) {
        double d = para - de;
        while (d > 180.0) {
            d = d - 360.0;
        }
        while (d <= -180.0) {
            d = d + 360.0;
        }
        return d;
    }
}
