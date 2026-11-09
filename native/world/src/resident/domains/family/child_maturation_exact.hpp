// 原出生人物的成年日历来源；只借 FamilyBirth 已有成熟表，不另持年龄或资格。
#pragma once
#include "born_character_source.hpp"
#include "resident/kernel/exact_phase_graph.hpp"

namespace hanmo::resident {

// 同一候选的出生、人物及原历法读链。
// 此口不声明照护适用性，也不以成年代替监护权或合法撤回。
struct ChildMaturationExactSources {
  const BornCharacterSourceContext &births;
  const OriginalCalendarRead &calendar;
  const CurrentMomentRead &clock;
};

// 只读原成熟事实及原生日推得的实际发生时刻。
// availability_enabled 在本精确支必须为 false，其他资格由各自 owner 决定。
struct ChildMaturationExactView {
  const ChildMaturationRecord *maturation = nullptr;
  const FamilyBirthRecord *birth = nullptr;
  const FactRecord *original_fact = nullptr;
  WorldMoment occurred_at;
};

// 验证原 18 岁生日门；不是任意 tick→Moment 转换，也不允许迟到补记。
// 仅提供历法数学，调用者仍须证明出生和当前人物的原来源。
Outcome<WorldMoment>
original_child_maturation_boundary(const OriginalCalendarRead &,
                                   Integer actual_birth_tick);

// 源局部验证只读真实 Birth4、Common 头和历史生命前缀。
// 不反调完整 BirthDue/Care 验证，以免成果证明自身。
Outcome<ChildMaturationExactView>
read_child_maturation_exact_source(const ChildMaturationExactSources &,
                                   const SourceRef &);
Status
validate_child_maturation_exact_sources(const ChildMaturationExactSources &);

// 安装、冷读和稳定发布须覆盖所有已过且当时仍存活的首成年门。
// 私有同 M batch 尚未完成时不调用，不能把合法待处理记录错当遗漏。
Status validate_child_maturation_completed_through(
    const ChildMaturationExactSources &);

// 返回原生日的最早尚未处理边界；死亡者不再成熟。
// 已错过的活人成年门是坏来源，不能返回今天补作成功。
Outcome<std::optional<WorldMoment>>
next_child_maturation_boundary(const ChildMaturationExactSources &);

// 仅 guardianship_monitor 的实际 Calendar 尾门写原成熟表与 Common 头。
// 不改 available、Action、控制、能力、人口或
// CareRequirement；外层负责整候选回滚。
Outcome<SourceRef> record_child_maturation_at_calendar_in_candidate(
    CommonStore &, FamilyBirthState &, const ChildMaturationExactSources &,
    const BirthFactRef &, const ExactPeriodContext &, const ReadStamp &,
    const ReadStampAuthority &, const DomainReadVersionSource &);

// 在同一 guardianship Calendar gate 按真实出生 Common 序处理全部首成年边界。
// 不做 Care 适用规则裁决；每笔原写后失效旧 lease，失败由整个 World 候选撤回。
Status apply_due_child_maturations_in_candidate(
    WorldBundle &, const ChildMaturationExactSources &,
    const CurrentReadStampRead &, const ReadStampAuthority &,
    const DomainReadVersionSource &, const ExactPeriodContext &,
    ExactPhaseWriteContext &);

} // namespace hanmo::resident
