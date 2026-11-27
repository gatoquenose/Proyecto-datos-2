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
#include "tecflix/auth/JwtVerifier.hpp"
#include "tecflix/auth/SystemClock.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <nlohmann/json.hpp>
#include "MetadataRoutes.hpp"

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

        
        try {
            tecflix::metadata::ensure_indexes(baseDatos);
            std::cout << "Indices de MongoDB preparados correctamente\n";
        }
        catch (const std::exception& e) {
            std::cerr << "ERROR AL CREAR INDICES: "
                    << e.what() << std::endl;
            return 1;
        }

        // Comprobar que MongoDB responde
        auto comando = bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("ping", 1)
        );

        auto respuesta = baseDatos.run_command(comando.view());

        std::cout << "Conexion exitosa con MongoDB!" << std::endl;

        const char* secreto = std::getenv("JWT_SECRET");

        if (!secreto || !*secreto) {
            throw std::runtime_error("Falta JWT_SECRET");
        }

        tecflix::auth::SystemClock reloj;
        tecflix::auth::JwtVerifier verificador{secreto, reloj};
        
        // Iniciar el servidor HTTP
        
        auto handler = [&baseDatos, &verificador](const auto& request) {
            using namespace tecflix;

            if (auto extra = tecflix::metadata::handle_extra_routes(
                    request, baseDatos, verificador)) {
                return std::move(*extra);
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

                const auto valorHeader = request[http::field::authorization];

                std::string_view autorizacion{
                    valorHeader.data(), valorHeader.size()
                };

                constexpr std::string_view prefijo = "Bearer ";

                if (!autorizacion.starts_with(prefijo)) {
                    response.result(http::status::unauthorized);
                    response.body() = R"({"error":"Falta token Bearer"})";
                    response.prepare_payload();
                    return response;
                }

                auto verificado = verificador.verify(
                    autorizacion.substr(prefijo.size())
                );

                if (verificado.error != tecflix::auth::TokenError::None) {
                    response.result(http::status::unauthorized);
                    response.body() = R"({"error":"Token invalido o expirado"})";
                    response.prepare_payload();
                    return response;
                }
                
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
                            video.ownerId = verificado.claims.subject;
                            video.ownerUsername = verificado.claims.username;

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

            const std::string ruta(request.target().data(), request.target().size());
            const std::string prefijoGet = "/api/v1/videos/";

            if (request.method() == http::verb::get &&
                ruta.rfind(prefijoGet, 0) == 0) {

                std::string id = ruta.substr(prefijoGet.size());

                http::response<http::string_body> response{
                    http::status::ok, request.version()
                };
                response.set(http::field::content_type, "application/json");
                response.keep_alive(request.keep_alive());

                auto encabezado = request[http::field::authorization];
                std::string_view autorizacion{
                    encabezado.data(), encabezado.size()
                };

                if (autorizacion.substr(0, 7) != "Bearer ") {
                    response.result(http::status::unauthorized);
                    response.body() = R"({"error":"Falta token Bearer"})";
                } else {
                    auto verificado = verificador.verify(autorizacion.substr(7));

                    if (verificado.error != tecflix::auth::TokenError::None) {
                        response.result(http::status::unauthorized);
                        response.body() = R"({"error":"Token invalido"})";
                    } else {
                        try {
                            VideoRepository repositorio(baseDatos);
                            auto video = repositorio.buscarPorId(id);

                            if (!video) {
                                response.result(http::status::not_found);
                                response.body() = R"({"error":"Video no encontrado"})";
                            } else {
                                auto documento = video->view();
                                auto campoOwner = documento["ownerId"];

                                if (!campoOwner ||
                                    campoOwner.type() != bsoncxx::type::k_string) {
                                    response.result(http::status::forbidden);
                                    response.body() = R"({"error":"Acceso denegado"})";
                                } else {
                                    auto valor = campoOwner.get_string().value;
                                    std::string propietario(valor.data(), valor.size());

                                    if (propietario != verificado.claims.subject) {
                                        response.result(http::status::forbidden);
                                        response.body() = R"({"error":"Acceso denegado"})";
                                    } else {
                                        response.body() = bsoncxx::to_json(documento);
                                    }
                                }
                            }
                        } catch (const std::exception&) {
                            response.result(http::status::internal_server_error);
                            response.body() = R"({"error":"Error al consultar MongoDB"})";
                        }
                    }
                }

                response.prepare_payload();
                return response;
            }

            if (request.method() == http::verb::patch &&
                ruta.rfind("/api/v1/videos/", 0) == 0) {

                auto responder = [&](http::status estado, std::string cuerpo) {
                    http::response<http::string_body> respuesta{
                        estado, request.version()
                    };
                    respuesta.set(http::field::content_type, "application/json");
                    respuesta.keep_alive(request.keep_alive());
                    respuesta.body() = std::move(cuerpo);
                    respuesta.prepare_payload();
                    return respuesta;
                };

                std::string id = ruta.substr(std::string("/api/v1/videos/").size());

                bool cambiarVisibilidad = id.ends_with("/visibility");

                if (cambiarVisibilidad) {
                    id.resize(id.size() - std::string("/visibility").size());
                }

                if (id.empty() || id.find('/') != std::string::npos) {
                    return responder(http::status::bad_request,
                                    R"({"error":"ID invalido"})");
                }

                auto encabezado = request[http::field::authorization];
                std::string_view autorizacion{
                    encabezado.data(), encabezado.size()
                };

                if (!autorizacion.starts_with("Bearer ")) {
                    return responder(http::status::unauthorized,
                                    R"({"error":"Falta token Bearer"})");
                }

                auto verificado = verificador.verify(autorizacion.substr(7));

                if (verificado.error != tecflix::auth::TokenError::None) {
                    return responder(http::status::unauthorized,
                                    R"({"error":"Token invalido"})");
                }

                try {
                    VideoRepository repositorio(baseDatos);
                    auto video = repositorio.buscarPorId(id);

                    if (!video) {
                        return responder(http::status::not_found,
                                        R"({"error":"Video no encontrado"})");
                    }

                    auto propietario = video->view()["ownerId"];

                    if (!propietario ||
                        propietario.type() != bsoncxx::type::k_string) {
                        return responder(http::status::forbidden,
                                        R"({"error":"Acceso denegado"})");
                    }

                    auto valor = propietario.get_string().value;
                    std::string ownerId(valor.data(), valor.size());

                    if (ownerId != verificado.claims.subject) {
                        return responder(http::status::forbidden,
                                        R"({"error":"No eres el propietario"})");
                    }

                    if (request.body().size() > 65536) {
                        return responder(http::status::bad_request,
                                        R"({"error":"Solicitud demasiado grande"})");
                    }

                    auto datos = nlohmann::json::parse(
                        request.body(), nullptr, false
                    );

                    if (!datos.is_object() || datos.empty()) {
                        return responder(http::status::bad_request,
                                        R"({"error":"JSON invalido o vacio"})");
                    }

                    bool valido = true;

                    if (cambiarVisibilidad) {
                        valido = datos.size() == 1 &&
                                datos.contains("visibility") &&
                                datos["visibility"].is_string() &&
                                (datos["visibility"] == "public" ||
                                datos["visibility"] == "private");
                    } else {
                        for (auto it = datos.begin(); it != datos.end(); ++it) {
                            const std::string campo = it.key();
                            const auto& valorCampo = it.value();

                            if (campo == "tags") {
                                if (!valorCampo.is_array() || valorCampo.size() > 30) {
                                    valido = false;
                                    break;
                                }

                                for (const auto& tag : valorCampo) {
                                    if (!tag.is_string() ||
                                        tag.template get<std::string>().size() > 50) {
                                        valido = false;
                                    }
                                }
                                continue;
                            }

                            size_t maximo = 0;

                            if (campo == "title" || campo == "author") {
                                maximo = 200;
                            } else if (campo == "description") {
                                maximo = 5000;
                            } else if (campo == "topic") {
                                maximo = 100;
                            } else if (campo == "uploadedAt") {
                                maximo = 40;
                            } else {
                                valido = false;
                                break;
                            }

                            if (!valorCampo.is_string() ||
                                valorCampo.template get<std::string>().size() > maximo ||
                                (campo == "title" && valorCampo == "")) {
                                valido = false;
                                break;
                            }
                        }
                    }

                    if (!valido) {
                        return responder(http::status::bad_request,
                                        R"({"error":"Campos no permitidos o invalidos"})");
                    }

                    auto cambios = bsoncxx::from_json(datos.dump());

                    auto filtro = bsoncxx::builder::basic::make_document(
                        bsoncxx::builder::basic::kvp("_id", id),
                        bsoncxx::builder::basic::kvp(
                            "ownerId", verificado.claims.subject
                        )
                    );

                    auto actualizacion = bsoncxx::builder::basic::make_document(
                        bsoncxx::builder::basic::kvp("$set", cambios.view())
                    );

                    auto resultado = baseDatos["videos"].update_one(
                        filtro.view(), actualizacion.view()
                    );

                    if (!resultado || resultado->matched_count() == 0) {
                        return responder(http::status::not_found,
                                        R"({"error":"Video no encontrado"})");
                    }

                    auto actualizado = repositorio.buscarPorId(id);

                    return responder(
                        http::status::ok,
                        bsoncxx::to_json(actualizado->view())
                    );

                } catch (const std::exception&) {
                    return responder(http::status::internal_server_error,
                                    R"({"error":"Error al actualizar metadata"})");
                }
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