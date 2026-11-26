#include "VideoRepository.hpp"
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <stdexcept>
#include <optional>
#include <utility>

using bsoncxx::builder::basic::kvp;

VideoRepository::VideoRepository(mongocxx::database database)
    : videos_(database["videos"]) {
}

void VideoRepository::guardar(const Video& video) {

    bsoncxx::builder::basic::array etiquetas;

    for (const auto& tag : video.tags) {
        etiquetas.append(tag);
    }

    bsoncxx::builder::basic::document documento;

    documento.append(
        kvp("_id", video.id),
        kvp("ownerId", video.ownerId),
        kvp("ownerUsername", video.ownerUsername),
        kvp("title", video.title),
        kvp("description", video.description),
        kvp("author", video.author),
        kvp("topic", video.topic),
        kvp("tags", etiquetas.extract()),
        kvp("visibility", video.visibility),
        kvp("status", video.status),
        kvp("sizeBytes", video.sizeBytes),
        kvp("durationSeconds", video.durationSeconds),
        kvp("uploadedAt", video.uploadedAt),
        kvp("createdAt", video.createdAt),
        kvp("updatedAt", video.updatedAt)
    );

    if (video.deletedAt.has_value()) {
        documento.append(kvp("deletedAt", *video.deletedAt));
    }

    auto resultado = videos_.insert_one(documento.view());

    if (!resultado) {
        throw std::runtime_error(
            "No se pudo guardar el video en MongoDB"
        );
    }
}

std::optional<bsoncxx::document::value>
VideoRepository::buscarPorId(const std::string& id) {

    auto filtro = bsoncxx::builder::basic::make_document(
        kvp("_id", id)
    );

    auto resultado = videos_.find_one(filtro.view());

    if (resultado) {
        return std::move(*resultado);
    }

    return std::nullopt;
}