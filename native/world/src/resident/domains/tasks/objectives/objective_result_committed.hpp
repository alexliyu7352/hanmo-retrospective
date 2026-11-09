// 原 I§9.1 的有限目标通知：只投影已选旧采购成果，不存第二账或传任意业务 JSON。
#pragma once
#include "hanmo/resident/phase_contract.hpp"
#include "resident/model/objective_evidence.hpp"

namespace hanmo::resident {
class CommonStore;
struct TaskState;

// 当前只登记旧采购实交源；精确数量和其它专业来源不得降成这个 kind。
enum class ObjectiveResultEventKind { legacy_task_delivery };

// 公共八字段指向原事实；source_identity 是通知产生候选，不冒历史事实 revision。
struct ObjectiveResultCommitted {
  WorldIdentity source_identity;
  StableId source_domain;
  ObjectiveResultEventKind event_kind =
      ObjectiveResultEventKind::legacy_task_delivery;
  SourceRef fact_ref;
  std::vector<SubjectRef> affected_refs;
  Integer effective_tick = 0;
  Phase phase = Phase::actions_step;
  Integer sequence = 0;
  Integer source_schema = 1;
  ObjectiveReceiptRef objective_receipt_ref;
  SubjectRef beneficiary;
  SubjectRef actual_participant;
  ObjectiveMeasured measure;
  Integer completed_tick = 0;
};

// 原完成点或合法重观察边界回读同一源；through_tick 只能为当前或原下一半日。
Outcome<ObjectiveResultCommitted>
read_task_delivery_objective_result(const CommonStore &, const TaskState &,
                                    const StableId &delivery_id,
                                    Integer through_tick);

// 不把通知当事实：允许同世界/epoch 的旧 revision，字段必须逐项反读原成果一致。
Status validate_task_delivery_objective_result(const CommonStore &,
                                               const TaskState &,
                                               const ObjectiveResultCommitted &,
                                               Integer through_tick);
} // namespace hanmo::resident
