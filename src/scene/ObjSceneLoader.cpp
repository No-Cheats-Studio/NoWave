#include "scene/ObjSceneLoader.h"

#include "scene/MaterialLibrary.h"

#include <glm/geometric.hpp>
#include <tiny_obj_loader.h>

namespace nowave
{
SceneLoadResult ObjSceneLoader::loadFromFile(const std::filesystem::path& filePath)
{
    SceneLoadResult result;

    if (!std::filesystem::exists(filePath))
    {
        result.errorMessage = "File not found: " + filePath.string();
        return result;
    }

    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.triangulate = true;
    readerConfig.mtl_search_path = filePath.parent_path().string();

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filePath.string(), readerConfig))
    {
        result.errorMessage = reader.Error().empty()
                                  ? "Failed to parse OBJ file."
                                  : reader.Error();
        return result;
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& objMaterials = reader.GetMaterials();

    auto mesh = std::make_shared<RoomMesh>();
    mesh->name = filePath.stem().string();

    if (objMaterials.empty())
    {
        mesh->materials.push_back(AcousticMaterial{ "Default" });
    }
    else
    {
        mesh->materials.reserve(objMaterials.size() + 1);
        for (const auto& mat : objMaterials)
        {
            AcousticMaterial material { mat.name.empty() ? "Material" : mat.name };
            MaterialLibrary::matchByName(mat.name, material);
            mesh->materials.push_back(std::move(material));
        }
        // Fallback slot at the end for faces without an assigned material (-1).
        mesh->materials.push_back(AcousticMaterial{ "Default" });
    }

    const uint32_t fallbackMaterialId = static_cast<uint32_t>(mesh->materials.size() - 1);
    bool hasAnyNormal = false;

    for (const auto& shape : shapes)
    {
        size_t indexOffset = 0;
        const auto& numFaceVerts = shape.mesh.num_face_vertices;

        for (size_t f = 0; f < numFaceVerts.size(); ++f)
        {
            const size_t fv = static_cast<size_t>(numFaceVerts[f]);
            if (fv != 3)
            {
                indexOffset += fv;
                continue;
            }

            Triangle tri {};
            uint32_t baseVertex = static_cast<uint32_t>(mesh->vertices.size());

            for (size_t v = 0; v < 3; ++v)
            {
                const tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                Vertex vert {};

                if (idx.vertex_index >= 0)
                {
                    const size_t vi = static_cast<size_t>(idx.vertex_index) * 3;
                    if (vi + 2 < attrib.vertices.size())
                    {
                        vert.position = glm::vec3(attrib.vertices[vi],
                                                  attrib.vertices[vi + 1],
                                                  attrib.vertices[vi + 2]);
                    }
                }

                if (idx.normal_index >= 0)
                {
                    const size_t ni = static_cast<size_t>(idx.normal_index) * 3;
                    if (ni + 2 < attrib.normals.size())
                    {
                        vert.normal = glm::vec3(attrib.normals[ni],
                                                attrib.normals[ni + 1],
                                                attrib.normals[ni + 2]);
                        hasAnyNormal = true;
                    }
                }

                mesh->vertices.push_back(vert);
            }

            tri.i0 = baseVertex;
            tri.i1 = baseVertex + 1;
            tri.i2 = baseVertex + 2;

            if (f < shape.mesh.material_ids.size() && shape.mesh.material_ids[f] >= 0)
            {
                const auto matId = static_cast<uint32_t>(shape.mesh.material_ids[f]);
                tri.materialId = matId < mesh->materials.size() ? matId : fallbackMaterialId;
            }
            else
            {
                tri.materialId = fallbackMaterialId;
            }

            mesh->triangles.push_back(tri);
            indexOffset += fv;
        }
    }

    if (!mesh->isValid())
    {
        result.errorMessage = "OBJ file contains no valid triangles.";
        return result;
    }

    if (!hasAnyNormal)
        mesh->recomputeNormals();

    mesh->recomputeBounds();

    result.success = true;
    result.mesh = std::move(mesh);
    return result;
}
} // namespace nowave

