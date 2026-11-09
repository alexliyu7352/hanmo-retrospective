// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 一次零时本人确认只产生一个原认可，实际结果身份必须来自当前Common缓存对象。
#include "collateral_recognition_receipts.hpp"
#include "resident/kernel/command_types.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
Integer serial(const StableId &id, const char *prefix) {
  const auto lead = StableId{prefix} + ":";
  if (id.rfind(lead, 0) != 0)
    return -1;
  const auto text = id.substr(lead.size());
  Integer n = 0;
  auto p = std::from_chars(text.data(), text.data() + text.size(), n);
  return p.ec == std::errc{} && p.ptr == text.data() + text.size() && n > 0 &&
                 std::to_string(n) == text
             ? n
             : -1;
}
Status matches(const CollateralControlState &s, const StableId &id,
               const CollateralCommitBinding &b, const CommitResult &result) {
  const auto *wrapped = s.recognitions.find(id);
  const auto *source =
      wrapped ? std::get_if<CollateralRecognitionFact>(&wrapped->payload)
              : nullptr;
  const auto *effect = result.details
                           ? std::get_if<FamilyEffect>(&result.details->data)
                           : nullptr;
  if (!source || !result.ok || !effect || result.result_id != b.result_id ||
      result.kind != b.kind || result.revision != b.revision ||
      result.tick != source->source.tick)
    return common_failure("CollateralRecognitionActualResultMismatch", id);
  return validate_collateral_recognition_effect(s, id, result.kind, b.actor_id,
                                                result.tick, *effect);
}
Outcome<std::vector<StableId>> added(const CollateralControlState &before,
                                     const CollateralControlState &after) {
  try {
    auto trimmed = after;
    std::vector<StableId> ids;
    for (const auto &row : before.recognitions.rows())
      if (!after.recognitions.find(row.first))
        return {
            false,
            {},
            common_failure("CollateralRecognitionPriorSourceChanged").error};
    for (const auto &id : after.recognitions.order())
      if (!before.recognitions.find(id))
        ids.push_back(id);
    for (const auto &id : ids)
      trimmed.recognitions.erase(id);
    if (!compare_boundary(encode_collateral_control_state(before),
                          encode_collateral_control_state(trimmed))
             .exact())
      return {false,
              {},
              common_failure("CollateralRecognitionPriorSourceChanged").error};
    return {true, std::move(ids), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace
Status validate_collateral_recognition_effect(
    const CollateralControlState &s, const StableId &id, const StableId &kind,
    const StableId &actor, Integer tick, const FamilyEffect &e) {
  auto shape = validate_collateral_control_shape(s, 1);
  if (!shape.ok)
    return shape;

  const auto *wrapped = s.recognitions.find(id);
  const auto *r =
      wrapped ? std::get_if<CollateralRecognitionFact>(&wrapped->payload)
              : nullptr;
  const std::set<StableId> fields(e.field_order.begin(), e.field_order.end());
  if (!r || r->source.first_actor_id != actor || r->source.tick != tick ||
      kind != "family_collateral_recognize" ||
      e.success != std::optional<bool>{true} ||
      e.actor_id != std::optional<StableId>{actor} ||
      e.player_actor_id != e.actor_id ||
      e.fact_id != std::optional<StableId>{id} || e.already_completed ||
      e.code || e.heir_actor_id ||
      (!e.field_order.empty() &&
       (e.field_order.size() != fields.size() ||
        fields != std::set<StableId>{"ok", "success", "actor_id",
                                     "player_actor_id", "fact_id"})))
    return common_failure("CollateralRecognitionEffectMismatch", id);
  return common_success();
}
Status validate_collateral_recognition_result_bindings(
    const CommonStore &c, const CollateralControlState &s) {
  auto valid = validate_collateral_control_shape(s, 1);
  if (!valid.ok)
    return valid;
  if (s.recognitions.order() != s.recognition_commits.order())
    return common_failure("CollateralRecognitionCommitCoverageMismatch");
  std::set<StableId> results;
  std::set<Integer> revisions;
  std::set<std::pair<StableId, Integer>> requests;
  Integer prior_result = 0, prior_revision = 0, prior_tick = -1;
  const auto &common_order = c.state().facts.order();
  auto prior = common_order.begin();
  bool seen = false;
  for (const auto &row : s.recognition_commits.rows()) {
    const auto *r = s.recognitions.find(row.first);
    const auto &source = std::get<CollateralRecognitionFact>(r->payload).source;
    const auto &b = row.second;
    const auto *h = c.state().facts.find(row.first);
    const auto *sequence = c.state().request_sequences.find(b.actor_id);
    const auto source_id = serial(row.first, "family_collateral"),
               result_id = serial(b.result_id, "result");
    const auto at =
        std::find(common_order.begin(), common_order.end(), row.first);
    if (!h || h->id != row.first || h->kind != "family_collateral_recognized" ||
        h->domain != "family_collateral_legacy" || at == common_order.end() ||
        (seen && at <= prior) || source_id <= prior_result ||
        result_id <= source_id || result_id >= c.state().next_instance_id ||
        b.actor_id != source.first_actor_id ||
        b.kind != "family_collateral_recognize" || source.tick < prior_tick ||
        source.tick > c.tick() || source.revision != b.revision - 1 ||
        b.revision <= prior_revision || b.revision > c.revision() ||
        !sequence || b.request_seq < 1 || b.request_seq > *sequence ||
        !results.insert(b.result_id).second ||
        !revisions.insert(b.revision).second ||
        !requests.emplace(b.actor_id, b.request_seq).second)
      return common_failure("InvalidCollateralRecognitionCommit", row.first);
    prior = at;
    seen = true;
    prior_result = result_id;
    prior_revision = b.revision;
    prior_tick = source.tick;
    for (const auto &entry : c.state().history)
      if (entry.result_id == b.result_id &&
          (entry.actor_id != b.actor_id || entry.kind != b.kind ||
           entry.tick != source.tick))
        return common_failure("CollateralRecognitionHistoryMismatch",
                              row.first);
    const auto key = b.actor_id + ":" + std::to_string(b.request_seq);
    if (const auto *cached = c.state().results.find(key)) {
      if (!*cached)
        return common_failure("CollateralRecognitionActualResultRequired",
                              row.first);
      valid = matches(s, row.first, b, **cached);
      if (!valid.ok)
        return valid;
    }
    for (const auto &cached : c.state().results.rows())
      if (cached.second &&
          (cached.second->result_id == b.result_id ||
           cached.second->revision == b.revision) &&
          cached.first != key)
        return common_failure("CollateralRecognitionCacheIdentityMismatch",
                              row.first);
  }
  for (const auto &cached : c.state().results.rows())
    if (cached.second && cached.second->details) {
      const auto *e = std::get_if<FamilyEffect>(&cached.second->details->data);
      if (!e || !e->fact_id || !s.recognitions.find(*e->fact_id))
        continue;
      const auto *b = s.recognition_commits.find(*e->fact_id);
      if (!b ||
          cached.first != b->actor_id + ":" + std::to_string(b->request_seq))
        return common_failure("CollateralRecognitionCachedSourceReused",
                              *e->fact_id);
      valid = matches(s, *e->fact_id, *b, *cached.second);
      if (!valid.ok)
        return valid;
    }
  return common_success();
}
Status bind_collateral_recognition_result_in_candidate(
    CommonStore &c, const CollateralControlState &before,
    CollateralControlState &after, const StableId &actor, Integer sequence,
    const CommitResult &result) {
  auto valid = validate_collateral_recognition_result_bindings(c, before);
  if (!valid.ok)
    return valid;
  valid = validate_collateral_control_shape(after, 1);
  if (!valid.ok)
    return valid;
  auto difference = added(before, after);
  if (!difference.ok)
    return {false, {}, difference.error};
  if (difference.value.empty())
    return validate_collateral_recognition_result_bindings(c, after);
  if (difference.value.size() != 1)
    return common_failure("OneCollateralRecognitionPerRequest");
  const auto *cached =
      c.state().results.find(actor + ":" + std::to_string(sequence));
  const auto *requested = c.state().request_sequences.find(actor);
  if (!cached || cached->get() != &result || !requested ||
      *requested != sequence || result.revision != c.revision() ||
      result.tick != c.tick() || c.state().history.empty())
    return common_failure("CollateralRecognitionActualResultRequired");
  const auto &h = c.state().history.back();
  if (h.result_id != result.result_id || h.actor_id != actor ||
      h.kind != result.kind || h.tick != result.tick)
    return common_failure("CollateralRecognitionActualHistoryRequired");
  const auto &id = difference.value.front();
  CollateralCommitBinding b{result.result_id, actor,           result.kind,
                            sequence,         result.revision, {}};
  valid = matches(after, id, b, result);
  if (!valid.ok)
    return valid;
  if (!after.recognition_commits.insert(id, b))
    return common_failure("CollateralRecognitionAlreadyBound", id);
  valid = validate_collateral_recognition_result_bindings(c, after);
  if (!valid.ok)
    after.recognition_commits.erase(id);
  return valid;
}
Outcome<StableId>
read_collateral_recognition_for_result(const CommonStore &c,
                                       const CollateralControlState &s,
                                       const CommitResult &result) {
  Integer found = 0;
  for (const auto &cached : c.state().results.rows())
    if (cached.second.get() == &result)
      ++found;
  if (found != 1)
    return {false,
            {},
            common_failure("CollateralRecognitionActualResultRequired").error};
  auto valid = validate_collateral_recognition_result_bindings(c, s);
  if (!valid.ok)
    return {false, {}, valid.error};
  for (const auto &row : s.recognition_commits.rows())
    if (row.second.result_id == result.result_id) {
      valid = matches(s, row.first, row.second, result);
      return valid.ok ? Outcome<StableId>{true, row.first, {}}
                      : Outcome<StableId>{false, {}, valid.error};
    }
  return {false,
          {},
          common_failure("CollateralRecognitionResultSourceRequired").error};
}
} // namespace hanmo::resident
