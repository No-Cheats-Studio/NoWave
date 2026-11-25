#pragma once

#include "scene/ObjSceneLoader.h"

#include <filesystem>

namespace nowave
{
class FbxSceneLoader
{
public:
    static SceneLoadResult loadFromFile(const std::filesystem::path& filePath);
};
} // namespace nowave
