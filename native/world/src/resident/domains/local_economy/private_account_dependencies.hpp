// 本人私容器的本域责任预检；不授权继承、不清债、不移交产业或释放预约。
#pragma once
#include "local_economy_model.hpp"
namespace hanmo::resident {
struct EstateEmploymentState;struct PrivateShopState;
struct FiscalSettlementState;struct OccupationFiscalSettlementState;struct PublicReliefState;
struct EconomicPrivateAccountSelection {StableId actor_id;std::vector<StableId> wallet_ids,inventory_ids;};
struct EconomicPrivateAccountBlocker {StableId kind,source_id;Integer unpaid_amount=0;};
struct EconomicPrivateAccountDependencies {WorldIdentity source_identity;StableId actor_id;std::vector<EconomicPrivateAccountBlocker> blockers;};
// 仅已完整验证的同World内部调用，所有启用子域必须传同候选状态；null只代表未启用。
// 输出为空仅表示本域无待履行责任，不能替代Task/私人垫款、Estate、政治或家庭许可。
Outcome<EconomicPrivateAccountDependencies> read_economic_private_account_dependencies(
    const CommonStore&,const LocalEconomyState*,const LocalEconomyCatalog*,
    const FiscalSettlementState*,const OccupationFiscalSettlementState*,const PublicReliefState*,
    const EconomicPrivateAccountSelection&);
// 明确启用雇用段时，邀约预约与仍可履行的私人工资承诺也构成未结责任。
Outcome<EconomicPrivateAccountDependencies> read_economic_private_account_dependencies(
    const CommonStore&,const LocalEconomyState*,const LocalEconomyCatalog*,
    const FiscalSettlementState*,const OccupationFiscalSettlementState*,const PublicReliefState*,
    const EconomicPrivateAccountSelection&,const EstateEmploymentState*);
// 私铺显式启用后追加同候选本账，原空重载遇真实私铺事实必须拒，不能漏租约/店员责任。
Outcome<EconomicPrivateAccountDependencies> read_economic_private_account_dependencies(
    const CommonStore&,const LocalEconomyState*,const LocalEconomyCatalog*,
    const FiscalSettlementState*,const OccupationFiscalSettlementState*,const PublicReliefState*,
    const EconomicPrivateAccountSelection&,const EstateEmploymentState*,const PrivateShopState*);
}
