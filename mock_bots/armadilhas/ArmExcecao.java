// Armadilha: o pensar() lança exceção, estoura a pilha (StackOverflowError, que não é
// Exception) ou devolve null em alguns ticks. Antes o wrapper não respondia nesses ticks
// e o bot dessincronizava; agora a resposta é {} (parado) e o bot continua na partida.
import com.google.gson.JsonObject;

public class ArmExcecao {
    static int recursao(int n) { return recursao(n + 1) + 1; }

    public static JsonObject pensar(JsonObject estado) {
        int t = estado.get("tick").getAsInt();
        if (t % 17 == 2) throw new IllegalStateException("erro proposital");
        if (t % 17 == 9) recursao(0);
        if (t % 17 == 14) return null;
        JsonObject a = new JsonObject();
        a.addProperty("throttle", 0.6);
        a.addProperty("turn", -0.3);
        return a;
    }
}
