// 原青川军政计划只在真实宏观末尾提出一步；不预写成军、接触或战场。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct ExactPhaseWriteContext;
// 借外层唯一候选/capture，原报价拒绝等待，任何写后失败撤整个候选。
Status run_native_field_military_plans_in_candidate(WorldBundle &,
                                                    ExactPhaseWriteContext &);
} // namespace hanmo::resident
