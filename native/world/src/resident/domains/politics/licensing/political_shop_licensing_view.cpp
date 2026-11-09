// 有限办理页只发原Talk与本人申请参数；页面无写口，真正提交重新核同场Office和完整司法册。
#include "political_shop_licensing_view.hpp"
#include "political_shop_licensing_detail.hpp"
#include "political_shop_licensing_owned.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
using V = BoundaryValue;
V source_identity(const WorldIdentity &source) {
  StableId id;
  constexpr char hex[] = "0123456789abcdef";
  for (const auto byte : source.world_id) {
    id += hex[byte >> 4];
    id += hex[byte & 15];
  }
  return boundary::obj({{"world_id", V{id}},
                        {"execution_epoch", V{source.execution_epoch}},
                        {"revision", V{source.revision}},
                        {"tick", V{source.tick}}});
}
// 本人当面取得的证面可随身查阅；远方后来的通缉、撤证或换任不能从此页即时获知。
Status personal_documents_page(const PoliticalShopLicensingContext &x,
                               const StableId &actor, Integer cursor,
                               V::Dict &page) {
  V::Array rows;
  Integer count = 0;
  for (const auto &row : x.state.permits.rows()) {
    const auto *clear = x.state.clearances.find(row.second.clearance_fact_id);
    if (!clear)
      return common_failure("PoliticalShopPermitHistoryUnavailable", row.first);
    if (clear->request.actor_id != actor)
      continue;
    const auto index = count++;
    if (index < cursor || index >= cursor + 16)
      continue;
    rows.push_back(
        boundary::obj({{"permit_fact_id", V{row.first}},
                       {"clearance_fact_id", V{clear->id}},
                       {"court_id", V{clear->court_id}},
                       {"premises_id", V{clear->request.premises_id}},
                       {"place_id", V{clear->request.place_id}},
                       {"issued_tick", V{clear->tick}},
                       {"valid_through_tick", V{clear->valid_through_tick}},
                       {"issuer_actor_id", V{clear->authority.actor_id}},
                       {"issuer_office_id", V{clear->authority.office_id}}}));
  }
  if (cursor > count)
    return common_failure("InvalidPoliticalShopCursor");
  page.emplace_back("mode", V{StableId{"documents"}});
  page.emplace_back("enabled", V{count > 0});
  page.emplace_back("rows", V{std::move(rows)});
  if (count > cursor + 16)
    page.emplace_back("next_cursor", V{cursor + 16});
  return common_success();
}
// 行政页仅由当前当地持职本人读取；游标先截断再逐项报价，不为远方人物构造登记选项。
Outcome<bool> administration_page(const PoliticalShopLicensingContext &x,
                                  const PoliticalShopCourt &court,
                                  const StableId &actor, Integer cursor,
                                  V::Dict &page) {
  const auto *office = x.organizations.offices.find(court.office_id);
  if (!office || !office->holder_actor_id || *office->holder_actor_id != actor)
    return {true, false, {}};
  ActingAuthority authority{office->organization_id, court.office_id,
                            office->epoch, actor};
  const bool can_register =
      shop_law_detail::current_authority(x, court, authority,
                                         "political_register_wanted")
          .ok;
  const bool can_release = shop_law_detail::current_authority(
                               x, court, authority, "political_release_wanted")
                               .ok;
  const bool can_revoke =
      shop_law_detail::current_authority(x, court, authority,
                                         "political_revoke_shop_permit")
          .ok;
  if (!can_register && !can_release && !can_revoke)
    return {true, false, {}};
  const auto *jurisdiction =
      x.politics.jurisdictions.find(court.jurisdiction_id);
  if (!jurisdiction)
    return {false, false,
            common_failure("PoliticalShopJurisdictionUnavailable").error};
  V::Array rows;
  Integer count = 0;
  auto add = [&](const StableId &subject, const StableId &label,
                 const PoliticalShopLicensingCommand &command) {
    const auto index = count++;
    if (index < cursor || index >= cursor + 16)
      return;
    auto quote = preview_political_shop_licensing_owned(x, actor, command);
    rows.push_back(boundary::obj(
        {{"subject_actor_id", V{subject}},
         {"label", V{label}},
         {"command", V{political_shop_licensing_command_kind(command)}},
         {"params", encode_political_shop_licensing_command(command)},
         {"available", V{quote.ok}},
         {"code", V{quote.ok ? StableId{} : quote.error.original.code}}}));
  };
  if (can_register)
    for (const auto &row : x.common.state().actors.rows()) {
      const auto &person = row.second;
      const auto *at = std::get_if<SettlementLocation>(&person.location);
      if (!person.alive || !person.available || !at ||
          at->place_id != jurisdiction->seat_place_id ||
          shop_law_detail::wanted_before(x, court.id, row.first,
                                         x.common.state().next_instance_id))
        continue;
      for (const auto &reason : court.warrant_reason_codes)
        add(row.first, "登记通缉：" + row.first + " / " + reason,
            PoliticalRegisterWanted{authority, court.id, row.first, reason});
    }
  std::set<StableId> released, revoked;
  for (const auto &row : x.state.releases.rows())
    released.insert(row.second.warrant_fact_id);
  for (const auto &row : x.state.revocations.rows())
    revoked.insert(row.second.permit_fact_id);
  if (can_release)
    for (const auto &row : x.state.warrants.rows())
      if (row.second.court_id == court.id && !released.count(row.first))
        add(row.second.actor_id, "解除本册通缉：" + row.second.actor_id,
            PoliticalReleaseWanted{authority, row.first});
  if (can_revoke)
    for (const auto &row : x.state.permits.rows()) {
      const auto *clearance =
          x.state.clearances.find(row.second.clearance_fact_id);
      if (clearance && clearance->court_id == court.id &&
          !revoked.count(row.first) &&
          clearance->valid_through_tick >= x.common.tick())
        add(clearance->request.actor_id,
            "撤销铺位许可：" + clearance->request.premises_id,
            PoliticalRevokeShopPermit{authority, row.first});
    }
  if (cursor > count)
    return {false, false, common_failure("InvalidPoliticalShopCursor").error};
  page.emplace_back("mode", V{StableId{"administration"}});
  page.emplace_back("court_id", V{court.id});
  page.emplace_back("rows", V{std::move(rows)});
  if (count > cursor + 16)
    page.emplace_back("next_cursor", V{cursor + 16});
  return {true, true, {}};
}
} // namespace
BoundaryValue
encode_political_shop_licensing_quote(const PoliticalShopLicensingQuote &q) {
  V::Dict out{
      {"identity", source_identity(q.identity)},
      {"actor_id", V{q.actor_id}},
      {"duration", V{Integer{0}}},
      {"command_kind", V{political_shop_licensing_command_kind(q.command)}},
      {"command", encode_political_shop_licensing_command(q.command)},
      {"grant_id", V{q.grant_id}}};
  if (q.authority)
    out.emplace_back(
        "authority",
        boundary::obj({{"organization_id", V{q.authority->organization_id}},
                       {"office_id", V{q.authority->office_id}},
                       {"authority_epoch", V{q.authority->authority_epoch}},
                       {"actor_id", V{q.authority->actor_id}}}));
  return V{std::move(out)};
}
Outcome<BoundaryValue>
query_political_shop_licensing(const PoliticalShopLicensingContext &x,
                               const StableId &place, Integer cursor) {
  if (cursor < 0 || cursor > std::numeric_limits<Integer>::max() - 16)
    return {false, {}, common_failure("InvalidPoliticalShopCursor").error};
  auto valid = validate_political_shop_licensing(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto actor = x.common.state().metadata.player_actor_id;
  const auto at = politics_detail::place(x.common, actor);
  if (!at || at->place_id != place)
    return {false,
            {},
            common_failure("PoliticalShopLocalQueryRequired", place).error};
  const PoliticalShopCourt *court = nullptr;
  for (const auto &row : x.catalog.courts.rows()) {
    const auto *j = x.politics.jurisdictions.find(row.second.jurisdiction_id);
    if (j && j->seat_place_id == place) {
      if (court)
        return {false,
                {},
                common_failure("AmbiguousPoliticalShopCounter", place).error};
      court = &row.second;
    }
  }
  V::Dict page{{"ok", V{true}},
               {"source_identity", source_identity(x.common.state().identity)},
               {"observer_actor_id", V{actor}},
               {"place_id", V{place}},
               {"cursor", V{cursor}}};
  V::Array rows;
  if (!court) {
    auto documents = personal_documents_page(x, actor, cursor, page);
    return documents.ok ? Outcome<BoundaryValue>{true, V{std::move(page)}, {}}
                        : Outcome<BoundaryValue>{false, {}, documents.error};
  }
  page.emplace_back("enabled", V{true});
  std::optional<PoliticalShopCourt> successor_court;
  if (x.registry_successions) {
    auto endpoint = shop_law_detail::court_endpoint_owned(
        x, *court, x.common.tick(), x.common.state().next_instance_id, {});
    if (!endpoint.ok) {
      page.emplace_back("court_id", V{court->id});
      page.emplace_back("mode", V{StableId{"registry_unavailable"}});
      page.emplace_back("code", V{endpoint.error.original.code});
      page.emplace_back("rows", V{rows});
      if (cursor)
        return {false, {}, common_failure("InvalidPoliticalShopCursor").error};
      return {true, V{std::move(page)}, {}};
    }
    successor_court = *court;
    successor_court->office_id = endpoint.value.endpoint().office_id;
    successor_court->role_id = endpoint.value.endpoint().role_id;
    successor_court->dialogue_id = endpoint.value.endpoint().dialogue_id;
    successor_court->request_node_id =
        endpoint.value.endpoint().request_node_id;
    court = &*successor_court;
  }
  auto administration = administration_page(x, *court, actor, cursor, page);
  if (!administration.ok)
    return {false, {}, administration.error};
  if (administration.value)
    return {true, V{std::move(page)}, {}};
  const auto *office = x.organizations.offices.find(court->office_id);
  bool meeting = false;
  if (office && office->holder_actor_id) {
    auto source = preview_authored_provider_source(
        x.common, x.interactions, x.interaction_catalog, x.interaction_access,
        actor, *office->holder_actor_id);
    meeting = source.ok &&
              source.value.meeting.dialogue_id == court->dialogue_id &&
              source.value.meeting.node_id == court->request_node_id &&
              source.value.meeting.context.role_id == court->role_id &&
              !source.value.meeting.context.kind &&
              !source.value.meeting.context.encounter_id;
  }
  page.emplace_back("court_id", V{court->id});
  page.emplace_back("at_counter_declaration", V{meeting});
  if (!meeting) {
    if (cursor)
      return {false, {}, common_failure("InvalidPoliticalShopCursor").error};
    auto talk =
        preview_interaction(x.common, x.interactions, x.interaction_catalog,
                            x.interaction_access, actor, Talk{court->role_id});
    page.emplace_back(
        "counter_action",
        boundary::obj(
            {{"command", V{StableId{"talk"}}},
             {"params", boundary::obj({{"role_id", V{court->role_id}}})},
             {"available", V{talk.ok}},
             {"code", V{talk.ok ? StableId{} : talk.error.original.code}}}));
    page.emplace_back("rows", V{rows});
    return {true, V{std::move(page)}, {}};
  }
  if (static_cast<std::size_t>(cursor) > court->premises_ids.size())
    return {false, {}, common_failure("InvalidPoliticalShopCursor").error};
  const auto end = std::min(court->premises_ids.size(),
                            static_cast<std::size_t>(cursor + 16));
  for (auto i = static_cast<std::size_t>(cursor); i < end; ++i) {
    const auto &id = court->premises_ids[i];
    const auto *premises = x.premises.find(id);
    if (!premises)
      return {
          false, {}, common_failure("PoliticalShopPremisesMismatch", id).error};
    PoliticalShopLicensingCommand command =
        PoliticalRequestShopPermission{court->id, id};
    auto quote = preview_political_shop_licensing_owned(x, actor, command);
    V::Dict row{{"premises_id", V{id}},
                {"place_id", V{premises->place_id}},
                {"available", V{quote.ok}},
                {"code", V{quote.ok ? StableId{} : quote.error.original.code}},
                {"command", V{political_shop_licensing_command_kind(command)}},
                {"params", encode_political_shop_licensing_command(command)}};


    auto permission_request_memory = CheckpointMemoryCharge(0);
    auto current = shop_law_detail::permission_owned(
        x,
        {checkpoint_fresh_string_copy(actor), checkpoint_fresh_string_copy(id),
         checkpoint_fresh_string_copy(premises->place_id)},
        x.common.tick(), x.common.state().next_instance_id, {});
    if (current.ok) {
      const auto *p =
          x.state.permits.find(current.value.source().permit_fact_id);
      const auto *c =
          p ? x.state.clearances.find(p->clearance_fact_id) : nullptr;
      if (!c)
        return {false,
                {},
                common_failure("PoliticalShopPermitHistoryUnavailable").error};
      row.emplace_back("permit_fact_id", V{p->id});
      row.emplace_back("clearance_fact_id", V{c->id});
      row.emplace_back("valid_through_tick", V{c->valid_through_tick});
    }
    rows.emplace_back(std::move(row));
  }
  page.emplace_back("rows", V{std::move(rows)});
  if (end < court->premises_ids.size())
    page.emplace_back("next_cursor", V{static_cast<Integer>(end)});
  return {true, V{std::move(page)}, {}};
}
} // namespace hanmo::resident
