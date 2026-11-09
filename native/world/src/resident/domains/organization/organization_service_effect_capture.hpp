#pragma once
#include "organization_model.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <optional>
#include <string_view>

namespace hanmo::resident {
struct OrganizationServiceEffectBuildView {
    std::string_view duty_id,role_id,actor_id;
    bool duty_active_before=false,duty_active_after=false;
    std::optional<OrganizationRoleEnabledChange> role_enabled_change;
};
struct OrganizationServiceEffectsBuildView {
    CheckpointVector<OrganizationServiceEffectBuildView> changes;
};

}
