#include "political_budget_bucket.hpp"
#include "political_budget_history_order.hpp"
#include <algorithm>
#include <initializer_list>
#include <map>
#include <set>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
template<class T>T take(Outcome<T>r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
bool unit(const BudgetUnit&a,const BudgetUnit&b){if(a.index()!=b.index())return false;const auto*i=std::get_if<BudgetItemQuantity>(&a);return !i||i->item_id==std::get<BudgetItemQuantity>(b).item_id;}
bool period(const BudgetPeriod&a,const BudgetPeriod&b){return a.period_from_tick==b.period_from_tick&&a.period_until_tick==b.period_until_tick;}
bool has(const BudgetAuthorizationSource&s,BudgetExecutionPurpose p){return std::find(s.purposes.begin(),s.purposes.end(),p)!=s.purposes.end();}
std::size_t position(const CommonStore&c,const SourceRef&r){const auto&order=c.state().facts.order();const auto i=std::find(order.begin(),order.end(),r.id);const auto*h=c.state().facts.find(r.id);if(!h||h->id!=r.id||h->kind!=r.kind||h->domain!=r.domain||i==order.end())fail("PublicBudgetBucketActualHeaderRequired",r.id);return static_cast<std::size_t>(i-order.begin());}
const BudgetAuthorizationSource& root(const PoliticalBudgetState&s,const BudgetAuthorizationSource&source){
 const BudgetAuthorizationSource*p=&source;std::set<StableId>seen;
 while(p->parent_budget_ref){if(!seen.insert(p->budget_id).second)fail("PublicBudgetBucketParentCycle",source.budget_id);const auto&ref=*p->parent_budget_ref;const auto*parent=find_budget_authorization(s,ref.authorization_id,ref.authorization_revision);if(!parent||!same_source_ref(parent->approval_source,ref.approval_source)||parent->principal_org_id!=source.principal_org_id||!period(parent->period,source.period))fail("PublicBudgetBucketParentSourceMismatch",source.budget_id);p=parent;}
 const auto*first=find_budget_authorization(s,p->budget_id,1);if(!first||first->parent_budget_ref||first->decision!=PublicBudgetDecisionKind::authorize)fail("PublicBudgetBucketRootSourceRequired",source.budget_id);return *first;
}
std::vector<BudgetAuthorizationRef> reopening(const CommonStore&c,const PoliticalBudgetState&s,const StableId&principal,const BudgetPeriod&p,const std::vector<BudgetExecutionPurpose>&purposes,const SourceRef*boundary){
 const auto ordered=take(read_public_budget_authorizations_in_actual_order(c,s));
 std::map<StableId,const BudgetAuthorizationSource*>latest;const auto cut=boundary?position(c,*boundary):c.state().facts.order().size();
 for(const auto* source:ordered){const auto& row=*source;if(position(c,row.approval_source)<cut){const auto old=latest.find(row.budget_id);if(old==latest.end()||old->second->budget_revision<row.budget_revision)latest[row.budget_id]=&row;}}
 std::vector<BudgetAuthorizationRef>out;
 for(const auto* source:ordered){const auto& row=*source;const auto it=latest.find(row.budget_id);if(it==latest.end()||it->second!=&row||row.parent_budget_ref||!row.closed||row.principal_org_id!=principal||!period(row.period,p))continue;
  bool shared=false;for(const auto&category:row.category_caps)shared|=std::find(purposes.begin(),purposes.end(),category.purpose)!=purposes.end();
  if(shared)out.push_back({row.budget_id,row.budget_revision,row.approval_source});
 }
 return out;
}
bool active(const BudgetAuthorizationSource&s,const WorldMoment&at){return !s.closed&&take(compare_world_moments(WorldMoment{s.period.period_from_tick,{}},at))<=0&&take(compare_world_moments(at,WorldMoment{s.period.period_until_tick,{}}))<0;}
void scan(const CommonStore&c,const PoliticalBudgetState&s){
 if(s.schema_version!=2)fail("PublicBudgetBucketVersion2Required");
 std::map<StableId,const BudgetAuthorizationSource*>current;std::set<StableId> covered;std::size_t previous_position=0;bool first=true;
 for(const auto* source:take(read_public_budget_authorizations_in_actual_order(c,s))){const auto& row=*source;take(validate_budget_authorization_shape(row,2));const auto order=position(c,row.approval_source);if(!covered.insert(row.approval_source.id).second)fail("DuplicatePublicBudgetBucketSource",row.approval_source.id);if(!first&&order<=previous_position)fail("PublicBudgetBucketHistoryOrder",row.budget_id);first=false;previous_position=order;
  const auto prior=current.find(row.budget_id);
  if(prior==current.end()){if(row.budget_revision!=1)fail("PublicBudgetBucketFirstRevisionRequired",row.budget_id);}
  else {const auto&old=*prior->second;if(old.closed||old.budget_revision==INT64_MAX||row.budget_revision!=old.budget_revision+1||old.principal_org_id!=row.principal_org_id||!period(old.period,row.period)||bool(old.parent_budget_ref)!=bool(row.parent_budget_ref))fail("PublicBudgetBucketLineageChanged",row.budget_id);
   if(old.parent_budget_ref&&old.parent_budget_ref->authorization_id!=row.parent_budget_ref->authorization_id)fail("PublicBudgetBucketParentChanged",row.budget_id);
   for(auto purpose:row.purposes)if(!has(old,purpose))fail("PublicBudgetPurposeExpansionBeforeBoundary",row.budget_id);
   for(const auto&cap:old.category_caps)if(std::none_of(row.category_caps.begin(),row.category_caps.end(),[&](const auto&now){return cap.purpose==now.purpose&&unit(cap.unit,now.unit);}))fail("PublicBudgetCategoryHistoryCannotBeRemoved",row.budget_id);
  }
  const auto&original_root=root(s,row);if(position(c,original_root.approval_source)>order)fail("PublicBudgetBucketFutureRoot",row.budget_id);
  if(row.parent_budget_ref){const auto*p=find_budget_authorization(s,row.parent_budget_ref->authorization_id,row.parent_budget_ref->authorization_revision);if(!p||position(c,p->approval_source)>=order)fail("PublicBudgetBucketFutureParent",row.budget_id);
   for(const auto&category:row.category_caps){Integer old_cap=0;if(prior!=current.end())for(const auto&old:prior->second->category_caps)if(old.purpose==category.purpose&&unit(old.unit,category.unit))old_cap=old.cap;
    if(category.cap>old_cap&&!has(*p,category.purpose))fail("BudgetParentPurposeClosedForIncrease",row.budget_id);
   }
  }
  if(!row.parent_budget_ref){
   if(prior==current.end())for(const auto&ref:reopening(c,s,row.principal_org_id,row.period,row.purposes,&row.approval_source)){
    const auto*old=find_budget_authorization(s,ref.authorization_id,ref.authorization_revision);
    for(const auto&cap:old->category_caps)if(std::none_of(row.category_caps.begin(),row.category_caps.end(),[&](const auto&now){return cap.purpose==now.purpose&&unit(cap.unit,now.unit);}))fail("PublicBudgetReopeningCannotEraseCategory",row.budget_id);
   }
   for(const auto&entry:current){const auto&other=*entry.second;if(other.parent_budget_ref||other.budget_id==row.budget_id||other.principal_org_id!=row.principal_org_id)continue;
    bool shared=false;for(auto purpose:row.purposes)shared|=has(other,purpose);if(!shared)continue;
    if(active(row,row.approved_at)&&active(other,row.approved_at))fail("PublicBudgetConcurrentRootDenied",row.budget_id);
    if(prior==current.end()&&!period(row.period,other.period)){
     const auto at_start=take(compare_world_moments(row.approved_at,WorldMoment{row.period.period_from_tick,{}}));
     if(at_start||row.period.period_from_tick<other.period.period_until_tick)fail("PublicBudgetPeriodChangeRequiresBoundary",row.budget_id);
    }
   }
  }
  current[row.budget_id]=&row;
 }
 for(const auto&row:c.state().facts.rows()){const auto&fact=row.second;
  if(fact.domain=="politics"&&(fact.kind=="public_budget_authorized"||fact.kind=="public_budget_delegated"||fact.kind=="public_budget_amended"||fact.kind=="public_budget_closed")&&!covered.count(row.first))
   fail("PublicBudgetBucketPayloadMissing",row.first);
 }
}
}
Outcome<std::vector<BudgetAuthorizationRef>> read_public_budget_reopening_roots(const CommonStore&c,const PoliticalBudgetState&s,const StableId&principal,const BudgetPeriod&p,const std::vector<BudgetExecutionPurpose>&purposes,const SourceRef*boundary){try{return {true,reopening(c,s,principal,p,purposes,boundary),{}};}catch(const Failure&f){return {false,{},f.error};}}
Status validate_public_budget_bucket_usage(const CommonStore&c,const PoliticalBudgetState&s,const BudgetUsageRead&usage){try{
 scan(c,s);for(const auto&row:s.authorization_history){if(row.parent_budget_ref||current_budget_authorization(s,row.budget_id)!=&row)continue;for(const auto&category:row.category_caps){
  const auto v=take(usage.read_budget_bucket_usage({row.budget_id,row.budget_revision,row.period,category.unit,c.state().identity,0,64},category.purpose));
  if(v.spent_gross<0||v.committed_unpaid<0||v.total_spent_gross<0||v.total_committed_unpaid<0||v.spent_gross>INT64_MAX-v.committed_unpaid||v.total_spent_gross>INT64_MAX-v.total_committed_unpaid)fail("BudgetCapacityOverflow",row.budget_id);
  if(v.spent_gross+v.committed_unpaid>v.authorization.root_purpose_cap||v.total_spent_gross+v.total_committed_unpaid>v.authorization.root_total_cap)fail("BudgetRootCapacityBelowExistingLiability",row.budget_id);
  // Current descendants are already reserved by delegation, not charged again.
  // Only predecessor-root actual gross/unpaid augments this root's own leaf.
  const auto own=take(usage.read_budget_usage({row.budget_id,row.budget_revision,row.period,category.unit,c.state().identity,0,64}));
  const auto allocation=take(read_budget_allocation(s,row.budget_id,row.budget_revision,category.unit));
  const auto own_category=take(usage.read_budget_usage_for_purpose({row.budget_id,row.budget_revision,row.period,category.unit,c.state().identity,0,64},category.purpose));
  const auto category_allocation=take(read_budget_category_allocation(s,row.budget_id,row.budget_revision,category.unit,category.purpose));
  auto sum=[&](std::initializer_list<Integer>values){Integer out=0;for(auto value:values){if(value<0||out>INT64_MAX-value)fail("BudgetCapacityOverflow",row.budget_id);out+=value;}return out;};
  if(sum({own.spent_gross,own.committed_unpaid,allocation.delegated_cap,v.total_inherited_spent_gross,v.total_inherited_committed_unpaid})>v.authorization.root_total_cap||
     sum({own_category.spent_gross,own_category.committed_unpaid,category_allocation.delegated_cap,v.inherited_spent_gross,v.inherited_committed_unpaid})>v.authorization.root_purpose_cap)
   fail("BudgetRootCapacityBelowReservedInheritance",row.budget_id);
 }}return common_success();}catch(const Failure&f){return {false,{},f.error};}}
