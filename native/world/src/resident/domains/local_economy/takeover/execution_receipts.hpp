// 独立版本段只关联原真实收据与政治决定，不复制应付额、已付额或余额。
#pragma once
#include "responsibility_execution.hpp"
namespace hanmo::resident {
enum class EconomicTakenPaymentScope { captured_arrear, subsequent_obligation_period };
struct EconomicTakenPaymentSource {
    StableId receipt_id,arrear_id,registration_fact_id,approval_fact_id,decision_fact_id;
    EconomicTakenPaymentScope scope=EconomicTakenPaymentScope::captured_arrear;
};
// 操作来源只记责任键和真实许可IDs；数量和周期全部留在原口粮/生产收据。
struct EconomicOperatingPermissionSource {
    EconomicResponsibilityRef responsibility;
    StableId registration_fact_id;
    std::optional<StableId> approval_fact_id,decision_fact_id;
};
struct EconomicOperatingReceiptSource {
    StableId receipt_id;
    std::vector<EconomicOperatingPermissionSource> permissions;
};
struct EconomicConfigurationExecutionSource {
    StableId id,configuration_id,production_receipt_id;
    Integer tick=0,due_tick=0;
    bool applied=false;
    StableId code;
    std::vector<EconomicOperatingPermissionSource> permissions;
};
// 新配置/取消/替代只保存原业务fact的签发来源，金额人数仍由原配置目标唯一给出。
struct EconomicConfigurationCommandSource {
    StableId fact_id,unit_id,actor_id;
    ActingAuthority authority;
    std::optional<StableId> prior_configuration_id,request_configuration_id;
    std::vector<EconomicOperatingPermissionSource> permissions;
};
struct EconomicTakeoverExecutionState {
    Integer version=1;
    OrderedTable<EconomicTakenPaymentSource> payments;
    OrderedTable<EconomicOperatingReceiptSource> rations,production,relief;
    OrderedTable<EconomicConfigurationExecutionSource> configurations;
    OrderedTable<EconomicConfigurationCommandSource> configuration_requests;
};
struct EconomicTakenPaymentResult {
    // handled=false仅表示原欠据从未登记，原支付路径仍自行校验权源。
    bool handled=false;
    std::optional<StableId> receipt_id;
    StableId pending_code;
};
// 先核欠据自己的最新登记；绝不借持续义务决定覆盖旧欠据的pending/suspend。
// 仅从未捕获且真实产生于该登记之后的新欠期，可按同一obligation的明确承担支付。
// 未裁定/余额不足返回handled+明确pending，不改principal/beneficiary/原金额或创建新债。
// in_candidate要求调用者同时持Common保存点与本域两候选；任何失败必须整体丢弃。
Outcome<EconomicTakenPaymentResult> settle_taken_economic_arrear_in_candidate(
    CommonStore&,LocalEconomyState&,EconomicTakeoverExecutionState&,
    const EconomicTakeoverReadContext&,const StableId& arrear_id,Integer tick);
// 独立域入口自身保存点；正式World阶段使用上面的候选入口。
Outcome<EconomicTakenPaymentResult> settle_taken_economic_arrear(
    CommonStore&,LocalEconomyState&,EconomicTakeoverExecutionState&,
    const EconomicTakeoverReadContext&,const StableId& arrear_id,Integer tick);
// 历史冷验按原决定和真实事实先后，不要求今天仍有执行权，也不从当前Owner倒推。
Status validate_economic_taken_payment_source(const EconomicTakeoverReadContext&,
    const EconomicTakeoverExecutionState&,const StableId& receipt_id);
Status validate_economic_takeover_execution(const EconomicTakeoverReadContext&,
    const EconomicTakeoverExecutionState&);
Outcome<EconomicTakeoverExecutionState> decode_economic_takeover_execution(const BoundaryValue&);
class ProjectionWriter;
Status emit_economic_takeover_execution_fields(ProjectionWriter&,const EconomicTakeoverExecutionState&);
BoundaryValue encode_economic_takeover_execution(const EconomicTakeoverExecutionState&);
Status visit_economic_takeover_execution_checkpoint(CodecVisitor&,EconomicTakeoverExecutionState&);
}
