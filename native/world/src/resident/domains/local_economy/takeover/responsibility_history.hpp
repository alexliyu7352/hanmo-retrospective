// 接管冷验证的原责任全集；只读历史，不授权今天收款或经营。
#pragma once
#include "responsibility_source.hpp"
namespace hanmo::resident {
struct MilitaryOccupationFact;struct EstateEmploymentContext;struct PrivateShopResponsibilitySourceContext;
// 同一真实Military manifest/时序边界供显式新责任段复用，不读取政治决定。
Status validate_economic_occupation_history_boundary(const CommonStore&,const MilitaryOccupationFact&,const DynamicStepInput* phase=nullptr);
// 前置：同一World各原领域完整验证，且调用方已验证S2原occupation全链。
// Capture须紧接occupation事实分配且之前尚未applied，其间无新增经济责任。
// 本口再核原manifest与Common全局实例序；不要求当前容器Owner等于原组织。
// 永久configuration/result还原当时pending；当前pending、余额、人数均不替代历史。
// 原记录/时间来源丢失或未知动态责任明确失败，不能省略后宣称完整。
Outcome<std::vector<EconomicResponsibilityRef>> read_economic_public_account_responsibility_keys(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const MilitaryOccupationFact&);
// 同一半日候选专用；仍按实际实例序与fact顺序还原原责任。
Outcome<std::vector<EconomicResponsibilityRef>> read_economic_public_account_responsibility_keys(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const MilitaryOccupationFact&,const DynamicStepInput*);
Outcome<EconomicResponsibilitySource> read_economic_public_account_responsibility_history(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const MilitaryOccupationFact&,const EconomicResponsibilityRef&);
Outcome<EconomicResponsibilitySource> read_economic_public_account_responsibility_history(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const MilitaryOccupationFact&,const EconomicResponsibilityRef&,const DynamicStepInput*);
// 显式私雇段先完整核原服务与工资，再仅排除未被S2公共清单选中的原真人私钱包责任。
Outcome<std::vector<EconomicResponsibilityRef>> read_economic_public_account_responsibility_keys(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const MilitaryOccupationFact&,const DynamicStepInput*,const EstateEmploymentContext*);

// 原私铺完整验后只排除选中公库之外的双方真人私租/工资；独立来源请求仍走原严格拒绝。
Outcome<std::vector<EconomicResponsibilityRef>> read_economic_public_account_responsibility_keys(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const MilitaryOccupationFact&,const DynamicStepInput*,const EstateEmploymentContext*,const PrivateShopResponsibilitySourceContext*);
}
