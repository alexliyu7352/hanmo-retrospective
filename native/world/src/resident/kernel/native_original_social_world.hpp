// Original relation sources and their actual writers, owned by one World lease.
#pragma once
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/social/social_relation_original_sources.hpp"
#include "dynamic_result_time2.hpp"
namespace hanmo::resident {
struct WorldBundle;struct CompleteExactWorldReadBindings;struct NativeExactResultAccess;
class NativeOriginalSocialWorldRead final:public CheckpointAllocatedObject {
    const WorldBundle& world_;
    const CharacterConditionWorldRead& body_;
    const TaskCatalog& tasks_catalog_;
    const OriginalSocialRelationCatalog& catalog_;
    // The source reader borrows this original context; destruction is reversed.
    GrowthWorkServiceSources work_;
    OriginalSocialEventSources events_;
    NativeOriginalSocialRelationSourceRead sources_;
    NativeOriginalSocialWorldRead(const WorldBundle&,const CharacterConditionWorldRead&,
        const std::vector<ActionTimeRegistration>&);
public:
    NativeOriginalSocialWorldRead(const NativeOriginalSocialWorldRead&)=delete;
    NativeOriginalSocialWorldRead& operator=(const NativeOriginalSocialWorldRead&)=delete;
    NativeOriginalSocialWorldRead(NativeOriginalSocialWorldRead&&)=delete;
    NativeOriginalSocialWorldRead& operator=(NativeOriginalSocialWorldRead&&)=delete;
    static Outcome<std::unique_ptr<NativeOriginalSocialWorldRead>> create(
        const WorldBundle&,const CharacterConditionWorldRead&);
    bool bound_to(const WorldBundle&,const CharacterConditionWorldRead&)const noexcept;
    bool bound_to(const WorldBundle&)const noexcept;
    // Construction only verifies true roots/selection; it never calls World's
    // owning factory or validates a still-pending completion result.
    Status validate_bindings()const;
    Status validate_original_sources()const;
    Status validate_result_binding(const NativeExactResultAccess&)const;
    Status bind_result_sources(NativeExactResultAccess&)const;
    Outcome<ExactOwnerFactRead> read_supporting_fact(const FactRecord&)const;
    Outcome<WorldMoment> read_supporting_source_moment(const MomentOwnerSource&)const;
    const OriginalSocialRelationCatalog& catalog()const noexcept{return catalog_;}
    const GrowthWorkServiceSources& work()const noexcept{return work_;}
    const OriginalSocialEventSources& events()const noexcept{return events_;}
    const NativeOriginalSocialRelationSourceRead& sources()const noexcept{return sources_;}
};
class NativeOriginalSocialWorldWrite final:public CheckpointAllocatedObject {
    WorldBundle& world_;
    const NativeOriginalSocialWorldRead& read_;
    NativeOriginalSocialCalendarRelationWrite calendar_;
    NativeOriginalTaskReportRelationWrite task_;
    NativeOriginalGrowthServiceRelationWrite growth_;
    NativeOriginalSocialWorldWrite(WorldBundle&,const NativeOriginalSocialWorldRead&);
public:
    NativeOriginalSocialWorldWrite(const NativeOriginalSocialWorldWrite&)=delete;
    NativeOriginalSocialWorldWrite& operator=(const NativeOriginalSocialWorldWrite&)=delete;
    NativeOriginalSocialWorldWrite(NativeOriginalSocialWorldWrite&&)=delete;
    NativeOriginalSocialWorldWrite& operator=(NativeOriginalSocialWorldWrite&&)=delete;
    static Outcome<std::unique_ptr<NativeOriginalSocialWorldWrite>> create(
        WorldBundle&,const NativeOriginalSocialWorldRead&);
    Status validate_bindings()const;
    OriginalSocialCalendarRelationWrite& calendar()noexcept{return calendar_;}
    OriginalTaskReportRelationWrite& task_report()noexcept{return task_;}
    OriginalGrowthServiceRelationWrite& growth_service()noexcept{return growth_;}
};
Outcome<const NativeOriginalSocialWorldRead*> read_native_original_social_world(
    const WorldBundle&,const CompleteExactWorldReadBindings&);
}
