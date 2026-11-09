#include "resident/kernel/catalog_section_observation.hpp"
// 初册、通缉和许可逐真实Common顺序折叠；不从当前角色、当前位置或后来的解除反造过去清白。
#include "hanmo/resident/checkpoint_memory.hpp"
#include "political_shop_fact_identity.hpp"
#include "political_shop_licensing_bootstrap2.hpp"
#include "political_shop_licensing_detail.hpp"
#include "political_shop_licensing_exact2.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <limits>
#include <map>
#include <string_view>
namespace hanmo::resident {
// 只读索引键借本次实际原根；节点由同job allocator真实计账，不跨写入保留。
using LawBorrowedIds = std::set<std::string_view, std::less<>,
                                CheckpointAllocator<std::string_view>>;

using namespace shop_law_detail;
Status validate_political_shop_licensing_fact_header(
    const CommonStore &common, const PoliticalShopLicensingState &state,
    const FactRecord &head) {
  if ((state.version != 1 && state.version != 2) ||
      head.domain != political_shop_licensing_module ||
      head.kind == "private_shop_opened")
    return common_failure("PoliticalShopFactMismatch", head.id);
  auto sequence = fact_sequence(common, head.id, head.kind);
  if (!sequence.ok)
    return {false, {}, sequence.error};
  const auto *registry = state.registries.find(head.id);
  const auto *warrant = state.warrants.find(head.id);
  const auto *release = state.releases.find(head.id);
  const auto *clearance = state.clearances.find(head.id);
  const auto *permit = state.permits.find(head.id);
  const auto *revoke = state.revocations.find(head.id);
  if (unsigned(bool(registry)) + bool(warrant) + bool(release) +
          bool(clearance) + bool(permit) + bool(revoke) !=
      1)
    return common_failure("PoliticalShopPayloadMismatch", head.id);
  const bool actual = (registry && registry->id == head.id &&
                       head.kind == "political_shop_registry_initialized") ||
                      (warrant && warrant->id == head.id &&
                       head.kind == "political_wanted_registered") ||
                      (release && release->id == head.id &&
                       head.kind == "political_wanted_released") ||
                      (clearance && clearance->id == head.id &&
                       head.kind == political_shop_clearance_kind) ||
                      (permit && permit->id == head.id &&
                       head.kind == political_shop_permit_kind) ||
                      (revoke && revoke->id == head.id &&
                       head.kind == "political_shop_permit_revoked");
  return actual ? common_success()
                : common_failure("PoliticalShopPayloadMismatch", head.id);
}
Status validate_political_shop_licensing_catalog(
    const PoliticalShopLicensingContext &x) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 100, "PoliticalShopLicensingContext", "validate_political_shop_licensing_catalog", &x);

  if (x.catalog.version != 1 || x.catalog.courts.empty() ||
      x.catalog.courts.size() > 4096 || !x.politics_catalog.enabled ||
      !x.organization_catalog.enabled)
    return common_failure("InvalidPoliticalShopCatalog");
  LawBorrowedIds jurisdictions, premises;
  for (const auto &row : x.catalog.courts.rows()) {
    const auto &c = row.second;
    const auto *j = x.politics_catalog.jurisdictions.find(c.jurisdiction_id);
    const auto *office = x.organization_catalog.offices.find(c.office_id);
    const auto *role = x.common_catalog.roles.find(c.role_id);
    if (row.first != c.id || c.id.empty() || !c.complete_registry ||
        c.permit_ticks < 1 || c.permit_ticks > 4096 || !j || j->revision != 0 ||
        j->political_organization_id != j->controlling_organization_id ||
        !jurisdictions.insert(c.jurisdiction_id).second || !office ||
        office->organization_id != j->political_organization_id || !role ||
        role->organization_id != j->political_organization_id ||
        role->place_id != j->seat_place_id ||
        role->dialogue_id != c.dialogue_id || c.premises_ids.empty() ||
        c.premises_ids.size() > 4096 || c.warrant_reason_codes.empty() ||
        c.warrant_reason_codes.size() > 256 || c.initial_wanted.size() > 4096)
      return common_failure("InvalidPoliticalShopCourt", row.first);
    const DialogueDefinition *dialogue = nullptr;
    for (const auto &d : x.interaction_catalog.dialogues)
      if (d.id == c.dialogue_id) {
        if (dialogue)
          return common_failure("AmbiguousPoliticalShopDialogue", c.id);
        dialogue = &d;
      }
    const DialogueNode *node = nullptr;
    if (dialogue)
      for (const auto &n : dialogue->nodes)
        if (n.id == c.request_node_id) {
          if (node)
            return common_failure("AmbiguousPoliticalShopNode", c.id);
          node = &n;
        }
    if (!dialogue || dialogue->context_role != c.role_id || !node ||
        node->kind != "say" || node->speaker != "$provider")
      return common_failure("PoliticalShopProviderDeclarationRequired", c.id);
    // 作者初态也必须有同一原公职的完整专项Grant；只有牌匾或一个命令名不能成为可办理柜台。
    bool grant_declared = false;
    for (const auto &grant_id : office->grant_ids) {
      const auto *grant = x.organization_catalog.grants.find(grant_id);
      if (grant && grant->id == grant_id && grant->office_id == c.office_id &&
          grant->wallet_ids.empty() && grant->inventory_ids.empty() &&
          grant->amount_limit >= 0 &&
          has(grant->allowed_command_kinds, "political_issue_shop_permit") &&
          has(grant->allowed_command_kinds,
              "political_record_shop_clearance") &&
          std::any_of(grant->scopes.begin(), grant->scopes.end(),
                      [&](const auto &scope) {
                        return scope.kind == ScopeRef::Kind::site &&
                               scope.id == j->seat_place_id;
                      })) {
        grant_declared = true;
        break;
      }
    }
    if (!grant_declared)
      return common_failure("PoliticalShopGrantDeclarationRequired", c.id);
    // 许可只覆盖作者逐铺列出的同一真实聚落；柜台seat与市场铺位可以是不同place。
    const auto *seat = x.common_catalog.places.find(j->seat_place_id);
    if (!seat || seat->settlement_id.empty())
      return common_failure("PoliticalShopJurisdictionUnavailable", c.id);
    for (const auto &id : c.premises_ids) {
      const auto *p = x.premises.find(id);
      const auto *at = p ? x.common_catalog.places.find(p->place_id) : nullptr;
      if (!p || p->id != id || !at ||
          at->settlement_id != seat->settlement_id ||
          !has(x.common_catalog.actor_ids, p->lessor_actor_id) ||
          !premises.insert(id).second)
        return common_failure("PoliticalShopPremisesMismatch", id);
    }
    LawBorrowedIds reasons, actors;
    for (const auto &r : c.warrant_reason_codes)
      if (r.empty() || !reasons.insert(r).second)
        return common_failure("InvalidPoliticalWantedReason", c.id);
    for (const auto &s : c.initial_wanted)
      if (!has(x.common_catalog.actor_ids, s.actor_id) ||
          !has(c.warrant_reason_codes, s.reason_code) ||
          !actors.insert(s.actor_id).second)
        return common_failure("InvalidPoliticalWantedSeed", c.id);
  }
  // 本profile每个铺位均用于实际租铺，不允许作者通过遗漏辖区产生永远无法取证的入口。
  if (premises.size() != x.premises.size())
    return common_failure("PoliticalShopPremisesCoverageRequired");
  return common_success();
}
namespace {
template <bool InitialOnly>
Status validate_law(const PoliticalShopLicensingContext &x,
                    const OrganizationAuthorityClock *authority_clock,
                    const CurrentMomentRead *clock) {
  if (x.phase && (x.common.tick() < 0 ||
                  x.common.tick() == std::numeric_limits<Integer>::max() ||
                  x.phase->from_tick != x.common.tick() ||
                  x.phase->to_tick != x.common.tick() + 1))
    return common_failure("PoliticalShopLifePhaseMismatch");
  auto valid = validate_political_shop_licensing_catalog(x);
  if (!valid.ok)
    return valid;
  valid = validate_political_shop_licensing_time_layout(x.state);
  if (!valid.ok)
    return valid;
  const auto now_read = [&] {
    if constexpr (InitialOnly)
      return shop_law_bootstrap_detail::read_initial_moment(x, *authority_clock,
                                                            *clock);
    else
      return read_political_shop_licensing_actual_now2(x);
  };
  auto exact_now = now_read();
  if (!exact_now.ok)
    return {false, {}, exact_now.error};
  const WorldMoment *previous_exact = nullptr;
  const auto total = x.state.registries.size() + x.state.warrants.size() +
                     x.state.releases.size() + x.state.clearances.size() +
                     x.state.permits.size() + x.state.revocations.size();
  if (total > 65536)
    return common_failure("PoliticalShopHistoryLimit");
  LawBorrowedIds registered, released, permitted, revoked;
  using SeedKey = std::pair<std::string_view, std::string_view>;
  std::set<SeedKey, std::less<SeedKey>, CheckpointAllocator<SeedKey>> seeded;
  CheckpointMap<std::string_view, std::size_t> seed_count;
  std::size_t seen = 0;
  Integer previous = 0, last_tick = 0;
  for (const auto &head : x.common.state().facts.rows()) {
    const auto &h = head.second;
    const auto prefix = prefix_view(h.kind);
    if (h.domain != political_shop_licensing_module &&
        (prefix.empty() || h.kind == "private_shop_opened"))
      continue;
    auto sequence = fact_sequence(x.common, head.first, h.kind);
    if (!sequence.ok)
      return {false, {}, sequence.error};
    if (h.kind == "private_shop_opened" || sequence.value <= previous)
      return common_failure("PoliticalShopHistoryOrderMismatch", head.first);
    previous = sequence.value;
    const auto *reg = x.state.registries.find(head.first);
    const auto *warrant = x.state.warrants.find(head.first);
    const auto *release = x.state.releases.find(head.first);
    const auto *clearance = x.state.clearances.find(head.first);
    const auto *permit = x.state.permits.find(head.first);
    const auto *revoke = x.state.revocations.find(head.first);
    valid = validate_political_shop_licensing_fact_header(x.common, x.state, h);
    if (!valid.ok)
      return valid;
    if (x.state.version == 2) {
      const auto actual_read =
          [&]() -> Outcome<PoliticalShopLicensingActualSource2> {
        if constexpr (InitialOnly) {
          // 原完整header/唯一payload已在上方验；初册不能含任何司法签字。
          auto position = read_actual_common_fact_position(x.common, h);
          if (!position.ok)
            return {false, {}, position.error};
          const WorldMoment *at =
              reg && reg->occurred_at ? &*reg->occurred_at
              : warrant && !warrant->authority && warrant->occurred_at
                  ? &*warrant->occurred_at
                  : nullptr;
          if (!at)
            return {
                false,
                {},
                common_failure("PoliticalShopBootstrapPayloadRequired", h.id)
                    .error};
          auto zero = compare_world_moments(*at, *exact_now.value);
          if (!zero.ok)
            return {false, {}, zero.error};
          if (zero.value)
            return {
                false,
                {},
                common_failure("PoliticalShopBootstrapActualZeroRequired", h.id)
                    .error};
          return {true, {&h, at}, {}};
        } else {


          auto source_memory = CheckpointMemoryCharge(0);
          return read_political_shop_licensing_actual_source2(
              x, {checkpoint_fresh_string_copy(h.domain),
                  checkpoint_fresh_string_copy(h.kind),
                  checkpoint_fresh_string_copy(h.id)});
        }
      };
      auto actual = actual_read();
      if (!actual.ok)
        return {false, {}, actual.error};
      if (previous_exact) {
        auto order =
            compare_world_moments(*previous_exact, *actual.value.occurred_at);
        if (!order.ok)
          return {false, {}, order.error};
        if (order.value > 0)
          return common_failure("PoliticalShopExactHistoryOrderMismatch", h.id);
      }
      previous_exact = actual.value.occurred_at;
    }
    const PoliticalShopCourt *court = nullptr;
    Integer tick = 0;
    const ActingAuthority *authority = nullptr;
    const StableId *grant = nullptr;
    std::string_view command;
    if (reg) {
      court = x.catalog.courts.find(reg->court_id);
      tick = reg->tick;
      if (h.kind != "political_shop_registry_initialized" ||
          reg->id != head.first || !court || tick != 0 ||
          !registered.insert(court->id).second)
        return common_failure("PoliticalShopRegistryMismatch", head.first);
    }
    if (warrant) {
      court = x.catalog.courts.find(warrant->court_id);
      tick = warrant->tick;
      const auto *root = registry(x.state, warrant->court_id);
      if (h.kind != "political_wanted_registered" ||
          warrant->id != head.first || !court || !root ||
          root->id != warrant->registry_fact_id ||
          !registered.count(court->id) ||
          !before(x.common, root->id, warrant->id) ||
          !x.common.actor(warrant->actor_id) ||
          !has(court->warrant_reason_codes, warrant->reason_code))
        return common_failure("PoliticalWantedSourceMismatch", head.first);
      if (!warrant->authority) {
        if (tick != 0 || !warrant->grant_id.empty() ||
            !seeded.emplace(court->id, warrant->actor_id).second ||
            std::none_of(court->initial_wanted.begin(),
                         court->initial_wanted.end(), [&](const auto &seed) {
                           return seed.actor_id == warrant->actor_id &&
                                  seed.reason_code == warrant->reason_code;
                         }))
          return common_failure("PoliticalWantedSeedMismatch", head.first);
        ++seed_count[court->id];
      } else {
        authority = &*warrant->authority;
        grant = &warrant->grant_id;
        command = "political_register_wanted";
        if (wanted_before(x, court->id, warrant->actor_id, sequence.value))
          return common_failure("DuplicateActivePoliticalWarrant", head.first);
      }
    }
    if (release) {
      const auto *source = x.state.warrants.find(release->warrant_fact_id);
      court = source ? x.catalog.courts.find(source->court_id) : nullptr;
      tick = release->tick;
      if (h.kind != "political_wanted_released" || release->id != head.first ||
          !court || !before(x.common, source->id, release->id) ||
          source->tick > tick || !released.insert(source->id).second)
        return common_failure("PoliticalWantedReleaseMismatch", head.first);
      authority = &release->authority;
      grant = &release->grant_id;
      command = "political_release_wanted";
    }
    if (clearance) {
      court = x.catalog.courts.find(clearance->court_id);
      tick = clearance->tick;
      const auto *root = registry(x.state, clearance->court_id);
      const auto *j =
          court ? x.politics_catalog.jurisdictions.find(court->jurisdiction_id)
                : nullptr;
      const auto *premises = x.premises.find(clearance->request.premises_id);
      auto session =
          organization_instance_sequence(clearance->session_id, "interaction");
      if (h.kind != political_shop_clearance_kind ||
          clearance->id != head.first || !court || !root || !j ||
          !registered.count(court->id) ||
          clearance->registry_fact_id != root->id ||
          !before(x.common, root->id, head.first) ||
          !has(court->premises_ids, clearance->request.premises_id) ||
          (!premises || clearance->request.place_id != premises->place_id) ||
          !x.common.actor(clearance->request.actor_id) ||
          clearance->request.actor_id == clearance->authority.actor_id ||
          !session || *session >= sequence.value ||
          tick > std::numeric_limits<Integer>::max() - court->permit_ticks ||
          clearance->valid_through_tick != tick + court->permit_ticks ||
          wanted_before(x, court->id, clearance->request.actor_id,
                        sequence.value))
        return common_failure("PoliticalShopClearanceMismatch", head.first);
      auto endpoint =
          court_endpoint_owned(x, *court, tick, sequence.value, head.first);
      if (!endpoint.ok)
        return {false, {}, endpoint.error};
      if (clearance->role_id != endpoint.value.endpoint().role_id ||
          clearance->dialogue_id != endpoint.value.endpoint().dialogue_id ||
          clearance->node_id != endpoint.value.endpoint().request_node_id)
        return common_failure("PoliticalShopClearanceMismatch", head.first);
      authority = &clearance->authority;
      grant = &clearance->grant_id;
      command = "political_issue_shop_permit";
    }
    if (permit) {
      const auto *source = x.state.clearances.find(permit->clearance_fact_id);
      court = source ? x.catalog.courts.find(source->court_id) : nullptr;
      tick = permit->tick;
      auto source_seq = source ? fact_sequence(x.common, source->id,
                                               political_shop_clearance_kind)
                               : Outcome<Integer>{};
      if (h.kind != political_shop_permit_kind || permit->id != head.first ||
          !source || !court || tick != source->tick || !source_seq.ok ||
          sequence.value != source_seq.value + 1 ||
          !before(x.common, source->id, head.first) ||
          !permitted.insert(source->id).second)
        return common_failure("PoliticalShopPermitMismatch", head.first);
    }
    if (permit && x.state.version == 2) {
      const auto *c = x.state.clearances.find(permit->clearance_fact_id);
      if (!c || !c->occurred_at || !permit->occurred_at)
        return common_failure("PoliticalShopExactTimeMissing", head.first);
      auto same = compare_world_moments(*c->occurred_at, *permit->occurred_at);
      if (!same.ok)
        return {false, {}, same.error};
      if (same.value)
        return common_failure("PoliticalShopPermitMomentMismatch", head.first);
    }
    if (revoke) {
      const auto *source = x.state.permits.find(revoke->permit_fact_id);
      const auto *clear =
          source ? x.state.clearances.find(source->clearance_fact_id) : nullptr;
      court = clear ? x.catalog.courts.find(clear->court_id) : nullptr;
      tick = revoke->tick;
      if (h.kind != "political_shop_permit_revoked" ||
          revoke->id != head.first || !court ||
          !before(x.common, source->id, head.first) || source->tick > tick ||
          !revoked.insert(source->id).second)
        return common_failure("PoliticalShopRevocationMismatch", head.first);
      authority = &revoke->authority;
      grant = &revoke->grant_id;
      command = "political_revoke_shop_permit";
    }
    if (!court || tick < last_tick || tick > x.common.tick())
      return common_failure("PoliticalShopFactTickMismatch", head.first);
    last_tick = tick;
    if (clearance) {
      valid =
          person_alive_before(x, clearance->request.actor_id, tick, head.first);
      if (!valid.ok)
        return valid;
    }
    if (reg) {
      valid = stable_court_before(x, *court, tick, sequence.value, head.first);
      if (!valid.ok)
        return valid;
    }
    if (authority) {
      if (registered.size() != x.catalog.courts.size())
        return common_failure("PoliticalShopIncompleteInitialRegistry",
                              court->id);
      for (const auto &initial : x.catalog.courts.rows())
        if (seed_count[initial.first] != initial.second.initial_wanted.size())
          return common_failure("PoliticalShopIncompleteInitialRegistry",
                                initial.first);


      auto command_memory = CheckpointMemoryCharge(0);
      const StableId command_kind = checkpoint_fresh_text_copy(command);
      valid = historical_authority(x, *court, *authority, *grant, command_kind,
                                   tick, sequence.value, head.first);
      if (!valid.ok)
        return valid;
    }
    ++seen;
  }
  if (seen != total || registered.size() != x.catalog.courts.size() ||
      permitted.size() != x.state.clearances.size())
    return common_failure("PoliticalShopMissingHistory");
  for (const auto &row : x.catalog.courts.rows())
    if (seed_count[row.first] != row.second.initial_wanted.size())
      return common_failure("PoliticalShopIncompleteInitialRegistry",
                            row.first);
  return common_success();
}
} // namespace
Status
validate_political_shop_licensing(const PoliticalShopLicensingContext &x) {
  return validate_law<false>(x, nullptr, nullptr);
}
Status shop_law_bootstrap_detail::validate_initial_sources(
    const PoliticalShopLicensingContext &x,
    const OrganizationAuthorityClock &authority,
    const CurrentMomentRead &clock) {
  return validate_law<true>(x, &authority, &clock);
}
} // namespace hanmo::resident
