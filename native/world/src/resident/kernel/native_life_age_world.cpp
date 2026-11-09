#include "native_life_age_world.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "native_domain_read_versions.hpp"
#include "native_control_continuation.hpp"
#include "resident/domains/family/native_continuation_sources.hpp"
#include "resident/domains/family/born_character_source.hpp"
#include "resident/domains/family/care_life_condition.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/domains/military/military_return_source.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
[[noreturn]]void required(const char* code){throw Failure{common_failure(code,{},ErrorKind::unsupported_capability).error};}
[[noreturn]]void mismatch(const char* code){throw Failure{common_failure(code,{},ErrorKind::internal).error};}
void same(const WorldMoment& a,const WorldMoment& b){if(take(compare_world_moments(a,b)))throw Failure{common_failure("LifeAgeActualMomentMismatch",{},ErrorKind::internal).error};}
template<class T,class F>Outcome<T> guard(F f){try{return {true,f(),{}};}catch(Failure& error){return {false,{},std::move(error.error)};}}
}
NativeWorldLifeAgeRead::NativeWorldLifeAgeRead(const WorldBundle& w,OriginalCalendarRead calendar,const EnvironmentalSeaDeathSourceRead* sea,const BornCharacterSourceRead* born,
    const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns)
    :world_(w),clock_(w),calendar_(std::move(calendar)),
     control_{w.common,w.family,*w.catalog->family,*w.catalog->common,w.family_legacy_enabled()?&w.family_legacy:nullptr,&*w.actor_foundation,w.catalog->actor_foundation.get(),&clock_,w.control_continuation?&*w.control_continuation:nullptr},
     original_({w.common,*w.actor_foundation,*w.catalog->actor_foundation,w.family,w.family_birth,control_,calendar_,born},
       *w.catalog->life_age_checks,*w.life_age_checks,w.family_life,clock_,w.catalog->identity,w.military,w.military_exact?&*w.military_exact:nullptr,sea,formations,returns){}
