// Same-candidate original Growth/Work bridge. No cached Moment or copied world.
#pragma once
#include "resident/domains/growth/growth_capability_chain.hpp"
#include "resident/domains/growth/native_work_practice_source.hpp"
namespace hanmo::resident {
struct WorldBundle;class CharacterConditionWorldRead;
Status validate_native_growth_permanent_layout(const WorldBundle&);
// The enclosing complete-world lease owns work first, then the body reader,
// then this bridge. Every operation reconstructs the short-lived source view.
class NativeWorldGrowthPermanentBindings final:public GrowthCapabilityRevisionWrite,public WorkCompletedResultConsumer {
    WorldBundle& world_;
    const CharacterConditionWorldRead& conditions_;
    const WorkPracticeContributionRead& work_;
    template<class F>Status with_current_sources(F&&)const;
    Status require_live_write()const;
public:
    NativeWorldGrowthPermanentBindings(WorldBundle&,const CharacterConditionWorldRead&,const WorkPracticeContributionRead&);
    bool bound_to(const CommonStore&,const GrowthPermanentState&)const noexcept override;
    bool bound_to(const CommonStore&,const TaskState&)const noexcept override;
    Status record_actual_growth_change(const StableId&,const LifeGrowthContributionRef&,const WorldMoment&)override;
    Status record_actual_condition_change(const BoundActorConditionWrite&)override;
    Status consume_completed_result(const WorkResultSource&,const WorldMoment&)override;
    // Only the original new-actor initialization invokes this before any XP or
    // Condition mutation. Restore never invents an initial revision.
    Status initialize_actor_in_candidate(const StableId& actor);
    Status validate()const;
};
}
