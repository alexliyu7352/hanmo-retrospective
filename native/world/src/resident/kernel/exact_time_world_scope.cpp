// 受限首链的完整准入门；不是忽略未知对象后继续的fallback。
#include "exact_time_world.hpp"
#include "world_bundle.hpp"
#include "exact_social_period.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
// 所有容器必须真实空，普通默认标志不能掩盖已有对象。
template<class... T>bool empty(const T&... values){return (...&&values.empty());}
// 未接宏观对象属于不支持组合，不能当本次正常业务失败。
Status reject(const char* path){return common_failure("ExactRestMacroScopeNotConnected",path,ErrorKind::unsupported_capability);}
}
// 原profile无变化；新profile只接受明确固定的已接模块和空宏观状态。
Status validate_exact_rest_world_scope(const WorldBundle& w){
    if(!w.catalog->exact_time)return common_success();const auto& p=*w.catalog->exact_time;const bool social=exact_social_selected(p);
    auto allowed=exact_time_action_allowed(p,"service","rest");if(!allowed.ok)return allowed;
    // 原基础空模块保原版本；未知新增模块或既有高级组合不能悄悄进本首链。
    std::map<StableId,StableId> modules={{"dynamic_world","local_society_v1"},{"local_economy","1"},{"organization","2"},{"affairs","1"},{"director","2"},{"regional_life","1"},{"world_timeline","2"},{"action_elapsed","2"},{"actor_start_context","2"},{"character_origin_records","1"},{"character_condition_extensions","1"}};
    if(social)modules.emplace("social_period_sources","1");
    std::set<StableId> seen;for(const auto& module:w.catalog->identity.module_versions){const auto found=modules.find(module.first);if(found==modules.end()||found->second!=module.second||!seen.insert(module.first).second)return reject("module_versions");}
    for(const auto* required:{"world_timeline","action_elapsed","actor_start_context","character_origin_records","character_condition_extensions"})if(!seen.count(required))return reject("required_module");
    if(seen.size()!=modules.size()||!w.catalog->actor_foundation||w.catalog->actor_foundation->actors.empty())return reject("exact_profile_catalog_closure");
    const auto installed=w.catalog->actor_foundation->actors.rows().front().second.scenario_origin_ref.initial_tick;
    if(installed<0||installed>w.common.tick()||w.social.last_tick!=installed||w.local_economy.last_step_tick!=installed)return reject("unselected_macro_origin");
    for(const auto& actor:w.catalog->actor_foundation->actors.rows())if(actor.second.scenario_origin_ref.initial_tick!=installed)return reject("inconsistent_initial_moment");
    if(!w.catalog->regional_life||!w.catalog->growth||!w.catalog->growth->regional_life||w.catalog->dynamic_profile!=DynamicProfile::local_society_v1)return reject("regional_rest_profile");
    if(!w.catalog->local_economy||!w.catalog->organization||!w.catalog->affairs||!w.catalog->director||!w.catalog->family)return reject("missing_macro_catalog");
    const auto& ec=*w.catalog->local_economy;if(!empty(ec.households,ec.cohorts,ec.labor_allocations,ec.production_units,ec.obligations,ec.provision_bindings,ec.production_bindings)||ec.production_management||ec.household_provisioning)return reject("economy_catalog_future_objects");
    if(!social&&!ec.supply_sources.empty())return reject("economy_supply_sources");
    const auto& oc=*w.catalog->organization;if(!empty(oc.offices,oc.grants,oc.institutional_issuers,oc.member_policies,oc.duties,oc.members,oc.activity_terms,oc.initial_activities,oc.membership_terms,oc.named_membership_policies))return reject("organization_catalog_future_objects");
    if(!social&&!oc.organizations.empty())return reject("organization_sources");
    if(!empty(w.catalog->affairs->policies,w.catalog->affairs->obligation_policies,w.catalog->director->policies,w.catalog->director->admissions)||w.catalog->affairs->fiscal_need_policies||w.catalog->affairs->cohort_wage_need_policies||w.catalog->director->authored_task_continuation)return reject("future_review_policies");
    if(!w.catalog->social||!w.catalog->social->policies.empty()||!w.catalog->estate||!w.catalog->estate->estates.empty()||!w.catalog->tasks||!empty(w.catalog->tasks->offers,w.catalog->tasks->templates)||!w.catalog->director||!w.catalog->director->subjects.empty())return reject("macro_policies");
    if(!w.catalog->military||w.catalog->military->military||!empty(w.catalog->military->offices,w.catalog->military->garrisons,w.catalog->military->orders,w.catalog->military->strongholds,w.catalog->military->plans)||w.catalog->family->enabled)return reject("military_family_policy");
    for(const auto& role:w.catalog->growth->roles.rows())for(const auto& activity:role.second.activities.rows())if(activity.first!="rest")return reject("growth_role_activity");
    if(!empty(w.catalog->growth->shops,w.catalog->growth->etiquette,w.catalog->growth->bargain,w.catalog->growth->tradeable_items)||w.catalog->growth->cultivation||w.catalog->growth->study_segments||w.catalog->growth->innate_learning)return reject("growth_other_kinds");
    if(!w.common.state().parties.empty())return reject("parties");for(const auto& actor:w.common.state().actors.rows())if(!std::holds_alternative<SettlementLocation>(actor.second.location))return reject("actor_location");
    for(const auto& row:w.common.state().actions.rows()){const auto* payload=w.growth.actions.find(row.first);if(row.second.kind!="service"||!payload||payload->request.activity!="rest")return reject("active_action");}
    for(const auto& fact:w.common.state().facts.rows())if(!((fact.second.domain=="growth"&&fact.second.kind=="life_activity")||(social&&fact.second.domain=="local_economy"&&(fact.second.kind=="local_supply_source"||fact.second.kind=="local_supply_observation"))))return reject("common_fact");
    if(!empty(w.travel.actions,w.travel.road_actions,w.travel.rescues,w.travel.journey_facts,w.travel.itineraries))return reject("travel");
    const auto& m=w.military;if(!empty(m.field_commands,m.officer_assignments,m.request_facts,m.battles,m.sieges,m.training_facts,m.training_actions,m.march_facts,m.occupation_facts,m.relief_facts,m.siege_material_facts,m.logistics_actions,m.logistics_facts,m.return_facts,m.muster_facts,m.resignation_facts,m.delivery_facts,m.settlement_facts,m.contact_facts,m.battle_facts,m.casualty_facts,m.offices,m.garrisons,m.military_units,m.armies,m.campaigns,m.arrival_facts,m.strongholds,m.actions,m.action_facts,m.supply_facts,m.integration_facts))return reject("military");
    if(!w.catalog->interaction->events.empty()||!empty(w.interaction.events,w.interaction.interactions,w.interaction.encounters,w.social.relations,w.social.memories,w.social.events)||!w.social.dynamic||(!social&&!empty(w.social.dynamic->reports,w.social.dynamic->pending_messages,w.social.dynamic->known_reports)))return reject("social_interaction");
    if(!empty(w.estate.estates,w.estate.facts,w.estate.actions,w.tasks.tasks,w.tasks.actions,w.tasks.facts,w.tasks.offer_instances,w.tasks.contracts,w.tasks.deliveries,w.tasks.transfers,w.tasks.shipments,w.tasks.cargo_exposures,w.tasks.cargo_dispositions))return reject("estate_task");
    if(!empty(w.family.children,w.family.designations,w.family.successions,w.family.facts,w.family.actions,w.personal.battles,w.personal.facts,w.growth.facts))return reject("family_personal_growth");
    const auto& e=w.local_economy;if(!empty(e.households,e.cohorts,e.labor_allocations,e.production_units,e.obligations,e.provision_bindings,e.production_bindings,e.arrears,e.payments,e.rations,e.production_receipts,e.population_transfers,e.population_losses,e.obligation_remainders,e.membership_wage_bindings,e.membership_terminal_fractions)||e.production_management||e.household_provisioning)return reject("local_economy");
    if(!social&&(!empty(e.supply_registrations,e.supply_observations)||w.social_period_sources||w.social.dynamic->time2))return reject("unselected_social_source");
    const auto& o=w.organization;if(!empty(o.offices,o.duties,o.members,o.activities,o.facts,o.membership_offers,o.membership_decisions,w.affairs.affairs,w.affairs.needs,w.affairs.claims,w.affairs.contributions,w.director.schedules,w.director.plans,w.director.registrations,w.director.archived_plans))return reject("organization_affairs_director");
    if(!social&&!o.organizations.empty())return reject("organization_sources");
    return social?validate_exact_social_source_scope(w):common_success();
}
}
