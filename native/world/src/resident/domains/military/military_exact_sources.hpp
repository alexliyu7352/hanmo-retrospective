#include "resident/domains/military/military_refs.hpp"
// 原military来源与动态完整来源显式联合；同id不得两份payload。
#pragma once
#include "resident/domains/military/battle_world_model.hpp"
#include "dynamic_session_model.hpp"
#include "dynamic_action_model.hpp"
#include "dynamic_effect_model.hpp"
#include "dynamic_objective_model.hpp"
#include "military_tactical_ownership_model.hpp"
#include "duty/military_duty_contract.hpp"
#include "military_spatial_handoff.hpp"
#include "resident/domains/travel/movement_result_read.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
class MilitaryFormationSourceRead;class MilitaryReturnSourceRead;
// Source-local real Formation/Return dependencies for named commander results.
// actual_now is the same trusted observation used by the calling original read.
// This context does not call roster, spatial, body or whole-world validation.
// Only callers with an independently proved actual prefix select the new mode.
// Existing aggregate contexts retain the original public source capability.
enum class MilitaryCasualtyHistoryOrder { legacy_serial, actual_common_prefix };
struct MilitaryCasualtyHistoryContext {
 const CommonStore& common;WorldMoment actual_now;
 const MilitaryFormationSourceRead* formations=nullptr;
 const MilitaryReturnSourceRead* returns=nullptr;
 MilitaryCasualtyHistoryOrder ordering=MilitaryCasualtyHistoryOrder::legacy_serial;
};
struct MilitaryMemberEffectLink { StableId actor_id; Integer effect_index=0; StableId purpose; SourceRef effect_source; };
struct DynamicMilitaryBattleFact2 { StableId id,session_ref; WorldMoment effective_at,started_at; StableId effect_group_ref; std::vector<StableId> army_ids,unit_ids; std::optional<MilitaryBattleResult> result; SourceRef source_ref,contact_source_ref; std::optional<Integer> winner_side; bool final=false; };
struct DynamicMilitaryCasualtyFact2 { StableId id; WorldMoment effective_at; StableId session_ref,effect_group_ref,original_casualty_rule_ref; std::vector<MilitaryRosterReceipt> roster_receipts; std::vector<MilitaryActorChange> actor_changes; std::vector<MilitaryMemberEffectLink> member_effect_links; };
struct MilitarySupplyEffectSource2 { SourceRef original_supply_fact_ref; StableId period_ref; SourceRef opening_source_ref; WorldMoment occurred_at; std::vector<StableId> selected_named_actor_ids; Integer shortage_value=0; StableId original_rule_ref; };
// 此值最终落dynamic_military/2同名数组，旧military/B不追加精确行。
struct MilitaryExactState {
 // Original named children, emitted only by the explicit /3 selector. /2 bytes
 // remain unchanged and must reject nonempty /3 ownership instead of dropping it.
 OrderedTable<MilitaryTacticalFormation3> formations;
 OrderedTable<MilitaryTacticalMissionRecord3> missions;
 OrderedTable<DynamicMilitaryAction2> actions;OrderedTable<DynamicMilitarySessionCurrent2> session_current_states;OrderedTable<DynamicSessionStateSource2> session_state_sources;MilitaryLocalMotionState local_motion;MilitarySpatialHandoffState spatial_handoffs;OrderedTable<DynamicBattleEffectGroup2> effect_groups;OrderedTable<DynamicMilitaryBattleFact2> dynamic_battle_facts; OrderedTable<DynamicMilitaryCasualtyFact2> dynamic_casualty_facts; OrderedTable<MilitarySupplyEffectSource2> supply_effect_sources;
 // The original Duty owner is embedded once; no separate World/seed duty copy.
 MilitaryDutyState duty_sources;
 OrderedTable<DynamicMilitaryObjectiveEvidence2> objective_evidence; };

struct MilitaryFatalConsequenceBuildView {
 const StableId& actor_id;const StableId& casualty_id;const StableId& battle_id;
 const WorldMoment& occurred_at;const Integer& source_ordinal;
};

struct MilitaryFatalConsequence {
 CheckpointMemoryCharge copied_fields;
 StableId actor_id,casualty_id,battle_id;WorldMoment occurred_at;Integer source_ordinal=0;
 MilitaryFatalConsequence()=default;
 explicit MilitaryFatalConsequence(const MilitaryFatalConsequenceBuildView&);
 MilitaryFatalConsequence(const MilitaryFatalConsequence&)=default;
 MilitaryFatalConsequence(MilitaryFatalConsequence&& other):MilitaryFatalConsequence(static_cast<const MilitaryFatalConsequence&>(other)){}
 MilitaryFatalConsequence& operator=(const MilitaryFatalConsequence&)=delete;
 MilitaryFatalConsequence& operator=(MilitaryFatalConsequence&&)=delete;
};
struct MilitaryCasualtySourceBuildView {
 const SourceRef& source_ref;const Integer& schema;
 const StableId& session_ref;const StableId& battle_id;const WorldMoment& occurred_at;
 const std::vector<MilitaryRosterReceipt>& roster_receipts;
 const std::vector<MilitaryActorChange>& actor_changes;
 const std::vector<MilitaryMemberEffectLink>& member_effect_links;
};