Outcome<std::unique_ptr<NativeWorldLifeAgeRead>> NativeWorldLifeAgeRead::create(const WorldBundle& w,const EnvironmentalSeaDeathSourceRead* sea,const BornCharacterSourceRead* born,
    const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){return guard<std::unique_ptr<NativeWorldLifeAgeRead>>([&]{
    take(validate_native_life_age_layout(w));
    if(!w.catalog->life_age_checks||!w.actor_foundation)required("LifeAgeActualOriginalRootsRequired");
    take(validate_family_life4_original_military_selection(w.common,{w.catalog->identity,w.military,w.military_exact?&*w.military_exact:nullptr,nullptr,sea,formations,returns}));
    if(w.catalog->actor_foundation->sea_death_rule&&!sea)required("LifeAgeActualSeaDeathSourceRequired");
    if(born&&!born->bound_to(w.common))mismatch("LifeAgeBornCandidateMismatch");
    if(formations&&!formations->bound_to(w.common.state(),w.military))mismatch("LifeAgeFormationCandidateMismatch");
    if(returns&&!returns->bound_to(w.common,w.military))mismatch("LifeAgeReturnCandidateMismatch");
    for(const auto& row:w.common.state().facts.rows()){
        if(row.second.domain=="military"&&row.second.kind=="military_action_completion_applied"&&!formations)
            required("LifeAgeOriginalFormationHistoryRequired");
        if(row.second.domain=="military"&&row.second.kind=="military_logistics_completion_applied"&&!returns)
            required("LifeAgeOriginalReturnHistoryRequired");
    }
    if(w.catalog->family_birth&&w.catalog->family_birth->selected_version==4&&!born)required("LifeAgeActualBornSourceRequired");
    auto calendar=take(OriginalCalendarRead::admit(w.catalog->identity,{original_calendar_rule_id,1,w.catalog->identity.content_digest}));
    return std::unique_ptr<NativeWorldLifeAgeRead>(new NativeWorldLifeAgeRead(w,std::move(calendar),sea,born,formations,returns));
});}
LifeAgeExactReadBinding NativeWorldLifeAgeRead::bindings()const{return original_.bindings();}
Outcome<LifeCauseView4> NativeWorldLifeAgeRead::read_age_death_source(const NaturalAgeDeathCause4& ref)const{return original_.read_age_death_source(ref);}
Status validate_native_life_age_layout(const WorldBundle& w){
    if(!w.catalog)return common_failure("LifeAgeCatalogRequired",{},ErrorKind::internal);
    auto valid=validate_native_life_age_catalog(*w.catalog);if(!valid.ok)return valid;
    if(bool(w.catalog->life_age_checks)!=bool(w.life_age_checks)||bool(w.life_age_checks)!=w.life_age_checks_seed_complete)
        return common_failure("LifeAgeOriginalSeedRequired",{},ErrorKind::internal);
    return common_success();
}
namespace {
const LifeAgeExactReadLease& borrowed_age(const WorldBundle& w,const CompleteExactWorldBindingOwner& complete){
    const auto& read=complete.reads();
    if(!read.family_life_age)required("LifeAgeSameOwningReadRequired");
    if(!w.catalog||!w.catalog->life_age_checks||!w.life_age_checks||!w.actor_foundation)
        required("LifeAgeActualOriginalRootsRequired");
    const auto body=take(read.character_conditions.sources());
    const auto age=read.family_life_age->bindings();
    if(!body.life_sources||&age.sources.common!=&w.common||&age.sources.foundation!=&*w.actor_foundation||
       &age.sources.family!=&w.family||&age.sources.births!=&w.family_birth||
       &age.catalog!=w.catalog->life_age_checks.get()||&age.state!=&*w.life_age_checks||
       &age.deaths!=&w.family_life||age.sources.born_sources!=body.born_sources||
       age.life_sources.age!=body.life_sources->age||age.life_sources.sea!=body.life_sources->sea||
       age.life_sources.military_formations!=body.life_sources->military_formations||
       age.life_sources.military_returns!=body.life_sources->military_returns||
       age.life_sources.field_battles!=body.life_sources->field_battles||
       age.life_sources.field_application_clock!=body.life_sources->field_application_clock||
       age.life_sources.military_formations!=body.military_formations||
       age.life_sources.military_returns!=body.military_returns||
       age.life_sources.military_formations!=read.military_formations||
       age.life_sources.military_returns!=read.military_returns||
       &age.life_sources.military!=&w.military||age.life_sources.military_exact!=(w.military_exact?&*w.military_exact:nullptr)||age.life_sources.catalog_selection!=&w.catalog->identity)
        mismatch("LifeAgeSameOwningCandidateMismatch");
    take(validate_family_life4_original_military_selection(w.common,age.life_sources));
    same(take(w.actual_phase_moment()),take(age.clock.read_current_moment()));
    return *read.family_life_age;
}
}
Status validate_native_life_age_world(const WorldBundle& w){auto result=guard<Done>([&]{
    take(validate_native_life_age_layout(w));
    const bool life4=w.catalog->family_life&&w.catalog->family_life->enabled&&w.catalog->family_life->selected_version==4;
    if(!life4)return Done{};
    auto complete=take(w.complete_exact_time_bindings());if(!complete)required("Life4ActualOwningSourcesRequired");
    auto body=take(complete->reads().character_conditions.sources());const auto at=take(w.actual_phase_moment());same(body.actual_now,at);
    if(!body.life_sources)required("Life4ActualLifeSourcesRequired");
    if(&body.common!=&w.common||body.deaths!=&w.family_life)mismatch("Life4SameCandidateSourcesRequired");
    take(validate_family_life4(w.common,w.family_life,*body.life_sources,at));
    if(w.catalog->life_age_checks){auto original=borrowed_age(w,*complete).bindings();take(validate_life_age_state(original.sources,original.catalog,original.state,original.deaths,at));}
    return Done{};
});return {result.ok,{},std::move(result.error)};}
namespace {
class Read final:public LifeAgeExactReadLease,public CheckpointAllocatedObject {
    std::unique_ptr<CompleteExactWorldBindingOwner> complete_;
    const LifeAgeExactReadLease* age_=nullptr;
public:
    Read(const WorldBundle& w,std::unique_ptr<CompleteExactWorldBindingOwner> owner):complete_(std::move(owner)){
        if(!complete_)required("LifeAgeActualOwningSourcesRequired");if(!w.actor_foundation)mismatch("LifeAgeActualFoundationStateRequired");auto body=take(complete_->reads().character_conditions.sources());
        if(!body.life_sources)required("LifeAgeActualLifeSourcesRequired");
        if(&body.common!=&w.common||&body.state!=&*w.actor_foundation||body.deaths!=&w.family_life)mismatch("LifeAgeSameCandidateSourcesRequired");
        age_=&borrowed_age(w,*complete_);
    }
    LifeAgeExactReadBinding bindings()const override{return age_->bindings();}
};
class Write final:public LifeAgeExactWriteLease,public CheckpointAllocatedObject {
    WorldBundle& world_;
    std::unique_ptr<CompleteExactWorldBindingOwner> complete_;
    const LifeAgeExactReadLease* age_=nullptr;
    NativeDomainReadVersionSource versions_;
    std::unique_ptr<CurrentReadStampRead> current_;
    CareLifeConditionWrite* care_life_=nullptr;
public:
    Write(WorldBundle& w,std::unique_ptr<CompleteExactWorldBindingOwner> owner):world_(w),complete_(std::move(owner)),versions_(w,complete_?complete_->reads().military_formations:nullptr,
          complete_?complete_->reads().military_returns:nullptr,
          complete_?complete_->reads().military_supply_periods:nullptr){
        if(!complete_)required("LifeAgeActualOwningSourcesRequired");if(!w.actor_foundation)mismatch("LifeAgeActualFoundationStateRequired");auto body=take(complete_->reads().character_conditions.sources());
        if(!body.life_sources)required("LifeAgeActualLifeSourcesRequired");
        if(&body.common!=&w.common||&body.state!=&*w.actor_foundation||body.deaths!=&w.family_life)mismatch("LifeAgeSameCandidateSourcesRequired");
        care_life_=complete_->care_life_writer();
        if(care_life_condition_consumer_selected(*w.catalog->actor_foundation)&&!care_life_)
            required("LifeAgeActualCareLifeWriterRequired");
        if(care_life_&&!care_life_->bound_to(w.common,*w.actor_foundation,w.family_life))
            mismatch("LifeAgeCareLifeCandidateMismatch");
        age_=&borrowed_age(w,*complete_);current_=w.make_current_read_stamp_reader(versions_,{4096,4096,1024*1024});
    }
    // 同一个 complete_ 持有实际身体读者；寿终 writer 不拼接另一候选的生命来源。
    LifeAgeExactWriteBinding bindings()override{return {*age_,world_.common,*world_.actor_foundation,*world_.life_age_checks,world_.family_life,*current_,*world_.read_stamps,versions_,complete_->growth_capability_writer(),care_life_,&complete_->reads().character_conditions};}
};
Outcome<std::shared_ptr<const LifeAgeExactReadLease>> read(const WorldBundle& w,const WorldMoment& at){return guard<std::shared_ptr<const LifeAgeExactReadLease>>([&]{same(at,take(w.actual_phase_moment()));return std::shared_ptr<const LifeAgeExactReadLease>(new Read(w,take(w.complete_exact_time_bindings())));});}
Outcome<std::shared_ptr<LifeAgeExactWriteLease>> write(WorldBundle& w,const WorldMoment& at){return guard<std::shared_ptr<LifeAgeExactWriteLease>>([&]{same(at,take(w.actual_phase_moment()));if(!w.active_exact_write_context()||!w.common.actor_condition_capture_identity())required("LifeAgeOriginalLiveCaptureRequired");return std::shared_ptr<LifeAgeExactWriteLease>(new Write(w,take(w.complete_exact_time_bindings())));});}
Status after(WorldBundle& w,ExactPhaseWriteContext& writes){return consume_complete_growth_condition_inputs_in_candidate(w,writes);}
}
Status after_actual_death(WorldBundle& w,const DeathRef& death){
    auto result=guard<Done>([&]{
        take(validate_native_control_continuation_layout(w));
        if(!w.control_continuation)return Done{};
        if(!w.active_exact_write_context()||!w.common.actor_condition_capture_identity())
            required("ContinuationDeathActualCandidateRequired");
        auto owner=take(w.complete_exact_time_bindings());
        if(!owner)required("ContinuationDeathActualOwnerRequired");
        const auto* original=owner->reads().continuation_sources;
        if(!original||!original->bound_to(w.common,w.family,*w.control_continuation))
            required("ContinuationDeathSameOriginalSourcesRequired");
        same(take(w.actual_phase_moment()),take(original->actual_now()));
        take(open_after_death_continuation_in_candidate(w.common,*w.control_continuation,
            *w.catalog->control_continuation,w.catalog->identity,*original,death));
        w.note_write_boundary();
        return Done{};
    });
    return {result.ok,{},std::move(result.error)};
}
LifeAgeExactWorldFactory native_life_age_world_factory(){return {&read,&write,&after,&after_actual_death};}
}
