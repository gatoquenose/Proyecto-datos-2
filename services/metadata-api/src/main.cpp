#include "tecflix_http.hpp"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <iostream>
#include <exception>
#include "VideoRepository.hpp"
#include <bsoncxx/oid.hpp>
#include <bsoncxx/json.hpp>

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
        
        auto handler = [&baseDatos](const auto& request) {
            using namespace tecflix;


            const std::string ruta(
                request.target().data(),
                request.target().size()
            );

            const std::string prefijo = "/api/v1/videos/";

            if (request.method() == http::verb::get &&
                ruta.rfind(prefijo, 0) == 0) {

                std::string id = ruta.substr(prefijo.size());

                http::response<http::string_body> response{
                    http::status::ok,
                    request.version()
                };

                response.set(
                    http::field::content_type,
                    "application/json"
                );

                response.keep_alive(request.keep_alive());

                try {
                    VideoRepository repositorio(baseDatos);
                    auto video = repositorio.buscarPorId(id);

                    if (video) {
                        response.body() = bsoncxx::to_json(video->view());
                    } else {
                        response.result(http::status::not_found);
                        response.body() = R"({"error":"Video no encontrado"})";
                    }

                } catch (const std::exception&) {
                    response.result(http::status::internal_server_error);
                    response.body() = R"({"error":"Error al consultar MongoDB"})";
                }

                response.prepare_payload();
                return response;
            }

            if (request.method() == http::verb::post &&
                request.target() == "/api/v1/videos") {

                http::response<http::string_body> response{
                    http::status::not_implemented,
                    request.version()
                };

                response.set(
                    http::field::content_type,
                    "application/json"
                );

                response.keep_alive(request.keep_alive());
                
                try {
                    // Convertir el JSON recibido a BSON
                    auto documento = bsoncxx::from_json(request.body());

                    // Buscar el titulo del video
                    auto titulo = documento.view()["title"];

                    if (!titulo ||
                        titulo.type() != bsoncxx::type::k_string ||
                        titulo.get_string().value.empty()) {

                        response.result(http::status::bad_request);
                        response.body() =
                            R"({"error":"El titulo es obligatorio"})";

                    } else {
                        try {
                            Video video;

                            video.id = bsoncxx::oid{}.to_string();
                            video.ownerId = "dev-test";
                            video.ownerUsername = "dev-test";

                            auto valor = titulo.get_string().value;
                            video.title = std::string(valor.data(), valor.size());

                            video.visibility = "private";
                            video.status = "processing";

                            VideoRepository repositorio(baseDatos);
                            repositorio.guardar(video);

                            response.result(http::status::created);
                            response.body() =
                                std::string("{\"id\":\"") + video.id +
                                "\",\"message\":\"Video guardado\"}";

                        } catch (const std::exception& error) {
                            std::cerr << "Error al guardar: "
                                    << error.what() << std::endl;

                            response.result(http::status::internal_server_error);
                            response.body() =
                                R"({"error":"No se pudo guardar el video"})";
                        }
                    }


                } catch (const std::exception&) {
                    response.result(http::status::bad_request);
                    response.body() =
                        R"({"error":"JSON invalido"})";
                }


                response.prepare_payload();
                return response;
            }

            return response_for(request);
        };

        return tecflix::run_http_service(
            "metadata-api",
            8082,
            handler
        );


    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << std::endl;
        return 1;
    }
}
