#include "native_military_training_period.hpp"
#include "typed_supply_observation.hpp"
#include <limits>
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
[[noreturn]]void mismatch(const char* code,const StableId& id={}){throw Failure{common_failure(code,id,ErrorKind::internal).error};}
RuleError exhausted(){return common_failure("MilitaryTrainingPeriodReadResourceExhausted",{},ErrorKind::internal).error;}
}
bool NativeMilitaryTrainingPeriodRead::bound_to(const CommonStore& common,const EconomicWorkSourcesState& sources)const noexcept {
    return &calendar_.common==&common && &calendar_.sources==&sources;
}
Outcome<ExactTrainingPeriodView> NativeMilitaryTrainingPeriodRead::read_training_period(Integer from)const {try{
    typed_supply_observation::Context observed;
    observed.common_identity=&calendar_.common.state();observed.world_id=calendar_.common.state().identity.world_id;
    observed.revision=calendar_.common.state().identity.revision;observed.execution_epoch=calendar_.common.state().identity.execution_epoch;
    observed.common_tick=calendar_.common.state().identity.tick;observed.fact_count=calendar_.common.state().facts.order().size();
    observed.original_roots={&calendar_.economy,&calendar_.sources,&calendar_.foundation,&calendar_.foundation_catalog,&calendar_.clock};
    observed.has_requested_from=true;observed.requested_from=from;observed.item_count=calendar_.sources.period_openings.size();
    typed_supply_observation::Scope period_observation(typed_supply_observation::Stage::period_reader,observed);
    if(from<0||from==std::numeric_limits<Integer>::max())mismatch("MilitaryTrainingPeriodTickInvalid");
    const auto now=take(calendar_.clock.read_current_moment());
    {const auto encoded_now=take(encode_world_moment(now,budget_));period_observation.moment("actual_now",encoded_now);}
    if(take(compare_world_moments(WorldMoment{from,{}},now,budget_))>0)mismatch("MilitaryTrainingFuturePeriodRequired");
    const EconomicPeriodOpening* found=nullptr;
    for(const auto& entry:calendar_.sources.period_openings.rows())if(entry.second.opening_at.half_day_tick==from){
        if(found||entry.first!=entry.second.period_id)mismatch("MilitaryTrainingDuplicateOriginalPeriod",entry.first);
        found=&entry.second;
    }
    if(!found)mismatch("MilitaryTrainingOriginalPeriodMissing");
    const auto source=economic_period_opening_source(*found);
    // 原源专用门核完整日历身份链，不回调本训练 Action 的完整履约门形成递归。
    // 稳定世界仍由 S5 全 validator 检查 Opening 冻结的所有 Action/Activity。
    observed.source={source.domain,source.kind,source.id,1};
    typed_supply_observation::Scope identity_observation(typed_supply_observation::Stage::calendar_identity,observed);
    const auto* actual=take(read_economic_period_opening_identity(calendar_,source,1));
    identity_observation.stop();
    if(actual!=found||actual->through_tick!=from+1||
       take(compare_world_moments(actual->opening_at,WorldMoment{from,{}},budget_)))
        mismatch("MilitaryTrainingOriginalPeriodMismatch",found->period_id);
    period_observation.stop();
    return {true,{actual->period_id,from,actual->through_tick,actual->opening_at,MomentOwnerSource{source,1}},{}};
}catch(Failure& error){return {false,{},std::move(error.error)};}
 catch(const std::bad_alloc&){return {false,{},exhausted()};}
 catch(const std::length_error&){return {false,{},exhausted()};}}
}
