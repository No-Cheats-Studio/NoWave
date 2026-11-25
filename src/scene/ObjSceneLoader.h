#pragma once

#include "scene/RoomMesh.h"

#include <filesystem>
#include <memory>
#include <string>

namespace nowave
{
struct SceneLoadResult
{
    bool success { false };
    std::shared_ptr<RoomMesh> mesh;
    std::string errorMessage;
};

class ObjSceneLoader
{
public:
    static SceneLoadResult loadFromFile(const std::filesystem::path& filePath);
};
} // namespace nowave

