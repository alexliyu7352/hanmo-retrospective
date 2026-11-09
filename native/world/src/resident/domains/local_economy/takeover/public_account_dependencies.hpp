// 接管预览的内部关系读取；不授予征用权、不转移资产，也不另存责任生命周期。
#pragma once
#include "resident/domains/local_economy/local_economy_model.hpp"
namespace hanmo::resident {
struct FiscalSettlementState;struct PublicReliefState;
// 调用前须完整验证World，并由S2真实接管清单提供账户；本叶不能从玩家参数直接调用。
struct EconomicPublicAccountSelection {
    Owner original_owner;
    std::vector<StableId> wallet_ids,inventory_ids;
};
struct EconomicProductionDependency {
    StableId unit_id;
    bool output_selected=false;
    std::vector<StableId> selected_input_inventory_ids,asset_ids,labor_allocation_ids;
    std::optional<StableId> pending_configuration_id;
};
struct EconomicObligationDependency {
    StableId obligation_id;
    bool source_selected=false,destination_selected=false;
    std::vector<StableId> arrear_ids,unpaid_arrear_ids,payment_ids,provision_binding_ids;
};
struct EconomicFiscalDependency {
    StableId assessment_id,period_id;
    bool cash_source_selected=false,grain_source_selected=false;
    bool cash_destination_selected=false,grain_destination_selected=false;
    std::vector<StableId> payment_ids;
};
// 不从receipt/release有无推导S3任务状态；调用者仍须读取唯一TaskRecord。
struct EconomicReliefDependency {
    StableId terms_id,task_id,contract_id;
    bool cash_source_selected=false,grain_source_selected=false;
    std::vector<StableId> receipt_ids,release_ids;
};
struct EconomicPublicAccountDependencies {
    std::vector<EconomicProductionDependency> production;
    std::vector<EconomicObligationDependency> obligations;
    std::vector<EconomicFiscalDependency> fiscal;
    std::vector<EconomicReliefDependency> relief;
    std::vector<StableId> supply_source_keys;
};
// 必须在Owner实际转移前读取。完整历史引用保留，含已清偿债；不复制钱粮余额。
// 仅枚举本域原表的直接容器引用，军事动态付款人和公共预约仍由其唯一owner核验。
// 所有实际启用子域必须传入同一候选state；nullptr只代表World中明确未启用。
Outcome<EconomicPublicAccountDependencies> read_economic_public_account_dependencies(
    const CommonRead&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const PublicReliefState*,const EconomicPublicAccountSelection&);
}
