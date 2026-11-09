#include "political_shop_fact_identity.hpp"
#include "political_shop_licensing_exact2.hpp"
#include "political_shop_licensing_original_provenance.hpp"
#include "political_shop_member_history.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/organization/grant_authority.hpp"
#include "resident/domains/organization/office_authority.hpp"
// 司法签权逐原Office历史和专项Grant核验；占领不自动继承旧完整册的覆盖保证。
#include "hanmo/resident/checkpoint_memory.hpp"
#include "political_shop_licensing_detail.hpp"
#include "resident/domains/military/occupation_rules.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include "resident/domains/politics/takeover/politics_takeover_state.hpp"
#include <limits>
#include <string_view>
namespace hanmo::resident::shop_law_detail {
// 只读索引键借本次实际原根；节点由同job allocator真实计账，不跨写入保留。
using LawBorrowedIds = std::set<std::string_view, std::less<>,
                                CheckpointAllocator<std::string_view>>;

StableId prefix_for(const StableId &k) { return StableId(prefix_view(k)); }
Outcome<Integer> fact_sequence(const CommonStore &common, const StableId &id,
                               const StableId &kind) {
  const auto prefix = prefix_for(kind);
  const auto *h = common.state().facts.find(id);
  auto n = organization_instance_sequence(id, prefix);
  if (prefix.empty() || !h || h->id != id || h->kind != kind ||
      h->domain != (kind == "private_shop_opened"
                        ? "private_shops"
                        : political_shop_licensing_module) ||
      !n || *n >= common.state().next_instance_id)
    return {false, 0, common_failure("PoliticalShopFactMismatch", id).error};
  return {true, *n, {}};
}
bool before(const CommonStore &c, const StableId &a, const StableId &b) {
  bool found = false;
  for (const auto &row : c.state().facts.rows()) {
    if (row.first == b)
      return found;
    if (row.first == a)
      found = true;
  }
  return false;
}
Status person_alive_before(const PoliticalShopLicensingContext &x,
                           const StableId &actor, Integer tick,
                           const StableId &boundary) {
  const auto *person = x.common.actor(actor);
  if (!person || tick < 0 || tick > x.common.tick())
    return common_failure("PoliticalShopActorLifeHistoryUnavailable", actor);
  if (x.state.version == 2 && !boundary.empty()) {
    const auto *head = x.common.state().facts.find(boundary);
    if (!head)
      return common_failure("PoliticalShopActualLifeBoundaryRequired",
                            boundary);
    auto actual = read_political_shop_permission_boundary_moment2(
        x, {head->domain, head->kind, head->id});
    if (!actual.ok)
      return {false, {}, actual.error};
    auto life = read_character_availability_before_fact_v2(
        *x.character_sources, actor, {head->domain, head->kind, head->id},
        *actual.value, *x.moment_clock);
    if (!life.ok)
      return {false, {}, life.error};
    if (!life.value.present || !life.value.alive)
      return common_failure("PoliticalShopActorAlreadyDead", actor);
    if (x.original_sink) {
      auto sink = x.original_sink->availability(x, actor, *head, *actual.value);
      if (!sink.ok)
        return {false, {}, sink.error};
      if (!sink.value)
        return common_failure("PoliticalShopLifeProvenanceRequired", actor);
      auto visited = visit_character_availability_before_fact_provenance_v2(
          *x.character_sources, actor, {head->domain, head->kind, head->id},
          *actual.value, *x.moment_clock, *sink.value);
      if (!visited.ok)
        return {false, {}, visited.error};
      if (!visited.value.present || !visited.value.alive)
        return common_failure("PoliticalShopActorAlreadyDead", actor);
    }
    return common_success();
  }
  Integer limit = x.common.tick();
  if (x.phase) {
    if (limit < 0 || limit == std::numeric_limits<Integer>::max() ||
        x.phase->from_tick != limit || x.phase->to_tick != limit + 1)
      return common_failure("PoliticalShopLifePhaseMismatch");
    limit = x.phase->to_tick;
  }
  Integer cutoff = x.common.state().next_instance_id;
  if (!boundary.empty()) {
    const auto *head = x.common.state().facts.find(boundary);
    if (!head)
      return common_failure("PoliticalShopActorLifeHistoryUnavailable", actor);
    auto n = fact_sequence(x.common, boundary, head->kind);
    if (!n.ok)
      return {false, {}, n.error};
    cutoff = n.value;
  } else if (tick != x.common.tick())
    return common_failure("PoliticalShopActorLifeHistoryUnavailable", actor);
  if (!x.death_origin)
    return person->alive
               ? common_success()
               : common_failure("PoliticalShopActorLifeHistoryUnavailable",
                                actor);
  auto source = x.death_origin(actor);
  if (!source.ok)
    return {false, {}, source.error};
  if (!source.value)
    return person->alive
               ? common_success()
               : common_failure("PoliticalShopActorLifeHistoryUnavailable",
                                actor);
  const auto &death = *source.value;
  const auto *head = x.common.state().facts.find(death.casualty_fact_id);
  auto n = organization_instance_sequence(death.casualty_fact_id, "military");
  if (death.actor_id != actor || death.battle_id.empty() || death.tick < 0 ||
      death.tick > limit || !n || *n >= x.common.state().next_instance_id ||
      !head || head->id != death.casualty_fact_id ||
      head->kind != "military_casualties" || head->domain != "military")
    return common_failure("PoliticalShopActorLifeHistoryUnavailable", actor);
  const bool earlier = *n < cutoff;
  if (!boundary.empty() &&
      (earlier != before(x.common, death.casualty_fact_id, boundary) ||
       (earlier && death.tick > tick) || (!earlier && death.tick < tick)))
    return common_failure("PoliticalShopActorLifeOrderMismatch", actor);
  // 没有历史截面时只审当前活人；phase已发生的死亡也不能让当前办理越过原活人门。
  if (boundary.empty() || earlier)
    return common_failure("PoliticalShopActorAlreadyDead", actor);
  return common_success();
}
Status stable_court_before(const PoliticalShopLicensingContext &x,
                           const PoliticalShopCourt &court, Integer tick,
                           Integer sequence, const StableId &boundary) {
  const auto *original =
      x.politics_catalog.jurisdictions.find(court.jurisdiction_id);
  const auto *current = x.politics.jurisdictions.find(court.jurisdiction_id);
  if (!original || !current || tick < 0 || tick > x.common.tick() ||
      sequence < 1 || sequence > x.common.state().next_instance_id)
    return common_failure("PoliticalShopJurisdictionUnavailable", court.id);
  Integer revision = 0, at_revision = 0, previous = 0, prior_tick = -1;
  StableId owner = original->controlling_organization_id, previous_fact;
  if (x.takeover) {
    const auto &t = *x.takeover;
    if (&t.common != &x.common || !t.catalog.enabled ||
        t.catalog.version != 1 ||
        (x.state.version == 1 ? t.state.version != 1 : t.state.version != 2))
      return common_failure("PoliticalShopWrongCandidate");
    Integer control_limit = x.common.tick();
    if (t.phase) {
      if (control_limit < 0 ||
          control_limit == std::numeric_limits<Integer>::max() ||
          t.phase->from_tick != control_limit ||
          t.phase->to_tick != control_limit + 1)
        return common_failure("PoliticalShopControlPhaseMismatch");
      control_limit = t.phase->to_tick;
    }
    CheckpointMap<Integer, const PoliticalTakeoverRegistration *> ordered;
    for (const auto &row : t.state.registrations.rows()) {
      const auto *rule = t.catalog.rules.find(row.second.rule_id);
      if (!rule)
        return common_failure("PoliticalShopControlHistoryMismatch", row.first);
      if (rule->jurisdiction_id != court.jurisdiction_id)
        continue;
      auto n = organization_instance_sequence(row.first,
                                              "political_takeover_pending");
      if (row.first != row.second.id || !n ||
          !ordered.emplace(*n, &row.second).second)
        return common_failure("PoliticalShopControlHistoryMismatch", court.id);
    }
    for (const auto &entry : ordered) {
      const auto &r = *entry.second;
      if (entry.first >= x.common.state().next_instance_id ||
          entry.first <= previous || r.tick < prior_tick ||
          r.former_organization_id != owner ||
          (!x.common.state().facts.find(r.id) ||
           x.common.state().facts.find(r.id)->id != r.id ||
           x.common.state().facts.find(r.id)->kind !=
               political_takeover_pending_fact_kind ||
           x.common.state().facts.find(r.id)->domain != "politics") ||
          (!previous_fact.empty() && !before(x.common, previous_fact, r.id)))
        return common_failure("PoliticalShopControlHistoryMismatch", r.id);
      const auto *rule = t.catalog.rules.find(r.rule_id);
      const MilitaryOccupationFact *occupation = nullptr;
      for (const auto &fact : t.military.occupation_facts)
        if (fact.id == r.occupation_fact_id) {
          if (occupation)
            return common_failure("PoliticalShopControlHistoryMismatch", r.id);
          occupation = &fact;
        }
      if (!rule || !occupation || !occupation->assets_applied.value_or(false) ||
          !occupation->asset_manifest || occupation->tick != r.tick ||
          occupation->tick > control_limit ||
          occupation->from_organization_id != r.former_organization_id ||
          occupation->to_organization_id != r.successor_organization_id ||
          occupation->garrison_id != rule->garrison_id ||
          occupation->stronghold_id != rule->stronghold_id ||
          occupation->public_wallet_ids != r.public_wallet_ids ||
          occupation->public_inventory_ids != r.public_inventory_ids)
        return common_failure("PoliticalShopControlHistoryMismatch", r.id);
      const auto *occupation_head = x.common.state().facts.find(occupation->id);
      auto occupation_sequence =
          organization_instance_sequence(occupation->id, "military");
      if (!occupation_head || occupation_head->id != occupation->id ||
          occupation_head->kind != "military_occupation" ||
          occupation_head->domain != "military" || !occupation_sequence ||
          *occupation_sequence >= entry.first ||
          !before(x.common, occupation->id, r.id))
        return common_failure("PoliticalShopControlHistoryMismatch", r.id);
      auto actual = t.phase ? validate_applied_occupation_manifest(
                                  x.common, t.military, t.military_catalog,
                                  t.military_access, occupation->id, *t.phase)
                            : validate_applied_occupation_manifest(
                                  x.common, t.military, t.military_catalog,
                                  t.military_access, occupation->id);
      if (!actual.ok)
        return actual;
      if (!boundary.empty() &&
          ((entry.first < sequence) != before(x.common, r.id, boundary)))
        return common_failure("PoliticalShopControlOrderMismatch", r.id);
      if ((entry.first < sequence && r.tick > tick) ||
          (entry.first >= sequence && r.tick < tick))
        return common_failure("PoliticalShopControlTickMismatch", r.id);
      if (x.state.version == 2) {
        auto now = read_political_shop_licensing_actual_now2(x);
        if (!now.ok)
          return {false, {}, now.error};
        const WorldMoment *cut = &*now.value;
        if (!boundary.empty()) {
          const auto *head = x.common.state().facts.find(boundary);
          if (!head)
            return common_failure("PoliticalShopControlExactCutRequired",
                                  boundary);
          auto actual = read_political_shop_permission_boundary_moment2(
              x, {head->domain, head->kind, head->id});
          if (!actual.ok)
            return {false, {}, actual.error};
          cut = actual.value;
        }
        if (!r.registered_at || !occupation->exact_time)
          return common_failure("PoliticalShopControlExactSourceRequired",
                                r.id);
        auto occupied = compare_world_moments(
            occupation->exact_time->actual_occupation_at, *r.registered_at);
        if (!occupied.ok)
          return {false, {}, occupied.error};
        auto live = compare_world_moments(*r.registered_at, *now.value);
        if (!live.ok)
          return {false, {}, live.error};
        auto relation = compare_world_moments(*r.registered_at, *cut);
        if (!relation.ok)
          return {false, {}, relation.error};
        if (occupied.value > 0 || live.value > 0 ||
            (entry.first < sequence && relation.value > 0) ||
            (entry.first >= sequence && relation.value < 0))
          return common_failure("PoliticalShopControlExactOrderMismatch", r.id);
      }
      if (x.original_sink) {
        auto actual_cut = shop_law_provenance::boundary(x, boundary);
        if (!actual_cut.ok)
          return actual_cut;
        const auto *registration_head = x.common.state().facts.find(r.id);
        if (!registration_head)
          return common_failure("PoliticalShopControlHistoryMismatch", r.id);
        auto emitted =
            x.original_sink->control(x, court, t, *rule, r, *registration_head,
                                     *occupation, *occupation_head);
        if (!emitted.ok)
          return emitted;
      }
      ++revision;
      if (entry.first < sequence)
        ++at_revision;
      owner = r.successor_organization_id;
      previous = entry.first;
      prior_tick = r.tick;
      previous_fact = r.id;
    }
  }
  if (current->revision != revision ||
      current->controlling_organization_id != owner ||
      current->political_organization_id != original->political_organization_id)
    return common_failure("PoliticalShopControlHistoryUnavailable", court.id);
  // 后继政权必须另建具名完整册；即使原政权收复也不能无证明宣布中间通缉记录不存在。
  if (at_revision != 0 || original->controlling_organization_id !=
                              original->political_organization_id)
    return common_failure("PoliticalShopRegistryContinuityRequired", court.id);
  if (x.original_sink) {
    auto actual_cut = shop_law_provenance::boundary(x, boundary);
    if (!actual_cut.ok)
      return actual_cut;
    return x.original_sink->court(x, court);
  }
  return common_success();
}
namespace {
bool covers(const Grant &g, const PoliticalShopRegistryEndpoint &e,
            const StableId &command, Integer tick) {
  return g.office_id == e.office_id && g.wallet_ids.empty() &&
         g.inventory_ids.empty() && g.amount_limit >= 0 &&
         tick >= g.valid_from && (!g.valid_until || tick <= *g.valid_until) &&
         has(g.allowed_command_kinds, command) &&
         (command != "political_issue_shop_permit" ||
          has(g.allowed_command_kinds, "political_record_shop_clearance")) &&
         std::any_of(g.scopes.begin(), g.scopes.end(), [&](const auto &scope) {
           return scope.kind == ScopeRef::Kind::site && scope.id == e.site_id;
         });
}
} // namespace
Status historical_authority(const PoliticalShopLicensingContext &x,
                            const PoliticalShopCourt &court,
                            const ActingAuthority &a, const StableId &grant,
                            const StableId &command, Integer tick,
                            Integer sequence, const StableId &boundary) {
  auto endpoint = court_endpoint_owned(x, court, tick, sequence, boundary);
  if (!endpoint.ok)
    return {false, {}, endpoint.error};
  const auto &e = endpoint.value.endpoint();
  const auto *office = x.organization_catalog.offices.find(e.office_id);
  const auto *g = x.organization_catalog.grants.find(grant);
  // 旧1仍只认初始目录；显式2由下面真实Grant历史证明运行期签发/修订。
  if (!office || a.office_id != e.office_id ||
      a.organization_id != e.organization_id ||
      (x.state.version == 1 &&
       (!has(office->grant_ids, grant) || !g || !covers(*g, e, command, tick))))
    return common_failure("PoliticalShopHistoricalGrantDenied", grant);
  if (x.state.version == 2) {
    const auto *h = x.common.state().facts.find(boundary);
    if (!h)
      return common_failure("PoliticalShopActualAuthorityRequired", boundary);
    NativeShopLicensingAuthoritySourceRead2 actual(x);
    auto source = read_authority_grant_before_shop_licensing2(
        x.common, x.organizations, x.organization_catalog,
        x.organization_catalog.authority_catalog_identity,
        {{h->domain, h->kind, h->id}, 2}, actual);
    if (!source.ok)
      return {false, {}, source.error};
    if (!source.value.terms || source.value.terms->id != grant ||
        !covers(*source.value.terms, e, command, tick))
      return common_failure("PoliticalShopHistoricalGrantDenied", grant);
  }
  auto signer = validate_historical_office_signer(
      x.common, x.organizations, x.organization_catalog, a, tick, sequence);
  if (!signer.ok)
    return signer;
  auto member =
      validate_political_shop_historical_member_duty(x, a, tick, boundary);
  if (!member.ok)
    return member;
  auto alive = person_alive_before(x, a.actor_id, tick, boundary);
  if (!alive.ok)
    return alive;
  return shop_law_provenance::authority(x, a, boundary);
}
Outcome<const StableId *>
current_authority_id(const PoliticalShopLicensingContext &x,
                     const PoliticalShopCourt &court, const ActingAuthority &a,
                     const StableId &command) {
  const auto fail = [&](const StableId &code) {
    return Outcome<const StableId *>{
        false, {}, common_failure(code, court.id).error};
  };
  auto endpoint = court_endpoint_owned(x, court, x.common.tick(),
                                       x.common.state().next_instance_id, {});
  if (!endpoint.ok)
    return {false, {}, endpoint.error};
  const auto &e = endpoint.value.endpoint();
  const auto *office = x.organizations.offices.find(e.office_id);
  const auto *original = x.organization_catalog.offices.find(e.office_id);
  if (!office || !original || a.office_id != e.office_id ||
      a.organization_id != e.organization_id)
    return fail("PoliticalShopAuthorityDenied");
  auto available = politics_detail::available(x.common, x.activity, a.actor_id);
  if (!available.ok)
    return {false, {}, available.error};
  auto alive = person_alive_before(x, a.actor_id, x.common.tick(), {});
  if (!alive.ok)
    return {false, {}, alive.error};
  auto place = politics_detail::place(x.common, a.actor_id);
  if (!place || place->place_id != e.site_id)
    return fail("PoliticalShopLocalOfficerRequired");
  OrganizationAccess organization(x.common, x.organizations,
                                  x.organization_catalog, x.economy);
  if (!organization.actor_accepts_duty(a.actor_id, a.organization_id))
    return fail("PoliticalShopOfficerUnavailable");
  for (const auto &id : office->grant_ids) {
    const auto *g = x.organization_catalog.grants.find(id);
    if (x.state.version == 1 && (!has(original->grant_ids, id) || !g ||
                                 !covers(*g, e, command, x.common.tick())))
      continue;
    if (x.state.version == 2) {
      auto current = read_current_grant_terms_borrowed(
          x.common, x.organizations, x.organization_catalog, id);
      if (!current.ok) {
        if (current.error.kind != ErrorKind::rule)
          return {false, {}, current.error};
        continue;
      }
      if (!covers(*current.value, e, command, x.common.tick()))
        continue;
      auto actual = check_authority_with_single_grant(
          x.common, x.organizations, x.organization_catalog, a, command,
          {ScopeRef::Kind::site, e.site_id}, 0, *current.value, x.clock);
      if (!actual.ok) {
        if (actual.error.kind != ErrorKind::rule)
          return {false, {}, actual.error};
        continue;
      }
      if (command == "political_issue_shop_permit") {
        auto clearance = check_authority_with_single_grant(
            x.common, x.organizations, x.organization_catalog, a,
            "political_record_shop_clearance",
            {ScopeRef::Kind::site, e.site_id}, 0, *current.value, x.clock);
        if (!clearance.ok) {
          if (clearance.error.kind != ErrorKind::rule)
            return {false, {}, clearance.error};
          continue;
        }
      }
      return {true, &id, {}};
    }
    auto p = check_authority_with_single_grant(
        x.common, x.organizations, x.organization_catalog, a, command,
        {ScopeRef::Kind::site, e.site_id}, 0, *g, nullptr);
    if (p.ok)
      return {true, &id, {}};
  }
  return fail("PoliticalShopAuthorityDenied");
}
Outcome<StableId> current_authority(const PoliticalShopLicensingContext &x,
                                    const PoliticalShopCourt &court,
                                    const ActingAuthority &a,
                                    const StableId &command) {
  auto result = current_authority_id(x, court, a, command);
  if (!result.ok)
    return {false, {}, result.error};
  return {true, *result.value, {}};
}
const PoliticalShopRegistry *registry(const PoliticalShopLicensingState &s,
                                      const StableId &court) {
  const PoliticalShopRegistry *result = nullptr;
  for (const auto &row : s.registries.rows())
    if (row.second.court_id == court) {
      if (result)
        return nullptr;
      result = &row.second;
    }
  return result;
}
bool wanted_before(const PoliticalShopLicensingContext &x,
                   const StableId &court, const StableId &actor,
                   Integer sequence) {
  LawBorrowedIds released;
  for (const auto &row : x.state.releases.rows()) {
    auto n =
        organization_instance_sequence(row.first, "political_wanted_release");
    if (n && *n < sequence)
      released.insert(row.second.warrant_fact_id);
  }
  for (const auto &row : x.state.warrants.rows()) {
    const auto &w = row.second;
    auto n = organization_instance_sequence(w.id, "political_wanted");
    if (w.court_id == court && w.actor_id == actor && n && *n < sequence &&
        !released.count(w.id))
      return true;
  }
  return false;
}
} // namespace hanmo::resident::shop_law_detail
