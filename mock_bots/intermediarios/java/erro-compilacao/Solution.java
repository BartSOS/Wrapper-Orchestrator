// Bot de teste (Java) - ERRO DE COMPILAÇÃO PROPOSITAL.
// Implementa public static Acao pensar(Estado e) com a assinatura correta,
// para passar pela checagem estrutural do validador e chegar ao javac, que deve recusar:
//   1. tipos incompatíveis (String atribuída a double);
//   2. símbolo não declarado (velocidadeMaxima).
public class Solution extends Arena {
    public static Acao pensar(Estado e) {
        double angulo = "noventa graus";
        Acao a = new Acao();
        a.mover = velocidadeMaxima;
        a.angulo_torre = angulo + e.eu.angulo_chassi;
        a.atirar = true;
        a.ligar_escudo = false;
        return a;
    }
}
