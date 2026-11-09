#pragma once
#include "dynamic_session.hpp"
#include "military_roster_history.hpp"
#include "resident/domains/travel/actor_spatial_read.hpp"
namespace hanmo::resident {
// Borrowed view of the one actual S2 body. Coordinates are exact chart values,
// not a new MovementAnchor branch and not a second persistent Actor location.
struct MilitaryActorSpatialPointView {
 StableId actor_id,session_ref;Integer generation=0;SpatialCarrierRef carrier;
 MilitaryLocalBodyRef body;
 const MilitarySpatialHandoff2* actual_handoff=nullptr;
 const DynamicMilitarySessionCurrent2* actual_session=nullptr;
 const LocalHexMotionBasis2* actual_basis=nullptr;
 const LocalMotionCursor2* actual_cursor=nullptr;
 const MilitarySpatialChart2* actual_chart=nullptr;
 MilitarySpatialPoint local_position,world_position;WorldMoment observed_at;
 std::vector<SourceRef> original_sources;
 std::vector<MomentOwnerSource> known_source_schemas;
};
// Schema dispatch belongs to the actual source owner. SourceRef alone cannot
// infer a version for accepted intents, capability or external entry snapshots.
class MilitaryActorSpatialSourceSchemaRead {
public:
 virtual ~MilitaryActorSpatialSourceSchemaRead()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryExactState&)const noexcept=0;
 virtual Outcome<Integer> read_actual_spatial_source_schema(const SourceRef&)const=0;
};
struct MilitaryActorSpatialPointContext {
 const CommonStore& common;const MilitaryState& military;const MilitaryCatalog& catalog;
 const MilitaryExactState& exact;const MovementExactState& movement;
 const DynamicMilitarySessionOriginalRead& sessions;
 const MilitarySpatialOriginalSourceRead& handoffs;
 const MilitaryLocalMotionOriginalRead& local_motion;
 const CurrentMomentRead& clock;
 const PopulationMilitaryState* population=nullptr;
 const PopulationMilitaryCatalog* population_catalog=nullptr;
 ExactArithmeticBudget budget{};
 const MilitaryActorSpatialSourceSchemaRead* source_schemas=nullptr;
    const MilitaryFormationSourceRead* formations=nullptr;
 const MilitaryReturnSourceRead* returns=nullptr;
 const ExactTrainingPeriodRead* periods=nullptr;
};
// Only a named actor proved to belong to exactly one real original land unit is
// selected. An Army leader is not assigned to the first unit; vessel placement
// needs its original aboard/custody source and is explicitly outside this read.
Outcome<MilitaryActorSpatialPointView> read_actual_military_actor_spatial_point(
 const MilitaryActorSpatialPointContext&,const StableId& actor_id,
 const BattleMovementWriter& actual_writer);
class NativeBattleActorSpatialRead final:public BattleActorSpatialRead {
 MilitaryActorSpatialPointContext context_;
public:
 explicit NativeBattleActorSpatialRead(MilitaryActorSpatialPointContext x):context_(x){}
 bool bound_to(const CommonRead& c,const MovementExactState& m)const noexcept override{return &c==&context_.common&&&m==&context_.movement;}
 Outcome<ActorSpatialLocationView> read_battle_actor_location(const StableId&,const BattleMovementWriter&)const override;
};
}
