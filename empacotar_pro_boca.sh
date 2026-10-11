#!/bin/bash
# Gera os pacotes BOCA (.tgz) a partir das definicoes em linguagens/
# Trazendo o "boca_compile.sh" para "compile.sh" dentro do tgz

cd "$(dirname "$0")"

mkdir -p out_boca_packages
cd linguagens

for lang_dir in */; do
    lang=${lang_dir%/}
    if [ -d "$lang" ] && [ -f "$lang/boca_compile.sh" ]; then
        echo "Empacotando linguagem: $lang"
        
        # Cria um diretorio temporario para montar o tgz
        mkdir -p "/tmp/boca_pkg_$lang"
        
        # O script de validacao vira tanto compile quanto compile.sh
        cp "$lang/boca_compile.sh" "/tmp/boca_pkg_$lang/compile.sh"
        cp "$lang/boca_compile.sh" "/tmp/boca_pkg_$lang/compile"
        chmod +x "/tmp/boca_pkg_$lang/compile.sh" "/tmp/boca_pkg_$lang/compile"
        
        # O run do BOCA sera vazio (exit 0)
        printf '#!/bin/bash\nexit 0\n' > "/tmp/boca_pkg_$lang/run.sh"
        printf '#!/bin/bash\nexit 0\n' > "/tmp/boca_pkg_$lang/run"
        chmod +x "/tmp/boca_pkg_$lang/run.sh" "/tmp/boca_pkg_$lang/run"
        
        # Se C++, copia o header e json.hpp
        if [ "$lang" = "cpp" ]; then
            cp "../wrappers/cpp_header.hpp" "/tmp/boca_pkg_$lang/"
            mkdir -p "/tmp/boca_pkg_$lang/nlohmann"
            cp "../engine/nlohmann/json.hpp" "/tmp/boca_pkg_$lang/nlohmann/"
        fi

        # Se Java, copia a lib gson
        if [ "$lang" = "java" ] && [ -f "../libs/gson-2.10.1.jar" ]; then
            cp "../libs/gson-2.10.1.jar" "/tmp/boca_pkg_$lang/gson.jar"
        fi

        # Gera o TGZ com todos os arquivos do diretorio temporario
        tar -czf "../out_boca_packages/${lang}_boca.tgz" -C "/tmp/boca_pkg_$lang" .
        
        # Limpa o temp
        rm -rf "/tmp/boca_pkg_$lang"
    fi
done

echo "Pacotes gerados em battle-orchestrator/out_boca_packages/"
ls -lh ../out_boca_packages/
