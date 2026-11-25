#include "scene/FbxSceneLoader.h"

#include <ufbx.h>

#include <glm/geometric.hpp>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace nowave
{
namespace
{
glm::vec3 toGlm(const ufbx_vec3& value) noexcept
{
    return { static_cast<float>(value.x),
             static_cast<float>(value.y),
             static_cast<float>(value.z) };
}
} // namespace

SceneLoadResult FbxSceneLoader::loadFromFile(const std::filesystem::path& filePath)
{
    SceneLoadResult result;
    if (!std::filesystem::exists(filePath))
    {
        result.errorMessage = "File not found: " + filePath.string();
        return result;
    }

    ufbx_load_opts options {};
    options.target_axes = ufbx_axes_right_handed_y_up;
    options.target_unit_meters = 1.0;

    ufbx_error error {};
    std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> scene(
        ufbx_load_file(filePath.string().c_str(), &options, &error),
        &ufbx_free_scene);
    if (!scene)
    {
        result.errorMessage = error.description.data != nullptr
                                  ? std::string(error.description.data,
                                                error.description.length)
                                  : "Failed to parse FBX file.";
        return result;
    }

    auto mesh = std::make_shared<RoomMesh>();
    mesh->name = filePath.stem().string();
    mesh->materials.push_back(AcousticMaterial { "Default" });

    std::unordered_map<const ufbx_material*, uint32_t> materialIds;
    const uint32_t fallbackMaterialId = 0;
    const auto getMaterialId = [&](const ufbx_material* material) -> uint32_t
    {
        if (material == nullptr)
            return fallbackMaterialId;

        const auto existing = materialIds.find(material);
        if (existing != materialIds.end())
            return existing->second;

        std::string name;
        if (material->name.data != nullptr)
            name.assign(material->name.data, material->name.length);
        if (name.empty())
            name = "Material";

        const auto id = static_cast<uint32_t>(mesh->materials.size());
        mesh->materials.push_back(AcousticMaterial { std::move(name) });
        materialIds.emplace(material, id);
        return id;
    };

    for (size_t nodeIndex = 0; nodeIndex < scene->nodes.count; ++nodeIndex)
    {
        const ufbx_node* node = scene->nodes.data[nodeIndex];
        if (node == nullptr || node->mesh == nullptr)
            continue;

        const ufbx_mesh& source = *node->mesh;
        const ufbx_material_list& assignedMaterials =
            node->materials.count > 0 ? node->materials : source.materials;
        std::vector<uint32_t> triangulatedIndices(source.max_face_triangles * 3);

        for (size_t faceIndex = 0; faceIndex < source.faces.count; ++faceIndex)
        {
            const ufbx_face face = source.faces.data[faceIndex];
            const uint32_t numTriangles = ufbx_triangulate_face(
                triangulatedIndices.data(),
                triangulatedIndices.size(),
                &source,
                face);
            if (numTriangles == 0)
                continue;

            uint32_t materialId = fallbackMaterialId;
            if (faceIndex < source.face_material.count)
            {
                const uint32_t materialSlot = source.face_material.data[faceIndex];
                if (materialSlot < assignedMaterials.count)
                    materialId = getMaterialId(assignedMaterials.data[materialSlot]);
            }

            for (uint32_t triangleIndex = 0; triangleIndex < numTriangles; ++triangleIndex)
            {
                if (mesh->vertices.size() >
                    static_cast<size_t>(std::numeric_limits<uint32_t>::max()) - 3)
                {
                    result.errorMessage = "FBX file contains too many vertices.";
                    return result;
                }

                const uint32_t baseVertex = static_cast<uint32_t>(mesh->vertices.size());
                bool validTriangle = true;
                for (uint32_t corner = 0; corner < 3; ++corner)
                {
                    const uint32_t index = triangulatedIndices[triangleIndex * 3 + corner];
                    if (index >= source.num_indices)
                    {
                        validTriangle = false;
                        break;
                    }

                    const auto position = ufbx_get_vertex_vec3(&source.vertex_position, index);
                    Vertex vertex {};
                    vertex.position = toGlm(ufbx_transform_position(&node->geometry_to_world,
                                                                    position));
                    mesh->vertices.push_back(vertex);
                }

                if (!validTriangle)
                {
                    mesh->vertices.resize(baseVertex);
                    continue;
                }

                mesh->triangles.push_back(Triangle {
                    baseVertex,
                    baseVertex + 1,
                    baseVertex + 2,
                    materialId
                });
            }
        }
    }

    if (!mesh->isValid())
    {
        result.errorMessage = "FBX file contains no valid triangles.";
        return result;
    }

    mesh->recomputeNormals();
    mesh->recomputeBounds();
    result.success = true;
    result.mesh = std::move(mesh);
    return result;
}
} // namespace nowave
