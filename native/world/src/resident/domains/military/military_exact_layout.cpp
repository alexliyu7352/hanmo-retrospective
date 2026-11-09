#include "military_exact_layout.hpp"
#include "military_exact_root_bindings.hpp"
#include <algorithm>
#include <type_traits>

namespace hanmo::resident {
namespace {
// 职责：编译期将实际成员行型映到有限描述标识；未声明类型编译失败，
// 不能把新字段当任意 byte/blob 旁路写入。
template <class T> struct ValueKind;
#define HANMO_MILITARY_ROOT_VALUE(Type, Kind)                                  \
  template <> struct ValueKind<Type> {                                         \
    static constexpr auto value = MilitaryExactRootValue::Kind;                \
  }
HANMO_MILITARY_ROOT_VALUE(Integer, integer);
HANMO_MILITARY_ROOT_VALUE(MilitaryTacticalFormation3, tactical_formation3);
HANMO_MILITARY_ROOT_VALUE(MilitaryTacticalMissionRecord3, tactical_mission3);
HANMO_MILITARY_ROOT_VALUE(DynamicMilitaryAction2, dynamic_action2);
HANMO_MILITARY_ROOT_VALUE(DynamicMilitarySessionCurrent2, session_current2);
HANMO_MILITARY_ROOT_VALUE(DynamicSessionStateSource2, session_source2);
HANMO_MILITARY_ROOT_VALUE(LocalHexMotionBasis2, local_motion_basis2);
HANMO_MILITARY_ROOT_VALUE(LocalMotionCursor2, local_motion_cursor2);
HANMO_MILITARY_ROOT_VALUE(LocalMotionBoundarySource, local_motion_boundary);
HANMO_MILITARY_ROOT_VALUE(MilitarySpatialChart2, spatial_chart2);
HANMO_MILITARY_ROOT_VALUE(MilitarySpatialHandoff2, spatial_handoff2);
HANMO_MILITARY_ROOT_VALUE(DynamicBattleEffectGroup2, effect_group2);
HANMO_MILITARY_ROOT_VALUE(DynamicMilitaryBattleFact2, battle_fact2);
HANMO_MILITARY_ROOT_VALUE(DynamicMilitaryCasualtyFact2, casualty_fact2);
HANMO_MILITARY_ROOT_VALUE(MilitarySupplyEffectSource2, supply_effect_source2);
HANMO_MILITARY_ROOT_VALUE(MilitaryDutyState, duty_state);
HANMO_MILITARY_ROOT_VALUE(DynamicMilitaryObjectiveEvidence2,
                          objective_evidence2);
#undef HANMO_MILITARY_ROOT_VALUE

// 职责：观察真实登记而不执行 Getter/行 callback；固定容量避免为反射
// 创建 allocator、空 State 或第二份世界。运行 codec 使用同一 visit。
class DescriptorArchive {
  MilitaryExactLayoutDescriptor &out_;
  Status status_ = common_success();

