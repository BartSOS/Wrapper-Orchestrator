import java.util.Scanner;
import java.io.PrintStream;
import com.google.gson.Gson;
import com.google.gson.JsonObject;

public class JavaRunner {
    private static final Gson gson = new Gson();

    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        PrintStream originalOut = System.out;
        System.setOut(System.err); // Redireciona System.out para evitar poluição no IPC

        // Toda linha de estado recebe exatamente uma resposta: se o pensar() lançar qualquer
        // erro (inclusive StackOverflowError) ou devolver null/NaN, a resposta é a ação vazia {}.
        // Sem isso a engine esperaria a resposta que não veio, e o bot seria eliminado.
        while (scanner.hasNextLine()) {
            String linha = scanner.nextLine();
            if (linha.trim().isEmpty()) continue;
            String resposta = "{}";
            try {
                JsonObject estado = gson.fromJson(linha, JsonObject.class);
                JsonObject comandoSaida = CLASS_PLACEHOLDER.pensar(estado);
                if (comandoSaida == null) {
                    System.err.println("[ERRO NO BOT JAVA]: pensar() devolveu null (acao deste tick: {})");
                } else {
                    String texto = comandoSaida.toString();
                    // O Gson escreve NaN/Infinity, que não são JSON válido
                    if (texto.contains("NaN") || texto.contains("Infinity")) {
                        System.err.println("[ERRO NO BOT JAVA]: a acao tem NaN ou Infinity (acao deste tick: {})");
                    } else {
                        resposta = texto;
                    }
                }
            } catch (Throwable e) {
                System.err.println("[ERRO NO BOT JAVA]: " + e + " (acao deste tick: {})");
            }
            originalOut.println(resposta);
            originalOut.flush();
        }
        scanner.close();
    }
}
