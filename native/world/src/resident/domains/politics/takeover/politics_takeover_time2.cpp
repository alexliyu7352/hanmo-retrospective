#include "resident/domains/military/military_model.hpp"
// 接管/2使用原共同append顺序与原单Grant历史；绝不由今日Owner或tick补精确时刻。
#include "politics_takeover_time2.hpp"
#include "politics_takeover_detail.hpp"
namespace hanmo::resident {
namespace {
Outcome<WorldMoment> missing(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
Status check_moment(const PoliticalTakeoverContext& x,const WorldMoment& at,Integer tick,const StableId& id){
 if(!x.actual_clock)return common_failure("PoliticalTakeoverActualClockRequired",id);
 auto canonical=encode_world_moment(at);if(!canonical.ok)return {false,{},canonical.error};
 if(at.half_day_tick!=tick)return common_failure("PoliticalTakeoverTickProjectionMismatch",id);
 auto now=read_organization_authority_now(x.common,x.actual_clock);if(!now.ok)return {false,{},now.error};
 auto compared=compare_world_moments(at,now.value);if(!compared.ok)return {false,{},compared.error};
 return compared.value>0?common_failure("PoliticalTakeoverSourceInFuture",id):common_success();
}
}
Outcome<WorldMoment> NativePoliticalTakeoverAuthorityRead::read_organization_authority_now()const{
 if(!context_.actual_clock)return missing("PoliticalTakeoverActualClockRequired");
 return hanmo::resident::read_organization_authority_now(context_.common,context_.actual_clock);
}
Outcome<AuthorityIssueSourceView> NativePoliticalTakeoverAuthorityRead::read_authority_issue_source(const SourceRef& source)const{
 auto time=read_political_takeover_source_moment2(context_,source,2);if(!time.ok)return {false,{},time.error};
 const auto* approval=context_.state.approvals.find(source.id);
 if(source.kind!=political_takeover_fact_kind||!approval)return {false,{},common_failure("PoliticalTakeoverNotAuthorityIssue",source.id).error};
 ActorFoundationAt at;at.kind=ActorFoundationAt::Kind::exact_moment;at.exact_moment=time.value;
 return {true,{source,approval->authority.actor_id,political_takeover_command_kind,std::move(at),{}},{}};
}
SingleGrantAuthorityRequest political_takeover_authority_request(const ActingAuthority& acting,const PoliticalTakeoverRegistration& registration,const PoliticalTakeoverRule& rule){
 SingleGrantAuthorityRequest request;request.acting=acting;request.command_kind=political_takeover_command_kind;request.scope={ScopeRef::Kind::site,rule.site_id};
 for(const auto& id:registration.public_wallet_ids)request.resources.push_back({ResourceKey::Kind::wallet,id});
 for(const auto& id:registration.public_inventory_ids)request.resources.push_back({ResourceKey::Kind::inventory,id});
 return request;
}
Outcome<SingleGrantAuthorityRead> read_current_political_takeover_authority(const PoliticalTakeoverContext& x,const ActingAuthority& acting,const PoliticalTakeoverRegistration& registration,const PoliticalTakeoverRule& rule){
 if(x.catalog.version!=2||x.state.version!=2||!x.actual_clock||!x.organization_runtime||!x.organization_catalog.authority_lifecycle_enabled||!x.organization_catalog.exact_fact_clock_enabled)return {false,{},common_failure("PoliticalTakeoverTime2AuthorityRequired").error};
 return check_current_grant_authority(x.common,x.organizations,x.organization_catalog,x.organization_catalog.authority_catalog_identity,political_takeover_authority_request(acting,registration,rule),x.actual_clock);
}
Status validate_political_takeover_approval_authority2(const PoliticalTakeoverContext& x,const PoliticalTakeoverApprovalRecord& approval){
 if(!x.organization_runtime||!x.actual_clock)return common_failure("PoliticalTakeoverTime2AuthorityRequired",approval.id);
 const auto* registration=x.state.registrations.find(approval.registration_fact_id);const auto* rule=registration?x.catalog.rules.find(registration->rule_id):nullptr;
 if(!registration||!rule)return common_failure("PoliticalTakeoverApprovalSourceMissing",approval.id);
 NativePoliticalTakeoverAuthorityRead sources(x);
 // business source为2；此引用明确仍为原AuthorityBasis/1。
 auto basis=read_registered_authority_basis(x.common,x.organization_runtime->authority_bases,x.organizations,x.organization_catalog,x.organization_catalog.authority_catalog_identity,{{"politics",political_takeover_fact_kind,approval.id},1},sources);
 if(!basis.ok)return {false,{},basis.error};
 const auto& b=*basis.value;const auto expected=political_takeover_authority_request(approval.authority,*registration,*rule);
 if(b.actor_id!=approval.authority.actor_id||b.organization_id!=approval.authority.organization_id||b.office_id!=approval.authority.office_id||b.office_epoch!=approval.authority.authority_epoch||b.grant_id!=approval.grant_id||b.command_kind!=expected.command_kind||b.scope.kind!=expected.scope.kind||b.scope.id!=expected.scope.id||b.amount!=0||b.budget_ref||!b.amount_components.empty()||b.resources.size()!=expected.resources.size())return common_failure("PoliticalTakeoverAuthorityBasisMismatch",approval.id);
 for(const auto& resource:expected.resources)if(std::count_if(b.resources.begin(),b.resources.end(),[&](const auto& actual){return resource.kind==actual.kind&&resource.id==actual.id;})!=1)return common_failure("PoliticalTakeoverAuthorityResourcesMismatch",approval.id);
 return common_success();
}
Outcome<WorldMoment> read_political_takeover_source_moment2(const PoliticalTakeoverContext& x,const SourceRef& source,Integer schema){
 if(schema!=2||x.catalog.version!=2||x.state.version!=2||source.domain!="politics")return missing("PoliticalTakeoverTime2SourceRequired",source.id);
 const auto* header=x.common.state().facts.find(source.id);if(!header||header->id!=source.id||header->domain!=source.domain||header->kind!=source.kind)return missing("PoliticalTakeoverSourceHeaderMismatch",source.id);
 const WorldMoment* at=nullptr;Integer tick=0;
 if(source.kind==political_takeover_pending_fact_kind){const auto* r=x.state.registrations.find(source.id);if(!r||!r->registered_at)return missing("PoliticalTakeoverRegistrationMomentRequired",source.id);at=&*r->registered_at;tick=r->tick;}
 else if(source.kind==political_takeover_fact_kind){const auto* r=x.state.approvals.find(source.id);if(!r||!r->approved_at)return missing("PoliticalTakeoverApprovalMomentRequired",source.id);at=&*r->approved_at;tick=r->tick;}
 else if(source.kind==political_takeover_decision_fact_kind){const auto* d=x.state.decisions.find(source.id);const auto* a=d?x.state.approvals.find(d->approval_fact_id):nullptr;if(!d||!a||!a->approved_at||std::count(a->decision_fact_ids.begin(),a->decision_fact_ids.end(),source.id)!=1||d->decision.takeover_fact_id!=a->id)return missing("PoliticalTakeoverDecisionApprovalMissing",source.id);at=&*a->approved_at;tick=a->tick;}
 else return missing("PoliticalTakeoverTime2SourceKind",source.id);
 auto valid=check_moment(x,*at,tick,source.id);return {valid.ok,valid.ok?*at:WorldMoment{},valid.error};
}
Status validate_political_takeover_moments2(const PoliticalTakeoverContext& x){
 if(x.catalog.version!=2||x.state.version!=2||!x.actual_clock||!x.organization_runtime)return common_failure("PoliticalTakeoverTime2DependenciesRequired");
 const auto& order=x.common.state().facts.order();std::optional<WorldMoment> previous;
 // 扫实际共同采用序，既查跨表M单调，也拒非真实append的自称先后。
 for(const auto& id:order){const auto* head=x.common.state().facts.find(id);if(!head||head->domain!="politics"||(head->kind!=political_takeover_pending_fact_kind&&head->kind!=political_takeover_fact_kind&&head->kind!=political_takeover_decision_fact_kind))continue;
  auto at=read_political_takeover_source_moment2(x,{head->domain,head->kind,id},2);if(!at.ok)return {false,{},at.error};if(previous){auto cmp=compare_world_moments(*previous,at.value);if(!cmp.ok)return {false,{},cmp.error};if(cmp.value>0)return common_failure("PoliticalTakeoverMomentOrder",id);}previous=at.value;
  if(head->kind==political_takeover_fact_kind){const auto& a=*x.state.approvals.find(id);const auto* r=x.state.registrations.find(a.registration_fact_id);if(!r||!r->registered_at)return common_failure("PoliticalTakeoverRegistrationMomentRequired",id);auto before=std::find(order.begin(),order.end(),r->id),current=std::find(order.begin(),order.end(),id);if(before==order.end()||before>=current)return common_failure("PoliticalTakeoverApprovalBeforeRegistration",id);auto cmp=compare_world_moments(*r->registered_at,at.value);if(!cmp.ok)return {false,{},cmp.error};if(cmp.value>0)return common_failure("PoliticalTakeoverApprovalMomentOrder",id);}
 }
 for(const auto& r:x.state.registrations.rows()){auto at=read_political_takeover_source_moment2(x,{"politics",political_takeover_pending_fact_kind,r.first},2);if(!at.ok)return {false,{},at.error};}
 for(const auto& r:x.state.approvals.rows()){auto at=read_political_takeover_source_moment2(x,{"politics",political_takeover_fact_kind,r.first},2);if(!at.ok)return {false,{},at.error};}
 return common_success();
}
}
