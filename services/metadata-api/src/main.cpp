#include "tecflix_http.hpp"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <iostream>
#include <exception>

int main() {
    try {
        // Inicializar el driver de MongoDB
        mongocxx::instance instancia{};

        // Crear la conexion con MongoDB
        mongocxx::client cliente{
            mongocxx::uri{"mongodb://127.0.0.1:27017"}
        };

        // Seleccionar la base de datos Tecflix
        auto baseDatos = cliente["tecflix"];

        // Comprobar que MongoDB responde
        auto comando = bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("ping", 1)
        );

        auto respuesta = baseDatos.run_command(comando.view());

        std::cout << "Conexion exitosa con MongoDB!" << std::endl;

        // Iniciar el servidor HTTP
        return tecflix::run_http_service("metadata-api");

    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << std::endl;
        return 1;
    }
}
