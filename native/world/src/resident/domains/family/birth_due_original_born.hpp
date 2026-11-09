// BirthDue 正向来源的原 Birth4、S1 与 Named 人口前缀。
// 不实现完整 BirthDueBornSourceRead；供养、照护及条件载荷尚须各真实 owner
// 独立闭合。
#pragma once
#include "born_character_creation.hpp"
#include "resident/domains/local_economy/born_population_sources.hpp"

namespace hanmo::resident {
// 只借同一不可移动完整世界中的真实 owner，禁止临时空表或第二人口账。
struct BirthDueOriginalBornContext {
  const BornCharacterSourceContext &births;
  const AuthoredActorFoundationState &foundation;
  const ActorFoundationCatalog &foundation_catalog;
  const BirthPlacementRead &placement;
  const BornPopulationSourceContext &population;
};
// 返回值只在原同步借用期间有效，不缓存为准备许可或完整出生成功。
// requires_actual_passenger_load 由真实 S1 放置派生，不由 caller 选择。
struct BirthDueOriginalBornView {
  BornCharacterSourceView birth;
  BirthPlacementApplicationRead placement;
  const NamedBirthPopulationTransition *population = nullptr;
  bool requires_actual_passenger_load = false;
};
// 安装和冷读核同世界的原基础来源连接，不核缺失的 Calendar 供养授权。
Status
validate_birth_due_original_born_bindings(const BirthDueOriginalBornContext &);
// 真实已写出生、身份初值、实际位置与人口逐源反读；不能拿本前缀冒 full Born
// gate。 不读 BirthDueExecution 输出证明自身因果，也不以今日母亲位置补历史。
Outcome<BirthDueOriginalBornView>
read_birth_due_original_born_prefix(const BirthDueOriginalBornContext &,
                                    const ScheduledBirthSource &,
                                    const BornBirthDueResult &);
} // namespace hanmo::resident
