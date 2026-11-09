#include "resident/domains/military/military_model.hpp"
#include "political_shop_registry_control.hpp"
#include "political_shop_licensing_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_state.hpp"
#include "resident/domains/military/occupation_rules.hpp"
#include <map>
#include <limits>
namespace hanmo::resident {
Outcome<PoliticalShopRegistryControlSource> read_political_shop_registry_control(const PoliticalShopControlContext& x,const PoliticsState& politics,const PoliticsCatalog& catalog,const PoliticalShopCourt& court,const StableId& boundary,std::optional<Integer> boundary_tick){
 const auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<PoliticalShopRegistryControlSource>{return {false,{},common_failure(code,id).error};};
 const auto& common=x.common;const auto* original=catalog.jurisdictions.find(court.jurisdiction_id);const auto* current=politics.jurisdictions.find(court.jurisdiction_id);
 if(!original||!current||original->revision!=0||!x.catalog.enabled||x.catalog.version!=1||x.state.version!=1||common.tick()<0)return fail("PoliticalShopControlHistoryUnavailable",court.id);
 Integer observed=common.tick(),cutoff=common.state().next_instance_id,limit=common.tick();
 if(x.phase){if(limit==std::numeric_limits<Integer>::max()||x.phase->from_tick!=limit||x.phase->to_tick!=limit+1)return fail("PoliticalShopControlPhaseMismatch");limit=x.phase->to_tick;}
 std::map<StableId,std::size_t> positions;for(std::size_t i=0;i<common.state().facts.size();++i)if(!positions.emplace(common.state().facts.rows()[i].first,i).second)return fail("PoliticalShopControlOrderMismatch");
 auto prior=[&](const StableId& a,const StableId& b){return positions.count(a)&&positions.count(b)&&positions.at(a)<positions.at(b);};
 if(!boundary.empty()){
  const auto* head=common.state().facts.find(boundary);if(!head||head->id!=boundary||!boundary_tick||*boundary_tick<0||*boundary_tick>common.tick())return fail("PoliticalShopRegistryBoundaryRequired",boundary);
  if(head->kind==political_shop_registry_accept_kind&&head->domain==political_shop_registry_succession_module){auto n=organization_instance_sequence(boundary,political_shop_registry_accept_prefix);if(!n||*n>=cutoff)return fail("PoliticalShopRegistryBoundaryRequired",boundary);cutoff=*n;}
  else {auto n=shop_law_detail::fact_sequence(common,boundary,head->kind);if(!n.ok)return {false,{},n.error};cutoff=n.value;}
  observed=*boundary_tick;
 }else if(boundary_tick&&*boundary_tick!=observed)return fail("PoliticalShopRegistryBoundaryRequired");
 PoliticalShopRegistryControlSource result;result.source_identity=common.state().identity;result.organization_id=original->controlling_organization_id;result.observed_tick=observed;
 std::map<Integer,const PoliticalTakeoverRegistration*> registrations;
 for(const auto& row:x.state.registrations.rows()){
  const auto* rule=x.catalog.rules.find(row.second.rule_id);if(!rule)return fail("PoliticalShopControlHistoryMismatch",row.first);if(rule->jurisdiction_id!=court.jurisdiction_id)continue;
  auto n=organization_instance_sequence(row.first,"political_takeover_pending");if(row.first!=row.second.id||!n||!registrations.emplace(*n,&row.second).second)return fail("PoliticalShopControlHistoryMismatch",row.first);
 }
 StableId owner=original->controlling_organization_id,previous_fact;Integer revision=0,previous_tick=-1;
 for(const auto& entry:registrations){const auto& r=*entry.second;const auto* rule=x.catalog.rules.find(r.rule_id);const auto* h=common.state().facts.find(r.id);
  if(entry.first>=common.state().next_instance_id||r.captured_next_instance_id!=entry.first||r.tick<previous_tick||r.tick<0||r.tick>limit||r.former_organization_id!=owner||r.source_identity.world_id!=common.state().identity.world_id||!h||h->id!=r.id||h->kind!=political_takeover_pending_fact_kind||h->domain!="politics"||(!previous_fact.empty()&&!prior(previous_fact,r.id)))return fail("PoliticalShopControlHistoryMismatch",r.id);
  const MilitaryOccupationFact* occupation=nullptr;for(const auto& f:x.military.occupation_facts)if(f.id==r.occupation_fact_id){if(occupation)return fail("PoliticalShopControlHistoryMismatch",r.id);occupation=&f;}
  auto n=organization_instance_sequence(r.occupation_fact_id,"military");
  if(!occupation||!occupation->assets_applied.value_or(false)||!occupation->asset_manifest||!n||*n==std::numeric_limits<Integer>::max()||*n+1!=entry.first||!prior(occupation->id,r.id)||occupation->tick!=r.tick||occupation->from_organization_id!=r.former_organization_id||occupation->to_organization_id!=r.successor_organization_id||occupation->stronghold_id!=rule->stronghold_id||occupation->garrison_id!=rule->garrison_id||occupation->public_wallet_ids!=r.public_wallet_ids||occupation->public_inventory_ids!=r.public_inventory_ids)return fail("PoliticalShopControlHistoryMismatch",r.id);
  auto checked=x.phase?validate_applied_occupation_manifest(common,x.military,x.military_catalog,x.military_access,occupation->id,*x.phase):validate_applied_occupation_manifest(common,x.military,x.military_catalog,x.military_access,occupation->id);if(!checked.ok)return {false,{},checked.error};
  const bool earlier=entry.first<cutoff;if((earlier&&r.tick>observed)||(!boundary.empty()&&(earlier!=prior(r.id,boundary)||(!earlier&&r.tick<observed))))return fail("PoliticalShopControlOrderMismatch",r.id);
  owner=r.successor_organization_id;++revision;previous_fact=r.id;previous_tick=r.tick;
  if(earlier){result.organization_id=owner;result.revision=revision;result.registration_fact_id=r.id;result.occupation_fact_id=r.occupation_fact_id;result.occupation_tick=r.tick;}
 }
 if(current->revision!=revision||current->controlling_organization_id!=owner||current->political_organization_id!=original->political_organization_id)return fail("PoliticalShopControlHistoryUnavailable",court.id);
 // 每个登记的批准链独立保留；只选截面前最新占领的最新实际批准，不把之后签字倒填。
 std::map<StableId,StableId> previous;Integer last_sequence=0,last_tick=-1;
 for(const auto& row:x.state.approvals.rows()){
  const auto& a=row.second;const auto* r=x.state.registrations.find(a.registration_fact_id);if(!r)return fail("PoliticalShopRegistryApprovalSourceRequired",row.first);
  const auto* rule=x.catalog.rules.find(r->rule_id);if(!rule)return fail("PoliticalShopRegistryApprovalSourceRequired",row.first);if(rule->jurisdiction_id!=court.jurisdiction_id)continue;
  auto n=organization_instance_sequence(row.first,"political_takeover");const auto* h=common.state().facts.find(row.first);auto prev=previous.find(r->id);const auto expected=prev==previous.end()?std::nullopt:std::optional<StableId>{prev->second};
  if(row.first!=a.id||!n||*n<=last_sequence||*n>=common.state().next_instance_id||a.tick<last_tick||a.tick<r->tick||a.tick>common.tick()||!h||h->id!=a.id||h->kind!=political_takeover_fact_kind||h->domain!="politics"||a.authority.organization_id!=r->successor_organization_id||a.previous_approval_fact_id!=expected||!prior(r->id,a.id)||(prev!=previous.end()&&!prior(prev->second,a.id)))return fail("PoliticalShopRegistryApprovalSourceRequired",a.id);
  const PoliticalTakeoverRegistration* controlled=nullptr;for(const auto& entry:registrations){if(entry.first>=*n)break;controlled=entry.second;}
  if(controlled!=r)return fail("PoliticalShopRegistryApprovalControlMismatch",a.id);
  const bool earlier=*n<cutoff;if(!boundary.empty()&&(earlier!=prior(a.id,boundary)||(earlier&&a.tick>observed)||(!earlier&&a.tick<observed)))return fail("PoliticalShopRegistryApprovalOrderMismatch",a.id);
  if(earlier&&r->id==result.registration_fact_id)result.approval_fact_id=a.id;
  previous[r->id]=a.id;last_sequence=*n;last_tick=a.tick;
 }
 return {true,std::move(result),{}};
}
}
