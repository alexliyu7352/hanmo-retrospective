// 冷命令只解析正式布阵字段；未知键或任务标签必须拒绝，不运行军事规则。
#pragma once
#include "field_deployment_command_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<FieldDeploymentCommand>
decode_field_deployment_command(const StableId &kind,
                                const BoundaryValue &params);
} // namespace hanmo::resident
