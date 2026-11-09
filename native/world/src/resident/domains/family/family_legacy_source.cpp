// 现成真人的死亡接续关系只读；私产、意愿和可操作处境仍须各自权威流程批准。
#include "family_legacy_source.hpp"
#include "family_continuation_people4.hpp"
#include "succession_consent_source.hpp"
#include <algorithm>
namespace hanmo::resident {
// 同tick也按原事实顺序证明关系在死亡原因前已存在，不能在遗留登记间隙补造关系。
Outcome<FamilyLegacyAdoptionBasis> read_family_legacy_adoption_basis(
    const CommonStore& common,const FamilyState& family,const FamilyDeathState& deaths,
    const FamilyDeathOriginRead& read,const StableId& predecessor,const StableId& successor,const FamilyContinuationPeopleRead4* complete){
    if(complete){
        if(!complete->bound_to(common,deaths))return {false,{},common_failure("FamilyContinuationCandidateMismatch",{},ErrorKind::internal).error};
        auto valid=complete->life().validate();if(!valid.ok)return {false,{},valid.error};
        auto death=complete->life().read_actor_death(predecessor);if(!death.ok)return {false,{},death.error};if(!death.value||predecessor==successor)return {false,{},common_failure("FamilyDeathSourceUnavailable",predecessor).error};
        auto alive=complete->read_current_presence(successor);if(!alive.ok)return {false,{},alive.error};if(!alive.value.alive)return {false,{},common_failure("InvalidLegacyContinuationParties").error};
        auto age=complete->read_current_age(successor);if(!age.ok)return {false,{},age.error};if(age.value.age_years<18)return {false,{},common_failure("SuccessionRequiresAdult",successor).error};
        const auto& actual=*death.value;auto relation=succession_adoption_basis(common,family,predecessor,successor,actual.effective_at.exact_moment.half_day_tick);if(!relation.ok)return {false,{},relation.error};
        const auto& order=common.state().facts.order();auto a=std::find(order.begin(),order.end(),relation.value),b=std::find(order.begin(),order.end(),actual.cause_ref.id);if(a==order.end()||b==order.end()||a>=b)return {false,{},common_failure("LegacyRelationshipAfterDeath",relation.value).error};
        FamilyLegacyAdoptionBasis result;result.checked_identity=common.state().identity;result.predecessor_actor_id=predecessor;result.successor_actor_id=successor;result.death_fact_id=actual.death_ref.id;result.relation_fact_id=std::move(relation.value);result.death_tick=actual.effective_at.exact_moment.half_day_tick;result.successor_birth_tick=common.state().actors.find(successor)->birth_tick;result.verified_death=actual;
        if(actual.cause_kind=="military_casualty")result.casualty_fact_id=actual.cause_ref.id;return {true,std::move(result),{}};
    }
    auto valid=validate_family_deaths(common,deaths,read,true);if(!valid.ok)return {false,{},valid.error};
    const auto* original=common.state().actors.find(predecessor);const auto* next=common.state().actors.find(successor);
    if(!original||!next)return {false,{},common_failure("UnknownActor",!original?predecessor:successor).error};
    if(predecessor==successor||original->alive||!next->alive)return {false,{},common_failure("InvalidLegacyContinuationParties").error};
    constexpr Integer limit=2147483647,adult_ticks=18*720;
    if(common.tick()<0||common.tick()>limit||!next->birth_tick||*next->birth_tick< -limit||*next->birth_tick>common.tick())return {false,{},common_failure("ActorAgeUnknown",successor).error};
    if(common.tick()-*next->birth_tick<adult_ticks)return {false,{},common_failure("SuccessionRequiresAdult",successor).error};
    FamilyLegacyAdoptionBasis result;const FamilyDeathRecord* death=nullptr;
    for(const auto& row:deaths.deaths.rows())if(row.second.actor_id==predecessor){result.death_fact_id=row.first;death=&row.second;break;}
    if(!death)return {false,{},common_failure("FamilyDeathSourceUnavailable",predecessor).error};
    auto relation=succession_adoption_basis(common,family,predecessor,successor,death->death_tick);if(!relation.ok)return {false,{},relation.error};
    const auto& facts=common.state().facts.order();auto before=std::find(facts.begin(),facts.end(),relation.value),cause=std::find(facts.begin(),facts.end(),death->cause_fact_id);
    if(before==facts.end()||cause==facts.end()||before>=cause)return {false,{},common_failure("LegacyRelationshipAfterDeath",relation.value).error};
    result.checked_identity=common.state().identity;result.predecessor_actor_id=predecessor;result.successor_actor_id=successor;
    result.casualty_fact_id=death->cause_fact_id;result.relation_fact_id=std::move(relation.value);result.death_tick=death->death_tick;result.successor_birth_tick=*next->birth_tick;
    return {true,std::move(result),{}};
}
}
