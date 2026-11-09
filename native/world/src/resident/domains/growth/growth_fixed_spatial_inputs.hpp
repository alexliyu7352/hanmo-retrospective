// FSL borrows the original S1 position and transport sources. No shadow location.
#pragma once
#include "growth_fixed_service_model.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/travel/actor_spatial_history.hpp"
#include <optional>
namespace hanmo::resident {
struct GrowthFixedSpatialSources {
 const MovementExactState& movement;
 const SpatialCarrierState& carriers;
 const TransportRegistrationState& registrations;
 const SpatialCarrierSources& original;
 ActorBattleSpatialSources battle;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
 const BirthPlacementRead* births=nullptr;
 const MilitaryActionPlacementRead* military_actions=nullptr;
 // 同一 Complete 私有构造阶段一次补齐；真实 owner 和本时刻边界先于公开读口。
 const NamedMilitaryCustodyPlacementRead* custody_releases=nullptr;
 const FieldBattleExitPlacementRead* field_exits=nullptr;
 std::optional<WorldMoment> history_owner_at;
};
// 保留带实际 Field body 快照的历史 reader 时，跨 M 必须重建完整 owner。
Status validate_growth_fixed_spatial_source_moment(
 const GrowthFixedSpatialSources&,const WorldMoment& actual_now);
// Compiled original site reducer. The actual place remains the original Terms
// field; no author-supplied location reducer or extra work/efficiency rule.
ActorFoundationRuleRef growth_fixed_original_site_rule(const CatalogIdentity&);
bool growth_fixed_was_suspended(const GrowthState&,const LifeFixedServiceBoundary&);
bool growth_fixed_location_equal(const Location&,const Location&);
bool growth_fixed_location_heads_equal(const LifeFixedActorInputs&,const LifeFixedActorInputs&);
// Reprojects an unchanged immutable S1 head at this real boundary M. Continuous
// motion while suspended does not generate a boundary for every technical slice.
Status refresh_growth_fixed_spatial_projection(const CharacterConditionSources&,
 const GrowthFixedSpatialSources&,LifeFixedServiceBoundary&);
Status validate_growth_fixed_position_input_change(const CharacterConditionSources&,
 const GrowthFixedSpatialSources&,const GrowthState&,const LifeFixedServiceBoundary&,
 const LifeActualPositionInputChange&);
Outcome<bool> read_growth_fixed_actor_at_original_site(const CharacterConditionSources&,
 const GrowthFixedSpatialSources&,const GrowthState&,const LifeFixedServiceBoundary&,
 const LifeFixedActorInputs&);
// Original S1 walk, limited to this selected actor and the saved FSL cut.
Outcome<std::vector<ActorSpatialHeadChange>> read_growth_fixed_spatial_head_changes(
 const CharacterConditionSources&,const GrowthFixedSpatialSources&,
 const LifeFixedServiceBoundary& from,const LifeFixedActorInputs& actor,
 const LifeFixedServiceBoundary* terminal=nullptr);
// Called in the same real owner capture after the actor/role set is selected.
// Captures actual immutable S1 heads only; a running carrier cannot be a fixed site.
Status capture_growth_fixed_spatial_inputs_in_candidate(const CharacterConditionSources&,
 const GrowthFixedSpatialSources&,GrowthState&,const StableId& actual_boundary_id,
 const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
// Replays the saved exact same-M head and all positive supplied intervals. Later
// same-M movement cannot invalidate a prior accepted instant or backfill a span.
Status validate_growth_fixed_spatial_inputs(const CharacterConditionSources&,
 const GrowthFixedSpatialSources&,const GrowthState&);
}
