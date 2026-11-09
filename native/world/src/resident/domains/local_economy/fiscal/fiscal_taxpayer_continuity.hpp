// 原辖区仍合法征税而原组织税源已失产时的显式连续性段；不改变旧财政1或旧纳税人。
#pragma once
#include "fiscal_settlement.hpp"
#include "resident/domains/politics/takeover/politics_occupation_taxpayer_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* fiscal_taxpayer_continuity_module="fiscal_taxpayer_continuity";
// 一条暂停只指向原财政已推进的空政策周期，不生成新债、税额或第二本余额。
struct FiscalTaxpayerSuspendedPeriod {
 StableId period_id,district_id,policy_source_fact_id;
 Integer due_tick=0;
 std::vector<StableId> blocked_source_ids;
};
struct FiscalTaxpayerContinuityState {
 Integer version=1,initialized_tick=0,last_step_tick=0;
 OrderedTable<FiscalTaxpayerSuspendedPeriod> suspended_periods;
};
// 显式传入才启用新规则；同候选资产读必须回传精确Common身份，phase只许current→current+1。
struct FiscalTaxpayerContinuityContext {
 const OccupationFiscalTaxpayerRead& assets;
 const FiscalTaxpayerContinuityState& state;
 const DynamicStepInput* phase=nullptr;
};
// 写入口另持同一对象的显式可变借口，冷验证和页面无需const_cast或复制状态。
struct FiscalTaxpayerContinuityExecutionContext: FiscalTaxpayerContinuityContext {
 FiscalTaxpayerContinuityState& mutable_state;
 FiscalTaxpayerContinuityExecutionContext(const OccupationFiscalTaxpayerRead& read,FiscalTaxpayerContinuityState& value,const DynamicStepInput* step=nullptr):FiscalTaxpayerContinuityContext{read,value,step},mutable_state(value){}
};
// 仅显式新profile调用这些重载；旧符号始终转入空continuity，原银行不自动添暂停。
Outcome<LocalEconomyEffects> step_fiscal_settlement(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const FiscalPolicyRead&,const FiscalSettlementCatalog&,FiscalSettlementState&,const DynamicStepInput&,const EconomicTakeoverReadContext*,FiscalTaxpayerContinuityExecutionContext*);
Status validate_fiscal_settlement(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const FiscalPolicyRead&,const FiscalSettlementCatalog&,const FiscalSettlementState&,const EconomicTakeoverReadContext*,const FiscalTaxpayerContinuityContext*);
Outcome<FiscalTaxpayerContinuityState> make_initial_fiscal_taxpayer_continuity(const CommonStore&,const FiscalSettlementState&);
Status decode_fiscal_taxpayer_continuity_profile(const BoundaryValue&);
Status validate_fiscal_taxpayer_continuity_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>&);
class ProjectionWriter;
Status emit_fiscal_taxpayer_continuity_fields(ProjectionWriter&,const FiscalTaxpayerContinuityState&);
BoundaryValue encode_fiscal_taxpayer_continuity(const FiscalTaxpayerContinuityState&);
Outcome<FiscalTaxpayerContinuityState> decode_fiscal_taxpayer_continuity(const BoundaryValue&);
Status visit_fiscal_taxpayer_continuity_checkpoint(CodecVisitor&,FiscalTaxpayerContinuityState&);
}
