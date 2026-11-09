#pragma once
#include "dynamic_session.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <string_view>
namespace hanmo::resident {
struct DynamicMilitaryAcceptedIntentRead {
    SourceRef accepted_intent_source;StableId session_ref;Integer generation=0;
    StableId unit_ref,timing_definition_ref;Integer timing_definition_version=0,action_instance_ordinal=0;
    MilitaryOrder original_order;WorldMoment accepted_at;
};
enum class DynamicTacticalEffectUse { activation,projectile_release,projectile_impact };
struct DynamicMilitaryActualEffectReadBuildView {
    MomentOwnerSourceBuildView source;
    std::string_view action_ref;
    const WorldMoment& effective_at;
    DynamicTacticalEffectUse use;
};
struct DynamicMilitaryActualEffectRead {
private:
    static CheckpointMemoryCharge reserve_default();
    static CheckpointMemoryCharge reserve(const DynamicMilitaryActualEffectReadBuildView&);
    static CheckpointMemoryCharge reserve(const DynamicMilitaryActualEffectRead&);
public:
    CheckpointMemoryCharge copied_fields;
    MomentOwnerSource source;StableId action_ref;WorldMoment effective_at;
    DynamicTacticalEffectUse use=DynamicTacticalEffectUse::activation;
    DynamicMilitaryActualEffectRead();
    explicit DynamicMilitaryActualEffectRead(const DynamicMilitaryActualEffectReadBuildView&);
    DynamicMilitaryActualEffectRead(const DynamicMilitaryActualEffectRead&);
    DynamicMilitaryActualEffectRead(DynamicMilitaryActualEffectRead&& other)
      :DynamicMilitaryActualEffectRead(static_cast<const DynamicMilitaryActualEffectRead&>(other)){}
    DynamicMilitaryActualEffectRead& operator=(const DynamicMilitaryActualEffectRead&)=delete;
    DynamicMilitaryActualEffectRead& operator=(DynamicMilitaryActualEffectRead&&)=delete;
};
// Accepted intent, whole-span service and actual effects each read their own
// original record. A caller cannot supply credit, a quote, or a completed flag.
class DynamicMilitaryTacticalSources {
public:
    virtual ~DynamicMilitaryTacticalSources()=default;
    virtual Outcome<DynamicMilitaryAcceptedIntentRead> read_accepted_intent(const SourceRef&)const=0;
    virtual Status validate_action_acceptance(const DynamicMilitaryAction2&)const=0;
    virtual Outcome<std::vector<MomentOwnerSource>> read_actual_service_participation(const DynamicMilitaryAction2&,MilitaryTacticalWorkStage,const MomentSpan&)const=0;
    virtual Status validate_actual_service(const DynamicMilitaryAction2&,MilitaryTacticalWorkStage,const MilitaryTacticalServiceSpan2&)const=0;
    virtual Status validate_actual_interruption(const DynamicMilitaryAction2&,const MomentOwnerSource&,const WorldMoment&)const=0;
    virtual Outcome<WorldMoment> read_actual_source_moment(const MomentOwnerSource&)const=0;
};
struct DynamicMilitaryActionContext {
    const DynamicMilitaryActionCatalog2& catalog;const DynamicMilitaryTacticalSources& sources;
    const DynamicMilitarySessionOriginalRead& sessions;const CurrentMomentRead& clock;
    const TimeScaleRef& scale;ExactArithmeticBudget budget={};
};
Status validate_dynamic_military_action_catalog(const DynamicMilitaryActionCatalog2&,const MilitaryBattleCatalog&,ExactArithmeticBudget budget={});
Outcome<StableId> start_dynamic_military_action_in_candidate(CommonStore&,const MilitaryState&,MilitaryExactState&,const DynamicMilitaryActionContext&,const SourceRef& accepted_intent);
Status provide_dynamic_military_action_service_in_candidate(const CommonStore&,MilitaryExactState&,const DynamicMilitaryActionContext&,const StableId& action,const MomentSpan&);
// The effect group has already generated its actual source and original cost.
// Adoption closes the one prepare gate and begins only the authored recovery.
Status adopt_dynamic_military_action_effect_in_candidate(const CommonStore&,MilitaryExactState&,const DynamicMilitaryActionContext&,const MomentOwnerSource& actual_effect);
Status complete_dynamic_military_action_recovery_in_candidate(CommonStore&,MilitaryExactState&,const DynamicMilitaryActionContext&,const StableId& action);
Status interrupt_dynamic_military_action_in_candidate(const CommonStore&,MilitaryExactState&,const DynamicMilitaryActionContext&,const StableId& action,const MomentOwnerSource& actual_cause);
struct DynamicMilitaryActionDue {
    StableId action_ref;WorldMoment at;MilitaryTacticalWorkStage stage=MilitaryTacticalWorkStage::prepare;
    bool committed_projectile=false;
};
// Full finite set. A new intent at M has a positive prepare and cannot insert
// itself into the already selected M group. Released impact survives its actor.
Outcome<std::vector<DynamicMilitaryActionDue>> collect_dynamic_military_actions_at(const CommonStore&,const MilitaryExactState&,const DynamicMilitaryActionContext&,const StableId& session,Integer generation,const WorldMoment& at);
Outcome<std::optional<WorldMoment>> predict_dynamic_military_action_due(const DynamicMilitaryAction2&,const DynamicMilitaryActionContext&);
Status validate_dynamic_military_actions(const CommonStore&,const MilitaryState&,const MilitaryExactState&,const DynamicMilitaryActionContext&,const WorldMoment&);
// Same-candidate group closure checks only its original session. Other sessions
// may still have genuine same-M due gates; the enclosing phase performs the
// full-world validator once every such group has been consumed.
Status validate_dynamic_military_session_actions(const CommonStore&,const MilitaryState&,const MilitaryExactState&,const DynamicMilitaryActionContext&,const StableId& session,Integer generation,const WorldMoment&);
Outcome<WorldMoment> read_dynamic_military_action_source_moment(const CommonStore&,const MilitaryExactState&,const SourceRef&,Integer schema);
}
