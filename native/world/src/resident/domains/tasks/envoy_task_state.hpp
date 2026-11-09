// 使者sidecar只保条款、原动作参数和事实引用；任务状态与剩余工时仍由TaskRecord/Common Action持有。
#pragma once
#include "envoy_task_contract.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
class ProjectionWriter;
// 此处只有明确递交或返报，旅程始终走原交通行动，不创建使者专用旅行钟。
enum class EnvoyTaskActionKind { deliver,report };
// 保存已开始交接的真实上下文；加载后按当前Common参与者复核，不把旧报价epoch保存成权限。
struct EnvoyTaskAction {
    StableId action_id;
    EnvoyTaskActionKind kind=EnvoyTaskActionKind::deliver;
    InteractionContext context;
};
// 同一合同只引用一份外交presentation，返报与任务取消不能删除已发生的外交送达。
struct EnvoyTaskContract {
    EnvoyTaskAcceptance acceptance;
    std::optional<EnvoyTaskAction> action;
    std::optional<EnvoyTaskDelivery> delivery;
    std::optional<EnvoyTaskCompletion> completion;
    StableId pending_code;
    std::vector<EnvoyHandoverInterruption> interruptions;
};
// v1默认保持原字段/字节；显式v2才编码最终政治报告引用，不改变唯一TaskState或旧tasks section。
struct EnvoyTaskState {
    Integer version=1;
    OrderedTable<EnvoyTaskContract> envoy_contracts;
};
Status visit_envoy_task_checkpoint(CodecVisitor&,EnvoyTaskState&);
Outcome<EnvoyTaskState> decode_envoy_task_state(const BoundaryValue&);
// 同原字段表借读已有数据；源注册和领域验证仍由原调用点完成。
Status emit_envoy_task_state_fields(ProjectionWriter&,const EnvoyTaskState&);
BoundaryValue encode_envoy_task_state(const EnvoyTaskState&);
}
