#include "native_original_social_world.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "native_exact_result_owners.hpp"
#include "resident/domains/social/social_original_result_projection.hpp"
#include "original_catalog_identity.hpp"
#include <new>
#include <stdexcept>
#include <string_view>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
[[noreturn]]void required(const char*code){throw Failure{common_failure(code,{},ErrorKind::unsupported_capability).error};}
[[noreturn]]void mismatch(const char*code){throw Failure{common_failure(code,{},ErrorKind::internal).error};}
template<class T,class F>Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("OriginalSocialOwningResourceExhausted",{},ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("OriginalSocialOwningResourceExhausted",{},ErrorKind::internal).error};}}
const std::vector<ActionTimeRegistration>& selected(const WorldBundle&w,const CharacterConditionWorldRead&body){
    if(!w.catalog||!w.catalog->social||!w.catalog->tasks||!w.catalog->growth||!w.catalog->common||
       !w.catalog->social->original_relations||!w.catalog->exact_time||!exact_complete_selected(*w.catalog->exact_time)||
       !w.world_timeline||!w.action_elapsed||!w.actor_foundation||!w.social_period_sources||w.social.original_relation_version!=2)
        required("OriginalSocialActualOwningRootsRequired");
    bool seen=false;for(const auto&row:w.catalog->identity.module_versions)if(row.first=="social"){
        if(seen)mismatch("DuplicateOriginalSocialSelection");seen=true;if(row.second!="2")required("OriginalSocialPhysicalVersionRequired");
    }
    if(!seen)required("OriginalSocialPhysicalSelectionRequired");
    if(!same_original_catalog_identity(w.catalog->identity,w.catalog->social->original_relations->identity))
        mismatch("OriginalSocialCatalogIdentityMismatch");
    const auto source=take(body.sources());
    if(&source.common!=&w.common||&source.state!=&*w.actor_foundation||source.growth!=&w.growth||
       source.growth_catalog!=w.catalog->growth.get()||source.shared!=w.catalog->common.get()||
       source.elapsed!=&*w.action_elapsed||source.scale!=&w.world_timeline->scale_ref||
       source.interaction!=&w.interaction||source.interaction_catalog!=w.catalog->interaction.get())
        mismatch("OriginalSocialBodyOwningCandidateMismatch");
    if(!source.action_registrations)required("OriginalSocialGrowthRegistrationsRequired");
    if(take(compare_world_moments(source.actual_now,take(w.actual_phase_moment()))))
        mismatch("OriginalSocialBodyActualMomentMismatch");
    return *source.action_registrations;
}
}
NativeOriginalSocialWorldRead::NativeOriginalSocialWorldRead(const WorldBundle&w,const CharacterConditionWorldRead&body,const std::vector<ActionTimeRegistration>&registrations)
    :world_(w),body_(body),tasks_catalog_(*w.catalog->tasks),catalog_(*w.catalog->social->original_relations),
     work_{w.common,w.growth,*w.catalog->growth,*w.catalog->common,*w.action_elapsed,w.world_timeline->scale_ref,w.catalog->identity,registrations,{},&w.interaction,w.catalog->interaction.get()},
     events_{w.common,w.social,*w.social_period_sources,w.catalog->identity},
     sources_(w.common,w.social,w.tasks,tasks_catalog_,w.growth,w.catalog->identity,&work_,&events_){}
