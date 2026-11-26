#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

struct Video {
    std::string id;
    std::string ownerId;
    std::string ownerUsername;

    std::string title;
    std::string description;
    std::string author;
    std::string topic;
    std::vector<std::string> tags;

    std::string visibility;
    std::string status = "processing";

    int64_t sizeBytes = 0;
    int durationSeconds = 0;

    std::string uploadedAt;
    std::string createdAt;
    std::string updatedAt;

    std::optional<std::string> deletedAt;
};
