// 私人垫款属于原Task合同；只保存冻结授权和实际收付款事实，余额仍由Common唯一持有。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct TaskState;struct TaskCatalog;struct CodecVisitor;
inline constexpr const char* task_advance_fact_kind="task_personal_advance_paid";
inline constexpr const char* task_advance_repayment_fact_kind="task_personal_advance_repaid";
// 作者必须明确允许以及累计上限；没有此规则的旧合同不接受私钱注入。
struct TaskPersonalAdvancePolicy {
    StableId id,template_id,task_template_id;
    Integer version=1,maximum_cash=0;
    StableId repayment_policy="authorized_advance_first_v1";
};
// 与接受时原冻结条款绑定；此处不另存任务进度、可花余额或第二份债务生命周期。
struct TaskPersonalAdvanceTerms {
    StableId contract_id,task_id,claim_id,terms_version;
    TaskPersonalAdvancePolicy policy;
    StableId lender_actor_id,personal_wallet_id,escrow_wallet_id,repayment_wallet_id;
    Owner original_issuer;
    Integer frozen_tick=0;
};
struct TaskPersonalAdvancePayment {
    StableId fact_id,contract_id,task_id,lender_actor_id,from_wallet_id,to_wallet_id;
    Integer amount=0,tick=0;
};
// 实际清算一次归还所有已授权垫款；原报酬/预算退款仍是原Contract的独立字段。
struct TaskPersonalAdvanceRepayment {
    StableId fact_id,contract_id,task_id,lender_actor_id,to_wallet_id,escrow_wallet_id,issuer_wallet_id;
    std::vector<StableId> payment_fact_ids;
    Integer amount=0,from_escrow=0,from_issuer=0,tick=0;
};
struct TaskPersonalAdvanceState {
    Integer version=1;
    OrderedTable<TaskPersonalAdvanceTerms> terms;
    OrderedTable<TaskPersonalAdvancePayment> payments;
    OrderedTable<TaskPersonalAdvanceRepayment> repayments;
};
// 报价是短期完整身份；付款额就是本人明确确认额，不自动抽取剩余私钱。
struct TaskPersonalAdvanceQuote {
    WorldIdentity source_identity;
    StableId contract_id,task_id,actor_id,from_wallet_id,to_wallet_id;
    Integer amount=0,previous_total=0,maximum_cash=0;
    std::vector<ReadVersion> resource_versions;
};
// 纯守恒拆分，不授权从当前雇主付款；原结算必须先核权源和资源，再同候选记真实收据。
struct TaskAdvanceSettlementAmounts {
    Integer principal_due=0,from_escrow=0,from_issuer=0,unused_original_budget=0;
};
struct TaskPersonalAdvanceSummary {
    StableId contract_id,task_id,lender_actor_id,personal_wallet_id;
    std::vector<StableId> payment_fact_ids;
    std::optional<StableId> repayment_fact_id;
    Integer paid=0,repaid=0;
};
// 暂缓不是任务失败：无资金/真实权源变化时不产生收据，仍保原待清算责任。
enum class TaskAdvanceRepaymentKind { no_advance,pending,repaid,already_repaid };
struct TaskAdvanceRepaymentAttempt {TaskAdvanceRepaymentKind kind=TaskAdvanceRepaymentKind::no_advance;StableId code;std::optional<StableId> repayment_fact_id;};
class TaskPersonalAdvanceRead {
public:
    virtual ~TaskPersonalAdvanceRead()=default;
    virtual Outcome<TaskPersonalAdvanceSummary> task_personal_advance_source(const StableId& contract_id)const=0;
};
// 独立显式载荷；当前不挂入旧Task段，不给旧档追加默认字节。
Status visit_task_personal_advance_checkpoint(CodecVisitor&,TaskPersonalAdvanceState&);
BoundaryValue encode_task_personal_advance_state(const TaskPersonalAdvanceState&);
Outcome<TaskPersonalAdvanceState> decode_task_personal_advance_state(const BoundaryValue&);
}
