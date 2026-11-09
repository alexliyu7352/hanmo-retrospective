#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_session_definition_catalog.hpp"

#include "dynamic_action_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"

namespace hanmo::resident {
namespace {
// 职责：唯一五槽顺序同时服务作者读写，duration 复用原精确 codec。
// 这里只陈列原 DTO 字段，业务合法性仍由原 Session validator 决定。
template <class C>
Status original_fields(C &codec, DynamicMilitarySessionDefinitionRead &value) {
  codec.field("definition_lock_ref", value.definition_lock_ref);
  codec.field("objective_definition_ref", value.objective_definition_ref);
  codec.field("random_algorithm_version", value.random_algorithm_version);
  dynamic_action_codec::field(codec, "deadline_duration",
                              value.deadline_duration);
  dynamic_action_codec::field(codec, "hold_duration", value.hold_duration);
  return codec.status();
}
} // namespace

// 职责：沿原 root_table 预算和唯一五槽 codec 接纳有限作者表。
// 原 rule/technical 错误保持，不补缺值、不创建会话或消耗随机数。
Outcome<MilitarySessionDefinitionCatalog>
decode_military_session_definition_catalog(const BoundaryValue &input) {
  auto *budget = ScopedCheckpointJsonDecode::current();
  if (!budget)
    return {false,
            {},
            common_failure("CheckpointJsonContextRequired",
                           "dynamic_military/session_definitions",
                           ErrorKind::internal)
                .error};
  return movement_detail::guard<MilitarySessionDefinitionCatalog>([&] {
    try {
      budget->default_values<MilitarySessionDefinitionCatalog>();
      MilitarySessionDefinitionCatalog catalog;
      ActorFoundationJsonVisitor original(input);
      movement_detail::take(original.root_table(
          catalog.definitions, "definitions",
          [](auto &codec, auto &row) {
            return original_fields(codec, row);
          },
          [](auto &destination,
             std::pair<StableId, DynamicMilitarySessionDefinitionRead> &source)
              -> Status {
            destination.emplace_back(std::move(source));
            return common_success();
          }));
      movement_detail::take(original.finish());
      movement_detail::take(
          validate_military_session_definition_catalog(catalog));
      return catalog;
    } catch (const exact_fields::CodecFailure &failure) {
      throw movement_detail::Failure{failure.error};
    }
  });
}

// 职责：原五槽经过同一 field visitor 输出，只用于作者边界而非当前态。
// 不在输出时校正非法值，读入端仍会按原规则明确拒绝。
Outcome<BoundaryValue> encode_military_session_definition(
    const DynamicMilitarySessionDefinitionRead &value) {
  return movement_detail::guard<BoundaryValue>([&] {
    try {
      BoundaryValue output;
      BoundaryProjectionWriter sink(output);
      movement_detail::take(sink.object("", [&](ProjectionWriter &body) {
        ActorFoundationProjectionArchive original(body);
        return original_fields(
            original,
            const_cast<DynamicMilitarySessionDefinitionRead &>(value));
      }));
      movement_detail::take(sink.finish());
      return output;
    } catch (const exact_fields::CodecFailure &failure) {
      throw movement_detail::Failure{failure.error};
    }
  });
}

// 职责：只添加目录 key/lock 对应检查，原数值和引用规则不复制。
// 空表合法但不会提供任何已选会话的定义。
Status validate_military_session_definition_catalog(
    const MilitarySessionDefinitionCatalog &catalog,
    ExactArithmeticBudget budget) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 82, "MilitarySessionDefinitionCatalog", "validate_military_session_definition_catalog", &catalog);

  for (const auto &row : catalog.definitions.rows()) {
    if (row.first.empty() || row.first != row.second.definition_lock_ref)
      return common_failure("MilitarySessionDefinitionKeyMismatch", row.first);
    auto valid =
        validate_dynamic_military_session_definition(row.second, budget);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}

// 职责：返回同一目录的原 definition lock 行，验证仍走唯一原规则。
// 没有任何 current、round、RNG 或目标 default 回退分支。
Outcome<const DynamicMilitarySessionDefinitionRead *>
read_military_session_definition(
    const MilitarySessionDefinitionCatalog &catalog, const StableId &lock,
    ExactArithmeticBudget budget) {
  const auto *definition = catalog.definitions.find(lock);
  if (!definition)
    return {false, nullptr,
            common_failure("UnknownMilitarySessionDefinition", lock).error};
  if (lock.empty() || definition->definition_lock_ref != lock)
    return {false, nullptr,
            common_failure("MilitarySessionDefinitionKeyMismatch", lock).error};
  auto valid =
      validate_dynamic_military_session_definition(*definition, budget);
  if (!valid.ok)
    return {false, nullptr, std::move(valid.error)};
  return {true, definition, {}};
}
} // namespace hanmo::resident
