// Armadilha: o pensar() lança exceção, estoura a pilha (StackOverflowError, que não é
// Exception) ou devolve null em alguns frames. Antes o wrapper não respondia nesses frames
// e o bot dessincronizava; agora a resposta é {} (parado) e o bot continua na partida.
public class ArmExcecao extends Arena {

    public static Acao pensar(Estado e) {
        int t = e.frame;
        if (t % 17 == 2) {
            throw new IllegalStateException("erro proposital");
        }
        if (t % 17 == 9) {
            recursao(0);
        }
        if (t % 17 == 14) {
            return null;
        }
        Tanque meu = e.eu;
        Acao acao = new Acao();
        acao.mover = 0.6;
        acao.direcao = meu.angulo_chassi - 3.0;
        acao.angulo_torre = meu.angulo_torre;
        acao.atirar = false;
        acao.ligar_escudo = false;
        return acao;
    }

    public static int recursao(int n) {
        return recursao(n + 1) + 1;
    }
}