  // 职责：只记录成功登记过的有限根，拒绝越界、重复根及无名根。
  Status append(std::string_view name, MilitaryExactRootStorage storage,
                MilitaryExactRootValue value, Integer selection) {
    if (!status_.ok)
      return status_;
    if (name.empty() || out_.root_count == out_.roots.size())
      return reject("MilitaryExactDescriptorCapacityMismatch");
    for (std::size_t i = 0; i < out_.root_count; ++i)
      if (out_.roots[i].name == name)
        return reject("MilitaryExactDescriptorDuplicateRoot");
    out_.roots[out_.root_count++] = {name, storage, value, selection};
    return status_;
  }

public:
  // 职责：借本次固定描述结果，不持有或构造 Military 状态。
  explicit DescriptorArchive(MilitaryExactLayoutDescriptor &out) : out_(out) {
  }
  // 职责：保留第一错误，供共同绑定按原调用顺序短路。
  Status status() const {
    return status_;
  }
  // 职责：记录静态登记失败；不伪造业务来源或调用原世界写者。
  Status reject(std::string_view code) {
    if (status_.ok)
      status_ = common_failure(StableId(code), "dynamic_military");
    return status_;
  }
  // 职责：schema 根仍是实际 Integer 字段，选中版本保在描述身份。
  Status schema(Integer selected) {
    out_.selected_version = selected;
    return append("schema", MilitaryExactRootStorage::integer,
                  ValueKind<Integer>::value, 0);
  }
  // 职责：关联原真实表成员及行型，Getter 和 Fields 均不执行。
  template <class Row, class Getter, class Fields>
  Status table(std::string_view name, Getter, Fields, Integer selection) {
    military_exact_root_bindings::require_table_member<Row, Getter>();
    return append(name, MilitaryExactRootStorage::ordered_table,
                  ValueKind<Row>::value, selection);
  }
  // 职责：关联原具名嵌入 owner，不能把它误登记成独立物理段或数组。
  template <class Value, class Getter, class Fields>
  Status object(std::string_view name, Getter, Fields, Integer selection) {
    using Actual = std::remove_reference_t<decltype(std::declval<Getter>()(
        std::declval<MilitaryExactState &>()))>;
    static_assert(std::is_same_v<Actual, Value>);
    return append(name, MilitaryExactRootStorage::owned_object,
                  ValueKind<Value>::value, selection);
  }
};

// 职责：登记原基底经军医附录 A 覆盖后的固定 26 根。军医未选仍保
// 空尾槽；名字不含持值/codec，不会冒充缺失 owner 或所选业务已实现。
constexpr std::array<std::string_view, 26> canonical_registered_roots = {
    "schema",
    "definition_locks",
    "formations",
    "missions",
    "actions",
    "session_current_states",
    "session_state_sources",
    "local_motion_bases",
    "local_motion_cursors",
    "local_motion_prefix_closes",
    "spatial_charts",
    "spatial_handoffs",
    "effect_groups",
    "dynamic_battle_facts",
    "dynamic_casualty_facts",
    "supply_effect_sources",
    "continuation_designations",
    "continuation_results",
    "rejoin_sources",
    "duty_sources",
    "probe_sources",
    "objective_evidence",
    "fortification_nodes",
    "fortification_work_sources",
    "source_once_summaries",
    "military_medical_receipt_links"};

// 职责：比较原登记的全部身份槽，不将某一个文档摘要当完整组合身份。
bool same_reference(const MilitaryExactCanonicalReference &a,
                    const MilitaryExactCanonicalReference &b) {
  return a.section_name == b.section_name && a.base_version == b.base_version &&
         a.authority_sha256 == b.authority_sha256;
}
}

// 职责：从实际 typed 绑定生成描述；既有 codec 不读取这份结果自证。
Outcome<MilitaryExactLayoutDescriptor>
read_military_exact_codec_layout(Integer selected) {
  MilitaryExactLayoutDescriptor out;
  out.section_name = "dynamic_military";
  DescriptorArchive archive(out);
  auto visited = military_exact_root_bindings::visit(archive, selected);
  if (!visited.ok)
    return {false, {}, std::move(visited.error)};
  return {true, out, {}};
}

// 职责：核整份描述等同真实 callback，缺/多/乱序及错类型分别拒绝。
Status validate_military_exact_codec_layout(
    const MilitaryExactLayoutDescriptor &input) {
  if (input.section_name != "dynamic_military")
    return common_failure("MilitaryExactDescriptorSectionMismatch");
  auto actual = read_military_exact_codec_layout(input.selected_version);
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  if (input.root_count > input.roots.size() ||
      input.root_count != actual.value.root_count)
    return common_failure("MilitaryExactDescriptorRootCountMismatch");
  for (std::size_t i = 0; i < input.root_count; ++i) {
    const auto &a = input.roots[i];
    const auto &b = actual.value.roots[i];
    if (a.name != b.name)
      return common_failure("MilitaryExactDescriptorRootOrderMismatch",
                            StableId(b.name));
    if (a.storage != b.storage || a.value != b.value ||
        a.callback_selection != b.callback_selection)
      return common_failure("MilitaryExactDescriptorTypeBindingMismatch",
                            StableId(b.name));
  }
  return common_success();
}

// 职责：原基底＋军医/SEA/时间卡固定完整登记；只用于实现身份核验。
const MilitaryExactCanonicalReference &
military_exact_canonical_reference() noexcept {
  static constexpr MilitaryExactCanonicalReference reference{
      "dynamic_military",
      2,
      {"3a1c36f29a12b7b13d9abcf1e0183bb5e1e90c5dd91d9d679b551a94248366aa",
       "a216d7b44e4dad6db482c22d54d325fdb320b5ae947df8a4b56eec1089a7be74",
       "a67b1a513ab98b78ad4659d546650fc926904226b802a3f5647b611b69d685a9",
       "4393843a7baec45110521f1c7e471460ea9dd1405462c1393451f7364e4c129c"}};
  return reference;
}

// 职责：真实绑定和登记身份均合法后，逐根报告基底覆盖及后继边界。
Outcome<MilitaryExactCanonicalCoverage>
inspect_military_exact_canonical_coverage(
    const MilitaryExactLayoutDescriptor &input,
    const MilitaryExactCanonicalReference &reference) {
  auto actual = validate_military_exact_codec_layout(input);
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  if (!same_reference(reference, military_exact_canonical_reference()))
    return {
        false,
        {},
        common_failure("MilitaryCanonicalRegistrationIdentityMismatch").error};
  MilitaryExactCanonicalCoverage out;
  out.successor_registration_required =
      input.selected_version != reference.base_version;
  std::size_t actual_index = 0;
  for (const auto name : canonical_registered_roots) {
    if (actual_index < input.root_count &&
        input.roots[actual_index].name == name)
      ++actual_index;
    else
      out.missing_roots[out.missing_count++] = name;
  }
  if (actual_index != input.root_count)
    return {false,
            {},
            common_failure("MilitaryCanonicalRootSequenceMismatch").error};
  return {true, out, {}};
}

// 职责：核最终固定根绑定，缺项逐一保真实失败。即使所有根将来齐全，
// 独立军医业务与后继来源仍须各自原准入；本门不接管旧 codec。
Status validate_military_exact_canonical_bindings(
    const MilitaryExactLayoutDescriptor &input,
    const MilitaryExactCanonicalReference &reference) {
  auto coverage = inspect_military_exact_canonical_coverage(input, reference);
  if (!coverage.ok)
    return {false, {}, std::move(coverage.error)};
  if (coverage.value.missing_count)
    return common_failure("MilitaryCanonicalRootBindingRequired",
                          StableId(coverage.value.missing_roots[0]),
                          ErrorKind::unsupported_capability);
  if (coverage.value.successor_registration_required)
    return common_failure("MilitaryCanonicalSuccessorRegistrationRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
}
