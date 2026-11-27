#pragma once
#include "video.hpp"
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <optional>
#include <bsoncxx/document/value.hpp>

class VideoRepository {
public:
    explicit VideoRepository(mongocxx::database database);

    void guardar(const Video& video);

    std::optional<bsoncxx::document::value> buscarPorId(const std::string& id);

private:
    mongocxx::collection videos_;
};
