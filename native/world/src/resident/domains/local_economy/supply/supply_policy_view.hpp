// 经营草案及原批准条款的有限本地页；只有当前本人真实inspect公职可查阅。
#pragma once
#include "supply_policy_boundary.hpp"
#include "../views/local_economy_view.hpp"
namespace hanmo::resident {
struct EconomicSupplyViewSource {bool authored=false;StableId id,registration_fact_id;};
struct EconomicSupplyPolicyIndex {
 WorldIdentity identity;StableId observer_actor_id,place_id;
 std::vector<ActingAuthority> authorities;
 std::map<LocalEconomyAuthorityKey,std::vector<EconomicSupplyViewSource>> sources;
};
struct EconomicSupplyPolicyViewRow {
 StableId id,registration_fact_id,original_source_key,status,unavailable_code;
 NeedPolicy policy;Integer target_amount=0,maximum_contract_commitment=0,issued_tick=0,effective_tick=0,through_tick=0;
 StableId last_observation_fact_id;std::optional<Integer> observed_amount;
 std::optional<IssueEconomicSupplyPolicyCommand> issue_command;
};
struct EconomicSupplyPolicyPage {LocalEconomyPage base;std::vector<EconomicSupplyPolicyViewRow> rows;};
Outcome<EconomicSupplyPolicyIndex> build_economic_supply_policy_index(const EconomicSupplyPolicyContext&,const OrganizationState&,const OrganizationRead&,const PoliticalTakeoverRead&);
Outcome<EconomicSupplyPolicyPage> query_economic_supply_policies(const EconomicSupplyPolicyContext&,const OrganizationRead&,const PoliticalTakeoverRead&,const EconomicSupplyPolicyIndex&,const LocalEconomyQuery&);
BoundaryValue encode_economic_supply_policy_page(const EconomicSupplyPolicyPage&);
}
