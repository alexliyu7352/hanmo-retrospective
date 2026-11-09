// 新计划profile使用独立严格版本；旧profile完全不调用这些冷边界函数。
#pragma once
#include "plan_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// 原 JSON 的逐字段借读出口；whole 保原 /2 shape 前置及两版本精确字段。
Status emit_player_plan_state_fields(ProjectionWriter&,const PlayerPlanState&);
Status emit_player_plan_terms_fields(ProjectionWriter&,const PlayerPlanTerms&);
Status emit_player_plan_step_data_fields(ProjectionWriter&,const PlayerPlanStepData&);
Outcome<PlayerPlanState> decode_player_plan_state(const BoundaryValue&);
BoundaryValue encode_player_plan_state(const PlayerPlanState&);
Outcome<PlayerPlanEdit> decode_player_plan_edit(const StableId& kind,const BoundaryValue&);
BoundaryValue encode_player_plan_step_data(const PlayerPlanStepData&);
BoundaryValue encode_player_plan_terms(const PlayerPlanTerms&);
Status visit_player_plan_checkpoint(CodecVisitor&,PlayerPlanState&);
} // 缺字段、未知字段、版本和引用错误不能通过默认值迁移。
