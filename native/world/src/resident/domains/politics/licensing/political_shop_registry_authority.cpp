#include "political_shop_member_history.hpp"
#include "political_shop_registry_detail.hpp"
#include "resident/domains/organization/office_authority.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include <limits>
namespace hanmo::resident::shop_registry_detail {
namespace {
bool covers(const Grant& g,const PoliticalShopRegistryEndpoint& e,Integer tick){return g.office_id==e.office_id&&g.wallet_ids.empty()&&g.inventory_ids.empty()&&g.amount_limit>=0&&g.valid_from<=tick&&(!g.valid_until||tick<=*g.valid_until)&&shop_law_detail::has(g.allowed_command_kinds,political_shop_registry_accept_command)&&std::any_of(g.scopes.begin(),g.scopes.end(),[&](const ScopeRef& s){return s.kind==ScopeRef::Kind::site&&s.id==e.site_id;});}
Status alive(const PoliticalShopRegistryContext& x,const StableId& actor,Integer tick,const StableId& boundary){
 const auto& l=x.legal;const auto* person=l.common.actor(actor);if(!person||tick<0||tick>l.common.tick())return common_failure("PoliticalShopRegistryLifeHistoryUnavailable",actor);
 if(boundary.empty())return shop_law_detail::person_alive_before(l,actor,tick,{});
 auto n=organization_instance_sequence(boundary,political_shop_registry_accept_prefix);const auto* h=l.common.state().facts.find(boundary);if(!n||*n>=l.common.state().next_instance_id||!h||h->id!=boundary||h->kind!=political_shop_registry_accept_kind||h->domain!=political_shop_registry_succession_module)return common_failure("PoliticalShopRegistryAcceptanceBoundaryRequired",boundary);
 if(!l.death_origin)return person->alive?common_success():common_failure("PoliticalShopRegistryLifeHistoryUnavailable",actor);
 auto death=l.death_origin(actor);if(!death.ok)return {false,{},death.error};if(!death.value)return person->alive?common_success():common_failure("PoliticalShopRegistryLifeHistoryUnavailable",actor);
 Integer limit=l.common.tick();if(l.phase){if(limit==std::numeric_limits<Integer>::max()||l.phase->from_tick!=limit||l.phase->to_tick!=limit+1)return common_failure("PoliticalShopLifePhaseMismatch");limit=l.phase->to_tick;}
 const auto& d=*death.value;auto seq=organization_instance_sequence(d.casualty_fact_id,"military");const auto* head=l.common.state().facts.find(d.casualty_fact_id);
 if(d.actor_id!=actor||d.battle_id.empty()||d.tick<0||d.tick>limit||!seq||*seq>=l.common.state().next_instance_id||!head||head->id!=d.casualty_fact_id||head->kind!="military_casualties"||head->domain!="military")return common_failure("PoliticalShopRegistryLifeHistoryUnavailable",actor);
 const bool earlier=*seq<*n;if(earlier!=shop_law_detail::before(l.common,d.casualty_fact_id,boundary)||(earlier&&d.tick>tick)||(!earlier&&d.tick<tick))return common_failure("PoliticalShopRegistryLifeOrderMismatch",actor);
 return earlier?common_failure("PoliticalShopActorAlreadyDead",actor):common_success();
}
}
Outcome<PoliticalShopRegistryEndpoint> endpoint(const PoliticalShopRegistryContext& x,const PoliticalShopRegistrySuccessorPolicy& p){
 const auto fail=[&](){return Outcome<PoliticalShopRegistryEndpoint>{false,{},common_failure("PoliticalShopRegistryPolicyMismatch",p.id).error};};const auto& l=x.legal;
 const auto* court=l.catalog.courts.find(p.court_id);const auto* j=court?l.politics_catalog.jurisdictions.find(court->jurisdiction_id):nullptr;const auto* office=l.organization_catalog.offices.find(p.office_id);const auto* role=l.common_catalog.roles.find(p.role_id);
 if(!court||!j||!office||office->organization_id!=p.organization_id||!role||role->organization_id!=p.organization_id||role->place_id!=j->seat_place_id||role->dialogue_id!=p.dialogue_id||p.id.empty())return fail();
 const DialogueDefinition* dialogue=nullptr;for(const auto& d:l.interaction_catalog.dialogues)if(d.id==p.dialogue_id){if(dialogue)return fail();dialogue=&d;}const DialogueNode* node=nullptr;if(dialogue)for(const auto& n:dialogue->nodes)if(n.id==p.request_node_id){if(node)return fail();node=&n;}
 if(!dialogue||dialogue->context_role!=p.role_id||!node||node->kind!="say"||node->speaker!="$provider")return fail();
 PoliticalShopRegistryEndpoint result{court->id,court->jurisdiction_id,p.organization_id,p.office_id,j->seat_place_id,p.role_id,p.dialogue_id,p.request_node_id};
 bool declared=false;for(const auto& id:office->grant_ids){const auto* g=l.organization_catalog.grants.find(id);if(g&&g->id==id&&covers(*g,result,g->valid_from))declared=true;}
 if(!declared)return {false,{},common_failure("PoliticalShopRegistryGrantDeclarationRequired",p.id).error};return {true,std::move(result),{}};
}
Status authority_before(const PoliticalShopRegistryContext& x,const PoliticalShopRegistryEndpoint& e,const ActingAuthority& a,const StableId& id,const StableId& boundary,Integer tick){
 const auto& l=x.legal;const auto* office=l.organization_catalog.offices.find(e.office_id);const auto* grant=l.organization_catalog.grants.find(id);auto sequence=organization_instance_sequence(boundary,political_shop_registry_accept_prefix);
 if(!office||!grant||!sequence||a.office_id!=e.office_id||a.organization_id!=e.organization_id||!shop_law_detail::has(office->grant_ids,id)||!covers(*grant,e,tick))return common_failure("PoliticalShopRegistryHistoricalGrantDenied",id);
 auto checked=validate_historical_office_signer(l.common,l.organizations,l.organization_catalog,a,tick,*sequence);if(!checked.ok)return checked;checked=validate_political_shop_historical_member_duty(l,a,tick,boundary);if(!checked.ok)return checked;return alive(x,a.actor_id,tick,boundary);
}
Outcome<StableId> authority_now(const PoliticalShopRegistryContext& x,const PoliticalShopRegistryEndpoint& e,const ActingAuthority& a){
 const auto& l=x.legal;auto fail=[&](){return Outcome<StableId>{false,{},common_failure("PoliticalShopRegistryAuthorityDenied",e.court_id).error};};
 if(a.office_id!=e.office_id||a.organization_id!=e.organization_id)return fail();auto checked=politics_detail::available(l.common,l.activity,a.actor_id);if(!checked.ok)return {false,{},checked.error};checked=alive(x,a.actor_id,l.common.tick(),{});if(!checked.ok)return {false,{},checked.error};auto place=politics_detail::place(l.common,a.actor_id);if(!place||place->place_id!=e.site_id)return fail();
 const auto* original=l.organization_catalog.offices.find(e.office_id);const auto* office=l.organizations.offices.find(e.office_id);const auto* role=l.common.state().roles.find(e.role_id);if(!original||!office||!role||!role->enabled||role->actor_id!=a.actor_id)return fail();OrganizationAccess organization(l.common,l.organizations,l.organization_catalog,l.economy);if(!organization.actor_accepts_duty(a.actor_id,a.organization_id))return fail();
 for (const auto &id : office->grant_ids) {
   const auto *g = l.organization_catalog.grants.find(id);
   if (!g || !shop_law_detail::has(original->grant_ids, id) ||
       !covers(*g, e, l.common.tick()))
     continue;
   auto checked = check_authority_with_single_grant(
       l.common, l.organizations, l.organization_catalog, a,
       political_shop_registry_accept_command,
       {ScopeRef::Kind::site, e.site_id}, 0, *g, nullptr);
   if (checked.ok)
     return {true, id, {}};
 }
 return fail();
}
const PoliticalShopRegistryAcceptance* latest(const PoliticalShopRegistryContext& x,const StableId& court,Integer sequence){const PoliticalShopRegistryAcceptance* found=nullptr;Integer last=0;for(const auto& row:x.state.acceptances.rows()){auto n=organization_instance_sequence(row.first,political_shop_registry_accept_prefix);if(n&&*n<sequence&&*n>last&&row.second.court_id==court){last=*n;found=&row.second;}}return found;}
}
