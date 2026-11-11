// Bot de teste (Java) - Peão: anda de ré girando sem parar e atira o tempo todo.
public class Solution extends Arena {

    public static Acao pensar(Estado e) {
        Tanque meu = e.eu;
        Acao acao = new Acao();
        acao.mover = RE;                          // sempre de ré
        acao.direcao = meu.angulo_chassi + 9.0;   // gira 9 graus por frame, igual um peão
        acao.angulo_torre = 0.0;
        acao.atirar = true;                       // atira sempre que pode
        acao.ligar_escudo = false;
        return acao;
    }
}
