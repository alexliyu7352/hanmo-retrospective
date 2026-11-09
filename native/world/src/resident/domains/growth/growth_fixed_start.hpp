#pragma once
#include "growth_fixed_control_inputs.hpp"
#include "growth_fixed_spatial_inputs.hpp"
#include "study_service_sources.hpp"
namespace hanmo::resident {
struct CommitResult;
struct InteractionState;
struct InteractionCatalog;
class GrowthStartedParticipationWrite;
struct GrowthMarketExactSources;
struct GrowthFixedCaptureServices {
  const CurrentReadStampRead &reads;
  const ReadStampAuthority &authority;
  const DomainReadVersionSource &versions;
  const GrowthAccess &original;
  GrowthStartedParticipationWrite *participation = nullptr;
  const GrowthMarketExactSources *market_exact = nullptr;
};
// Same successful original submit tail, after the real Common result/history is
// bound. Inputs are read from the current candidate; no caller-provided
// snapshot. Any failure rejects that entire private candidate, including the
// original fees.
Outcome<StableId> bind_growth_fixed_started_command_in_candidate(
    const GrowthState &before, CommonStore &, GrowthState &,
    const CharacterConditionSources &, ActionElapsedStore &,
    const GrowthFixedCaptureServices &, const StableId &actual_actor,
    Integer actual_request_seq, const CommitResult &actual_result);
// Original successful start effect, before Director adoption/FSL exists. The
// caller supplies the actual candidate moment; this never reads a route or FSL.
Outcome<MomentOwnerSource> read_growth_started_business_source(
    const GrowthState &before, const GrowthState &after, const CommonStore &,
    const ActionCoreView &, const WorldMoment &actual_moment);
} // namespace hanmo::resident

namespace hanmo::resident {
// Borrowed cold identity only. The original Started boundary supplies exact M;
// this does not validate its route/inputs or recurse into Director adoption.
struct GrowthStartedBusinessIdentity {
  const StableId *actor_id = nullptr;
  const StableId *action_kind = nullptr;
  const LifeFixedServiceBoundary *actual_start = nullptr;
  const LifeActualCommand *actual_command = nullptr;
  const GrowthAction *original_action = nullptr;
};
Outcome<GrowthStartedBusinessIdentity> read_growth_started_business_identity(
    const CommonStore &, const GrowthState &, const MomentOwnerSource &,
    const WorldMoment &actual_now,
    const InteractionState *interaction = nullptr,
    const InteractionCatalog *interaction_catalog = nullptr);
} // namespace hanmo::resident

namespace hanmo::resident {
// Original Growth term proof over the acyclic immutable S3 confirmation leaf.
// It neither calls the FSL chain nor re-executes/re-decodes the command.
Status validate_growth_dialogue_confirmation(
    const CommonStore &, const GrowthState &, const InteractionState *,
    const InteractionCatalog *, const StableId &actual_result_id,
    Integer actual_request_seq, const StableId &actor,
    const StableId &action_kind, const StableId &action_id,
    const WorldMoment &actual_started_at);
} // namespace hanmo::resident
