// 内部接管预览读取原责任；不是玩家查询、产权转移或执行授权。
#pragma once
#include "public_account_dependencies.hpp"
namespace hanmo::resident {
struct FiscalSettlementCatalog;
enum class EconomicResponsibilityKind {
    production_unit, production_asset_use, pending_configuration,
    obligation, arrear, fiscal_assessment, relief_contract, supply_source
};
struct EconomicResponsibilityRef {
    EconomicResponsibilityKind kind=EconomicResponsibilityKind::obligation;
    StableId id;
    // 仅production_asset_use必填；id为asset_id，(unit_id,id)才是唯一使用责任。
    StableId unit_id;
};
enum class EconomicContainerRole { payment, receipt, production_input, production_output };
struct EconomicResponsibilityContainer {
    StableId id;
    bool inventory=false;
    EconomicContainerRole role=EconomicContainerRole::payment;
    // 粮/生产输入输出保实际item；现金为空。不是容器现Owner的快照。
    StableId item_id;
};
struct EconomicProductionAssetSource { StableId id,source_key; Owner original_owner; };
struct EconomicAuthoredResponsibilitySource { StableId table,id,parent_id; };
struct EconomicResponsibilitySource {
    EconomicResponsibilityRef reference;
    WorldIdentity source_identity;
    // 债务主体保SubjectRef，cohort不能转换为Actor或家庭Owner。
    // 生产/设施/供给没有债务受益人，两个字段明确为空。
    std::optional<SubjectRef> original_principal;
    std::vector<SubjectRef> original_beneficiaries;
    // 只在原记录有产权来源时返回；从不读当前公库Owner反推历史。
    std::optional<Owner> original_operating_owner;
    std::vector<EconomicProductionAssetSource> production_assets;
    std::vector<EconomicResponsibilityContainer> containers;
    std::vector<StableId> source_fact_ids;
    // 原初态作者条目不是发生事实，必须显式区分。
    std::vector<EconomicAuthoredResponsibilitySource> authored_sources;
    StableId site_id,obligation_id,period_id,task_id,contract_id;
};
// 调用者必须持完整已验证的同一World/候选责任状态，selection来自S2已验证的实际
// manifest。可在同一候选只转了Common Owner之后读取不变的旧责任；不得由此绕过
// 后续政治责任处置和全World验证。不验证当前Owner，避免以接管后产权倒改旧主体。
// 所有启用子域都传实际state/catalog；成对空仅代表明确未启用。
// 返回值只属于当前identity；未知ID、错复合键、缺来源、未关联所选公库均明确拒绝。
Outcome<EconomicResponsibilitySource> read_economic_responsibility_source(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const EconomicPublicAccountSelection&,const EconomicResponsibilityRef&);
// 仅供真实政治登记已验证完整历史keys后的本域适配使用。已应用/取消的旧配置仍保原
// 配置事实；本入口不自行证明它曾在接管时pending，也不能替代occupation历史枚举。
Outcome<EconomicResponsibilitySource> read_economic_retained_responsibility_source(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const EconomicPublicAccountSelection&,const EconomicResponsibilityRef&);
}