Status validate_public_budget_bucket_history(const CommonStore&c,const PoliticalBudgetState&s){try{scan(c,s);return common_success();}catch(const Failure&f){return {false,{},f.error};}}
Outcome<PublicBudgetBucketMembers> read_public_budget_bucket_members(const CommonStore&c,const PoliticalBudgetState&s,const StableId&id,Integer revision,const BudgetPeriod&p,const BudgetUnit&u,BudgetExecutionPurpose purpose){try{
 scan(c,s);const auto*source=find_budget_authorization(s,id,revision);if(!source||!period(source->period,p))fail("PublicBudgetBucketQueryMismatch",id);
 const auto&r=root(s,*source);PublicBudgetBucketMembers out;out.principal_org_id=source->principal_org_id;const auto*latest=current_budget_authorization(s,r.budget_id);if(!latest)fail("PublicBudgetBucketCurrentRootRequired",id);out.root={latest->budget_id,latest->budget_revision,latest->approval_source};out.period=p;out.purpose=purpose;out.unit=u;
 bool total=false,category=false;for(const auto&cap:latest->total_caps)if(unit(cap.unit,u)){out.authorized_root_total_cap=cap.cap;total=true;}for(const auto&cap:latest->category_caps)if(unit(cap.unit,u)&&cap.purpose==purpose){out.authorized_root_purpose_cap=cap.cap;category=true;}if(!total||!category)fail("PublicBudgetBucketCapRequired",id);
 std::set<BudgetExecutionPurpose> accounted;for(const auto&cap:latest->category_caps)if(unit(cap.unit,u))accounted.insert(cap.purpose);out.root_accounted_purposes.assign(accounted.begin(),accounted.end());
 std::set<StableId>prior_roots,current_members,prior_members;const auto root_order=position(c,r.approval_source);
 for(const auto&row:s.authorization_history)if(!row.parent_budget_ref&&row.principal_org_id==out.principal_org_id&&period(row.period,p)&&has(row,purpose)&&position(c,row.approval_source)<root_order)prior_roots.insert(row.budget_id);
 for(const auto&row:s.authorization_history){if(row.principal_org_id!=out.principal_org_id||!period(row.period,p))continue;if(!has(row,purpose)&&std::none_of(row.category_caps.begin(),row.category_caps.end(),[&](const auto&v){return v.purpose==purpose&&unit(v.unit,u);}))continue;const auto&owner=root(s,row);if(owner.budget_id==r.budget_id)current_members.insert(row.budget_id);else if(prior_roots.count(owner.budget_id))prior_members.insert(row.budget_id);}
 out.current_root_member_budget_ids.assign(current_members.begin(),current_members.end());out.inherited_root_member_budget_ids.assign(prior_members.begin(),prior_members.end());return {true,std::move(out),{}};
 }catch(const Failure&f){return {false,{},f.error};}}
}
