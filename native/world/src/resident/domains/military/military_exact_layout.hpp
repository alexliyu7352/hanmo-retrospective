// Military 原根布局的有限编译登记。描述由实际 codec 的唯一 typed 绑定
// 生成，原文身份只用于实现登记核证，不冒充银行或作者 schema 字段。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <array>
#include <cstddef>
#include <string_view>

namespace hanmo::resident {
// 职责：区分真实根容器类型；没有通用 JSON 或“未知即空表”分支。
enum class MilitaryExactRootStorage { integer, ordered_table, owned_object };
// 职责：为当前实际 callback 的真实 C++ 持值给出有限类型标识。
// 未实现的原根不占假 Row 标签，也不因此获得可安装能力。
enum class MilitaryExactRootValue {
  integer,
  tactical_formation3,
  tactical_mission3,
  dynamic_action2,
  session_current2,
  session_source2,
  local_motion_basis2,
  local_motion_cursor2,
  local_motion_boundary,
  spatial_chart2,
  spatial_handoff2,
  effect_group2,
  battle_fact2,
  casualty_fact2,
  supply_effect_source2,
  duty_state,
  objective_evidence2
};
// 职责：记录实际根名、容器和行型，以及原 callback 的显式选版参数。
// callback_selection=0 只表示该 callback 无选版参数，不是 schema0。
struct MilitaryExactRootDescriptor {
  std::string_view name;
  MilitaryExactRootStorage storage = MilitaryExactRootStorage::integer;
  MilitaryExactRootValue value = MilitaryExactRootValue::integer;
  Integer callback_selection = 0;
};
inline constexpr std::size_t military_exact_max_canonical_roots = 26;
// 职责：固定容量保存同一 visitor 的观察结果，不复制世界或创建持值表。
struct MilitaryExactLayoutDescriptor {
  std::string_view section_name;
  Integer selected_version = 0;
  std::array<MilitaryExactRootDescriptor, military_exact_max_canonical_roots>
      roots{};
  std::size_t root_count = 0;
};
// 职责：锁住原基底与三卡的实现登记身份，不新增作者输入或物理段头。
// 此记录不是银行内字段，任何调用者提供的身份必须完整匹配唯一原登记。
struct MilitaryExactCanonicalReference {
  std::string_view section_name;
  Integer base_version = 0;
  std::array<std::string_view, 4> authority_sha256{};
};
// 职责：逐根报告实际 callback 与原基底间的缺项；报告不是 ready 标志。
// /3、/4 仍保实际原分支，未核其后继 authority 不冒 base/2 安装结论。
struct MilitaryExactCanonicalCoverage {
  std::array<std::string_view, military_exact_max_canonical_roots>
      missing_roots{};
  std::size_t missing_count = 0;
  bool successor_registration_required = false;
  // 军医卡固定尾槽，不随业务未选而省略；根齐全仍不等于来源已就绪。
  bool selected_business_sources_not_checked = true;
};
// 职责：不构造 MilitaryExactState，直接遍历同一静态成员/typed callback 登记。
Outcome<MilitaryExactLayoutDescriptor>
read_military_exact_codec_layout(Integer selected_version);
// 职责：核描述与当前真实 callback 的完整顺序、容器、型关联及选择。
Status
validate_military_exact_codec_layout(const MilitaryExactLayoutDescriptor &);
// 职责：提供唯一原基底＋三卡身份；固定摘要不写入 CatalogIdentity。
const MilitaryExactCanonicalReference &
military_exact_canonical_reference() noexcept;
// 职责：核登记身份、原序及全部真实缺根，不把缺根默认成空表。
Outcome<MilitaryExactCanonicalCoverage>
inspect_military_exact_canonical_coverage(
    const MilitaryExactLayoutDescriptor &,
    const MilitaryExactCanonicalReference &);
// 职责：只检查最终固定根的 typed 关联，不声称独立军医业务或后继已就绪。
// 完整准入还须真实所选尾及来源闭包；旧 codec 不自动调用本检查。
Status validate_military_exact_canonical_bindings(
    const MilitaryExactLayoutDescriptor &,
    const MilitaryExactCanonicalReference &);
}
