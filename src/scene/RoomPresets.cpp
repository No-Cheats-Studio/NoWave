#include "scene/RoomPresets.h"

#include <array>
#include <functional>
#include <glm/geometric.hpp>

namespace nowave
{
namespace
{
enum MaterialSlot : uint32_t
{
    Stone = 0,
    Glass,
    Brick,
    Wood,
    Curtain,
    MineralWool
};

std::vector<AcousticMaterial> makeMaterials()
{
    return {
        { "Polished Stone", 0.02f, 0.03f, 0.04f, 0.03f, SurfaceTexture::Stone },
        { "Glass", 0.01f, 0.02f, 0.02f, 0.02f, SurfaceTexture::Glass },
        { "Rough Brick", 0.03f, 0.05f, 0.08f, 0.55f, SurfaceTexture::Brick },
        { "Wood Panel / Parquet", 0.10f, 0.12f, 0.18f, 0.20f, SurfaceTexture::Wood },
        { "Heavy Velour Curtain", 0.10f, 0.45f, 0.75f, 0.65f, SurfaceTexture::Curtain },
        { "Mineral Wool (100 mm)", 0.25f, 0.88f, 0.97f, 0.35f, SurfaceTexture::MineralWool }
    };
}

void addQuadInward(RoomMesh& mesh,
                   const glm::vec3& p0,
                   const glm::vec3& p1,
                   const glm::vec3& p2,
                   const glm::vec3& p3,
                   uint32_t materialId)
{
    const glm::vec3 edge1 = p1 - p0;
    const glm::vec3 edge2 = p2 - p0;
    const glm::vec3 n = glm::normalize(glm::cross(edge1, edge2));

    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(Vertex{ p0, n });
    mesh.vertices.push_back(Vertex{ p1, n });
    mesh.vertices.push_back(Vertex{ p2, n });
    mesh.vertices.push_back(Vertex{ p3, n });

    mesh.triangles.push_back(Triangle{ base, base + 1, base + 2, materialId });
    mesh.triangles.push_back(Triangle{ base, base + 2, base + 3, materialId });
}

glm::vec3 quadPoint(const glm::vec3& p0,
                    const glm::vec3& p1,
                    const glm::vec3& p2,
                    const glm::vec3& p3,
                    float u,
                    float v)
{
    return glm::mix(glm::mix(p0, p1, u), glm::mix(p3, p2, u), v);
}

template <typename MaterialForCell>
void addQuadGrid(RoomMesh& mesh,
                 const glm::vec3& p0,
                 const glm::vec3& p1,
                 const glm::vec3& p2,
                 const glm::vec3& p3,
                 int columns,
                 int rows,
                 MaterialForCell materialForCell)
{
    for (int row = 0; row < rows; ++row)
    {
        const float v0 = static_cast<float>(row) / static_cast<float>(rows);
        const float v1 = static_cast<float>(row + 1) / static_cast<float>(rows);
        for (int column = 0; column < columns; ++column)
        {
            const float u0 = static_cast<float>(column) / static_cast<float>(columns);
            const float u1 = static_cast<float>(column + 1) / static_cast<float>(columns);
            addQuadInward(mesh,
                          quadPoint(p0, p1, p2, p3, u0, v0),
                          quadPoint(p0, p1, p2, p3, u1, v0),
                          quadPoint(p0, p1, p2, p3, u1, v1),
                          quadPoint(p0, p1, p2, p3, u0, v1),
                          materialForCell(row, column));
        }
    }
}

template <typename WallMaterial>
std::shared_ptr<RoomMesh> buildBoxRoom(const std::string& name,
                                       float width,
                                       float height,
                                       float depth,
                                       WallMaterial wallMaterial)
{
    auto mesh = std::make_shared<RoomMesh>();
    mesh->name = name;
    mesh->materials = makeMaterials();

    const float hx = width * 0.5f;
    const float hz = depth * 0.5f;

    const glm::vec3 b00(-hx, 0.0f, -hz);
    const glm::vec3 b10( hx, 0.0f, -hz);
    const glm::vec3 b11( hx, 0.0f,  hz);
    const glm::vec3 b01(-hx, 0.0f,  hz);

    const glm::vec3 t00(-hx, height, -hz);
    const glm::vec3 t10( hx, height, -hz);
    const glm::vec3 t11( hx, height,  hz);
    const glm::vec3 t01(-hx, height,  hz);

    addQuadGrid(*mesh, b00, b01, b11, b10, 6, 4,
                [](int, int) { return static_cast<uint32_t>(Wood); });
    addQuadGrid(*mesh, t00, t10, t11, t01, 6, 4,
                [](int row, int column)
                {
                    return static_cast<uint32_t>((row + column) % 2 == 0
                                                     ? MineralWool
                                                     : Stone);
                });

    const std::array<std::array<glm::vec3, 4>, 4> walls {{
        {{ b00, b10, t10, t00 }},
        {{ b11, b01, t01, t11 }},
        {{ b01, b00, t00, t01 }},
        {{ b10, b11, t11, t10 }}
    }};
    for (int face = 0; face < static_cast<int>(walls.size()); ++face)
    {
        const auto& wall = walls[static_cast<size_t>(face)];
        addQuadGrid(*mesh, wall[0], wall[1], wall[2], wall[3], 8, 6,
                    [&](int row, int column)
                    {
                        return wallMaterial(face, row, column);
                    });
    }

    mesh->recomputeBounds();
    return mesh;
}
} // namespace

const std::vector<RoomPresetInfo>& RoomPresets::getAll()
{
    static const std::vector<RoomPresetInfo> presets = {
        { RoomPresetId::Studio, "studio", "Treated Studio",
          "6m x 3m x 5.5m - mineral wool, brick, wood and velour" },
        { RoomPresetId::ConcertHall, "concert_hall", "Concert Hall",
          "22m x 12m x 36m - parquet, timber, stone and curtains" },
        { RoomPresetId::GlassAtrium, "glass_atrium", "Glass Atrium",
          "18m x 11m x 28m - glass, stone, brick and absorbers" }
    };
    return presets;
}

std::optional<RoomPresetId> RoomPresets::fromKey(std::string_view key)
{
    if (key == "small_room")
        return RoomPresetId::Studio;
    if (key == "cathedral")
        return RoomPresetId::GlassAtrium;

    for (const auto& info : getAll())
        if (key == info.key)
            return info.id;
    return std::nullopt;
}

const char* RoomPresets::toKey(RoomPresetId id) noexcept
{
    switch (id)
    {
        case RoomPresetId::Studio: return "studio";
        case RoomPresetId::ConcertHall: return "concert_hall";
        case RoomPresetId::GlassAtrium: return "glass_atrium";
    }
    return "studio";
}

std::shared_ptr<RoomMesh> RoomPresets::createMesh(RoomPresetId id)
{
    switch (id)
    {
        case RoomPresetId::Studio:
            return buildBoxRoom("Treated Studio", 6.0f, 3.0f, 5.5f,
                                [](int face, int row, int column)
                                {
                                    if (face == 0 && row >= 1 && row <= 4 &&
                                        column >= 2 && column <= 5)
                                        return static_cast<uint32_t>(Curtain);
                                    if ((row == 1 || row == 3) &&
                                        (column + face) % 3 != 0)
                                        return static_cast<uint32_t>(MineralWool);
                                    if (row == 0)
                                        return static_cast<uint32_t>(Wood);
                                    return static_cast<uint32_t>(Brick);
                                });

        case RoomPresetId::ConcertHall:
            return buildBoxRoom("Concert Hall", 22.0f, 12.0f, 36.0f,
                                [](int face, int row, int column)
                                {
                                    if ((face == 0 || face == 1) && row >= 2 &&
                                        column >= 2 && column <= 5)
                                        return static_cast<uint32_t>(Curtain);
                                    if ((row == 0 || row == 1) && column % 2 == 0)
                                        return static_cast<uint32_t>(Brick);
                                    if (row == 5)
                                        return static_cast<uint32_t>(Stone);
                                    return static_cast<uint32_t>(Wood);
                                });

        case RoomPresetId::GlassAtrium:
            return buildBoxRoom("Glass Atrium", 18.0f, 11.0f, 28.0f,
                                [](int face, int row, int column)
                                {
                                    if (face < 2)
                                    {
                                        if ((row == 1 || row == 4) &&
                                            column >= 1 && column <= 6)
                                            return static_cast<uint32_t>(MineralWool);
                                        if (row == 0)
                                            return static_cast<uint32_t>(Brick);
                                        return static_cast<uint32_t>(Glass);
                                    }
                                    if (row == 1 && column % 2 == 0)
                                        return static_cast<uint32_t>(Curtain);
                                    if (row == 0)
                                        return static_cast<uint32_t>(Wood);
                                    return static_cast<uint32_t>(Stone);
                                });
    }

    return buildBoxRoom("Treated Studio", 6.0f, 3.0f, 5.5f,
                        [](int, int, int) { return static_cast<uint32_t>(MineralWool); });
}
} // namespace nowave
