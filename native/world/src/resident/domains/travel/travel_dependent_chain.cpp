#include "travel_dependent_state.hpp"
#include "resident/model/common_model.hpp"
#include "resident/storage/common_store.hpp"
#include "movement_exact_detail.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {using namespace movement_detail;namespace {
struct Chain {std::map<StableId,const TravelDependentBinding*> active;};
Chain fold(const CommonStore& common,const TravelDependentState& state,const std::optional<SourceRef>& before,ExactArithmeticBudget budget){
 const auto& order=common.state().facts.order();std::size_t cut=order.size();
 if(before){const auto* head=common.state().facts.find(before->id);const auto at=std::find(order.begin(),order.end(),before->id);if(!head||head->domain!=before->domain||head->kind!=before->kind||at==order.end())fail("TravelDependentActualCutRequired",before->id);cut=std::size_t(at-order.begin());}
 Chain result;std::set<StableId> seen_results,created_bindings,born_children;std::map<StableId,WorldMoment> last;
 for(std::size_t n=0;n<order.size();++n){const auto* head=common.state().facts.find(order[n]);if(!head)fail("TravelDependentCommonOrderMismatch",order[n]);if(head->domain!="travel"||head->kind!="travel_dependent_binding_change")continue;
  const auto* value=state.binding_change_results.find(head->id);if(!value||!seen_results.insert(head->id).second)fail("TravelDependentResultCoverageMismatch",head->id);
  if(n>=cut)continue;
  std::visit([&](const auto& row){using T=std::decay_t<decltype(row)>;
   if(row.id!=head->id||row.child_actor_id.empty()||!common.actor(row.child_actor_id)||row.before_binding_refs.size()>1||row.after_binding_refs.size()>1)fail("TravelDependentResultIdentityMismatch",head->id);
   take(encode_world_moment(row.occurred_at,budget));const auto previous=last.find(row.child_actor_id);if(previous!=last.end()&&take(compare_world_moments(previous->second,row.occurred_at,budget))>0)fail("TravelDependentResultTimeOrderMismatch",row.id);last[row.child_actor_id]=row.occurred_at;
   const auto active=result.active.find(row.child_actor_id);
   if(row.before_binding_refs.empty()){if(active!=result.active.end())fail("TravelDependentMissingBeforeBinding",row.id);}
   else if(active==result.active.end()||active->second->binding_id!=row.before_binding_refs.front())fail("TravelDependentBeforeBindingMismatch",row.id);
   if constexpr(std::is_same_v<T,BirthDependentBindingChange>){
    if(!row.before_binding_refs.empty()||!born_children.insert(row.child_actor_id).second)fail("TravelDependentBirthPredecessorMismatch",row.id);
   }else{
    if(row.actual_handover_source.source_schema!=3||row.actual_handover_source.source_ref.domain!="care_requirements"||row.actual_handover_source.source_ref.kind!="care_handover_execution"||row.carer_actor_id==row.child_actor_id||!common.actor(row.carer_actor_id)||!std::holds_alternative<SettlementLocation>(row.actual_location)||!row.after_binding_refs.empty())fail("TravelDependentCareResultShapeMismatch",row.id);
   }
   if(active!=result.active.end())result.active.erase(active);
   if(!row.after_binding_refs.empty()){
    const auto& id=row.after_binding_refs.front();const auto* binding=state.bindings.find(id);
    if(!binding||binding->binding_id!=id||binding->dependent_actor_id!=row.child_actor_id||binding->carrier_actor_id==row.child_actor_id||!common.actor(binding->carrier_actor_id)||binding->revision<1||binding->policy!=TravelDependentPolicy::follow_carrier_until_explicit_handover||!same_moment(binding->attached_at,row.occurred_at,budget)||!created_bindings.insert(id).second)fail("TravelDependentAfterBindingMismatch",row.id);
    result.active.emplace(row.child_actor_id,binding);
   }
  },*value);
 }
 if(seen_results.size()!=state.binding_change_results.size())fail("TravelDependentResultHeaderCoverageMismatch");
 if(!before&&created_bindings.size()!=state.bindings.size())fail("TravelDependentBindingOriginCoverageMismatch");
 for(const auto& item:result.active){std::set<StableId> visited;auto cursor=item.first;for(;;){if(!visited.insert(cursor).second)fail("TravelDependentBindingCycle",cursor);const auto next=result.active.find(cursor);if(next==result.active.end())break;cursor=next->second->carrier_actor_id;}}
 return result;
}
}
Outcome<const TravelDependentBinding*> read_active_travel_dependent_binding(const CommonStore& c,const TravelDependentState& s,const StableId& actor,const std::optional<SourceRef>& before,ExactArithmeticBudget budget){return guard<const TravelDependentBinding*>([&]{if(!c.actor(actor))fail("TravelDependentActorRequired",actor);auto actual=fold(c,s,before,budget);const auto it=actual.active.find(actor);return it==actual.active.end()?nullptr:it->second;});}
Status validate_travel_dependent_binding_chain(const CommonStore& c,const TravelDependentState& s,ExactArithmeticBudget budget){return status([&]{fold(c,s,{},budget);});}
}
