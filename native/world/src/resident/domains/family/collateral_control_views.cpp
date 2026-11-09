// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 有限本人页不从全族谱枚举亲属；只有当前真实声明与本人已认可的原关系进入页面。
#include "collateral_control_commands.hpp"
#include "resident/views/common_presentation.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V = BoundaryValue;
V identity(const CommonStore &c) {
  using namespace boundary;
  StableId world;
  const char *hex = "0123456789abcdef";
  for (auto n : c.state().identity.world_id) {
    world += hex[n >> 4];
    world += hex[n & 15];
  }
  const auto &i = c.state().identity;
  return obj({{"world_id", V{world}},
              {"execution_epoch", V{i.execution_epoch}},
              {"revision", V{i.revision}},
              {"tick", V{i.tick}}});
}
V person(const CollateralRecognitionContext &r, const StableId &id) {
  using namespace boundary;
  auto name = CommonPresentation(r.people.common.state(), r.people.catalog)
                  .display_name(id);
  return obj({{"actor_id", V{id}}, {"name", V{name.ok ? name.value : id}}});
}
V parameters(const CollateralControlCommand &cmd) {
  V::Dict out{{"request_id", V{cmd.request_id}}};
  for (const auto &field :
       {std::make_pair("candidate_actor_id", cmd.candidate_actor_id),
        std::make_pair("recognition_fact_id", cmd.recognition_fact_id),
        std::make_pair("consent_fact_id", cmd.consent_fact_id)})
    if (!field.second.empty())
      out.emplace_back(field.first, V{field.second});
  return V{std::move(out)};
}
} // namespace
Outcome<BoundaryValue> query_collateral_control_page(
    const CollateralControlDecisionContext &x, const FamilyState &family,
    const FamilyDeathState &deaths, const FamilyDeathOriginRead &death_read,
    const InteractionState &interactions, const InteractionAccess &access,
    const FamilyLegacyState &legacy, const FamilyLegacyPlayableRead &playable,
    Integer page, const DiscipleControlRelationContext *disciple,
    const BiologicalControlDecisionContext *biological) {
  using namespace boundary;
  if (page < 0 || page > 4095)
    return {false, {}, common_failure("InvalidCollateralControlPage").error};
  const auto &r = x.recognition;
  const auto &c = r.people.common;
  auto valid = validate_collateral_control_context(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_collateral_recognition_history(r, x.state);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_family_legacy(
      c, family, deaths, death_read, r.people.catalog, r.interactions,
      r.original_requests, legacy, playable, disciple, biological, &x);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_collateral_control_result_bindings(c, legacy, r.catalog,
                                                      x.state);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &actor = c.state().metadata.player_actor_id;
  const auto *current = c.actor(actor);
  if (!current)
    return {false, {}, common_failure("UnknownActor", actor).error};
  V::Array requests, relations, consents;
  auto operation = [&](const CollateralControlCommand &cmd) {
    auto quote = preview_collateral_control_command(
        x, family, deaths, death_read, interactions, access, legacy, playable,
        actor, cmd, disciple, biological);
    return obj(
        {{"command", V{cmd.kind}},
         {"params", parameters(cmd)},
         {"available", V{quote.ok}},
         {"code", V{quote.ok ? StableId{} : quote.error.original.code}}});
  };
  std::set<std::pair<StableId, StableId>> offered;
  if (current->alive)
    for (const auto &session : interactions.interactions) {
      if (session.actor_id != actor || session.phase != "say" ||
          session.context.provider_actor_id.empty())
        continue;
      const auto &candidate = session.context.provider_actor_id;
      for (const auto &row : r.catalog.definitions.rows()) {
        CollateralControlCommand cmd{
            "family_collateral_recognize", row.first, candidate, {}, {}};
        auto quote = preview_collateral_control_command(
            x, family, deaths, death_read, interactions, access, legacy,
            playable, actor, cmd, disciple, biological);
        if (!quote.ok || !offered.emplace(row.first, candidate).second)
          continue;
        requests.push_back(obj({{"request_id", V{row.first}},
                                {"candidate", person(r, candidate)},
                                {"action", operation(cmd)}}));
      }
    }
  for (const auto &row : x.state.recognitions.rows()) {
    const auto &source =
        std::get<CollateralRecognitionFact>(row.second.payload).source;
    if (source.first_actor_id != actor && source.second_actor_id != actor)
      continue;
    const auto other = source.first_actor_id == actor ? source.second_actor_id
                                                      : source.first_actor_id;
    V::Array actions;
    if (current->alive)
      for (const auto &policy : r.catalog.definitions.rows()) {
        CollateralControlCommand cmd{
            "family_collateral_accept", policy.first, other, row.first, {}};
        auto quote = preview_collateral_control_command(
            x, family, deaths, death_read, interactions, access, legacy,
            playable, actor, cmd, disciple, biological);
        if (quote.ok)
          actions.push_back(operation(cmd));
      }
    relations.push_back(obj({{"recognition_fact_id", V{row.first}},
                             {"recognized_tick", V{source.tick}},
                             {"relative", person(r, other)},
                             {"actions", V{std::move(actions)}}}));
  }
  for (const auto &row : legacy.consents.rows()) {
    const auto &source = row.second.source;
    if (!source.collateral_dialogue || (source.predecessor_actor_id != actor &&
                                        source.successor_actor_id != actor))
      continue;
    StableId status = "accepted";
    for (const auto &r : legacy.revocations.rows())
      if (r.second.consent_fact_id == row.first)
        status = "revoked";
    for (const auto &a : legacy.activations.rows())
      if (a.second.consent_fact_id == row.first)
        status = "activated";
    V::Array actions;
    if (status == "accepted" && source.predecessor_actor_id == actor) {
      if (current->alive) {
        CollateralControlCommand cmd{
            "family_collateral_revoke", source.request_id, {}, {}, row.first};
        auto quote = preview_collateral_control_command(
            x, family, deaths, death_read, interactions, access, legacy,
            playable, actor, cmd, disciple, biological);
        if (quote.ok)
          actions.push_back(operation(cmd));
      } else {
        auto quote = preview_family_legacy_activation(
            c, family, deaths, death_read, r.people.catalog, r.interactions,
            r.original_requests, legacy, playable, row.first, disciple,
            biological, &x);
        actions.push_back(obj(
            {{"command", V{StableId{"family_legacy_activate"}}},
             {"params", obj({{"consent_fact_id", V{row.first}}})},
             {"available", V{quote.ok}},
             {"code", V{quote.ok ? StableId{} : quote.error.original.code}}}));
      }
    }
    consents.push_back(obj({{"consent_fact_id", V{row.first}},
                            {"recognition_fact_id", V{source.relation_fact_id}},
                            {"accepted_tick", V{source.tick}},
                            {"successor", person(r, source.successor_actor_id)},
                            {"status", V{status}},
                            {"actions", V{std::move(actions)}}}));
  }
  const Integer request_count = requests.size(),
                relation_count = relations.size(),
                consent_count = consents.size();
  auto slice = [page](V::Array &a) {
    V::Array out;
    for (Integer i = page * 16; i < std::min<Integer>(a.size(), page * 16 + 16);
         ++i)
      out.push_back(std::move(a[static_cast<std::size_t>(i)]));
    a = std::move(out);
  };
  slice(requests);
  slice(relations);
  slice(consents);
  return {true,
          obj({{"actor_id", V{actor}},
               {"relation_basis", V{StableId{"collateral"}}},
               {"source_identity", identity(c)},
               {"page", V{page}},
               {"request_count", V{request_count}},
               {"relation_count", V{relation_count}},
               {"consent_count", V{consent_count}},
               {"requests", V{std::move(requests)}},
               {"relations", V{std::move(relations)}},
               {"consents", V{std::move(consents)}},
               {"awaiting_control_choice", V{!current->alive}},
               {"scope", V{StableId{"own_life_control_only"}}},
               {"assets_transferred", V{false}},
               {"knowledge_granted", V{false}},
               {"public_authority_transferred", V{false}}}),
          {}};
}
} // namespace hanmo::resident
