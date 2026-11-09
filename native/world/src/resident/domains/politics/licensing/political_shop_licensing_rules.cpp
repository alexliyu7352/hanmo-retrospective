#include "political_shop_fact_identity.hpp"
#include "political_shop_licensing_owned.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
// 柜台只在本人原say声明中办理，查核与许可同候选原子写入；没有暗中收费、司法免除或新人物。
#include "political_shop_licensing_bootstrap2.hpp"
#include "political_shop_licensing_detail.hpp"
#include "political_shop_licensing_exact2.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include <limits>
#include <type_traits>
namespace hanmo::resident {
namespace {
using namespace shop_law_detail;
template <class T>
Outcome<T> fail(const StableId &code, const StableId &id = {}) {
  return {false, {}, common_failure(code, id).error};
}
Outcome<ProducedStableId> append(CommonStore &common, std::string_view kind) {
  auto id = common.next_owned_id(prefix_view(kind));
  if (!id.ok)
    return id;
  auto written = common.append_fact_borrowed(id.value.id(), kind,
                                             political_shop_licensing_module);
  if (!written.ok)
    return {false, {}, written.error};
  return id;
}


// 调用者已按这些真实字段先租；所有字符串fresh构造再移动，避免空string赋值增长容量。
ActingAuthority copy_authority_preleased(const ActingAuthority &a) {
  return {checkpoint_fresh_string_copy(a.organization_id),
          checkpoint_fresh_string_copy(a.office_id), a.authority_epoch,
          checkpoint_fresh_string_copy(a.actor_id)};
}
PoliticalShopLicensingCommand
copy_command_preleased(const PoliticalShopLicensingCommand &command) {
  return std::visit(
      [](const auto &row) -> PoliticalShopLicensingCommand {
        using T = std::decay_t<decltype(row)>;
        if constexpr (std::is_same_v<T, PoliticalRequestShopPermission>)
          return PoliticalRequestShopPermission{
              checkpoint_fresh_string_copy(row.court_id),
              checkpoint_fresh_string_copy(row.premises_id)};
        else if constexpr (std::is_same_v<T, PoliticalRegisterWanted>)
          return PoliticalRegisterWanted{
              copy_authority_preleased(row.authority),
              checkpoint_fresh_string_copy(row.court_id),
              checkpoint_fresh_string_copy(row.actor_id),
              checkpoint_fresh_string_copy(row.reason_code)};
        else if constexpr (std::is_same_v<T, PoliticalReleaseWanted>)
          return PoliticalReleaseWanted{
              copy_authority_preleased(row.authority),
              checkpoint_fresh_string_copy(row.warrant_fact_id)};
        else
          return PoliticalRevokeShopPermit{
              copy_authority_preleased(row.authority),
              checkpoint_fresh_string_copy(row.permit_fact_id)};
      },
      command);
}
// 与原四record的编码字段逐项相等；只比较真实typed命令，不制造两棵临时JSON。
bool same_command(const PoliticalShopLicensingCommand &a,
                  const PoliticalShopLicensingCommand &b) {
  if (a.index() != b.index())
    return false;
  const auto same_authority = [](const ActingAuthority &x,
                                 const ActingAuthority &y) {
    return x.organization_id == y.organization_id &&
           x.office_id == y.office_id &&
           x.authority_epoch == y.authority_epoch && x.actor_id == y.actor_id;
  };
  return std::visit(
      [&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        const auto *y = std::get_if<T>(&b);
        if (!y)
          return false;
        if constexpr (std::is_same_v<T, PoliticalRequestShopPermission>)
          return x.court_id == y->court_id && x.premises_id == y->premises_id;
        else if constexpr (std::is_same_v<T, PoliticalRegisterWanted>)
          return same_authority(x.authority, y->authority) &&
                 x.court_id == y->court_id && x.actor_id == y->actor_id &&
                 x.reason_code == y->reason_code;
        else if constexpr (std::is_same_v<T, PoliticalReleaseWanted>)
          return same_authority(x.authority, y->authority) &&
                 x.warrant_fact_id == y->warrant_fact_id;
        else
          return same_authority(x.authority, y->authority) &&
                 x.permit_fact_id == y->permit_fact_id;
      },
      a);
}
void keep_result(OwnedPoliticalShopLicensingEffects &out, const StableId &id) {


  (out.memory).grow(0);
  out.fact_ids.push_back(checkpoint_fresh_string_copy(id));
}
const PoliticalShopCourt *
command_court(const PoliticalShopLicensingContext &x,
              const PoliticalShopLicensingCommand &command) {
  if (const auto *request =
          std::get_if<PoliticalRequestShopPermission>(&command))
    return x.catalog.courts.find(request->court_id);
  if (const auto *wanted = std::get_if<PoliticalRegisterWanted>(&command))
    return x.catalog.courts.find(wanted->court_id);
  if (const auto *released = std::get_if<PoliticalReleaseWanted>(&command)) {
    const auto *w = x.state.warrants.find(released->warrant_fact_id);
    return w ? x.catalog.courts.find(w->court_id) : nullptr;
  }
  const auto *p = x.state.permits.find(
      std::get<PoliticalRevokeShopPermit>(command).permit_fact_id);
  const auto *c = p ? x.state.clearances.find(p->clearance_fact_id) : nullptr;
  return c ? x.catalog.courts.find(c->court_id) : nullptr;
}
} // namespace
namespace {
template <bool InitialOnly>
Status initialize_law(CommonStore &common, PoliticalShopLicensingState &state,
                      const PoliticalShopLicensingContext &x,
                      const OrganizationAuthorityClock *authority,
                      const CurrentMomentRead *clock) {
  if (&common != &x.common || &state != &x.state || common.tick() != 0 ||
      (state.version != 1 && state.version != 2) || !state.registries.empty() ||
      !state.warrants.empty() || !state.releases.empty() ||
      !state.clearances.empty() || !state.permits.empty() ||
      !state.revocations.empty())
    return common_failure("PoliticalShopInitialBoundaryMismatch");
  auto valid = validate_political_shop_licensing_catalog(x);
  if (!valid.ok)
    return valid;
  const auto moment_read = [&] {
    if constexpr (InitialOnly)
      return shop_law_bootstrap_detail::read_initial_moment(x, *authority,
                                                            *clock);
    else
      return read_political_shop_licensing_write_moment(x);
  };
  auto moment = moment_read();
  if (!moment.ok)
    return {false, {}, moment.error};
  if (moment.value) {
    auto zero = compare_world_moments(*moment.value, WorldMoment{});
    if (!zero.ok)
      return {false, {}, zero.error};
    if (zero.value != 0)
      return common_failure("PoliticalShopExactInitialBoundaryRequired");
  }
  for (const auto &h : common.state().facts.rows())
    if (h.second.domain == political_shop_licensing_module)
      return common_failure("PoliticalShopInitialHistoryExists");
  for (const auto &row : x.catalog.courts.rows()) {
    auto stable = stable_court_before(x, row.second, 0,
                                      common.state().next_instance_id, {});
    if (!stable.ok)
      return stable;
    auto id = append(common, "political_shop_registry_initialized");
    if (!id.ok)
      return {false, {}, id.error};
    {


      auto registry_memory = CheckpointMemoryCharge(0);
      PoliticalShopRegistry registry_row{
          checkpoint_fresh_string_copy(id.value.id()),
          checkpoint_fresh_string_copy(row.first), 0, moment.value};
      if (!state.registries.insert(id.value.id(), std::move(registry_row)))
        return common_failure("DuplicatePoliticalShopRegistry");
    }
    for (const auto &seed : row.second.initial_wanted) {
      auto wanted = append(common, "political_wanted_registered");
      if (!wanted.ok)
        return {false, {}, wanted.error};


      auto row_memory = CheckpointMemoryCharge(0);
      PoliticalWantedRecord record;
      record.id = checkpoint_fresh_string_copy(wanted.value.id());
      record.court_id = checkpoint_fresh_string_copy(row.first);
      record.actor_id = checkpoint_fresh_string_copy(seed.actor_id);
      record.reason_code = checkpoint_fresh_string_copy(seed.reason_code);
      record.registry_fact_id = checkpoint_fresh_string_copy(id.value.id());
      record.occurred_at = moment.value;
      if (!state.warrants.insert(wanted.value.id(), std::move(record)))
        return common_failure("DuplicatePoliticalWantedSeed");
    }
  }
  if constexpr (InitialOnly)
    return shop_law_bootstrap_detail::validate_initial_sources(x, *authority,
                                                               *clock);
  else
    return validate_political_shop_licensing(x);
}
} // namespace
Status initialize_political_shop_licensing_in_candidate(
    CommonStore &common, PoliticalShopLicensingState &state,
    const PoliticalShopLicensingContext &x) {
  return initialize_law<false>(common, state, x, nullptr, nullptr);
}
Status initialize_political_shop_licensing_bootstrap2_in_candidate(
    CommonStore &common, PoliticalShopLicensingState &state,
    const PoliticalShopLicensingContext &x,
    const OrganizationAuthorityClock &authority,
    const CurrentMomentRead &clock) {
  return initialize_law<true>(common, state, x, &authority, &clock);
}
Outcome<OwnedPoliticalShopLicensingQuote>
preview_political_shop_licensing_owned(
    const PoliticalShopLicensingContext &x, const StableId &actor,
    const PoliticalShopLicensingCommand &command) {
  auto valid = validate_political_shop_licensing(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *court = command_court(x, command);
  if (!court)
    return fail<OwnedPoliticalShopLicensingQuote>(
        "PoliticalShopCourtUnavailable");
  OwnedPoliticalShopLicensingQuote result;


  std::visit(
      [&](const auto &row) {
        using T = std::decay_t<decltype(row)>;
        if constexpr (std::is_same_v<T, PoliticalRequestShopPermission>) {

        } else {

          if constexpr (std::is_same_v<T, PoliticalRegisterWanted>)
            (void)0;
          else if constexpr (std::is_same_v<T, PoliticalReleaseWanted>)
            (void)0;
          else
            (void)0;
        }
      },
      command);
  (result.memory_).grow(0);
  auto &out = result.value_;
  out.identity = x.common.state().identity;
  out.actor_id = checkpoint_fresh_string_copy(actor);
  out.command = copy_command_preleased(command);
  if (const auto *request =
          std::get_if<PoliticalRequestShopPermission>(&command)) {
    auto endpoint = court_endpoint_owned(x, *court, x.common.tick(),
                                         x.common.state().next_instance_id, {});
    if (!endpoint.ok)
      return {false, {}, endpoint.error};
    const auto &e = endpoint.value.endpoint();
    const auto *j = x.politics.jurisdictions.find(court->jurisdiction_id);
    const auto *office = x.organizations.offices.find(e.office_id);
    const auto *p = x.premises.find(request->premises_id);
    if (!j || !office || !office->holder_actor_id || !p ||
        !has(court->premises_ids, request->premises_id))
      return fail<OwnedPoliticalShopLicensingQuote>(
          "PoliticalShopPremisesMismatch");
    auto available = politics_detail::available(x.common, x.activity, actor);
    if (!available.ok)
      return {false, {}, available.error};


    auto authority_memory = CheckpointMemoryCharge(0);
    ActingAuthority authority{
        checkpoint_fresh_string_copy(office->organization_id),
        checkpoint_fresh_string_copy(office->id), office->epoch,
        checkpoint_fresh_string_copy(*office->holder_actor_id)};


    auto issue_memory = CheckpointMemoryCharge(0);
    const StableId issue_kind =
        checkpoint_fresh_text_copy("political_issue_shop_permit");
    auto granted = current_authority_id(x, *court, authority, issue_kind);
    if (!granted.ok)
      return {false, {}, granted.error};
    auto meeting = preview_authored_provider_source_owned(
        x.common, x.interactions, x.interaction_catalog, x.interaction_access,
        actor, authority.actor_id);
    if (!meeting.ok)
      return {false, {}, meeting.error};
    const auto &m = meeting.value.proof().meeting;
    if (m.context.role_id != e.role_id || m.context.place_id != e.site_id ||
        m.context.kind || m.context.encounter_id ||
        m.dialogue_id != e.dialogue_id || m.node_id != e.request_node_id)
      return fail<OwnedPoliticalShopLicensingQuote>(
          "PoliticalShopCounterDeclarationRequired");
    if (wanted_before(x, court->id, actor, x.common.state().next_instance_id))
      return fail<OwnedPoliticalShopLicensingQuote>(
          "PoliticalShopApplicantWanted");
    if (x.common.tick() >
        std::numeric_limits<Integer>::max() - court->permit_ticks)
      return fail<OwnedPoliticalShopLicensingQuote>(
          "PoliticalShopDeadlineOverflow");


    (result.memory_).grow(0);
    out.authority = copy_authority_preleased(authority);
    out.grant_id = checkpoint_fresh_string_copy(*granted.value);
    out.meeting =
        copy_authored_provider_source_preleased(meeting.value.proof());


    auto request_memory = CheckpointMemoryCharge(0);
    auto current = permission_owned(x,
                                    {checkpoint_fresh_string_copy(actor),
                                     checkpoint_fresh_string_copy(p->id),
                                     checkpoint_fresh_string_copy(p->place_id)},
                                    x.common.tick(),
                                    x.common.state().next_instance_id, {});
    if (current.ok)
      return fail<OwnedPoliticalShopLicensingQuote>(
          "PoliticalShopPermitAlreadyValid");
    else if (current.error.original.code != "LegalPermissionUnavailable" &&
             current.error.original.code != "PoliticalShopPermissionExpired" &&
             current.error.original.code != "PoliticalShopPermissionRevoked")
      return {false, {}, current.error};
  } else {
    const ActingAuthority *selected_authority = nullptr;
    if (const auto *wanted = std::get_if<PoliticalRegisterWanted>(&command)) {
      selected_authority = &wanted->authority;
      if (!x.common.actor(wanted->actor_id) ||
          !has(court->warrant_reason_codes, wanted->reason_code))
        return fail<OwnedPoliticalShopLicensingQuote>(
            "InvalidPoliticalWantedSubject");
      if (wanted_before(x, court->id, wanted->actor_id,
                        x.common.state().next_instance_id))
        return fail<OwnedPoliticalShopLicensingQuote>(
            "DuplicateActivePoliticalWarrant");
    } else if (const auto *release =
                   std::get_if<PoliticalReleaseWanted>(&command)) {
      selected_authority = &release->authority;
      for (const auto &row : x.state.releases.rows())
        if (row.second.warrant_fact_id == release->warrant_fact_id)
          return fail<OwnedPoliticalShopLicensingQuote>(
              "PoliticalWarrantAlreadyReleased");
    } else {
      const auto &revoke = std::get<PoliticalRevokeShopPermit>(command);
      selected_authority = &revoke.authority;
      for (const auto &row : x.state.revocations.rows())
        if (row.second.permit_fact_id == revoke.permit_fact_id)
          return fail<OwnedPoliticalShopLicensingQuote>(
              "PoliticalShopPermitAlreadyRevoked");
    }
    const auto &authority = *selected_authority;
    if (authority.actor_id != actor)
      return fail<OwnedPoliticalShopLicensingQuote>(
          "PoliticalShopCommandActorMismatch");
    // 固定原command字串在构造前租用；不由Grant反推业务语义。
    std::string_view kind =
        std::holds_alternative<PoliticalRegisterWanted>(command)
            ? "political_register_wanted"
        : std::holds_alternative<PoliticalReleaseWanted>(command)
            ? "political_release_wanted"
            : "political_revoke_shop_permit";


    auto command_memory = CheckpointMemoryCharge(0);
    StableId command_kind = checkpoint_fresh_text_copy(kind);
    auto granted = current_authority_id(x, *court, authority, command_kind);
    if (!granted.ok)
      return {false, {}, granted.error};


    (result.memory_).grow(0);
    out.authority = copy_authority_preleased(authority);
    out.grant_id = checkpoint_fresh_string_copy(*granted.value);
  }
  return {true, std::move(result), {}};
}
// 旧裸ABI只兼容；正式prepare必须持有owned结果直到调用者独立计账复制完成。
Outcome<PoliticalShopLicensingQuote>
preview_political_shop_licensing(const PoliticalShopLicensingContext &x,
                                 const StableId &actor,
                                 const PoliticalShopLicensingCommand &command) {
  auto result = preview_political_shop_licensing_owned(x, actor, command);
  if (!result.ok)
    return {false, {}, result.error};
  return {true, result.value.quote(), {}};
}
Outcome<OwnedPoliticalShopLicensingEffects>
apply_political_shop_licensing_owned_in_candidate(
    CommonStore &common, PoliticalShopLicensingState &state,
    const PoliticalShopLicensingContext &x, const StableId &actor,
    const PoliticalShopLicensingCommand &command,
    const PoliticalShopLicensingQuote &quote) {
  if (&common != &x.common || &state != &x.state || actor != quote.actor_id ||
      !same_identity(common.state().identity, quote.identity) ||
      !same_command(command, quote.command))
    return fail<OwnedPoliticalShopLicensingEffects>("StalePoliticalShopQuote");
  auto fresh = preview_political_shop_licensing_owned(x, actor, command);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  if (fresh.value.quote().grant_id != quote.grant_id)
    return fail<OwnedPoliticalShopLicensingEffects>("StalePoliticalShopQuote");
  if (quote.meeting) {
    auto current = revalidate_authored_provider_source(
        common, x.interactions, x.interaction_catalog, x.interaction_access,
        *quote.meeting);
    if (!current.ok)
      return {false, {}, current.error};
  } else if (fresh.value.quote().meeting)
    return fail<OwnedPoliticalShopLicensingEffects>("StalePoliticalShopQuote");
  auto moment = read_political_shop_licensing_write_moment(x);
  if (!moment.ok)
    return {false, {}, moment.error};
  const auto *court = command_court(x, command);
  if (!court || !fresh.value.quote().authority)
    return fail<OwnedPoliticalShopLicensingEffects>(
        "PoliticalShopAuthorityDenied");
  const auto &authority = *fresh.value.quote().authority;
  OwnedPoliticalShopLicensingEffects result;
  if (const auto *request =
          std::get_if<PoliticalRequestShopPermission>(&command)) {
    const auto *root = registry(state, court->id);
    const auto *premises = x.premises.find(request->premises_id);
    if (!root || !premises || !fresh.value.quote().meeting)
      return fail<OwnedPoliticalShopLicensingEffects>(
          "PoliticalShopRegistryUnavailable");
    const auto &m = fresh.value.quote().meeting->meeting;
    auto clearance = append(common, political_shop_clearance_kind);
    if (!clearance.ok)
      return {false, {}, clearance.error};
    {


      auto row_memory = CheckpointMemoryCharge(0);
      PoliticalShopClearance c;
      c.id = checkpoint_fresh_string_copy(clearance.value.id());
      c.court_id = checkpoint_fresh_string_copy(court->id);
      c.registry_fact_id = checkpoint_fresh_string_copy(root->id);
      c.grant_id = checkpoint_fresh_string_copy(fresh.value.quote().grant_id);
      c.request = {checkpoint_fresh_string_copy(actor),
                   checkpoint_fresh_string_copy(premises->id),
                   checkpoint_fresh_string_copy(premises->place_id)};
      c.authority = copy_authority_preleased(authority);
      c.session_id = checkpoint_fresh_string_copy(m.session_id);
      c.role_id = checkpoint_fresh_string_copy(m.context.role_id);
      c.dialogue_id = checkpoint_fresh_string_copy(m.dialogue_id);
      c.node_id = checkpoint_fresh_string_copy(m.node_id);
      c.tick = common.tick();
      c.valid_through_tick = c.tick + court->permit_ticks;
      c.occurred_at = moment.value;
      if (!state.clearances.insert(clearance.value.id(), std::move(c)))
        return fail<OwnedPoliticalShopLicensingEffects>(
            "DuplicatePoliticalShopClearance");
    }
    keep_result(result, clearance.value.id());
    auto permit = append(common, political_shop_permit_kind);
    if (!permit.ok)
      return {false, {}, permit.error};
    {


      auto permit_memory = CheckpointMemoryCharge(0);
      PoliticalShopPermit permit_row{
          checkpoint_fresh_string_copy(permit.value.id()),
          checkpoint_fresh_string_copy(clearance.value.id()), common.tick(),
          moment.value};
      if (!state.permits.insert(permit.value.id(), std::move(permit_row)))
        return fail<OwnedPoliticalShopLicensingEffects>(
            "DuplicatePoliticalShopPermit");
    }
    keep_result(result, permit.value.id());
  } else if (const auto *wanted =
                 std::get_if<PoliticalRegisterWanted>(&command)) {
    const auto *root = registry(state, court->id);
    if (!root)
      return fail<OwnedPoliticalShopLicensingEffects>(
          "PoliticalShopRegistryUnavailable");
    auto id = append(common, "political_wanted_registered");
    if (!id.ok)
      return {false, {}, id.error};


    auto row_memory = CheckpointMemoryCharge(0);
    PoliticalWantedRecord w;
    w.id = checkpoint_fresh_string_copy(id.value.id());
    w.court_id = checkpoint_fresh_string_copy(court->id);
    w.actor_id = checkpoint_fresh_string_copy(wanted->actor_id);
    w.reason_code = checkpoint_fresh_string_copy(wanted->reason_code);
    w.registry_fact_id = checkpoint_fresh_string_copy(root->id);
    w.grant_id = checkpoint_fresh_string_copy(fresh.value.quote().grant_id);
    w.authority = copy_authority_preleased(authority);
    w.tick = common.tick();
    w.occurred_at = moment.value;
    if (!state.warrants.insert(id.value.id(), std::move(w)))
      return fail<OwnedPoliticalShopLicensingEffects>(
          "DuplicatePoliticalWarrant");
    keep_result(result, id.value.id());
  } else if (const auto *release =
                 std::get_if<PoliticalReleaseWanted>(&command)) {
    auto id = append(common, "political_wanted_released");
    if (!id.ok)
      return {false, {}, id.error};


    auto row_memory = CheckpointMemoryCharge(0);
    PoliticalWantedRelease row{
        checkpoint_fresh_string_copy(id.value.id()),
        checkpoint_fresh_string_copy(release->warrant_fact_id),
        checkpoint_fresh_string_copy(fresh.value.quote().grant_id),
        copy_authority_preleased(authority),
        common.tick(),
        moment.value};
    if (!state.releases.insert(id.value.id(), std::move(row)))
      return fail<OwnedPoliticalShopLicensingEffects>(
          "DuplicatePoliticalWantedRelease");
    keep_result(result, id.value.id());
  } else {
    const auto &revoke = std::get<PoliticalRevokeShopPermit>(command);
    auto id = append(common, "political_shop_permit_revoked");
    if (!id.ok)
      return {false, {}, id.error};


    auto row_memory = CheckpointMemoryCharge(0);
    PoliticalShopRevocation row{
        checkpoint_fresh_string_copy(id.value.id()),
        checkpoint_fresh_string_copy(revoke.permit_fact_id),
        checkpoint_fresh_string_copy(fresh.value.quote().grant_id),
        copy_authority_preleased(authority),
        common.tick(),
        moment.value};
    if (!state.revocations.insert(id.value.id(), std::move(row)))
      return fail<OwnedPoliticalShopLicensingEffects>(
          "DuplicatePoliticalShopRevocation");
    keep_result(result, id.value.id());
  }
  auto valid = validate_political_shop_licensing(x);
  return valid.ok
             ? Outcome<OwnedPoliticalShopLicensingEffects>{true,
                                                           std::move(result),
                                                           {}}
             : Outcome<OwnedPoliticalShopLicensingEffects>{
                   false, {}, valid.error};
}
// 旧ABI仅兼容既有调用；正式caller使用上方owned结果直到其独立copy结束。
Outcome<std::vector<StableId>> apply_political_shop_licensing_in_candidate(
    CommonStore &common, PoliticalShopLicensingState &state,
    const PoliticalShopLicensingContext &x, const StableId &actor,
    const PoliticalShopLicensingCommand &command,
    const PoliticalShopLicensingQuote &quote) {
  auto result = apply_political_shop_licensing_owned_in_candidate(
      common, state, x, actor, command, quote);
  if (!result.ok)
    return {false, {}, result.error};
  std::vector<StableId> legacy;
  legacy.reserve(result.value.fact_ids.size());
  for (const auto &id : result.value.fact_ids)
    legacy.push_back(id);
  return {true, std::move(legacy), {}};
}
} // namespace hanmo::resident
