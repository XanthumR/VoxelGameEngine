#include "Gameplay/Picking.h"

#include "Gameplay/Camera.h"
#include "World/Raycast.h"
#include "World/VoxelWorld.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {

const float FIELD_OF_VIEW = 60.0f;  // Degrees; must match VoxelRenderer::Render
const float MAX_PICK_DISTANCE = 0.6f; // World units (768 voxels): past the strategy camera's max zoom

} // namespace

// Same view and projection as VoxelRenderer::Render
static void CameraMatrices(const ICamera& camera, glm::ivec2 windowSize, glm::mat4& view, glm::mat4& projection) {
    glm::vec3 position = camera.Position();
    view = glm::lookAt(position, position + camera.Front(), camera.Up());
    projection = glm::perspective(glm::radians(FIELD_OF_VIEW), (float)windowSize.x / (float)windowSize.y, 0.1f, 100.0f);
}

glm::vec3 ScreenToWorldDirection(const ICamera& camera, glm::vec2 cursor, glm::ivec2 windowSize) {
    glm::mat4 view, projection;
    CameraMatrices(camera, windowSize, view, projection);

    // Same as cameraRay in shaders/include/camera.glsl; window y goes down, GL y goes up
    glm::vec2 ndc(cursor.x / (float)windowSize.x * 2.0f - 1.0f, 1.0f - cursor.y / (float)windowSize.y * 2.0f);
    glm::vec4 target = glm::inverse(projection) * glm::vec4(ndc, 1.0f, 1.0f);
    glm::vec3 viewDirection = glm::normalize(glm::vec3(target) / target.w);
    return glm::normalize(glm::vec3(glm::inverse(view) * glm::vec4(viewDirection, 0.0f)));
}

bool WorldToScreen(const ICamera& camera, glm::vec3 worldPosition, glm::ivec2 windowSize, glm::vec2& screen) {
    if (windowSize.x <= 0 || windowSize.y <= 0) return false;
    glm::mat4 view, projection;
    CameraMatrices(camera, windowSize, view, projection);
    glm::vec4 clip = projection * view * glm::vec4(worldPosition, 1.0f);
    if (clip.w <= 0.0f) return false;
    glm::vec2 ndc = glm::vec2(clip) / clip.w;
    screen = glm::vec2((ndc.x * 0.5f + 0.5f) * (float)windowSize.x, (0.5f - ndc.y * 0.5f) * (float)windowSize.y);
    return true;
}

PickResult PickUnderCursor(const ICamera& camera, glm::vec2 cursor, glm::ivec2 windowSize, const VoxelWorld& world) {
    PickResult result;
    if (windowSize.x <= 0 || windowSize.y <= 0) return result;

    glm::vec3 direction = ScreenToWorldDirection(camera, cursor, windowSize);
    RayHit hit = Raycast(world, camera.Position(), direction, MAX_PICK_DISTANCE, true);
    if (!hit.hit) return result;

    result.hit = true;
    result.voxel = hit.mapPos;
    result.normal = hit.normal;
    result.block = world.GetVoxel(hit.mapPos.x, hit.mapPos.y, hit.mapPos.z);
    return result;
}
