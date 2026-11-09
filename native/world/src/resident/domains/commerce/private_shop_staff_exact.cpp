// Exact adaptation of the original accepted-staff expiry/death lifecycle. It
// creates only the original staff decision, original DayResult and S0 terminal.
#include "private_shop_staff.hpp"
#include "private_shop_operation_sources.hpp"
#include "resident/domains/family/family_life_history4.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
const FamilyLifeHistoryRead4& life(const PrivateShopOperationSourceContext&x){if(!x.life4||!x.shops.family_deaths||!x.life4->bound_to(x.shops.common,*x.shops.family_deaths)||cmp(take(x.life4->read_current_moment()),take(x.clock.read_current_moment())))fail("PrivateShopExactLifeHistoryRequired");return *x.life4;}
}
Outcome<PrivateShopExactStaffTerminal> read_private_shop_exact_staff_terminal(const PrivateShopOperationSourceContext&x,const SourceRef&source){try{
 if(source.domain!=private_shop_module||source.kind!="private_shop_staff_ended")fail("PrivateShopActualStaffTerminalRequired",source.id);
 const auto*d=x.shops.state.staff.decisions.find(source.id);if(!d||d->id!=source.id)fail("PrivateShopActualStaffTerminalRequired",source.id);take(read_actual_common_fact_position(x.shops.common,source));
 const auto*c=take(read_original_private_shop_staff_contract(x.shops,{private_shop_module,"private_shop_staff_accepted",d->contract_id}));
 if(c->terminal_fact_id!=d->id||c->status!=d->decision||!d->actor_id.empty())fail("PrivateShopExactStaffLifecycleMismatch",source.id);
 WorldMoment at;
 if(d->decision=="expired"){if(d->tick!=c->through_tick||!d->cause_fact_id.empty())fail("PrivateShopExactStaffExpiryMismatch",source.id);at={c->through_tick,{}};}
 else if(d->decision=="worker_dead"||d->decision=="employer_dead"){
  const auto actor=d->decision=="worker_dead"?c->terms.worker_actor_id:c->terms.employer_actor_id;
  const auto death=take(life(x).read_actor_death_before_original_fact(actor,d->tick,source));
  if(!death||death->death_ref.id!=d->cause_fact_id||death->recorded_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("PrivateShopExactStaffDeathMismatch",source.id);
  at=death->recorded_at.exact_moment;if(at.half_day_tick!=d->tick||cmp(at,{c->through_tick,{}})>=0)fail("PrivateShopExactStaffDeathBoundaryMismatch",source.id);
 }else fail("PrivateShopExactStaffLifecycleSourceRequired",source.id);
 if(cmp(at,take(x.clock.read_current_moment()))>0)fail("PrivateShopStaffTerminalFromFuture",source.id);
 return {true,{c,d,at},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
// 只登记原店员终止事实的真实依赖；死亡核验失败时返回原错误。
Status visit_private_shop_exact_staff_terminal_pins(
    const PrivateShopOperationSourceContext &x, const SourceRef &source,
    PrivateShopOperationSourcePinSink &sink) {
  try {
    const auto original =
        take(read_private_shop_exact_staff_terminal(x, source));
    take(sink.source({{private_shop_module, "private_shop_staff_accepted",
                       original.contract->id},
                      1}));
    if (original.decision->decision == "worker_dead" ||
        original.decision->decision == "employer_dead") {
      const auto &actor = original.decision->decision == "worker_dead"
                              ? original.contract->terms.worker_actor_id
                              : original.contract->terms.employer_actor_id;
      const auto death = take(life(x).read_actor_death_before_original_fact(
          actor, original.decision->tick, source));
      if (!death || death->death_ref.domain != "family_life" ||
          death->death_ref.kind != "family_death" ||
          death->death_ref.id != original.decision->cause_fact_id ||
          death->recorded_at.kind != ActorFoundationAt::Kind::exact_moment ||
          cmp(death->recorded_at.exact_moment, original.occurred_at))
        fail("PrivateShopExactStaffDeathMismatch", source.id);
      take(sink.source({{death->death_ref.domain, death->death_ref.kind,
                         death->death_ref.id},
                        4}));
    }
    // 到期只取原已接受合约的不可变
    // through_tick，不另造边界事实、行动或进度来源。
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<std::vector<StableId>> finish_private_shop_staff_lifecycle_in_candidate(CommonStore&c,LocalEconomyState&e,PrivateShopState&s,
 PrivateShopOperationSourcesState&sources,PrivateShopOperationCapabilitiesState&capabilities,ActionElapsedStore&elapsed,const PrivateShopOperationSourceContext&x){try{
 if(&c!=&x.shops.common||&e!=&x.shops.economy||&s!=&x.shops.state||&sources!=&x.sources||&capabilities!=&x.capabilities||&elapsed!=&x.elapsed||x.shops.exact_operations!=&x)fail("PrivateShopExactStaffLifecycleCandidateMismatch");
 const auto at=take(x.clock.read_current_moment());std::vector<StableId>results;const auto ids=s.staff.contracts.order();
 for(const auto&id:ids){const auto original=*take(read_original_private_shop_staff_contract(x.shops,{private_shop_module,"private_shop_staff_accepted",id}));if(original.status!="active")continue;
  StableId reason,cause;
  if(cmp(at,{original.through_tick,{}})>=0){if(cmp(at,{original.through_tick,{}}))fail("PrivateShopStaffMissedExactExpiry",id);reason="expired";}
  else for(const auto&party:{std::make_pair(original.terms.worker_actor_id,StableId{"worker_dead"}),std::make_pair(original.terms.employer_actor_id,StableId{"employer_dead"})}){
   const auto*actor=c.actor(party.first);if(!actor)fail("UnknownActor",party.first);if(actor->alive)continue;
   const auto death=take(life(x).read_actor_death(party.first));if(!death||death->recorded_at.kind!=ActorFoundationAt::Kind::exact_moment||cmp(death->recorded_at.exact_moment,at))fail("PrivateShopStaffMissedExactDeath",id);
   reason=party.second;cause=death->death_ref.id;break;
  }
  if(reason.empty())continue;std::vector<StableId>operations;
  for(const auto&row:s.operations.rows())if(row.second.staff_contract_id==id&&row.second.result_fact_id.empty()){
   const auto&acceptance=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(row.first),2}));if(acceptance.operation_ref!=row.first)fail("PrivateShopExactStaffOperationRequired",row.first);operations.push_back(row.first);
  }
  const auto decision=take(c.next_id("private_shop_staff_decision"));take(c.append_fact({decision,"private_shop_staff_ended",private_shop_module}));
  if(!s.staff.decisions.insert(decision,{decision,id,{},reason,at.half_day_tick,cause}))fail("PrivateShopStaffDuplicateDecision",decision);
  auto*contract=s.staff.contracts.find(id);contract->status=reason;contract->terminal_fact_id=decision;
  take(read_private_shop_exact_staff_terminal(x,{private_shop_module,"private_shop_staff_ended",decision}));
  for(const auto&operation:operations)results.push_back(take(interrupt_private_shop_exact_staff_operation_in_candidate(c,e,s,sources,capabilities,elapsed,x,operation,{private_shop_module,"private_shop_staff_ended",decision})));
 }
 take(validate_private_shop_staff_services(x.shops));return {true,std::move(results),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
