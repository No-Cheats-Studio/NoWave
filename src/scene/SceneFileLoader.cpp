#include "scene/SceneFileLoader.h"

#include "scene/FbxSceneLoader.h"

#include <algorithm>
#include <cctype>

namespace nowave
{
SceneLoadResult SceneFileLoader::loadFromFile(const std::filesystem::path& filePath)
{
    auto extension = filePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::tolower(character));
                   });

    if (extension == ".obj")
        return ObjSceneLoader::loadFromFile(filePath);
    if (extension == ".fbx")
        return FbxSceneLoader::loadFromFile(filePath);

    return { false, nullptr, "Unsupported room file format: " + extension };
}
} // namespace nowave
