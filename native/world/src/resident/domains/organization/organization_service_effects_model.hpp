// Explicit organization_service_effects/1 adjunct; owned only by its original fact.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct OrganizationRoleEnabledChange { bool before=false,after=false; };
struct OrganizationServiceEffect {
    StableId duty_id,role_id,actor_id;
    bool duty_active_before=false,duty_active_after=false;
    std::optional<OrganizationRoleEnabledChange> role_enabled_change;
};
struct OrganizationServiceEffects { std::vector<OrganizationServiceEffect> changes; };
} // namespace hanmo::resident
