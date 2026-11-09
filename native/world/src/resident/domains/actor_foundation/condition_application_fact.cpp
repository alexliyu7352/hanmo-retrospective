// 不把有效 Moment 当实际应用序；唯一事实序来自原 Common 同候选 append。
#include "condition_application_fact.hpp"
#include "condition_capture.hpp"
#include "model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <utility>
namespace hanmo::resident {
namespace {
constexpr std::string_view domain = "actor_foundation";
constexpr std::string_view kind = "condition_receipt";
struct SelectedApplicationOwners {
  bool family = false, politics = false, residence = false, injury = false;
};
// 唯一物理能力选择表；各原能力可共选，不将其它同域模块等同它们。
Outcome<SelectedApplicationOwners>
application_owners(const CatalogIdentity &identity) {
  SelectedApplicationOwners result;
  bool local_family2 = false, residence_seen = false;
  Integer condition_count = 0;
  bool condition8 = false;
  for (const auto &module : identity.module_versions) {
    if (module.first == "character_condition_extensions") {
      ++condition_count;
      condition8 = condition8 || module.second == "8";
      continue;
    }
    if (module.first == "character-injury-sources") {
      if (result.injury)
        return {false, {}, common_failure("DuplicateConditionApplicationSelection", module.first).error};
      if (module.second != "1")
        return {false, {}, common_failure("UnsupportedConditionApplicationSelection", module.first,
                                         ErrorKind::unsupported_capability).error};
      result.injury = true;
      continue;
    }
    if (module.first == "private_estate_services") {
      if (residence_seen)
        return {false,
                {},
                common_failure("DuplicateConditionApplicationSelection",
                               module.first)
                    .error};
      residence_seen = true;
      // 暂定内部 owner 命名，不改变 RESIDENCE-2 玩法。旧1不补应用头或历史能力。
      if (module.second == "1")
        continue;
      if (module.second != "2")
        return {false,
                {},
                common_failure("UnsupportedConditionApplicationSelection",
                               module.first, ErrorKind::unsupported_capability)
                    .error};
      result.residence = true;
      continue;
    }
    bool *choice = nullptr;
    if (module.first == "director_family_autonomy")
      choice = &result.family;
    else if (module.first == "political_continuity")
      choice = &result.politics;
    else
      continue;
    if (*choice)
      return {
          false,
          {},
          common_failure("DuplicateConditionApplicationSelection", module.first)
              .error};
    const bool family2 =
        module.first == "director_family_autonomy" && module.second == "2";
    if (module.second != "1" && !family2)
      return {false,
              {},
              common_failure("UnsupportedConditionApplicationSelection",
                             module.first, ErrorKind::unsupported_capability)
                  .error};
    *choice = true;
    local_family2 = local_family2 || family2;
  }
  // New source/1 and Condition8 are one explicit physical pair. Old1..7
  // keep their prior owners and never gain application headers by accident.
  if ((result.injury || condition8) &&
      (!result.injury || !condition8 || condition_count != 1))
    return {false, {}, common_failure("ConditionInjuryApplicationCompositionRequired",
                                     "character-injury-sources",
                                     ErrorKind::unsupported_capability).error};
  // v1f 本地2是明确静态组合，Content2由婚议3派生，没有同名新module键。
  // 此处只选真实应用头义务；作者两新根/定义/形成来源仍由完整安装门验证。
  // 旧autonomy1的合法接受域完全不变，不给旧无头profile迁移补序。
  if (local_family2) {
    bool marriage3 = false, communication2 = false;
    for (const auto &module : identity.module_versions) {
      bool *choice = nullptr;
      std::string_view version;
      if (module.first == "family_marriage") {
        choice = &marriage3;
        version = "3";
      } else if (module.first == "communication_links") {
        choice = &communication2;
        version = "2";
      } else {
        continue;
      }
      if (*choice || module.second != version)
        return {false,
                {},
                common_failure("ConditionApplicationFamily2CompositionRequired",
                               module.first, ErrorKind::unsupported_capability)
                    .error};
      *choice = true;
    }
    if (!marriage3 || !communication2)
      return {false,
              {},
              common_failure("ConditionApplicationFamily2CompositionRequired",
                             "director_family_autonomy",
                             ErrorKind::unsupported_capability)
                  .error};
  }
  return {true, result, {}};
}
// 域内最低同锁检查；完整 PreparedCatalog 身份由原 World 装配负责。
Outcome<bool> selected(const CommonStore &common,
                       const ActorFoundationCatalog &catalog) {
  auto choice = condition_application_facts_selected(catalog.identity);
  if (!choice.ok || !choice.value)
    return choice;
  if (catalog.identity.content_digest.empty() ||
      catalog.identity.content_digest !=
          common.state().metadata.content_digest ||
      catalog.scenario_id != common.state().metadata.scenario_id)
    return {false, false,
            common_failure("ConditionApplicationCatalogMismatch", {},
                           ErrorKind::internal)
                .error};
  return choice;
}
// 只允许原同 ID 头和已拥有正文；不是原 cause 许可 validator。
Outcome<ConditionApplicationFactView>
borrowed(const CommonStore &common, const AuthoredActorFoundationState &state,
         const FactRecord &header) {
  const auto *receipt =
      state.character_condition_extensions.applied_receipts.find(header.id);
  if (common.state().facts.find(header.id) != &header || header.kind != kind ||
      header.domain != domain || !receipt || receipt->result_id != header.id ||
      receipt->actor_id.empty() || !common.actor(receipt->actor_id))
    return {
        false,
        {},
        common_failure("ConditionApplicationOriginalHeaderRequired", header.id)
            .error};
  if (receipt->effective_at.kind != ActorFoundationAt::Kind::exact_moment)
    return {
        false,
        {},
        common_failure("ConditionApplicationExactReceiptRequired", header.id)
            .error};
  auto moment = compare_world_moments(receipt->effective_at.exact_moment,
                                      receipt->effective_at.exact_moment);
  if (!moment.ok)
    return {false, {}, moment.error};
  return {true, {&header, receipt, 2}, {}};
}
} // namespace
// 不新增模块/物理版本；只补首次已选本人表达能力的原历史来源义务。
Outcome<bool>
condition_application_facts_selected(const CatalogIdentity &identity) {
  auto owners = application_owners(identity);
  return owners.ok
             ? Outcome<bool>{true,
                             owners.value.family || owners.value.politics ||
                                 owners.value.residence || owners.value.injury,
                             {}}
             : Outcome<bool>{false, false, std::move(owners.error)};
}
Outcome<bool>
condition_application_facts_selected(const CatalogIdentity &identity,
                                     ConditionApplicationHistoryOwner owner) {
  auto owners = application_owners(identity);
  if (!owners.ok)
    return {false, false, std::move(owners.error)};
  switch (owner) {
  case ConditionApplicationHistoryOwner::family_autonomy:
    return {true, owners.value.family, {}};
  case ConditionApplicationHistoryOwner::political_continuity:
    return {true, owners.value.politics, {}};
  case ConditionApplicationHistoryOwner::estate_residence:
    return {true, owners.value.residence, {}};
  case ConditionApplicationHistoryOwner::injury_sources:
    return {true, owners.value.injury, {}};
  }
  return {false, false,
          common_failure("UnsupportedConditionApplicationOwner").error};
}
// parser 仅借唯一有限 kind；domain/实际正文仍由其原阶段另核。
bool owns_condition_application_fact_kind(std::string_view value) noexcept {
  return value == kind;
}
// 原实际 capture 的 scalar write-set 必须先成立，不能从冷 receipt 补历史头。
Status append_condition_application_fact_in_candidate(
    CommonStore &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog,
    const BoundActorConditionWrite &write) {
  auto choice = selected(common, catalog);
  if (!choice.ok)
    return {false, {}, choice.error};
  if (!choice.value)
    return common_success();
  auto actual = read_actual_actor_condition_write(common, state, write);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &rows =
      state.character_condition_extensions.applied_receipts.rows();
  if (rows.empty() || &rows.back().second != actual.value ||
      rows.back().first != actual.value->result_id)
    return common_failure("ConditionApplicationImmediateReceiptRequired",
                          write.receipt_ref.result_id);
  // 原 append 自带实际分配预算和 savepoint undo；失败由外层整候选撤回。
  return common.append_fact_borrowed(actual.value->result_id, kind, domain);
}
// 历史头不接受未经选型的 profile，也不把 copied header 当本世界原对象。
Outcome<ConditionApplicationFactView> read_condition_application_fact_header(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog, const FactRecord &header) {
  auto choice = selected(common, catalog);
  if (!choice.ok)
    return {false, {}, choice.error};
  if (!choice.value)
    return {false,
            {},
            common_failure("ConditionApplicationHistorySelectionRequired",
                           header.id, ErrorKind::unsupported_capability)
                .error};
  return borrowed(common, state, header);
}
// 原 once 命中不得调用 append；无头不能以今天执行补作“过去已应用”。
Status validate_condition_application_fact_for_receipt(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog, const ConditionReceiptV2 &receipt) {
  auto choice = selected(common, catalog);
  if (!choice.ok)
    return {false, {}, choice.error};
  const auto *header = common.state().facts.find(receipt.result_id);
  if (!choice.value)
    return header
               ? common_failure("ConditionApplicationHistorySelectionRequired",
                                receipt.result_id)
               : common_success();
  if (!header)
    return common_failure("ConditionApplicationOriginalHeaderRequired",
                          receipt.result_id);
  auto actual = borrowed(common, state, *header);
  if (!actual.ok)
    return {false, {}, actual.error};
  return actual.value.receipt == &receipt
             ? common_success()
             : common_failure("ConditionApplicationOriginalReceiptRequired",
                              receipt.result_id);
}
// 一次线性扫原 Common 顺序；不按 ID 数字或 Moment 对账，不保存派生索引。
Status validate_condition_application_fact_headers(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog) {
  auto choice = selected(common, catalog);
  if (!choice.ok)
    return {false, {}, choice.error};
  const auto &receipts = state.character_condition_extensions.applied_receipts;
  const auto &facts = common.state().facts;
  if (!choice.value) {
    for (const auto &row : facts.rows())
      if (row.second.kind == kind)
        return common_failure("ConditionApplicationHistorySelectionRequired",
                              row.first);
    return common_success();
  }
  if (facts.order().size() != facts.rows().size() ||
      receipts.order().size() != receipts.rows().size())
    return common_failure("ConditionApplicationOriginalOrderRequired");
  std::size_t next = 0;
  for (std::size_t n = 0; n < facts.order().size(); ++n) {
    const auto &id = facts.order()[n];
    const auto *header = facts.find(id);
    if (!header || header->id != id || facts.rows()[n].first != id ||
        &facts.rows()[n].second != header)
      return common_failure("ConditionApplicationOriginalOrderRequired", id);
    if (header->kind != kind)
      continue;
    if (next >= receipts.order().size() || receipts.order()[next] != id ||
        receipts.rows()[next].first != id)
      return common_failure("ConditionApplicationOrderCoverageMismatch", id);
    auto actual = borrowed(common, state, *header);
    if (!actual.ok)
      return {false, {}, actual.error};
    if (actual.value.receipt != &receipts.rows()[next].second)
      return common_failure("ConditionApplicationOriginalReceiptRequired", id);
    ++next;
  }
  return next == receipts.size()
             ? common_success()
             : common_failure("ConditionApplicationHeaderCoverageMissing");
}
// shared registry 只消费原借用头；不复制正文或注册第二 schema 推导表。
Status visit_condition_application_fact_headers(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog,
    ConditionApplicationFactVisitor &visitor) {
  auto valid =
      validate_condition_application_fact_headers(common, state, catalog);
  if (!valid.ok)
    return valid;
  auto choice = selected(common, catalog);
  if (!choice.ok)
    return {false, {}, choice.error};
  if (!choice.value)
    return common_success();
  for (const auto &row :
       state.character_condition_extensions.applied_receipts.rows()) {
    const auto *header = common.state().facts.find(row.first);
    if (!header)
      return common_failure("ConditionApplicationOriginalHeaderRequired",
                            row.first);
    auto actual = borrowed(common, state, *header);
    if (!actual.ok)
      return {false, {}, actual.error};
    valid = visitor.original_condition_application(actual.value);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
} // namespace hanmo::resident
