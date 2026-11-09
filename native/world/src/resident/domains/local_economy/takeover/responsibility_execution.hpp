// 本域执行只消费真实政治裁定；原债额、受益人和账户仍由原责任表持有。
#pragma once
#include "responsibility_source.hpp"
#include "resident/domains/politics/takeover/politics_takeover_contract.hpp"
namespace hanmo::resident {
struct OccupationFiscalSettlementState;class OccupationFiscalTaxpayerRead;
enum class EconomicTakeoverExecutionStatus { pending, approved, suspended };
struct EconomicTakeoverExecutionPlan {
    EconomicResponsibilitySource original;
    PoliticalTakeoverRegistrationSource registration;
    EconomicTakeoverExecutionStatus status=EconomicTakeoverExecutionStatus::pending;
    std::optional<PoliticalResponsibilityDecision> decision;
    std::optional<Owner> executing_owner;
    // 只投影原容器ID；付款继受不能替换收款家庭或另造可花账户。
    std::vector<EconomicResponsibilityContainer> containers;
    StableId code;
};
struct EconomicTakeoverReadContext {
    const CommonStore& common;
    const LocalEconomyState& economy;
    const LocalEconomyCatalog& catalog;
    const FiscalSettlementState* fiscal=nullptr;
    const FiscalSettlementCatalog* fiscal_catalog=nullptr;
    const PublicReliefState* relief=nullptr;
    const PoliticalTakeoverRead& politics;
    // 具名工资的历史接受链仍由原成员账证明，不能由接管决定替代。
    const OrganizationState* organizations=nullptr;
    const OrganizationCatalog* organization_catalog=nullptr;
    // 仅原半日候选可借用to_tick；冷态/命令读必须为空，不能任意查询未来。
    const DynamicStepInput* phase=nullptr;
    const OccupationFiscalSettlementState* occupation_fiscal=nullptr;
    const OccupationFiscalTaxpayerRead* taxpayer_assets=nullptr;
};
// 只转换封闭类型，不执行授权；错误复合引用仍拒绝。
Outcome<PoliticalResponsibilityRef> economic_responsibility_political_ref(const EconomicResponsibilityRef&);
// empty只表示此责任从未被实际接管登记；原有产权/机构授权检查仍必须继续。
// 非empty的pending/suspended不允许资产操作，也不抹掉原欠期或改写履约时钟。
// 调用者须处在同一已核S2/政治责任链的候选；本接口不是玩家任意ID查询。
Outcome<std::optional<EconomicTakeoverExecutionPlan>> read_economic_takeover_execution(
    const EconomicTakeoverReadContext&,const EconomicResponsibilityRef&);
// 供原冷验证的Owner等式失败分支使用。只证明原容器的实际接管历史，不能作付款授权。
// expected_original_owner必须来自原本域记录，不能从当前容器反推。
Status validate_economic_taken_container_history(const EconomicTakeoverReadContext&,
    const EconomicResponsibilityRef&,const StableId& container_id,bool inventory,
    const Owner& expected_original_owner);
// 生产单元与各具名设施必须分别获准；单元裁定不能越过某设施的暂停。
Outcome<std::optional<EconomicTakeoverExecutionPlan>> read_economic_production_execution(
    const EconomicTakeoverReadContext&,const StableId& unit_id);
// 供给原政策只能继续在原机构合法经营时发布；暂停投影不改旧Need、合同或历史库存快照。
struct EconomicSupplySuspension {
    StableId source_key,original_organization_id,site_id,inventory_id,item_id,registration_fact_id,code;
};
Outcome<std::vector<EconomicSupplySuspension>> read_economic_supply_suspensions(const EconomicTakeoverReadContext&);
// 仅供真实占领的pre-clock零时投影；不刷新库存观察，原单参读继续严格要求已commit时钟。
Outcome<std::vector<EconomicSupplySuspension>> read_economic_supply_suspensions(const EconomicTakeoverReadContext&,const DynamicStepInput&);

}
