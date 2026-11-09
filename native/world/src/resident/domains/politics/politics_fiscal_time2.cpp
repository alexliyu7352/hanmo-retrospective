#include "politics_fiscal_time2.hpp"
#include "politics_internal.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "takeover/politics_occupation_fiscal.hpp"
#include "resident/domains/military/occupation_time.hpp"
#include <algorithm>
#include <map>
namespace hanmo::resident {namespace {
constexpr const char* fiscal_kind="political_fiscal_policy_changed";
Status dependencies(const PoliticalFiscalTime2Context& x){
 if(x.catalog.version!=2||x.state.version!=2||!x.organization_catalog.authority_lifecycle_enabled||!x.organization_catalog.exact_fact_clock_enabled)
  return common_failure("PoliticalFiscalTime2DependenciesRequired");
 if(&x.ages.common!=&x.common)return common_failure("PoliticalFiscalAgeWorldMismatch");
 if(bool(x.occupation)!=bool(x.takeover))return common_failure("PoliticalFiscalTakeoverPairRequired");
 if(x.takeover&&(&x.takeover->common!=&x.common||&x.takeover->organizations!=&x.organizations||&x.takeover->organization_catalog!=&x.organization_catalog||x.takeover->organization_runtime!=&x.runtime||x.takeover->politics_state!=&x.politics||!x.takeover->actual_clock||x.takeover->catalog.version!=2||x.takeover->state.version!=2))return common_failure("PoliticalFiscalTakeoverWorldMismatch");
 return common_success();
}
bool valid_mode(FiscalTaxMode m){return m==FiscalTaxMode::lenient||m==FiscalTaxMode::normal||m==FiscalTaxMode::surcharge;}
Integer revision(const PoliticalFiscalState& s,const StableId& jurisdiction){Integer out=0;for(const auto& row:s.policies.rows())if(row.second.snapshot.jurisdiction_id==jurisdiction)out=std::max(out,row.second.snapshot.policy_revision);return out;}
SingleGrantAuthorityRequest request(const ActingAuthority& acting,const FiscalPolicySnapshot& p){
 SingleGrantAuthorityRequest out;out.acting=acting;out.command_kind="political_set_tax";out.scope={ScopeRef::Kind::site,p.site_id};
 out.resources={{ResourceKey::Kind::wallet,p.public_wallet_id},{ResourceKey::Kind::inventory,p.public_inventory_id}};return out;
}
FiscalPolicySnapshot selection(const Jurisdiction& j,const PoliticalSetTaxCommand& command){
 FiscalPolicySnapshot p;p.jurisdiction_id=j.id;p.site_id=j.seat_place_id;p.organization_id=j.political_organization_id;
 p.public_wallet_id=j.public_wallet_id;p.public_inventory_id=command.public_inventory_id;p.mode=command.mode;return p;
}
Status age_at(const PoliticalFiscalTime2Context& x,const StableId& actor,const WorldMoment& at,const StableId& before={}){
 auto basis=read_life_age_basis(x.ages,actor,at,before);if(!basis.ok)return {false,{},basis.error};
 return basis.value.age_years>=18?common_success():common_failure("OfficeRequiresAdult",actor);
}
Status source_shape(const PoliticalFiscalTime2Context& x,const FiscalPolicyRecord& r){
 auto dep=dependencies(x);if(!dep.ok)return dep;
 const auto* h=x.common.state().facts.find(r.id);const auto n=organization_instance_sequence(r.id,"politics_fiscal");const auto& p=r.snapshot;
 if(!n||*n>=x.common.state().next_instance_id||!h||h->id!=r.id||h->domain!="politics"||h->kind!=fiscal_kind||!r.issued_at||p.source_fact_id!=r.id||p.policy_revision<1||p.issued_tick<0||p.issued_tick==INT64_MAX||p.issued_tick!=r.issued_at->half_day_tick||p.effective_from_tick!=p.issued_tick+1||!valid_mode(p.mode))return common_failure("InvalidFiscalPolicyTime2Source",r.id);
 auto canonical=encode_world_moment(*r.issued_at);if(!canonical.ok)return {false,{},canonical.error};
 auto now=read_organization_authority_now(x.common,&x.clock);if(!now.ok)return {false,{},now.error};
 auto compared=compare_world_moments(*r.issued_at,now.value);if(!compared.ok)return {false,{},compared.error};
 return compared.value>0?common_failure("FiscalPolicyIssueInFuture",r.id):common_success();
}
}
Outcome<PoliticalFiscalState> initialize_political_fiscal2(const PoliticalFiscalCatalog& catalog){
 if(catalog.version!=2)return {false,{},common_failure("PoliticalFiscalTime2CatalogRequired").error};
 PoliticalFiscalState state;state.version=2;return {true,std::move(state),{}};
}
Outcome<WorldMoment> NativePoliticalFiscalAuthorityRead::read_organization_authority_now()const{return hanmo::resident::read_organization_authority_now(context_.common,&context_.clock);}
Outcome<AuthorityIssueSourceView> NativePoliticalFiscalAuthorityRead::read_authority_issue_source(const SourceRef& source)const{
 auto at=read_political_fiscal_source_moment2(context_,source,2);if(!at.ok)return {false,{},at.error};
 const auto* row=context_.state.policies.find(source.id);if(!row||row->issuer_actor_id.empty())return {false,{},common_failure("FiscalPolicyIssuerMissing",source.id).error};
 ActorFoundationAt actual;actual.kind=ActorFoundationAt::Kind::exact_moment;actual.exact_moment=at.value;
 return {true,{source,row->issuer_actor_id,"political_set_tax",std::move(actual),{}},{}};
}
Outcome<WorldMoment> read_political_fiscal_source_moment2(const PoliticalFiscalTime2Context& x,const SourceRef& source,Integer schema){
 if(schema!=2||source.domain!="politics"||source.kind!=fiscal_kind)return {false,{},common_failure("PoliticalFiscalTime2SourceRequired",source.id).error};
 const auto* r=x.state.policies.find(source.id);if(!r||r->id!=source.id)return {false,{},common_failure("FiscalPolicyOriginalRecordMissing",source.id).error};
 auto valid=source_shape(x,*r);return {valid.ok,valid.ok?*r->issued_at:WorldMoment{},valid.error};
}
Status preview_political_set_tax2(const PoliticalFiscalTime2Context& x,const ActivityRead& activity,const StableId& actor,const PoliticalSetTaxCommand& command){
 auto dep=dependencies(x);if(!dep.ok)return dep;
 if(!x.catalog.enabled)return common_failure("PoliticalFiscalNotEnabled",{},ErrorKind::unsupported_capability);
 if(actor!=command.authority.actor_id)return common_failure("PoliticalCommandActorMismatch");
 if(!valid_mode(command.mode))return common_failure("InvalidFiscalTaxMode");
 const auto* j=x.politics.jurisdictions.find(command.jurisdiction_id);if(!j)return common_failure("UnknownPoliticalJurisdiction",command.jurisdiction_id);
 if(x.takeover){auto original=require_legacy_fiscal_before_occupation(*x.takeover,j->id);if(!original.ok)return original;}
 if(j->revision!=command.expected_jurisdiction_revision)return common_failure("StalePoliticalJurisdiction",j->id);
 if(j->political_organization_id!=command.authority.organization_id||j->controlling_organization_id!=command.authority.organization_id)return common_failure("PoliticalControlDenied",j->id);
 if(revision(x.state,j->id)!=command.expected_policy_revision)return common_failure("StaleFiscalPolicy",j->id);
 auto now=read_organization_authority_now(x.common,&x.clock);if(!now.ok)return {false,{},now.error};
 if(command.expected_policy_revision==INT64_MAX||now.value.half_day_tick==INT64_MAX)return common_failure("FiscalPolicyCounterOverflow",j->id);
 auto available=politics_detail::available(x.common,activity,actor);if(!available.ok)return available;
 auto age=age_at(x,actor,now.value);if(!age.ok)return age;
 const auto here=politics_detail::place(x.common,actor);if(!here||here->place_id!=j->seat_place_id)return common_failure("FiscalPolicyRequiresLocalAuthority",j->seat_place_id);
 if(std::find(j->public_inventory_ids.begin(),j->public_inventory_ids.end(),command.public_inventory_id)==j->public_inventory_ids.end())return common_failure("FiscalPublicInventoryMismatch",command.public_inventory_id);
 const auto* wallet=x.common.wallet(j->public_wallet_id);const auto* stock=x.common.inventory(command.public_inventory_id);
 if(!wallet||!stock||wallet->owner.kind!=Owner::Kind::organization||stock->owner.kind!=Owner::Kind::organization||wallet->owner.id!=j->political_organization_id||stock->owner.id!=j->political_organization_id)return common_failure("FiscalPublicAssetMismatch",j->id);
 auto authority=check_current_grant_authority(x.common,x.organizations,x.organization_catalog,x.organization_catalog.authority_catalog_identity,request(command.authority,selection(*j,command)),&x.clock);
 return authority.ok?common_success():Status{false,{},authority.error};
}
Outcome<PoliticalFiscalEffects> apply_political_set_tax2_in_candidate(CommonStore& common,PoliticalFiscalState& state,OrganizationRuntime& runtime,const PoliticalFiscalTime2Context& x,const ActivityRead& activity,const StableId& actor,const PoliticalSetTaxCommand& command){
 if(&common!=&x.common||&state!=&x.state||&runtime!=&x.runtime)return {false,{},common_failure("PoliticalFiscalCandidateOwnershipMismatch").error};
 auto checked=preview_political_set_tax2(x,activity,actor,command);if(!checked.ok)return {false,{},checked.error};
 const auto& j=*x.politics.jurisdictions.find(command.jurisdiction_id);auto p=selection(j,command);auto req=request(command.authority,p);
 // 实际签发前捕获同一单Grant；本候选后续不重新使用今日Grant解释旧政策。
 auto proof=check_current_grant_authority(common,x.organizations,x.organization_catalog,x.organization_catalog.authority_catalog_identity,req,&x.clock);if(!proof.ok)return {false,{},proof.error};
 auto now=read_organization_authority_now(common,&x.clock);if(!now.ok)return {false,{},now.error};
 auto id=common.next_id("politics_fiscal");if(!id.ok)return {false,{},id.error};
 FiscalPolicyRecord r;r.id=id.value;r.issuer_actor_id=actor;r.issuer_office_id=command.authority.office_id;r.authority_epoch=command.authority.authority_epoch;r.grant_id=proof.value.effective_grant.id;r.issued_at=now.value;
 p.source_fact_id=r.id;p.policy_revision=command.expected_policy_revision+1;p.issued_tick=now.value.half_day_tick;p.effective_from_tick=p.issued_tick+1;r.snapshot=p;
 auto appended=common.append_fact({r.id,fiscal_kind,"politics"});if(!appended.ok)return {false,{},appended.error};
 if(!state.policies.insert(id.value,std::move(r)))return {false,{},common_failure("DuplicateFiscalPolicySource",id.value).error};
 NativePoliticalFiscalAuthorityRead sources(x);
 // business source/2与唯一AuthorityBasis/1是不同协议；只存原registry，不另复制权源。
 auto basis=register_authority_basis_for_fact(common,runtime.authority_bases,x.organizations,x.organization_catalog,x.organization_catalog.authority_catalog_identity,req,proof.value,{"politics",fiscal_kind,id.value},sources);
 if(!basis.ok)return {false,{},basis.error};
 auto valid=validate_political_fiscal_record2(x,*state.policies.find(id.value));if(!valid.ok)return {false,{},valid.error};
 return {true,{id.value,p.policy_revision,p.effective_from_tick},{}};
}
Status validate_political_fiscal_occupation_boundary2(const PoliticalFiscalTime2Context& x,const FiscalPolicyOccupationBoundary& boundary){
 if(!boundary.actual_occupation_at)return common_failure("FiscalOccupationActualMomentRequired",boundary.occupation_fact_id,ErrorKind::unsupported_capability);
 const auto* header=x.common.state().facts.find(boundary.occupation_fact_id);const auto& identity=x.common.state().identity;
 if(!header||header->domain!="military"||header->kind!="military_occupation"||boundary.actual_occupation_at->half_day_tick!=boundary.occupation_tick||boundary.source_identity.world_id!=identity.world_id||boundary.source_identity.execution_epoch!=identity.execution_epoch||boundary.source_identity.revision!=identity.revision||boundary.source_identity.tick!=identity.tick)return common_failure("FiscalOccupationMomentSourceMismatch",boundary.occupation_fact_id);
 auto canonical=encode_world_moment(*boundary.actual_occupation_at);if(!canonical.ok)return {false,{},canonical.error};
 auto now=read_organization_authority_now(x.common,&x.clock);if(!now.ok)return {false,{},now.error};
 if(!x.takeover)return common_failure("FiscalOccupationOriginalReaderRequired",boundary.occupation_fact_id);
 // Re-read S2's original precise producer/terminal row. The projected boundary
 // cannot certify its own timestamp, and /1 tick-only rows remain unsupported.
 auto original=read_military_occupation_actual_moment(x.common,x.takeover->military,x.takeover->military_catalog,
  {"military","military_occupation",boundary.occupation_fact_id},2,now.value);
 if(!original.ok)return {false,{},original.error};
 auto same=compare_world_moments(original.value,*boundary.actual_occupation_at);if(!same.ok)return {false,{},same.error};
 return same.value==0?common_success():common_failure("FiscalOccupationMomentSourceMismatch",boundary.occupation_fact_id);
}
Status validate_political_fiscal_record2(const PoliticalFiscalTime2Context& x,const FiscalPolicyRecord& r){
 auto shape=source_shape(x,r);if(!shape.ok)return shape;const auto& p=r.snapshot;
 const auto* j=x.politics.jurisdictions.find(p.jurisdiction_id);const auto* office=x.organizations.offices.find(r.issuer_office_id);
 if(!x.catalog.enabled||!j||!x.common_catalog.places.find(p.site_id)||j->seat_place_id!=p.site_id||j->political_organization_id!=p.organization_id||j->public_wallet_id!=p.public_wallet_id||std::find(j->public_inventory_ids.begin(),j->public_inventory_ids.end(),p.public_inventory_id)==j->public_inventory_ids.end()||!x.common.actor(r.issuer_actor_id)||!office||office->organization_id!=p.organization_id||r.authority_epoch<0)return common_failure("InvalidFiscalPolicyRecord",r.id);
 if(x.takeover){const auto& order=x.common.state().facts.order();auto issued=std::find(order.begin(),order.end(),r.id);for(const auto& row:x.takeover->state.registrations.rows()){const auto* rule=x.takeover->catalog.rules.find(row.second.rule_id);if(!rule||rule->jurisdiction_id!=p.jurisdiction_id)continue;auto captured=std::find(order.begin(),order.end(),row.second.occupation_fact_id);if(issued==order.end()||captured==order.end())return common_failure("FiscalPolicyControlSourceOrder",r.id);if(captured<issued)return common_failure("LegacyFiscalPolicyAfterOccupation",r.id);}}
 if(x.occupation){auto boundary=x.occupation->fiscal_policy_occupation(p);if(!boundary.ok)return {false,{},boundary.error};if(boundary.value){auto verified=validate_political_fiscal_occupation_boundary2(x,*boundary.value);if(!verified.ok)return verified;}}
 // 原署名之前的年龄基线；离任、死亡、换Grant不会把已签发责任倒改成无效。
 auto age=age_at(x,r.issuer_actor_id,*r.issued_at,r.id);if(!age.ok)return age;
 NativePoliticalFiscalAuthorityRead sources(x);
 auto basis=read_registered_authority_basis(x.common,x.runtime.authority_bases,x.organizations,x.organization_catalog,x.organization_catalog.authority_catalog_identity,{{"politics",fiscal_kind,r.id},1},sources);
 if(!basis.ok)return {false,{},basis.error};const auto& b=*basis.value;
 const ActingAuthority acting{p.organization_id,r.issuer_office_id,r.authority_epoch,r.issuer_actor_id};auto expected=request(acting,p);
 if(b.actor_id!=r.issuer_actor_id||b.organization_id!=p.organization_id||b.office_id!=r.issuer_office_id||b.office_epoch!=r.authority_epoch||b.grant_id!=r.grant_id||b.command_kind!=expected.command_kind||b.scope.kind!=expected.scope.kind||b.scope.id!=expected.scope.id||b.amount!=0||b.budget_ref||!b.amount_components.empty()||b.resources.size()!=expected.resources.size())return common_failure("FiscalPolicyAuthorityBasisMismatch",r.id);
 for(const auto& key:expected.resources)if(std::count_if(b.resources.begin(),b.resources.end(),[&](const auto& actual){return key.kind==actual.kind&&key.id==actual.id;})!=1)return common_failure("FiscalPolicyAuthorityResourcesMismatch",r.id);
 return common_success();
}
Status validate_political_fiscal2(const PoliticalFiscalTime2Context& x){
 auto dep=dependencies(x);if(!dep.ok)return dep;std::map<StableId,Integer> revisions;std::optional<WorldMoment> previous;std::size_t previous_sequence=0;bool first=true;const auto& order=x.common.state().facts.order();
 for(const auto& row:x.state.policies.rows()){
  const auto& r=row.second;if(row.first!=r.id)return common_failure("FiscalPolicyKeyMismatch",row.first);auto valid=validate_political_fiscal_record2(x,r);if(!valid.ok)return valid;
  const auto found=std::find(order.begin(),order.end(),r.id);if(found==order.end())return common_failure("FiscalPolicySourceOrder",r.id);const auto sequence=static_cast<std::size_t>(found-order.begin());
  if(!first&&sequence<=previous_sequence)return common_failure("FiscalPolicySourceOrder",r.id);
  if(previous){auto cmp=compare_world_moments(*previous,*r.issued_at);if(!cmp.ok)return {false,{},cmp.error};if(cmp.value>0)return common_failure("FiscalPolicyMomentOrder",r.id);}
  auto& revision=revisions[r.snapshot.jurisdiction_id];if(revision==INT64_MAX||r.snapshot.policy_revision!=revision+1)return common_failure("FiscalPolicyHistoryMismatch",r.id);revision=r.snapshot.policy_revision;previous=r.issued_at;previous_sequence=sequence;first=false;
 }
 for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="politics"&&row.second.kind==fiscal_kind&&!x.state.policies.find(row.first))return common_failure("OrphanFiscalPolicySource",row.first);
 for(const auto& row:x.runtime.authority_bases.rows())if(row.second.issued_source.domain=="politics"&&row.second.issued_source.kind==fiscal_kind&&!x.state.policies.find(row.second.issued_source.id))return common_failure("OrphanFiscalPolicyAuthorityBasis",row.first);
 return common_success();
}
}
