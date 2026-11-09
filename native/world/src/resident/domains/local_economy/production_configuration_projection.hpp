// 生产配置回执只投影原两组引用；原边界 writer 和预算 shape sink 共用此字段源。
#pragma once
#include "hanmo/resident/model_contract.hpp"

namespace hanmo::resident {
class ProjectionWriter;
struct LocalEconomyEffects;

// 保原 receipt_ids、fact_ids 顺序与值；失败沿 sink
// 原错误返回，不创造事实或成功结果。
Status emit_economic_configuration_effects_fields(ProjectionWriter &,
                                                  const LocalEconomyEffects &);
} // namespace hanmo::resident
