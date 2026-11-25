#pragma once

#include "scene/RoomMesh.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace nowave
{
enum class RoomPresetId
{
    Studio,
    ConcertHall,
    GlassAtrium,
    LivingRoom,
    ParkingGarage,
    Classroom
};

struct RoomPresetInfo
{
    RoomPresetId id;
    const char* key;
    const char* displayName;
    const char* description;
};

class RoomPresets
{
public:
    static const std::vector<RoomPresetInfo>& getAll();
    static std::optional<RoomPresetId> fromKey(std::string_view key);
    static const char* toKey(RoomPresetId id) noexcept;
    static std::shared_ptr<RoomMesh> createMesh(RoomPresetId id);
};
} // namespace nowave