struct MilitaryCasualtySourceRead {
 CheckpointMemoryCharge copied_fields;
 SourceRef source_ref;Integer schema=0;StableId session_ref,battle_id;WorldMoment occurred_at;
 std::vector<MilitaryRosterReceipt> roster_receipts;std::vector<MilitaryActorChange> actor_changes;
 std::vector<MilitaryMemberEffectLink> member_effect_links;
 MilitaryCasualtySourceRead()=default;
 explicit MilitaryCasualtySourceRead(const MilitaryCasualtySourceBuildView&);
 MilitaryCasualtySourceRead(const MilitaryCasualtySourceRead&)=default;
 MilitaryCasualtySourceRead(MilitaryCasualtySourceRead&& other):MilitaryCasualtySourceRead(static_cast<const MilitaryCasualtySourceRead&>(other)){}
 MilitaryCasualtySourceRead& operator=(const MilitaryCasualtySourceRead&)=delete;
 MilitaryCasualtySourceRead& operator=(MilitaryCasualtySourceRead&&)=delete;
};
// A single actual consequence is read through the same complete borrowed validator.
// Legacy /1 has no effect-group proof and is explicitly rejected by this /2 reader.
struct MilitaryActorConsequenceBuildView {
    const StableId& actor_id;const StableId& status;const StableId& casualty_id;
    const StableId& battle_id;const StableId& original_casualty_rule_ref;
    const WorldMoment& occurred_at;const Integer& source_ordinal;const SourceRef& effect_source;
};


struct MilitaryActorConsequence {
    // This original read result owns its first copy, before downstream S4
    // selection makes its separately charged derived rule/effect payload.
    CheckpointMemoryCharge copied_fields;
    StableId actor_id,status,casualty_id,battle_id,original_casualty_rule_ref;
    WorldMoment occurred_at;Integer source_ordinal=0;SourceRef effect_source;
    MilitaryActorConsequence()=default;
    explicit MilitaryActorConsequence(const MilitaryActorConsequenceBuildView&);
    MilitaryActorConsequence(const MilitaryActorConsequence&)=default;
    MilitaryActorConsequence(MilitaryActorConsequence&& other)
        :MilitaryActorConsequence(static_cast<const MilitaryActorConsequence&>(other)){}
    MilitaryActorConsequence& operator=(const MilitaryActorConsequence&)=delete;
    MilitaryActorConsequence& operator=(MilitaryActorConsequence&&)=delete;
};
struct DynamicMilitaryCommanderLossBuildView {
 const StableId& actor_id;const Integer& loss;const CheckpointVector<MomentOwnerSource>& actual_sources;
};

struct DynamicMilitaryCommanderLossRead {
 CheckpointMemoryCharge copied_fields;
 StableId actor_id;Integer loss=0;CheckpointVector<MomentOwnerSource> actual_sources;
 DynamicMilitaryCommanderLossRead()=default;
 explicit DynamicMilitaryCommanderLossRead(const StableId& actor);
 DynamicMilitaryCommanderLossRead(const DynamicMilitaryCommanderLossRead&)=default;
 DynamicMilitaryCommanderLossRead(DynamicMilitaryCommanderLossRead&&)noexcept=default;
 DynamicMilitaryCommanderLossRead& operator=(const DynamicMilitaryCommanderLossRead&)=delete;
 DynamicMilitaryCommanderLossRead& operator=(DynamicMilitaryCommanderLossRead&&)=delete;
};
// Permanent original commander loss comes from this session's retained actual
// casualty chain, not a four-kind timed status or today's Common hp/injury.
Outcome<DynamicMilitaryCommanderLossRead> read_dynamic_military_commander_loss_before_group(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const StableId& session,
 const StableId& actor,const SourceRef& actual_group,const WorldMoment& group_at,
 const WorldMoment& actual_now,ExactArithmeticBudget={},const MilitaryCasualtyHistoryContext* history=nullptr);
Outcome<MilitaryActorConsequence> read_military_actor_consequence(const MilitaryState&,const MilitaryExactState&,const SourceRef&,Integer schema,Integer source_ordinal,ExactArithmeticBudget budget={},const MilitaryCasualtyHistoryContext* history=nullptr);
Outcome<MilitaryCasualtySourceRead> read_military_casualty_source(const MilitaryState&,const MilitaryExactState&,const SourceRef&,Integer schema,ExactArithmeticBudget budget={},const MilitaryCasualtyHistoryContext* history=nullptr);
Outcome<MilitaryFatalConsequence> read_military_fatal_consequence(const MilitaryState&,const MilitaryExactState&,const SourceRef&,Integer schema,Integer source_ordinal,ExactArithmeticBudget budget={},const MilitaryCasualtyHistoryContext* history=nullptr);
Outcome<MilitaryFatalConsequence> read_military_fatal_consequence_for_actor(const MilitaryState&,const MilitaryExactState&,const SourceRef&,Integer schema,const StableId& actor_id,ExactArithmeticBudget budget={},const MilitaryCasualtyHistoryContext* history=nullptr);
// Only the original schema-1 casualty algorithm; selected-root caller proves
// dynamic absent. A Field segment must use its own genuine parent reader.
Outcome<WorldMoment> read_legacy_military_casualty_source_moment(const MilitaryState&,const SourceRef&,ExactArithmeticBudget budget={},const MilitaryCasualtyHistoryContext* history=nullptr);
Outcome<WorldMoment> read_military_source_moment(const MilitaryState&,const MilitaryExactState&,const SourceRef&,Integer schema,ExactArithmeticBudget budget={},const MilitaryCasualtyHistoryContext* history=nullptr);
Status validate_military_exact_sources(const CommonRead&,const MilitaryState&,const MilitaryExactState&,ExactArithmeticBudget budget={},const MilitaryCasualtyHistoryContext* history=nullptr);
Status visit_military_exact_source_payloads(CodecVisitor&,MilitaryExactState&);
}
