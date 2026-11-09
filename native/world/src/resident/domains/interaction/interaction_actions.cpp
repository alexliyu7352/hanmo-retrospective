#include "interaction_projection.hpp"
#include "resident/domains/interaction/party_consent/party_recruitment_consent.hpp"
// 既有场所发现与同行规则；不创造新知闻系统，不复制人物资产或任务账。
#include "interaction_actions.hpp"
#include "party_invitation_recruitment.hpp"
#include "resident/checkpoint/completion_codec.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 失败只返回原领域码，调用者丢弃整个候选。
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
// 所有查找保持旧容器序，查询不会建立记录。
template <class T>
const T *by_id(const std::vector<T> &rows, const StableId &id) {
  for (auto &row : rows)
    if (row.id == id)
      return &row;
  return nullptr;
}
const InteractionSession *session(const InteractionState &state,
                                  const StableId &actor) {
  for (auto &row : state.interactions)
    if (row.actor_id == actor)
      return &row;
  return nullptr;
}
InteractionSession *session(InteractionState &state, const StableId &actor) {
  for (auto &row : state.interactions)
    if (row.actor_id == actor)
      return &row;
  return nullptr;
}
// 同行身份取当前真正成员表，Following引用和领队直接身份两者均保留。
const PartyRecord *party(const CommonRead &common, const StableId &actor) {
  auto store = dynamic_cast<const CommonStore *>(&common);
  if (!store)
    return nullptr;
  auto person = common.actor(actor);
  if (person)
    if (auto loc = std::get_if<FollowingLocation>(&person->location))
      return store->state().parties.find(loc->party_id);
  for (auto &row : store->state().parties.rows())
    if (row.second.leader_id == actor)
      return &row.second;
  return nullptr;
}
// 缺上下文与坏上下文沿原区分，不以空合法结构伪造授权。
InteractionContext context(const std::optional<InteractionContext> &input) {
  if (input)
    return *input;
  InteractionContext result;
  result.input_error = "MissingInteractionContext";
  return result;
}
// 每条线索独立核作者来源；查看道具不能顺便获得别人的未开放委托。
Outcome<InteractionQuote>
discover(const CommonRead &common, const InteractionState &state,
         const InteractionCatalog &catalog, const InteractionAccess &access,
         const StableId &actor, const DiscoverOffer &command) {
  if (!command.offer_id_valid)
    return fail<InteractionQuote>("InvalidOffer");
  const OfferDiscoveryDefinition *offer = nullptr;
  for (auto &row : catalog.discoveries)
    if (row.offer_id == command.offer_id)
      offer = &row;
  if (!offer)
    return fail<InteractionQuote>("UnknownOffer");
  KnowledgeSource source;
  if (command.has_prop_id) {
    auto prop = by_id(catalog.props, command.prop_id);
    auto person = common.actor(actor);
    auto loc =
        person ? std::get_if<SettlementLocation>(&person->location) : nullptr;
    if (!prop || !loc || loc->place_id != prop->place_id)
      return fail<InteractionQuote>("NotAtPlace");
    source = {"prop", command.prop_id, {}};
  } else {
    auto valid = validate_interaction(common, state, catalog, access, actor,
                                      context(command.context), "information");
    if (!valid.ok)
      return {false, {}, valid.error};
    source = {"actor", valid.value.provider_actor_id, {}};
  }
  bool known = false;
  for (auto &allowed : offer->sources)
    if (allowed.kind == source.kind && allowed.id == source.id)
      known = true;
  if (!known)
    return fail<InteractionQuote>("SourceUnknown");
  InteractionQuote quote;
  quote.source = source;
  return {true, quote, {}};
}
} // namespace
// 比较全部保存命令值及字段存在位，不能只比较kind后让旧会话篡改目标或技能。
bool same_dialogue_command(const DialogueCommand &a, const DialogueCommand &b) {
  if (a.kind != b.kind || a.offer_id != b.offer_id || a.shop_id != b.shop_id ||
      a.service_id != b.service_id || a.target_actor_id != b.target_actor_id ||
      a.activity != b.activity || a.skill_id != b.skill_id ||
      a.place_id != b.place_id || bool(a.context) != bool(b.context))
    return false;
  for (auto key : {"offer_id", "shop_id", "service_id", "target_actor_id",
                   "activity", "skill_id", "place_id"})
    if (!a.field_order.empty() && !b.field_order.empty() &&
        (std::find(a.field_order.begin(), a.field_order.end(), key) !=
         a.field_order.end()) !=
            (std::find(b.field_order.begin(), b.field_order.end(), key) !=
             b.field_order.end()))
      return false;
  if (a.context) {
    auto &x = *a.context;
    auto &y = *b.context;
    if (x.place_id != y.place_id ||
        x.provider_actor_id != y.provider_actor_id || x.role_id != y.role_id ||
        x.kind != y.kind || x.encounter_id != y.encounter_id)
      return false;
  }
  return true;
}
// 额外命令只有明确枚举分支，不将未知kind解释为邀请或发现。
bool is_interaction_extra(const InteractionCommand &c) {
  return std::holds_alternative<AskNews>(c) ||
         std::holds_alternative<Inspect>(c) ||
         std::holds_alternative<DiscoverOffer>(c) ||
         std::holds_alternative<JoinParty>(c) ||
         std::holds_alternative<DismissCompanion>(c) ||
         std::holds_alternative<DialogueConfirm>(c) ||
         std::holds_alternative<DialogueCancel>(c) ||
         std::holds_alternative<CloseTrade>(c) ||
         std::holds_alternative<OpenTrade>(c);
}
// 真实知识首次写入，重复获知不改原tick或来源，不分配世界编号。
Status remember_interaction_offer(InteractionState &state,
                                  const StableId &actor, const StableId &offer,
                                  const KnowledgeSource &source, Integer tick) {
  ActorKnowledge *owned = nullptr;
  for (auto &row : state.knowledge)
    if (row.actor_id == actor)
      owned = &row;
  if (!owned)
    return common_failure("InvalidKnowledge");
  for (auto &row : owned->offers)
    if (row.offer_id == offer)
      return common_success();
  owned->offers.push_back({offer, tick, source, {}});
  return common_success();
}
// 输入已有公共守卫后，发现/交易逐项核来源；同行保其特有ActorBusy首错。
Outcome<InteractionQuote> preview_interaction_extra(
    const CommonRead &common, const InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const StableId &actor, const InteractionCommand &command) {
  // 保留原GD能力、phase和真实上下文的错误顺序；读取本身不分享消息。
  if (std::holds_alternative<AskNews>(command)) {
    if (!catalog.npc_society || !catalog.regional_enabled)
      return fail<InteractionQuote>("SocialUnavailable");
    const auto *current = session(state, actor);
    if (!current || (current->phase != "say" && current->phase != "choice"))
      return fail<InteractionQuote>("NoConversation");
    auto valid = validate_interaction(common, state, catalog, access, actor,
                                      current->context);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (!access.news_preview)
      return fail<InteractionQuote>("SocialUnavailable");
    auto allowed = access.news_preview(actor, valid.value.provider_actor_id);
    if (!allowed.ok)
      return {false, {}, allowed.error};
    if (!allowed.value)
      return fail<InteractionQuote>("MissingNewsQuote");
    InteractionQuote quote;
    quote.context = valid.value;
    quote.news_quote = allowed.value;
    return {true, std::move(quote), {}};
  }
  if (auto c = std::get_if<Inspect>(&command)) {
    auto prop = by_id(catalog.props, c->prop_id);
    if (!prop)
      return fail<InteractionQuote>("UnknownProp");
    auto person = common.actor(actor);
    auto loc =
        person ? std::get_if<SettlementLocation>(&person->location) : nullptr;
    if (!loc || loc->place_id != prop->place_id)
      return fail<InteractionQuote>("NotAtPlace");
    for (auto &offer : prop->offer_ids) {
      DiscoverOffer request;
      request.offer_id = offer;
      request.prop_id = c->prop_id;
      request.has_prop_id = true;
      auto q = discover(common, state, catalog, access, actor, request);
      if (!q.ok)
        return q;
    }
    if (access.supply_inspection_selected) {
      if (!access.inspect_supply_preview)
        return fail<InteractionQuote>("PublicSupplyInspectionReaderRequired");
      auto checked = access.inspect_supply_preview(actor, c->prop_id);
      if (!checked.ok)
        return {false, {}, checked.error};
    }
    return {true, {}, {}};
  }
  if (auto c = std::get_if<DiscoverOffer>(&command))
    return discover(common, state, catalog, access, actor, *c);
  if (std::holds_alternative<DialogueConfirm>(command)) {
    auto request = pending_dialogue_request(state, actor);
    if (!request.ok)
      return {false, {}, request.error};
    return fail<InteractionQuote>("DialogueCoordinationUnavailable");
  }
  if (std::holds_alternative<DialogueCancel>(command) ||
      std::holds_alternative<CloseTrade>(command)) {
    auto current = session(state, actor);
    if (!current || (current->phase != "action" && current->phase != "trade"))
      return fail<InteractionQuote>("NoPendingDialogueAction");
    return {true, {}, {}};
  }
  if (auto c = std::get_if<OpenTrade>(&command)) {
    if (!access.shop_exists || !access.shop_representative)
      return fail<InteractionQuote>("DialogueCoordinationUnavailable");
    if (!access.shop_exists(c->shop_id))
      return fail<InteractionQuote>("UnknownShop");
    auto valid = validate_interaction(common, state, catalog, access, actor,
                                      context(c->context), "trade");
    if (!valid.ok)
      return {false, {}, valid.error};
    if (!access.shop_representative(c->shop_id, valid.value))
      return fail<InteractionQuote>("WrongShopRepresentative");
    InteractionQuote q;
    q.context = valid.value;
    return {true, q, {}};
  }
  if (!std::holds_alternative<JoinParty>(command) &&
      !std::holds_alternative<DismissCompanion>(command))
    return fail<InteractionQuote>("UnsupportedAction");
  auto person = common.actor(actor);
  if (!person)
    return fail<InteractionQuote>("UnknownActor");
  if (!access.army_id)
    return fail<InteractionQuote>("InteractionCoordinationUnavailable");
  if (!access.army_id(actor).empty())
    return fail<InteractionQuote>("ArmyNotReturned");
  if (std::holds_alternative<FollowingLocation>(person->location))
    return fail<InteractionQuote>("NotPartyLeader");
  if (!person->alive || !person->available || !person->action_id.empty())
    return fail<InteractionQuote>("ActorBusy");
  auto loc = std::get_if<SettlementLocation>(&person->location);
  if (!loc)
    return fail<InteractionQuote>("PartyNeedsSettlement");
  auto existing = party(common, actor);
  InteractionQuote q;
  q.party_id = existing ? existing->id : StableId{};
  if (auto c = std::get_if<DismissCompanion>(&command)) {
    if (!existing || c->actor_id == actor ||
        std::find(existing->members.begin(), existing->members.end(),
                  c->actor_id) == existing->members.end())
      return fail<InteractionQuote>("NotCompanion");
    auto target = common.actor(c->actor_id);
    if (!target || !target->action_id.empty())
      return fail<InteractionQuote>("ParticipantBusy");
    q.target = c->actor_id;
    return {true, q, {}};
  }
  auto &c = std::get<JoinParty>(command);
  auto valid = validate_interaction(common, state, catalog, access, actor,
                                    context(c.context), "join_party");
  if (c.return_receipt_id) {
    if (!access.returned_retainer_context)
      return fail<InteractionQuote>("InteractionCoordinationUnavailable");
    valid = access.returned_retainer_context(actor, c.actor_id,
                                             *c.return_receipt_id);
  }
  if (!valid.ok)
    return {false, {}, valid.error};
  // 原上下文先合法，再拒专用邀请role被倒置为招募；返营凭据也不能翻转该作者意愿。
  auto direction = validate_legacy_party_recruitment(
      catalog.party_invitations.get(), valid.value.role_id);
  if (!direction.ok)
    return {false, {}, direction.error};
  auto target = valid.value.provider_actor_id;
  if (!access.army_id(target).empty())
    return fail<InteractionQuote>("ArmyNotReturned");
  if (party(common, target))
    return fail<InteractionQuote>("AlreadyInParty");
  if (existing && existing->members.size() >= 16)
    return fail<InteractionQuote>("PartyFull");
  for (auto &conversation : state.interactions)
    if (conversation.actor_id != actor &&
        (conversation.actor_id == target ||
         conversation.context.provider_actor_id == target))
      return fail<InteractionQuote>("ParticipantBusy");
  Integer required = 0;
  for (auto &terms : catalog.companions)
    if (terms.role_id == valid.value.role_id)
      required = terms.required_trust;
  auto trust = access.relation_trust
                   ? access.relation_trust(target, actor).value_or(0)
                   : 0;
  if (trust < required)
    return fail<InteractionQuote>("CompanionTrustRequired");
  auto role = catalog.common ? catalog.common->roles.find(valid.value.role_id)
                             : nullptr;
  if (!role)
    return fail<InteractionQuote>("UnknownRole");
  q.target = target;
  q.context = valid.value;
  q.cost_per_tick = role->companion_cost_per_tick;
  if (catalog.party_recruitment_consents) {
    const auto *store = dynamic_cast<const CommonStore *>(&common);
    if (!store)
      return fail<InteractionQuote>("PartyRecruitmentSourceUnavailable");
    auto consent = preview_party_recruitment_consent(
        *store, state, catalog, catalog.party_recruitment_consents.get(),
        access, actor, valid.value);
    if (!consent.ok)
      return {false, {}, consent.error};
    if (consent.value) {
      if (c.actor_id != target)
        return fail<InteractionQuote>("PartyRecruitmentSourceMismatch");
      if (consent.value->decision != "accept")
        return fail<InteractionQuote>("PartyRecruitmentDeclined");
      q.party_recruitment_consent =
          std::make_shared<const PartyRecruitmentConsentQuote>(
              std::move(*consent.value));
    }
  }
  return {true, q, {}};
}
// 确认请求只从实际action会话取，作者kind不能通过UI参数替换。
Outcome<DialogueCommand> pending_dialogue_request(const InteractionState &state,
                                                  const StableId &actor) {
  auto current = session(state, actor);
  if (!current || current->phase != "action" || !current->pending_command)
    return fail<DialogueCommand>("NoPendingDialogueAction");
  auto request = *current->pending_command;
  request.context = current->context;
  if (!request.field_order.empty() &&
      std::find(request.field_order.begin(), request.field_order.end(),
                "context") == request.field_order.end())
    request.field_order.push_back("context");
  return {true, request, {}};
}
// 真结果决定success/failure分支，保原last_outcome和清除等待引用的顺序。
Status resolve_dialogue(InteractionState &state,
                        const InteractionCatalog &catalog,
                        const StableId &actor, const DialogueOutcome &outcome) {
  auto current = session(state, actor);
  if (!current)
    return common_failure("NoInteraction");
  const DialogueNode *node = nullptr;
  for (auto &dialogue : catalog.dialogues)
    if (dialogue.id == current->dialogue_id)
      for (auto &item : dialogue.nodes)
        if (item.id == current->node_id)
          node = &item;
  if (!node)
    return common_failure("DialogueNodeMissing");
  current->node_id =
      (outcome.completion ? action_completion_succeeded(*outcome.completion)
                          : outcome.success.value_or(outcome.ok))
          ? node->on_success
          : node->on_failure;
  current->phase = "stepping";
  current->pending_action_id.clear();
  current->pending_command.reset();
  current->last_outcome = outcome;
  return common_success();
}
// 确认后保持交易/等待输入；零时动作解析成功后继续原自动对白。
Status finish_dialogue_confirmation(CommonWrite &common,
                                    InteractionState &state,
                                    const InteractionCatalog &catalog,
                                    const InteractionAccess &access,
                                    const StableId &actor, const StableId &kind,
                                    const std::optional<StableId> &action) {
  auto current = session(state, actor);
  if (!current)
    return common_success();
  if (current->regional_binding && current->regional_binding->task_command)
    current->regional_action_committed = true;
  if (kind == "open_trade") {
    current->phase = "trade";
    current->trade_completed = false;
    return common_success();
  }
  if (action) {
    current->phase = "waiting";
    current->pending_action_id = *action;
    return common_success();
  }
  DialogueOutcome success;
  success.present = true;
  success.ok = true;
  auto resolved = resolve_dialogue(state, catalog, actor, success);
  if (!resolved.ok)
    return resolved;
  return pump_dialogue(common, state, catalog, access, actor);
}
// 同行只更新原成员/条款和人物位置；共享party写口必须参加CommonStore原子写集。
Outcome<InteractionEffect>
apply_interaction_extra(CommonStore &common, InteractionState &state,
                        const InteractionCatalog &catalog,
                        const InteractionAccess &access, const StableId &actor,
                        const InteractionCommand &command,
                        const InteractionQuote &q) {
  // 实际分享沿当前say/choice再准入一次，回执只携Social已写入的来源ID。
  if (std::holds_alternative<AskNews>(command)) {
    auto quote = preview_interaction_extra(common, state, catalog, access,
                                           actor, command);
    if (!quote.ok)
      return {false, {}, quote.error};
    if (!q.context ||
        (q.context->place_id != quote.value.context->place_id ||
         q.context->provider_actor_id !=
             quote.value.context->provider_actor_id ||
         q.context->role_id != quote.value.context->role_id ||
         q.context->kind != quote.value.context->kind ||
         q.context->encounter_id != quote.value.context->encounter_id))
      return fail<InteractionEffect>("StaleNewsContext");
    if (!access.news_apply)
      return fail<InteractionEffect>("SocialUnavailable");
    if (!q.news_quote)
      return fail<InteractionEffect>("MissingNewsQuote");
    auto heard = access.news_apply(
        actor, quote.value.context->provider_actor_id, *q.news_quote);
    if (!heard.ok)
      return {false, {}, heard.error};
    InteractionEffect effect;
    effect.news_ids = std::move(heard.value);
    return {true, std::move(effect), {}};
  }
  if (auto c = std::get_if<Inspect>(&command)) {
    auto prop = by_id(catalog.props, c->prop_id);
    if (!prop)
      return fail<InteractionEffect>("UnknownProp");
    // 原供货读取与已有 offer 发现同属本次事务；任一失败撤整个候选。
    if (access.supply_inspection_selected) {
      if (!access.inspect_supply_apply)
        return fail<InteractionEffect>("PublicSupplyInspectionWriterRequired");
      auto inspected = access.inspect_supply_apply(actor, c->prop_id);
      if (!inspected.ok)
        return {false, {}, inspected.error};
    }
    for (auto &offer : prop->offer_ids) {
      auto r = remember_interaction_offer(
          state, actor, offer, {"prop", c->prop_id, {}}, common.tick());
      if (!r.ok)
        return {false, {}, r.error};
    }
    return {true, {}, {}};
  }
  if (auto c = std::get_if<DiscoverOffer>(&command)) {
    if (!q.source)
      return fail<InteractionEffect>("SourceUnknown");
    auto r = remember_interaction_offer(state, actor, c->offer_id, *q.source,
                                        common.tick());
    return r.ok ? Outcome<InteractionEffect>{true, {}, {}}
                : Outcome<InteractionEffect>{false, {}, r.error};
  }
  if (std::holds_alternative<OpenTrade>(command))
    return {true, {}, {}};
  if (std::holds_alternative<DialogueCancel>(command) ||
      std::holds_alternative<CloseTrade>(command)) {
    auto current = session(state, actor);
    if (!current)
      return fail<InteractionEffect>("NoPendingDialogueAction");
    DialogueOutcome outcome;
    outcome.present = true;
    outcome.ok =
        std::holds_alternative<CloseTrade>(command) && current->trade_completed;
    outcome.code = "Cancelled";
    auto r = resolve_dialogue(state, catalog, actor, outcome);
    if (!r.ok)
      return {false, {}, r.error};
    r = pump_dialogue(common, state, catalog, access, actor);
    return r.ok ? Outcome<InteractionEffect>{true, {}, {}}
                : Outcome<InteractionEffect>{false, {}, r.error};
  }
  if (!q.party_id || !q.target || !access.write_party)
    return fail<InteractionEffect>("InteractionCoordinationUnavailable");
  auto actor_before =
      capture_actor_social_occupancy(common, state, access, actor);
  if (!actor_before.ok)
    return {false, {}, actor_before.error};
  auto target_before =
      capture_actor_social_occupancy(common, state, access, *q.target);
  if (!target_before.ok)
    return {false, {}, target_before.error};
  StableId id = *q.party_id;
  auto record_party = [&](ActorSocialOccupancyCause cause) -> Status {
    auto a = record_captured_actor_social_occupancy(
        access, actor, actor_before.value, cause, id, actor);
    if (!a.ok)
      return a;
    return record_captured_actor_social_occupancy(
        access, *q.target, target_before.value, cause, id, actor);
  };
  ActorMutation mutation;
  mutation.actor_id = *q.target;
  if (std::holds_alternative<JoinParty>(command)) {
    if (state.social_occupancy && (state.social_occupancy->version == 2 ||
                                   state.social_occupancy->version == 3)) {
      auto fresh = preview_interaction_extra(common, state, catalog, access,
                                             actor, command);
      if (!fresh.ok)
        return {false, {}, fresh.error};
      if (fresh.value.party_id != q.party_id ||
          fresh.value.target != q.target ||
          fresh.value.cost_per_tick != q.cost_per_tick ||
          !fresh.value.context || !q.context ||
          fresh.value.context->place_id != q.context->place_id ||
          fresh.value.context->role_id != q.context->role_id ||
          fresh.value.context->provider_actor_id !=
              q.context->provider_actor_id ||
          fresh.value.context->kind != q.context->kind ||
          fresh.value.context->encounter_id != q.context->encounter_id)
        return fail<InteractionEffect>("StalePartyAcceptanceSource");
    }
    if (catalog.party_recruitment_consents) {
      auto fresh = preview_interaction_extra(common, state, catalog, access,
                                             actor, command);
      if (!fresh.ok)
        return {false, {}, fresh.error};
      if (bool(fresh.value.party_recruitment_consent) !=
          bool(q.party_recruitment_consent))
        return fail<InteractionEffect>("StalePartyRecruitmentConsent");
      if (q.party_recruitment_consent) {
        const auto &proof = *q.party_recruitment_consent;
        if (proof.source.meeting.actor_id != actor ||
            proof.source.speaker_actor_id != *q.target ||
            q.party_id != fresh.value.party_id ||
            q.target != fresh.value.target ||
            q.cost_per_tick != fresh.value.cost_per_tick || !q.context ||
            q.context->role_id != proof.role_id ||
            q.context->provider_actor_id != proof.source.speaker_actor_id ||
            q.context->place_id != proof.source.meeting.context.place_id ||
            q.context->kind != proof.source.meeting.context.kind ||
            q.context->encounter_id !=
                proof.source.meeting.context.encounter_id)
          return fail<InteractionEffect>("StalePartyRecruitmentConsent");
        auto valid = revalidate_party_recruitment_consent(
            common, state, catalog, catalog.party_recruitment_consents.get(),
            access, proof);
        if (!valid.ok)
          return {false, {}, valid.error};
      }
    } else if (q.party_recruitment_consent)
      return fail<InteractionEffect>("PartyRecruitmentProfileRequired");
    PartyRecord record;
    if (id.empty()) {
      auto allocated = common.next_id("party");
      if (!allocated.ok)
        return {false, {}, allocated.error};
      id = allocated.value;
      record.id = id;
      record.leader_id = actor;
      record.members = {actor};
      record.created_tick = common.tick();
    } else {
      auto existing = common.state().parties.find(id);
      if (!existing)
        return fail<InteractionEffect>("NotCompanion");
      record = *existing;
    }
    if (!q.context || !q.cost_per_tick)
      return fail<InteractionEffect>("MissingInteractionContext");
    record.members.push_back(*q.target);
    record.terms.set(*q.target, {q.context->role_id, *q.cost_per_tick, {}});
    auto written = access.write_party(id, record);
    if (!written.ok)
      return {false, {}, written.error};
    mutation.location = FollowingLocation{id};
  } else if (std::holds_alternative<DismissCompanion>(command)) {
    auto current = common.state().parties.find(id);
    if (!current)
      return fail<InteractionEffect>("NotCompanion");
    auto record = *current;
    record.members.erase(
        std::remove(record.members.begin(), record.members.end(), *q.target),
        record.members.end());
    record.terms.erase(*q.target);
    auto location = common.resolved_location(actor);
    if (!location.ok)
      return {false, {}, location.error};
    mutation.location = location.value;
    auto applied = common.apply_actor(mutation);
    if (!applied.ok)
      return {false, {}, applied.error};
    auto written = access.write_party(
        id, record.members.size() == 1 ? std::optional<PartyRecord>{}
                                       : std::optional<PartyRecord>{record});
    if (!written.ok)
      return {false, {}, written.error};
    auto recorded = record_party(ActorSocialOccupancyCause::party_dismissed);
    if (!recorded.ok)
      return {false, {}, recorded.error};
    InteractionEffect effect;
    effect.party_id = id;
    effect.person_id = *q.target;
    return {true, effect, {}};
  } else
    return fail<InteractionEffect>("UnsupportedAction");
  auto applied = common.apply_actor(mutation);
  if (!applied.ok)
    return {false, {}, applied.error};
  PartyMembershipAcceptance accepted;
  accepted.kind = q.party_recruitment_consent
                      ? PartyAcceptanceKind::declared_recruitment
                      : PartyAcceptanceKind::legacy_recruitment;
  accepted.leader_actor_id = actor;
  accepted.role_id = q.context->role_id;
  accepted.cost_per_tick = *q.cost_per_tick;
  accepted.context = *q.context;
  if (q.party_recruitment_consent) {
    const auto &proof = *q.party_recruitment_consent;
    const auto &meeting = proof.source.meeting;
    accepted.authored_node = PartyAcceptanceNode{
        meeting.actor_id, meeting.session_id, meeting.dialogue_id,
        meeting.node_id, meeting.node_kind};
    accepted.authored_declaration_id = proof.declaration_id;
  }
  auto recorded = record_captured_actor_social_occupancy(
      access, actor, actor_before.value,
      ActorSocialOccupancyCause::party_recruited, id, actor);
  if (!recorded.ok)
    return {false, {}, recorded.error};
  recorded = record_captured_party_member_acceptance(
      access, *q.target, target_before.value,
      ActorSocialOccupancyCause::party_recruited, id, actor, accepted);
  if (!recorded.ok)
    return {false, {}, recorded.error};
  InteractionEffect effect;
  effect.party_id = id;
  effect.person_id = *q.target;
  return {true, effect, {}};
}
// 只公开岗位主题当前诊断，失败主题保留而不会遮掉同人的其它岗位。
Outcome<BoundaryValue> query_interaction_topics(
    const CommonRead &common, const InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const StableId &actor, const StableId &place) {
  using V = BoundaryValue;
  using D = V::Dict;
  using A = V::Array;
  A result;
  if (!common.actor(actor) || !catalog.common ||
      !catalog.common->places.find(place))
    return {true, V{result}, {}};
  OrderedTable<A> groups;
  auto roles = catalog.common->roles.order();
  std::sort(roles.begin(), roles.end());
  for (auto &id : roles) {
    auto &role = *catalog.common->roles.find(id);
    if (role.place_id != place)
      continue;
    auto assignment = access.role ? access.role(id) : std::nullopt;
    if (!assignment || !common.actor(assignment->actor_id))
      continue;
    auto valid =
        preview_interaction(common, state, catalog, access, actor, Talk{id});
    if (!groups.find(assignment->actor_id))
      groups.insert(assignment->actor_id, {});
    groups.find(assignment->actor_id)
        ->push_back(V{D{
            {"role_id", V{id}},
            {"dialogue_id", V{valid.ok ? valid.value.dialogue_id : StableId{}}},
            {"services", boundary::string_array(role.services)},
            {"available", V{valid.ok}},
            {"code", V{valid.ok ? StableId{} : valid.error.original.code}}}});
  }
  auto people = groups.order();
  std::sort(people.begin(), people.end());
  for (auto &id : people)
    result.push_back(
        V{D{{"provider_actor_id", V{id}}, {"topics", V{*groups.find(id)}}}});
  return {true, V{result}, {}};
}
// 额外报价沿原构造顺序；无世界引用即可编码。
BoundaryValue encode_interaction_extra_quote(const StableId &kind,
                                             const InteractionQuote &q) {
  using V = BoundaryValue;
  using D = V::Dict;
  D d{{"ok", V{true}}};
  if (kind == "discover_offer") {
    if (!q.source)
      boundary::fail("MissingDiscoverySource", "quote");
    d.emplace_back("source",
                   V{D{{"kind", V{q.source->kind}}, {"id", V{q.source->id}}}});
    d.emplace_back("duration", V{q.duration});
    return V{d};
  }
  d.emplace_back("duration", V{q.duration});
  if (q.party_id)
    d.emplace_back("party_id", V{*q.party_id});
  if (q.target)
    d.emplace_back("target", V{*q.target});
  if (q.context)
    d.emplace_back("context", interaction_codec::encode_context(*q.context));
  if (q.cost_per_tick)
    d.emplace_back("cost_per_tick", V{*q.cost_per_tick});
  return V{d};
}
Status emit_interaction_extra_effect_fields(ProjectionWriter &out,
                                            const InteractionEffect &e) {
  if (e.party_id)
    out.field("party_id", *e.party_id);
  if (e.person_id)
    out.field("person_id", *e.person_id);
  if (e.news_ids)
    out.sequence("news_ids", *e.news_ids, [](auto &child, const auto &id) {
      return child.field("", id);
    });
  return out.status();
}
BoundaryValue encode_interaction_extra_effect(const InteractionEffect &e) {
  BoundaryValue value;
  BoundaryProjectionWriter writer(value);
  auto status = writer.object("", [&](auto &out) {
    return emit_interaction_extra_effect_fields(out, e);
  });
  if (status.ok)
    status = writer.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}
} // namespace hanmo::resident
namespace hanmo::resident {
// 单次节点泵保64步无输出上限，外层自动发现另有64次协议上限。
Status pump_dialogue_node(CommonWrite &, InteractionState &,
                          const InteractionCatalog &, const InteractionAccess &,
                          const StableId &);
// 只有原零时discover_offer自动执行；真正行动仍停在确认，不扩展NPC自主权限。
Status pump_dialogue(CommonWrite &common, InteractionState &state,
                     const InteractionCatalog &catalog,
                     const InteractionAccess &access, const StableId &actor) {
  for (int step = 0; step < 64; ++step) {
    auto pumped = pump_dialogue_node(common, state, catalog, access, actor);
    if (!pumped.ok)
      return pumped;
    auto current = session(state, actor);
    if (!current)
      return common_success();
    if (current->phase != "action" || !current->pending_command ||
        current->pending_command->kind != "discover_offer")
      return common_success();
    DiscoverOffer request;
    request.offer_id = current->pending_command->offer_id;
    request.context = current->context;
    auto guard = access.automatic_discovery_guard
                     ? access.automatic_discovery_guard(actor)
                     : common_success();
    auto quote = guard.ok ? preview_interaction_extra(common, state, catalog,
                                                      access, actor, request)
                          : Outcome<InteractionQuote>{false, {}, guard.error};
    DialogueOutcome outcome;
    outcome.present = true;
    outcome.ok = quote.ok;
    if (quote.ok) {
      auto written = remember_interaction_offer(
          state, actor, request.offer_id, *quote.value.source, common.tick());
      outcome.ok = written.ok;
      if (!written.ok)
        outcome.code = written.error.original.code;
    } else
      outcome.code = quote.error.original.code;
    auto resolved = resolve_dialogue(state, catalog, actor, outcome);
    if (!resolved.ok)
      return resolved;
  }
  return common_failure("DialogueStepLimit");
}
} // namespace hanmo::resident