Outcome<std::unique_ptr<NativeOriginalSocialWorldRead>> NativeOriginalSocialWorldRead::create(const WorldBundle&w,const CharacterConditionWorldRead&body){return guard<std::unique_ptr<NativeOriginalSocialWorldRead>>([&]{
    const auto&registrations=selected(w,body);
    auto out=std::unique_ptr<NativeOriginalSocialWorldRead>(new NativeOriginalSocialWorldRead(w,body,registrations));
    take(out->validate_bindings());return out;
});}
bool NativeOriginalSocialWorldRead::bound_to(const WorldBundle&w,const CharacterConditionWorldRead&body)const noexcept{return &world_==&w&&&body_==&body;}
bool NativeOriginalSocialWorldRead::bound_to(const WorldBundle&w)const noexcept{return &world_==&w;}
Status NativeOriginalSocialWorldRead::validate_bindings()const{return guard<Done>([&]{
    const auto&registrations=selected(world_,body_);
    if(&registrations!=&work_.registrations||&tasks_catalog_!=world_.catalog->tasks.get()||
       &catalog_!=&*world_.catalog->social->original_relations||&work_.common!=&world_.common||&work_.growth!=&world_.growth||
       &work_.catalog!=world_.catalog->growth.get()||&work_.shared!=world_.catalog->common.get()||
       &work_.elapsed!=&*world_.action_elapsed||&work_.scale!=&world_.world_timeline->scale_ref||&work_.identity!=&world_.catalog->identity||
       work_.interaction!=&world_.interaction||work_.interaction_catalog!=world_.catalog->interaction.get()||
       &events_.common!=&world_.common||&events_.social!=&world_.social||&events_.periods!=&*world_.social_period_sources||&events_.identity!=&world_.catalog->identity||
       !sources_.bound_to_events(events_)||!sources_.bound_to(world_.common,world_.social)||!sources_.bound_to_tasks(world_.tasks)||!sources_.bound_to_work_services(work_))
        mismatch("OriginalSocialOwningGraphChanged");
    return Done{};
});}
Status NativeOriginalSocialWorldRead::validate_original_sources()const{return guard<Done>([&]{
    take(validate_bindings());const auto now=take(world_.actual_phase_moment());
    take(validate_original_social_relations({world_.common,world_.social,catalog_,sources_,now}));return Done{};
});}
Status NativeOriginalSocialWorldRead::validate_result_binding(const NativeExactResultAccess&access)const{return guard<Done>([&]{
    take(validate_bindings());
    if(&access.common!=&world_.common||&access.tasks!=&world_.tasks||&access.affairs!=&world_.affairs||&access.social!=&world_.social||
       access.social_periods!=&*world_.social_period_sources)
        mismatch("OriginalSocialResultOwningCandidateMismatch");
    if(access.original_social&&access.original_social!=this)mismatch("OriginalSocialResultOwnerAlreadyBound");
    return Done{};
});}
Status NativeOriginalSocialWorldRead::bind_result_sources(NativeExactResultAccess&access)const{
    auto valid=validate_result_binding(access);if(!valid.ok)return valid;
    access.original_social=this;return common_success();
}
Outcome<ExactOwnerFactRead> NativeOriginalSocialWorldRead::read_supporting_fact(const FactRecord&header)const{return guard<ExactOwnerFactRead>([&]{
    take(validate_bindings());const auto now=take(world_.actual_phase_moment());
    const OriginalSocialRelationContext context{world_.common,world_.social,catalog_,sources_,now};
    const OriginalSocialResultRead original(context,events_);
    return take(original.read_selected_fact(header));
});}
Outcome<WorldMoment> NativeOriginalSocialWorldRead::read_supporting_source_moment(const MomentOwnerSource&source)const{return guard<WorldMoment>([&]{
    take(validate_bindings());const auto now=take(world_.actual_phase_moment());
    const OriginalSocialRelationContext context{world_.common,world_.social,catalog_,sources_,now};
    const OriginalSocialResultRead original(context,events_);
    return take(original.read_selected_source_moment(source));
});}
NativeOriginalSocialWorldWrite::NativeOriginalSocialWorldWrite(WorldBundle&w,const NativeOriginalSocialWorldRead&read)
    :world_(w),read_(read),calendar_(w.common,w.social,read.events(),read.catalog(),read.sources()),
     task_(w.common,w.social,w.tasks,*w.catalog->tasks,read.catalog(),read.sources()),
     growth_(w.common,w.social,read.work(),read.catalog(),read.sources()){}
Outcome<std::unique_ptr<NativeOriginalSocialWorldWrite>> NativeOriginalSocialWorldWrite::create(WorldBundle&w,const NativeOriginalSocialWorldRead&read){return guard<std::unique_ptr<NativeOriginalSocialWorldWrite>>([&]{
    if(!read.bound_to(w))mismatch("OriginalSocialWriteCandidateMismatch");take(read.validate_bindings());
    auto out=std::unique_ptr<NativeOriginalSocialWorldWrite>(new NativeOriginalSocialWorldWrite(w,read));
    take(out->validate_bindings());return out;
});}
Status NativeOriginalSocialWorldWrite::validate_bindings()const{return guard<Done>([&]{
    if(!read_.bound_to(world_))mismatch("OriginalSocialWriteCandidateMismatch");take(read_.validate_bindings());
    if(!calendar_.bound_to(world_.common,world_.social)||!task_.bound_to(world_.common,world_.social,world_.tasks)||!growth_.bound_to(world_.common,world_.social,world_.growth))
        mismatch("OriginalSocialWriteOwnerMismatch");return Done{};
});}
Outcome<const NativeOriginalSocialWorldRead*> read_native_original_social_world(const WorldBundle&w,const CompleteExactWorldReadBindings&bindings){return guard<const NativeOriginalSocialWorldRead*>([&]{
    if(!bindings.original_social)required("OriginalSocialCompleteOwningLeaseRequired");
    if(!bindings.original_social->bound_to(w,bindings.character_conditions))mismatch("OriginalSocialCompleteCandidateMismatch");
    take(bindings.original_social->validate_bindings());return bindings.original_social;
});}
}
