// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原认可作为可查社会事实保留；不会改亲生谱系，也不会替任何一方签控制许可。
#include "collateral_control_state.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
std::optional<Integer> serial(const CommonStore &c, const StableId &id,
                              const StableId &prefix) {
  const auto lead = prefix + ":";
  if (id.rfind(lead, 0) != 0)
    return {};
  auto digits = id.substr(lead.size());
  Integer n = 0;
  const auto p =
      std::from_chars(digits.data(), digits.data() + digits.size(), n);
  if (p.ec != std::errc{} || p.ptr != digits.data() + digits.size() || n < 1 ||
      n >= c.state().next_instance_id || std::to_string(n) != digits)
    return {};
  return n;
}
bool header(const CommonStore &c, const StableId &id) {
  const auto *h = c.state().facts.find(id);
  return serial(c, id, "family_collateral") && h && h->id == id &&
         h->kind == "family_collateral_recognized" &&
         h->domain == "family_collateral_legacy";
}
std::pair<StableId, StableId> pair(const CollateralRecognitionSource &s) {
  return std::minmax(s.first_actor_id, s.second_actor_id);
}
Outcome<CollateralRecognitionBasis>
read(const CommonStore &c, const CollateralControlState &state,
     const StableId &id, Integer tick, const StableId &fact, bool historical) {
  if (tick < 0 || tick > c.tick() ||
      (historical ? fact.empty() : (!fact.empty() || tick != c.tick())))
    return {false,
            {},
            common_failure("CollateralRecognitionBoundaryRequired").error};
  // 旧读口只消费物理1，不能借新wrapper默开source2。
  auto shape = validate_collateral_control_shape(state, 1);
  if (!shape.ok)
    return {false, {}, shape.error};
  const auto *wrapped = state.recognitions.find(id);
  const auto *row =
      wrapped ? std::get_if<CollateralRecognitionFact>(&wrapped->payload)
              : nullptr;
  const auto *commit = state.recognition_commits.find(id);
  if (!row || !header(c, id) || row->source.tick > tick || !commit)
    return {false,
            {},
            common_failure("CollateralRecognitionSourceRequired", id).error};
  const auto result = serial(c, commit->result_id, "result"),
             source = serial(c, id, "family_collateral");
  if (!source || !result || *result <= *source ||
      commit->actor_id != row->source.first_actor_id ||
      commit->kind != "family_collateral_recognize" || commit->revision < 1 ||
      commit->revision > c.revision() ||
      row->source.revision != commit->revision - 1)
    return {false,
            {},
            common_failure("CollateralRecognitionResultRequired", id).error};
  if (historical) {
    const auto *h = c.state().facts.find(fact);
    const auto split = fact.find(':');
    const auto boundary = split == StableId::npos
                              ? std::optional<Integer>{}
                              : serial(c, fact, fact.substr(0, split));
    const auto &order = c.state().facts.order();
    const auto a = std::find(order.begin(), order.end(), id),
               b = std::find(order.begin(), order.end(), fact);
    if (!h || h->id != fact || !boundary || *result >= *boundary ||
        a == order.end() || b == order.end() || a >= b)
      return {false,
              {},
              common_failure("CollateralRecognitionPriorResultRequired", fact)
                  .error};
  }
  return {true,
          {c.state().identity, id, row->source.first_actor_id,
           row->source.second_actor_id, fact, row->source.tick, tick,
           row->source.ancestry},
          {}};
}
} // namespace
Status
validate_collateral_recognition_history(const CollateralRecognitionContext &x,
                                        const CollateralControlState &state) {
  auto valid = validate_collateral_control_shape(state, 1);
  if (!valid.ok)
    return valid;
  valid = validate_collateral_control_catalog(x.catalog, x.original_requests,
                                              x.interactions, x.templates);
  if (!valid.ok)
    return valid;
  const auto &c = x.people.common;
  if (!x.catalog.enabled)
    return state.recognitions.empty() && state.recognition_commits.empty() &&
                   state.legacy_commits.empty()
               ? common_success()
               : common_failure("CollateralControlProfileRequired");
  std::vector<StableId> actual;
  std::set<std::pair<StableId, StableId>> pairs;
  Integer last_tick = -1, last_revision = -1, last_instance = 0;
  for (const auto &h : c.state().facts.rows())
    if (h.second.domain == "family_collateral_legacy")
      actual.push_back(h.first);
  if (actual != state.recognitions.order())
    return common_failure("CollateralRecognitionHeaderCoverageMismatch");
  for (const auto &row : state.recognitions.rows()) {
    if (!header(c, row.first) ||
        std::get<CollateralRecognitionFact>(row.second.payload).kind !=
            "family_collateral_recognized")
      return common_failure("CollateralRecognitionHeaderMismatch", row.first);
    const auto &s =
        std::get<CollateralRecognitionFact>(row.second.payload).source;
    const auto number = serial(c, row.first, "family_collateral");
    if (!number || *number <= last_instance || s.tick < last_tick ||
        s.revision <= last_revision || !pairs.insert(pair(s)).second)
      return common_failure("CollateralRecognitionSequenceMismatch", row.first);
    valid = validate_collateral_recognition_source_before_fact(x, s, row.first);
    if (!valid.ok)
      return valid;
    last_instance = *number;
    last_tick = s.tick;
    last_revision = s.revision;
  }
  return common_success();
}
Outcome<StableId> record_collateral_recognition_in_candidate(
    CommonStore &c, const CollateralRecognitionContext &x,
    CollateralControlState &state, const CollateralRecognitionSource &source) {
  if (&c != &x.people.common)
    return {false,
            {},
            common_failure("CollateralRecognitionCandidateMismatch").error};
  auto valid = validate_collateral_recognition_history(x, state);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = revalidate_collateral_recognition(x, source);
  if (!valid.ok)
    return {false, {}, valid.error};
  for (const auto &row : state.recognitions.rows())
    if (pair(std::get<CollateralRecognitionFact>(row.second.payload).source) ==
        pair(source))
      return {false,
              {},
              common_failure("CollateralAlreadyRecognized", row.first).error};
  auto id = c.next_id("family_collateral");
  if (!id.ok)
    return {false, {}, id.error};
  CollateralRecognitionFact fact;
  fact.source = source;
  if (!state.recognitions.insert(id.value, std::move(fact)))
    return {false,
            {},
            common_failure("DuplicateCollateralRecognition", id.value).error};
  valid = c.append_fact(
      {id.value, "family_collateral_recognized", "family_collateral_legacy"});
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_collateral_recognition_history(x, state);
  return valid.ok ? Outcome<StableId>{true, id.value, {}}
                  : Outcome<StableId>{false, {}, valid.error};
}
Outcome<CollateralRecognitionBasis> read_collateral_recognition(
    const CommonStore &c, const CollateralControlState &s, const StableId &id) {
  return read(c, s, id, c.tick(), {}, false);
}
Outcome<CollateralRecognitionBasis> read_collateral_recognition_before_fact(
    const CommonStore &c, const CollateralControlState &s, const StableId &id,
    Integer tick, const StableId &fact) {
  return read(c, s, id, tick, fact, true);
}
} // namespace hanmo::resident
