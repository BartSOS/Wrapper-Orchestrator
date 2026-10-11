// ============================================================================
// TOPCOM ARENA - STARTER KIT JAVA
// ============================================================================
// INSTRUÇÕES PARA O PARTICIPANTE:
// - Não crie o método main() nem utilize System.in ou System.out.
// - A arena compila seu código junto com o wrapper oficial de I/O.
// - Implemente apenas o método estático pensar(JsonObject estado).
// ============================================================================

import com.google.gson.JsonObject;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;

public class starter_kit {

    public static JsonObject pensar(JsonObject estado) {
        // 1. Extração dos dados do tanque do participante
        JsonObject self = estado.getAsJsonObject("self");
        double minhaX = self.get("x").getAsDouble();
        double minhaY = self.get("y").getAsDouble();
        double meuAngulo = self.get("angle").getAsDouble();
        double meuCooldown = self.get("cooldown").getAsDouble();
        double minhaEnergia = self.get("energy").getAsDouble();

        // 2. Ação Base Neutra
        JsonObject acao = new JsonObject();
        acao.addProperty("throttle", 1.0);
        acao.addProperty("turn", 0.5);
        acao.addProperty("turret_angle", meuAngulo);
        acao.addProperty("shoot", false);
        acao.addProperty("shield", false);

        // 3. Radar: Buscar Inimigo Mais Próximo
        double menorDistancia = 99999.0;
        JsonObject alvo = null;

        JsonArray enemies = estado.getAsJsonArray("enemies");
        if (enemies != null) {
            for (JsonElement elem : enemies) {
                JsonObject inimigo = elem.getAsJsonObject();
                double distX = inimigo.get("x").getAsDouble() - minhaX;
                double distY = inimigo.get("y").getAsDouble() - minhaY;
                double distancia = Math.hypot(distX, distY);

                if (distancia < menorDistancia) {
                    menorDistancia = distancia;
                    alvo = inimigo;
                }
            }
        }

        // 4. Se encontrou inimigo, mira e toma decisão
        if (alvo != null) {
            double alvoX = alvo.get("x").getAsDouble() - minhaX;
            double alvoY = alvo.get("y").getAsDouble() - minhaY;

            double anguloRad = Math.atan2(alvoY, alvoX);
            double anguloGraus = Math.toDegrees(anguloRad);
            double anguloAlvo = (anguloGraus + 360.0) % 360.0;

            acao.addProperty("turret_angle", anguloAlvo);

            // Controle de Direção do Chassi
            double diffAng = anguloAlvo - meuAngulo;
            while (diffAng > 180.0) diffAng -= 360.0;
            while (diffAng < -180.0) diffAng += 360.0;

            if (diffAng > 10.0) acao.addProperty("turn", 1.0);
            else if (diffAng < -10.0) acao.addProperty("turn", -1.0);
            else acao.addProperty("turn", 0.0);

            // Ajuste de Marcha
            if (menorDistancia < 120.0) acao.addProperty("throttle", -1.0);
            else acao.addProperty("throttle", 1.0);

            // Disparo autorizado se tiver energia
            if (menorDistancia < 600.0 && meuCooldown == 0.0 && minhaEnergia > 35.0) {
                acao.addProperty("shoot", true);
            }
        }

        return acao;
    }
}

