// Exact S1 group contact remains an exact source, not a forged legacy action.
#pragma once
#include "field_time.hpp"
#include "hanmo/resident/world_moment.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryState;
struct MilitaryCatalog;
struct MilitaryAccess;
struct FieldBattleWorldState;
struct FieldBattleCatalog;
struct TravelCatalog;
class TravelServices;
struct GroupMovementContext;
class AuthoredInteractionFlags;
struct InteractionState;
struct MovementExactState;
struct SpatialCarrierState;
struct GroupMovementExecutionState;
struct CharacterConditionSources;
class FieldSegmentApplicationRead;
class MilitaryFormationSourceRead;
class ExactTrainingPeriodRead;
struct MilitaryMarchAuthorSelection;
struct ArmyRecord;
struct GroupMovementJourney;
struct GroupMovementOpeningSource;
struct MilitaryGroupOrderAcceptance;
struct MovementUnitCloseSource;
struct RoadContactEndpoint;
struct ExactMovementContact;
struct RoadMovementBasis;
struct ExactContactMotionWitness;
struct FieldOriginalRosterReadContext;
class GroupContactMemberReleaseRead;
class MilitaryFiniteReturnSourceRead;
struct FieldExactContactReadContext {
  const CommonStore &common;
  const MilitaryState &military;
  const MilitaryCatalog &military_catalog;
  const FieldBattleWorldState &field;
  const TravelCatalog &travel;
  const MovementExactState &movement;
  const SpatialCarrierState &carriers;
  const GroupMovementExecutionState &groups;
  const CharacterConditionSources &conditions;
  const FieldSegmentApplicationRead &clock;
  const MilitaryFormationSourceRead &formations;
  const MilitaryMarchAuthorSelection &author;
  // Actual hot continuation inputs, never a substitute for historical proof.
  const TravelServices *travel_services = nullptr;
  const GroupMovementContext *group_context = nullptr;
  // Only the explicit native no-flag-writer selection supplies these roots.
  const AuthoredInteractionFlags *original_flags = nullptr;
  const InteractionState *original_interaction = nullptr;
  const ExactTrainingPeriodRead *periods = nullptr;
  // 仅借同World原军事owner；聚落控制沿原占领链/cut读，不新选政治接管。
  const MilitaryAccess *original_military_access = nullptr;
  // 援军冷续行只借同World原名册/期首owner，不在读口构造替身。
  const FieldOriginalRosterReadContext *original_rosters = nullptr;
  // 冷参战历史借既有Field成员交接owner，不要求热GroupMovementContext存在。
  const GroupContactMemberReleaseRead *original_member_releases = nullptr;
  // 同根有限返程原来源，冷读不借当前office或伪造新CampaignDepart。
  const MilitaryFiniteReturnSourceRead *original_finite_returns = nullptr;
};
struct FieldExactRoadEndpointRead {
  const ArmyRecord *army = nullptr;
  const StableId *faction_id = nullptr;
  const GroupMovementJourney *journey = nullptr;
  const GroupMovementOpeningSource *opening = nullptr;
  const MilitaryGroupOrderAcceptance *order = nullptr;
  const MovementUnitCloseSource *closed = nullptr;
  const RoadContactEndpoint *geometry_endpoint = nullptr;
  const RoadMovementBasis *original_basis = nullptr;
  const ExactContactMotionWitness *original_mapping = nullptr;
};
struct FieldExactRoadContactRead {
  const ExactMovementContact *contact = nullptr;
  std::array<FieldExactRoadEndpointRead, 2> endpoints;
  FieldBattleTime field_at;
};
// Both current installation and cold callers borrow the same actual contact,
// original finite military acceptance and retained close. This grants no start
// permission and never assumes P3 hostility.
Outcome<FieldExactRoadContactRead> read_field_exact_road_contact_source(
    const FieldExactContactReadContext &, const SourceRef &actual_contact,
    const FieldBattleParameters &, ExactArithmeticBudget = {});
Outcome<GeneratedBattleField> regenerate_field_exact_road_battle(
    const FieldExactContactReadContext &, const SourceRef &actual_contact,
    const FieldBattleCatalog &, ExactArithmeticBudget = {});

} // namespace hanmo::resident
