// 将真实S2历史与S3/S5唯一责任读口组合；不读取政治批准，避免递归证成。
#pragma once
#include "politics_takeover_rules.hpp"
#include "politics_takeover_source_conversion.hpp"
namespace hanmo::resident {
struct OccupationFiscalSettlementState;
struct EstateEmploymentContext;
struct PrivateShopResponsibilitySourceContext;
struct PoliticalTakeoverOriginalContext {
    const CommonStore& common;
    const MilitaryState& military;
    const MilitaryCatalog& military_catalog;
    const MilitaryAccess& military_access;
    const LocalEconomyState& economy;
    const LocalEconomyCatalog& economy_catalog;
    const FiscalSettlementState* fiscal=nullptr;
    const FiscalSettlementCatalog* fiscal_catalog=nullptr;
    const PublicReliefState* relief=nullptr;
    const TaskState& tasks;
    const TaskCatalog& task_catalog;
    const GovernanceTaskState* governance=nullptr;
    const EnvoyTaskState* envoy=nullptr;
    const DynamicStepInput* phase=nullptr;
    const OccupationFiscalSettlementState* occupation_fiscal=nullptr;
    const EstateEmploymentContext* employment=nullptr;
    // 早位只借原租金/工资结构，不构造法律/Task/Growth完整资格链；末位World仍须全验后复核政治。
    const PrivateShopResponsibilitySourceContext* private_shops=nullptr;
};
class PoliticalTakeoverOriginalAccess final:public PoliticalTakeoverOriginalRead {
public:
    explicit PoliticalTakeoverOriginalAccess(PoliticalTakeoverOriginalContext context):context_(context){}
    Outcome<std::vector<PoliticalResponsibilityRef>> responsibility_keys(const MilitaryOccupationFact&) const override;
    Outcome<PoliticalOriginalResponsibility> original_responsibility(const MilitaryOccupationFact&,const PoliticalResponsibilityRef&) const override;
private:
    Status validate_source(const MilitaryOccupationFact&) const;
    PoliticalTakeoverOriginalContext context_;
};
}
