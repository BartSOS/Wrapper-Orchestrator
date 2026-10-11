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

        while (scanner.hasNextLine()) {
            String linha = scanner.nextLine();
            if (linha.trim().isEmpty()) continue;
            try {
                JsonObject estado = gson.fromJson(linha, JsonObject.class);
                JsonObject comandoSaida = CLASS_PLACEHOLDER.pensar(estado);
                originalOut.println(comandoSaida.toString());
                originalOut.flush();
            } catch (Exception e) {
                System.err.println("[ERRO NO BOT JAVA]: " + e.getMessage());
            }
        }
        scanner.close();
    }
}
