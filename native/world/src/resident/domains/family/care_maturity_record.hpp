// v1b §12.4 的原成年结果持值，只承担 Care 根的完整保存责任。
// 形状有效不授予成年资格，也不关闭照护；真正原年龄与适用规则另验。
#pragma once
#include "life_age_basis.hpp"
#include "resident/kernel/original_calendar.hpp"
#include "resident/kernel/world_timeline.hpp"

namespace hanmo::resident {
// 原教育/成年记录共享的有限年龄阶段；不保存可由当前年龄随意改写的状态。
enum class CareAgeStage { early_childhood, foundational, apprentice, adult };

// 精确保存原九字段，来源由真实条件/生命边界拥有者持有。
// effective_tick 与 calendar_boundary_ordinal 保原 Calendar
// 整数，绝不截整实际源时点。
struct MaturityResult {
  StableId result_id, actor_id;
  LifeAgeBasisRef age_basis_ref;
  CalendarRuleRef calendar_profile_ref;
  Integer calendar_boundary_ordinal = 0, effective_tick = 0;
  MomentOwnerSource original_condition_or_life_boundary_ref;
  CareAgeStage previous_stage = CareAgeStage::early_childhood;
  CareAgeStage new_stage = CareAgeStage::early_childhood;
};

class CodecVisitor;
class ProjectionWriter;
// 同一九字段 visitor 供存档使用，不读取当前世界、不产生成年事件。
Status visit_care_maturity_record(CodecVisitor &, MaturityResult &);
// 输出原字段顺序，不增加资格、照护关闭或控制权投影。
Status emit_care_maturity_record_fields(ProjectionWriter &,
                                        const MaturityResult &);
// 把 typed 原记录转换为传输边界值，长期拥有状态仍只保存 typed record。
Outcome<BoundaryValue> encode_care_maturity_record(const MaturityResult &);
// 严格读取全部原字段；未知阶段、坏来源或漏字段均拒绝，不补默认历史。
Outcome<MaturityResult> decode_care_maturity_record(const BoundaryValue &);
} // namespace hanmo::resident
