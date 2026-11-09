// 治理sidecar只保存冻结责任与事实；进度、占用和终态仍归唯一TaskRecord/Common Action。
#pragma once
#include "governance_task_contract.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
class ProjectionWriter;
// 每份接受记录只有一个原动作；等待资源不重新开动作，也不保存第二倒计时。
struct GovernanceTaskContract {
    GovernanceTaskAcceptance acceptance;
    StableId action_id;
    std::optional<GovernanceWorkCompletion> work;
    std::optional<GovernanceTaskCompletion> completion;
    std::optional<GovernanceWorkInterruption> interruption;
    StableId execution_note_code;
    StableId pending_code;
    Integer last_settlement_attempt_tick=-1;
};
// 本模块启用时严格恢复独立v1域；旧tasks字节不增加任何伪默认治理记录。
struct GovernanceTaskState {
    Integer version=1;
    OrderedTable<GovernanceTaskContract> governance_contracts;
};
Status visit_governance_task_checkpoint(CodecVisitor&,GovernanceTaskState&);
Outcome<GovernanceTaskState> decode_governance_task_state(const BoundaryValue&);
// 同原字段表借读已有数据；源注册和领域验证仍由原调用点完成。
Status emit_governance_task_state_fields(ProjectionWriter&,const GovernanceTaskState&);
BoundaryValue encode_governance_task_state(const GovernanceTaskState&);
}
