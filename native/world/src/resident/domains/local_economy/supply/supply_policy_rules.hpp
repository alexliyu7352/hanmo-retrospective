// 新供给政策的真实签发、批准只读与库存观察；不持有或改写任务资金及旧合同。
#pragma once
#include "supply_policy.hpp"
#include "resident/domains/politics/supply/politics_supply_contract.hpp"
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {
struct EconomicSupplyPolicyContext {
 EconomicSupplyPendingContext ledger;
 const TaskCatalog& tasks;
 const PoliticalSupplyPolicyAuthorityRead& authority;
 const PoliticalSupplyAdoptionRead& adoptions;
};
struct EconomicSupplyPolicyQuote {
 WorldIdentity source_identity;StableId actor_id;
 IssueEconomicSupplyPolicyCommand command;
 EconomicSupplyPolicyTermsSource proposed;
 PoliticalSupplyPolicyAuthoritySource authorization;
};
struct EconomicSupplyPolicyReceipt {StableId policy_fact_id,registration_fact_id,original_source_key;};
// 验全部历史签权及观察前的真实批准；不以当前Owner否定合法旧记录。
Status validate_economic_supply_policies(const EconomicSupplyPolicyContext&);
Outcome<EconomicSupplyPolicyState> make_initial_economic_supply_policies(const CommonStore&);
Outcome<EconomicSupplyPolicyQuote> preview_economic_supply_policy(const EconomicSupplyPolicyContext&,const StableId& actor,const IssueEconomicSupplyPolicyCommand&);
Outcome<EconomicSupplyPolicyReceipt> issue_economic_supply_policy(CommonStore&,EconomicSupplyPolicyState&,const EconomicSupplyPolicyContext&,const StableId& actor,const IssueEconomicSupplyPolicyCommand&);
// 中央已有完整Common候选保存点，只重验原quote并原址写账；不嵌套Common保存点。
Outcome<EconomicSupplyPolicyReceipt> issue_economic_supply_policy_in_candidate(CommonStore&,EconomicSupplyPolicyState&,const EconomicSupplyPolicyContext&,const StableId& actor,const IssueEconomicSupplyPolicyCommand&,const EconomicSupplyPolicyQuote&);
// 稳定clock原finish入口；每个获准政策每个实际观察tick至多复用同数量/版本快照。
Outcome<std::vector<SupplyObservation>> refresh_economic_supply_observations(CommonStore&,EconomicSupplyPolicyState&,const EconomicSupplyPolicyContext&);
// 原零时命令外层已有Common保存点时用此入口，晚失败由外层整候选回滚。
Outcome<std::vector<SupplyObservation>> refresh_economic_supply_observations_in_candidate(CommonStore&,EconomicSupplyPolicyState&,const EconomicSupplyPolicyContext&);
class EconomicSupplyPolicyAccess final:public EconomicSupplyPolicyRead {
public:
 explicit EconomicSupplyPolicyAccess(EconomicSupplyPolicyContext context):context_(context){}
 Outcome<std::vector<StableId>> economic_supply_policy_ids()const override;
 Outcome<std::optional<EconomicSupplyPolicySource>> economic_supply_policy(const StableId&)const override;
 Outcome<std::optional<EconomicSupplyObservationSource>> economic_supply_observation(const StableId&)const override;
private:EconomicSupplyPolicyContext context_;
};
}
