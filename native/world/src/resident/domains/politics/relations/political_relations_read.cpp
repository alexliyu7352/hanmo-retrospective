#include "political_relations_read.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <map>
#include <new>
#include <set>
#include <stdexcept>
#include <string_view>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
  return std::move(r.value);
}
void take(Status r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
}
using Pair = std::pair<std::string_view, std::string_view>;
using RelationMap = CheckpointMap<Pair, PoliticalRelation>;
using BorrowedIds = std::set<std::string_view, std::less<>,
                             CheckpointAllocator<std::string_view>>;
Pair key(const PoliticalFactionPair &p) { return {p.first, p.second}; }
Pair key(const Pair &p) { return p; }
std::size_t change_position(const CommonStore &c,
                            const PoliticalBattleRelationChange &r) {
  return take(read_actual_common_fact_position(
      c, SourceRefBuildView{"politics", "political_battle_relation_changed",
                            r.id}));
}
std::size_t position(const CommonStore &c, const SourceRef &ref) {
  return take(read_actual_common_fact_position(c, ref));
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
Pair pair(const PoliticalRelationsReadContext &x, const StableId &a,
          const StableId &b) {
  if (a.empty() || b.empty() || a == b)
    fail("InvalidPoliticalFactionPair");
  const StableId &first = a < b ? a : b;
  const StableId &second = a < b ? b : a;
  if (!x.organizations.organizations.find(first) ||
      !x.organizations.organizations.find(second))
    fail("PoliticalRelationFactionMissing", a);
  return {first, second};
}
void binding(const PoliticalRelationsReadContext &x) {
  if (!x.actual_clock.bound_to(x.common, x.military, x.field) ||
      !x.formations.bound_to(x.common.state(), x.military))
    fail("PoliticalRelationsForeignClockOwner");
  take(validate_political_relations_catalog(x.catalog, x.organization_catalog));
}
RelationMap initial(const PoliticalRelationsReadContext &x) {
  RelationMap out;
  for (const auto &r : x.catalog.initial_relations) {
    pair(x, r.factions.first, r.factions.second);
    out.emplace(key(r.factions), r.relation);
  }
  return out;
}
PoliticalRelation value(const RelationMap &values, const Pair &p) {
  const auto i = values.find(p);
  return i == values.end() ? PoliticalRelation::neutral : i->second;
}
struct Scan {
  CheckpointVector<const PoliticalBattleRelationChange *> ordered;
};
Scan scan(const PoliticalRelationsReadContext &x,
          const SourceRef *single_pending_start = nullptr) {
  binding(x);
  take(validate_political_relations_shape(x.state));
  const auto init_pos = position(x.common, x.state.initialization);
  const auto now = take(x.actual_clock.read_current_moment());
  if (compare(x.state.initialized_at, now) > 0)
    fail("PoliticalRelationsInitializationInFuture");
  auto values = initial(x);
  BorrowedIds seen, starts, breaches;
  Scan out;
  std::size_t prior_start_pos = init_pos;
  for (const auto &id : x.common.state().facts.order()) {
    const auto *row = x.state.battle_changes.find(id);
    if (!row)
      continue;
    const auto row_pos = change_position(x.common, *row);
    if (row_pos <= init_pos || !seen.insert(id).second)
      fail("PoliticalRelationChangeOrder", id);
    const auto source = take(read_field_battle_started_source(
        x.common, x.military, x.field, row->battle_start.source_ref,
        x.actual_clock, x.formations));
    if (!source.session || !source.actual_source || !source.started_actual_at ||
        !source.factions[0] || !source.factions[1] || source.source_schema != 1)
      fail("PoliticalRelationOriginalStartMissing", id);
    const auto sides = pair(x, *source.factions[0], *source.factions[1]);
    if (source.session->battle_id != row->battle_id ||
        key(sides) != key(row->factions) ||
        compare(*source.started_actual_at, row->occurred_at) != 0 ||
        compare(x.state.initialized_at, row->occurred_at) > 0 ||
        compare(row->occurred_at, now) > 0 ||
        position(x.common, *source.actual_source) >= row_pos ||
        !starts.insert(source.actual_source->id).second)
      fail("PoliticalRelationOriginalStartMismatch", id);
    const auto start_pos = position(x.common, *source.actual_source);
    if (start_pos <= prior_start_pos)
      fail("PoliticalRelationOriginalStartOrder", id);
    prior_start_pos = start_pos;
    const auto before = value(values, key(sides));
    if (row->before != before)
      fail("PoliticalRelationHistoricalBeforeMismatch", id);
    if (row->alliance_breach) {
      const auto *breach =
          x.state.alliance_breaches.find(row->alliance_breach->id);
      if (!breach || before != PoliticalRelation::allied ||
          position(x.common, *row->alliance_breach) <= row_pos ||
          !breaches.insert(breach->id).second)
        fail("PoliticalAllianceBreachOriginalMismatch", id);
    }
    values[key(sides)] = PoliticalRelation::hostile;
    out.ordered.push_back(row);
  }
  if (seen.size() != x.state.battle_changes.rows().size() ||
      breaches.size() != x.state.alliance_breaches.rows().size())
    fail("PoliticalRelationsPayloadWithoutFact");
  for (const auto &f : x.common.state().facts.rows())
    if (f.second.domain == "politics") {
      if (f.second.kind == "political_relations_initialized" &&
          f.first != x.state.initialization.id)
        fail("DuplicatePoliticalRelationsInitialization", f.first);
      if (f.second.kind == "political_battle_relation_changed" &&
          !seen.count(f.first))
        fail("PoliticalRelationChangePayloadMissing", f.first);
      if (f.second.kind == "political_alliance_breached" &&
          !breaches.count(f.first))
        fail("PoliticalAllianceBreachPayloadMissing", f.first);
    }
  for (const auto &f : x.common.state().facts.rows())
    if (f.second.domain == "military" &&
        f.second.kind == "field_battle_started") {


      auto original_memory = CheckpointMemoryCharge(0);
      const SourceRef original{checkpoint_fresh_string_copy(f.second.domain),
                               checkpoint_fresh_string_copy(f.second.kind),
                               checkpoint_fresh_string_copy(f.first)};
      take(read_field_battle_started_source(x.common, x.military, x.field,
                                            original, x.actual_clock,
                                            x.formations));
      if (!starts.count(f.first) &&
          (!single_pending_start ||
           !same_source_ref(*single_pending_start, original)))
        fail("PoliticalRelationOriginalStartUnadopted", f.first);
    }
  for (const auto &[id, s] : x.field.sessions)
    if (s.started_source) {
      auto source = take(read_field_battle_started_source(
          x.common, x.military, x.field, *s.started_source, x.actual_clock,
          x.formations));
      if (!source.session || source.session->battle_id != id)
        fail("PoliticalRelationOriginalBattleIdentity", id);
      if (!starts.count(s.started_source->id) &&
          (!single_pending_start ||
           !same_source_ref(*single_pending_start, *s.started_source)))
        fail("PoliticalRelationBattleNotAdopted", id);
    }
  return out;
}
PoliticalRelationOriginalView view(const PoliticalRelationsReadContext &x,
                                   const Scan &checked, const Pair &factions,
                                   std::optional<std::size_t> cut) {
  auto values = initial(x);
  PoliticalRelationOriginalView out{
      value(values, factions),
      x.common.state().facts.find(x.state.initialization.id), nullptr};
  if (!out.initialization)
    fail("PoliticalRelationsPayloadWithoutFact");
  for (const auto *row : checked.ordered) {
    if (cut && change_position(x.common, *row) >= *cut)
      break;
    if (key(row->factions) == factions) {
      out.relation = PoliticalRelation::hostile;
      out.last_change = x.common.state().facts.find(row->id);
      if (!out.last_change)
        fail("PoliticalRelationChangePayloadMissing", row->id);
    }
  }
  return out;
}
PoliticalRelation folded(const PoliticalRelationsReadContext &x,
                         const Scan &checked, const Pair &factions,
                         std::optional<std::size_t> cut) {
  return view(x, checked, factions, cut).relation;
}
// 旧裸View仅末端兼容；正式原头读不经过这份拥有复制。
PoliticalRelationView
compatibility_view(const PoliticalRelationOriginalView &v) {
  PoliticalRelationView out{
      v.relation,
      {v.initialization->domain, v.initialization->kind, v.initialization->id},
      std::nullopt};
  if (v.last_change)
    out.last_change = SourceRef{v.last_change->domain, v.last_change->kind,
                                v.last_change->id};
  return out;
}
void field_cut(const PoliticalRelationsReadContext &x, const SourceRef &cut) {
  if (position(x.common, cut) <= position(x.common, x.state.initialization))
    fail("PoliticalRelationCutBeforeInitialization", cut.id);
  if (cut.domain != "military")
    fail("PoliticalRelationFieldCutRequired", cut.id);
  if (cut.kind == "field_battle_started") {
    take(read_field_battle_started_source(x.common, x.military, x.field, cut,
                                          x.actual_clock, x.formations));
    return;
  }
  if (cut.kind == "field_battle_contact") {
    const auto i = x.field.contacts.find(cut.id);
    const auto b = x.field.boundary_contacts.find(cut.id);
    const bool initial = i != x.field.contacts.end(),
               boundary = b != x.field.boundary_contacts.end();
    if (initial == boundary)
      fail("PoliticalRelationOriginalContactAmbiguous", cut.id);
    const auto &id = initial ? i->second.contact_id : b->second.contact_id;
    const auto &battle = initial ? i->second.battle_id : b->second.battle_id;
    if (id != cut.id || x.field.sessions.find(battle) == x.field.sessions.end())
      fail("PoliticalRelationOriginalContactMissing", cut.id);
    return;
  }
  fail("PoliticalRelationFieldCutUnsupported", cut.id);
}
} // namespace
Status validate_political_relations(const PoliticalRelationsReadContext &x) {
  try {
    scan(x);
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<PoliticalRelationsOriginalSource>
read_political_relations_original_source(const PoliticalRelationsReadContext &x,
                                         const MomentOwnerSource &source) {
  try {
    const auto &ref = source.source_ref;
    if (source.source_schema != 1 || ref.domain != "politics" ||
        (ref.kind != "political_relations_initialized" &&
         ref.kind != "political_battle_relation_changed" &&
         ref.kind != "political_alliance_breached"))
      fail("PoliticalRelationsOriginalSourceRequired", ref.id);
    // 原完整扫描仍承担目录/唯一主权/所有真实开战及双向事实覆盖；
    // 此出口不靠仅存在头或当前关系替代原源合法性。
    scan(x);
    position(x.common, ref);
    const auto *head = x.common.state().facts.find(ref.id);
    if (!head)
      fail("PoliticalRelationsOriginalSourceRequired", ref.id);
    const WorldMoment *occurred_at = nullptr;
    if (ref.kind == "political_relations_initialized") {
      if (!same_source_ref(ref, x.state.initialization))
        fail("PoliticalRelationsOriginalInitializationRequired", ref.id);
      occurred_at = &x.state.initialized_at;
    } else if (ref.kind == "political_battle_relation_changed") {
      const auto *row = x.state.battle_changes.find(ref.id);
      if (!row || row->id != ref.id)
        fail("PoliticalRelationChangePayloadMissing", ref.id);
      occurred_at = &row->occurred_at;
    } else {
      const auto *breach = x.state.alliance_breaches.find(ref.id);
      const auto *parent =
          breach ? x.state.battle_changes.find(breach->relation_change.id)
                 : nullptr;
      if (!breach || breach->id != ref.id || !parent ||
          !parent->alliance_breach ||
          !same_source_ref(*parent->alliance_breach, ref))
        fail("PoliticalAllianceBreachOriginalMismatch", ref.id);
      occurred_at = &parent->occurred_at;
    }
    return {true, {head, occurred_at, 1}, {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("PoliticalRelationsOriginalSourceMemory").error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("PoliticalRelationsOriginalSourceMemory").error};
  }
}

Outcome<PoliticalRelationOriginalView>
read_current_political_relation_original_view(
    const PoliticalRelationsReadContext &x, const StableId &a,
    const StableId &b) {
  try {
    const auto p = pair(x, a, b);
    const auto checked = scan(x);
    return {true, view(x, checked, p, std::nullopt), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<PoliticalRelationOriginalView>
read_political_relation_original_view_before_field_source(
    const PoliticalRelationsReadContext &x, const SourceRef &cut,
    const StableId &a, const StableId &b) {
  try {
    const auto p = pair(x, a, b);
    const auto checked = scan(x);
    field_cut(x, cut);
    return {true, view(x, checked, p, position(x.common, cut)), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<PoliticalRelationView>
read_current_political_relation_view(const PoliticalRelationsReadContext &x,
                                     const StableId &a, const StableId &b) {
  auto v = read_current_political_relation_original_view(x, a, b);
  if (!v.ok)
    return {false, {}, std::move(v.error)};
  return {true, compatibility_view(v.value), {}};
}
Outcome<PoliticalRelationView> read_political_relation_view_before_field_source(
    const PoliticalRelationsReadContext &x, const SourceRef &cut,
    const StableId &a, const StableId &b) {
  auto v =
      read_political_relation_original_view_before_field_source(x, cut, a, b);
  if (!v.ok)
    return {false, {}, std::move(v.error)};
  return {true, compatibility_view(v.value), {}};
}
Outcome<PoliticalRelation>
read_current_political_relation(const PoliticalRelationsReadContext &x,
                                const StableId &a, const StableId &b) {
  try {
    const auto p = pair(x, a, b);
    const auto checked = scan(x);
    return {true, folded(x, checked, p, std::nullopt), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<PoliticalRelation> read_political_relation_before_field_source(
    const PoliticalRelationsReadContext &x, const SourceRef &cut,
    const StableId &a, const StableId &b) {
  try {
    const auto p = pair(x, a, b);
    const auto checked = scan(x);
    field_cut(x, cut);
    return {true, folded(x, checked, p, position(x.common, cut)), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
namespace {
Status initialize_original(CommonStore &common, PoliticalRelationsState &state,
                           const PoliticalRelationsReadContext &x,
                           CheckpointMemoryCharge *root_memory) {
  try {
    if (root_memory && root_memory->resource()) {
      const auto current = current_checkpoint_memory();
      if (!current || !root_memory->resource()->shares_job(*current))
        fail("PoliticalRelationsInitializationJobMismatch");
    }
    if (&common != &x.common || &state != &x.state ||
        !state.initialization.id.empty() ||
        !state.battle_changes.rows().empty() ||
        !state.alliance_breaches.rows().empty())
      fail("PoliticalRelationsInitializationCandidateMismatch");
    binding(x);
    initial(x);
    for (const auto &[id, s] : x.field.sessions)
      if (s.started_source)
        fail("PoliticalRelationsInitializationAfterBattle", id);
    state.schema_version = 1;
    state.initialized_at = take(x.actual_clock.read_current_moment());
    auto id = take(common.next_owned_id("political_relations_initialization"));


    if (root_memory)
      (*root_memory).grow(0);
    state.initialization = {
        checkpoint_fresh_text_copy("politics"),
        checkpoint_fresh_text_copy("political_relations_initialized"),
        checkpoint_fresh_string_copy(id.id())};
    take(common.append_fact_borrowed(id.id(), "political_relations_initialized",
                                     "politics"));
    scan(x);
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  } catch (const std::bad_alloc &) {
    return common_failure("PoliticalRelationsInitializationMemory");
  } catch (const std::length_error &) {
    return common_failure("PoliticalRelationsInitializationMemory");
  }
}
} // namespace
Status initialize_political_relations_owned_in_candidate(
    CommonStore &common, PoliticalRelationsState &state,
    const PoliticalRelationsReadContext &x,
    CheckpointMemoryCharge &root_memory) {
  return initialize_original(common, state, x, &root_memory);
}
Status initialize_political_relations_in_candidate(
    CommonStore &common, PoliticalRelationsState &state,
    const PoliticalRelationsReadContext &x) {
  return initialize_original(common, state, x, nullptr);
}

namespace {
const FactRecord *adopted_head(const CommonStore &common, const StableId &id) {
  const auto *head = common.state().facts.find(id);
  if (!head || head->id != id || head->domain != "politics" ||
      head->kind != "political_battle_relation_changed")
    fail("PoliticalRelationChangePayloadMissing", id);
  take(read_actual_common_fact_position(common, *head));
  return head;
}
} // namespace
Outcome<const FactRecord *> adopt_field_battle_relation_fact_in_candidate(
    CommonStore &common, PoliticalRelationsState &state,
    const PoliticalRelationsReadContext &x, const MomentOwnerSource &start) {
  try {
    if (&common != &x.common || &state != &x.state || start.source_schema != 1)
      fail("PoliticalRelationCandidateMismatch");
    const auto actual = take(read_field_battle_started_source(
        common, x.military, x.field, start.source_ref, x.actual_clock,
        x.formations));
    if (!actual.session || !actual.actual_source || !actual.started_actual_at ||
        !actual.factions[0] || !actual.factions[1])
      fail("PoliticalRelationOriginalStartMissing");
    const auto checked = scan(x, &start.source_ref);
    for (const auto *old : checked.ordered)
      if (same_source_ref(old->battle_start.source_ref, start.source_ref))
        return {true, adopted_head(common, old->id), {}};
    if (compare(*actual.started_actual_at,
                take(x.actual_clock.read_current_moment())) != 0)
      fail("PoliticalRelationStartNotCurrentCandidate");
    const auto factions = pair(x, *actual.factions[0], *actual.factions[1]);
    const auto before =
        folded(x, checked, factions, position(common, start.source_ref));
    auto produced =
        take(common.next_owned_id("political_battle_relation_change"));


    auto row_memory = CheckpointMemoryCharge(0);
    PoliticalBattleRelationChange row;
    row.id = checkpoint_fresh_string_copy(produced.id());
    row.battle_id = checkpoint_fresh_string_copy(actual.session->battle_id);
    row.factions = {checkpoint_fresh_text_copy(factions.first),
                    checkpoint_fresh_text_copy(factions.second)};
    row.battle_start = {{checkpoint_fresh_string_copy(start.source_ref.domain),
                         checkpoint_fresh_string_copy(start.source_ref.kind),
                         checkpoint_fresh_string_copy(start.source_ref.id)},
                        start.source_schema};
    row.occurred_at = *actual.started_actual_at;
    row.before = before;
    take(common.append_fact_borrowed(
        produced.id(), "political_battle_relation_changed", "politics"));
    if (row.before == PoliticalRelation::allied) {
      auto breach_id = take(common.next_owned_id("political_alliance_breach"));


      auto breach_memory = CheckpointMemoryCharge(0);
      PoliticalAllianceBreach breach;
      breach.id = checkpoint_fresh_string_copy(breach_id.id());
      breach.relation_change = {
          checkpoint_fresh_text_copy("politics"),
          checkpoint_fresh_text_copy("political_battle_relation_changed"),
          checkpoint_fresh_string_copy(produced.id())};
      take(common.append_fact_borrowed(
          breach_id.id(), "political_alliance_breached", "politics"));
      if (!state.alliance_breaches.insert(breach_id.id(), std::move(breach)))
        fail("DuplicatePoliticalAllianceBreach", breach_id.id());


      (row_memory).grow(0);
      row.alliance_breach =
          SourceRef{checkpoint_fresh_text_copy("politics"),
                    checkpoint_fresh_text_copy("political_alliance_breached"),
                    checkpoint_fresh_string_copy(breach_id.id())};
    }
    if (!state.battle_changes.insert(produced.id(), std::move(row)))
      fail("DuplicatePoliticalBattleRelationChange", produced.id());
    scan(x);
    return {true, adopted_head(common, produced.id()), {}};
  } catch (const Failure &e) {
    return {false, nullptr, e.error};
  } catch (const std::bad_alloc &) {
    return {false, nullptr,
            common_failure("PoliticalRelationsResourceExhausted").error};
  } catch (const std::length_error &) {
    return {false, nullptr,
            common_failure("PoliticalRelationsResourceExhausted").error};
  }
}
// 旧拥有ABI只作末端兼容；正式新caller应借上面的真实Common头。
Outcome<SourceRef> adopt_field_battle_relation_in_candidate(
    CommonStore &common, PoliticalRelationsState &state,
    const PoliticalRelationsReadContext &x, const MomentOwnerSource &start) {
  try {
    auto result =
        adopt_field_battle_relation_fact_in_candidate(common, state, x, start);
    if (!result.ok)
      return {false, {}, std::move(result.error)};
    return {
        true, {result.value->domain, result.value->kind, result.value->id}, {}};
  } catch (const std::bad_alloc &) {
    return {
        false, {}, common_failure("PoliticalRelationsResourceExhausted").error};
  } catch (const std::length_error &) {
    return {
        false, {}, common_failure("PoliticalRelationsResourceExhausted").error};
  }
}
} // namespace hanmo::resident
