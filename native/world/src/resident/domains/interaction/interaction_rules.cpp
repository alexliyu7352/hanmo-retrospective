// 路遇、对白与事件沿原顺序操作同一候选；任何回调错误交外层原子事务回滚。
#include "face_to_face_contact_source.hpp"
#include "interaction_actions.hpp"
#include "interaction_model.hpp"
#include "regional_life_dialogue.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/model/common_operations.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
template <class T> T *find_id(std::vector<T> &rows, const StableId &id) {
  for (auto &r : rows)
    if (r.id == id)
      return &r;
  return nullptr;
}
template <class T>
const T *find_id(const std::vector<T> &rows, const StableId &id) {
  for (auto &r : rows)
    if (r.id == id)
      return &r;
  return nullptr;
}
template <class T> bool contains(const std::vector<T> &rows, const T &item) {
  return std::find(rows.begin(), rows.end(), item) != rows.end();
}
Status fail(const StableId &code, ErrorKind kind = ErrorKind::rule,
            const StableId &source = {}) {
  return {false, {}, {{code, {}}, kind, {}, "interaction", {}, source}};
}
template <class T> Outcome<T> error(const Status &s) {
  return {false, {}, s.error};
}
InteractionSession *session(InteractionState &s, const StableId &actor) {
  for (auto &v : s.interactions)
    if (v.actor_id == actor)
      return &v;
  return nullptr;
}
const InteractionSession *session(const InteractionState &s,
                                  const StableId &actor) {
  for (auto &v : s.interactions)
    if (v.actor_id == actor)
      return &v;
  return nullptr;
}
// 动态人物沿原Identity.definition读取实际prototype，不把新actor
// ID误当静态目录键。
const ActorDialogueDefinition *actor_dialogue(const CommonRead &common,
                                              const InteractionCatalog &c,
                                              const StableId &actor) {
  StableId prototype = actor;
  if (auto store = dynamic_cast<const CommonStore *>(&common))
    if (auto person = store->state().actors.find(actor))
      prototype = person->prototype_id.value_or(actor);
  for (auto &v : c.actor_dialogues)
    if (v.actor_id == prototype)
      return &v;
  return nullptr;
}
const RoleDefinition *role_def(const InteractionCatalog &c,
                               const StableId &role) {
  return c.common ? c.common->roles.find(role) : nullptr;
}
// 不利用浮点容差比较持久位置；所有标签与整数身份必须精确匹配。
bool location_equal(const Location &a, const Location &b) {
  if (a.index() != b.index())
    return false;
  if (auto p = std::get_if<SettlementLocation>(&a)) {
    auto &q = std::get<SettlementLocation>(b);
    return p->settlement_id == q.settlement_id && p->place_id == q.place_id;
  }
  if (auto p = std::get_if<WildernessLocation>(&a)) {
    auto &q = std::get<WildernessLocation>(b);
    return p->map_id == q.map_id && p->origin == q.origin &&
           p->point.x == q.point.x && p->point.y == q.point.y;
  }
  if (auto p = std::get_if<FollowingLocation>(&a))
    return p->party_id == std::get<FollowingLocation>(b).party_id;
  if (auto p = std::get_if<InArmyLocation>(&a))
    return p->army_id == std::get<InArmyLocation>(b).army_id;
  auto &p = std::get<OnRouteLocation>(a);
  auto &q = std::get<OnRouteLocation>(b);
  return p.edge_id == q.edge_id && p.origin == q.origin &&
         p.progress == q.progress;
}
// 真实看管只放行同处对白；缺来源仍不可用，死亡和其它业务不能借此放宽。
Outcome<bool> dialogue_available(const CommonRead &c,
                                 const InteractionAccess &a,
                                 const StableId &actor, bool allow_custody) {
  const auto *person = c.actor(actor);
  if (!person || !person->alive)
    return {true, false, {}};
  if (person->available)
    return {true, true, {}};
  if (!allow_custody || !a.custody_dialogue_available)
    return {true, false, {}};
  return a.custody_dialogue_available(actor);
}
// 路遇对白仍核原运动接触；离开分支不借看管豁免。
Outcome<bool> encounter_side(const CommonRead &c, const InteractionAccess &a,
                             const EncounterRecord &e, std::size_t side,
                             bool allow_custody = false) {
  const auto *person = c.actor(e.leaders[side]);
  auto available = dialogue_available(c, a, e.leaders[side], allow_custody);
  if (!available.ok)
    return available;
  if (!available.value)
    return error<bool>(fail("EncounterMoved"));
  if (e.exact_origin) {
    const auto &contact = e.exact_origin->contact_source;
    if (contact.source_schema != 1 || contact.source_ref.domain != "travel" ||
        contact.source_ref.kind != "movement_contact" || !a.exact_contact_side)
      return error<bool>(fail("ExactContactPresenceReadRequired",
                              ErrorKind::unsupported_capability, e.id));
    return a.exact_contact_side(e.action_ids[side], contact);
  }
  if (!location_equal(person->location, e.anchors[side]) ||
      person->action_id != e.action_ids[side])
    return error<bool>(fail("EncounterMoved"));
  return {true, false, {}};
}
StableId place(const InteractionAccess &a, const CommonRead &c,
               const StableId &id) {
  auto v = a.resolve_location ? a.resolve_location(id) : c.actor(id)->location;
  auto p = std::get_if<SettlementLocation>(&v);
  return p ? p->place_id : StableId{};
}
const DialogueNode *node(const InteractionCatalog &c,
                         const InteractionSession &s) {
  auto d = find_id(c.dialogues, s.dialogue_id);
  return d ? find_id(d->nodes, s.node_id) : nullptr;
}
// 开会话只分配一个interaction编号；对白的自动推进仍由调用方在同候选随后执行。
Status begin(CommonWrite &c, InteractionState &s, const InteractionCatalog &cat,
             const InteractionAccess &a, const StableId &actor,
             const InteractionQuote &q) {
  auto before = capture_actor_social_occupancy(c, s, a, actor);
  if (!before.ok)
    return {false, {}, before.error};
  auto d = find_id(cat.dialogues, q.dialogue_id);
  if (!d || !q.context)
    return fail("NoDialogueForRole");
  auto id = c.next_id("interaction");
  if (!id.ok)
    return {false, {}, id.error};
  InteractionSession v;
  v.actor_id = actor;
  v.id = id.value;
  v.dialogue_id = d->id;
  v.context = *q.context;
  v.node_id = d->entry;
  v.phase = "stepping";
  v.regional_binding = q.regional_binding;
  const bool replacing = session(s, actor) != nullptr;
  if (auto old = session(s, actor))
    *old = std::move(v);
  else
    s.interactions.push_back(std::move(v));
  auto boundary =
      open_interaction_response_boundary_in_candidate(s, cat, id.value);
  if (!boundary.ok)
    return boundary;
  return record_captured_actor_social_occupancy(
      a, actor, before.value,
      replacing ? ActorSocialOccupancyCause::dialogue_replaced
                : ActorSocialOccupancyCause::dialogue_open,
      id.value, actor);
}
Outcome<InteractionQuote>
can_begin(const CommonRead &c, const InteractionState &s,
          const InteractionCatalog &cat, const InteractionAccess &a,
          const StableId &actor, const StableId &role) {
  auto r = role_def(cat, role);
  if (!r)
    return error<InteractionQuote>(fail("UnknownRole"));
  if (!a.role)
    return error<InteractionQuote>(fail("UnsupportedNativeCapability",
                                        ErrorKind::unsupported_capability,
                                        "role_lookup"));
  auto as = a.role(role);
  if (!as)
    return error<InteractionQuote>(fail("UnknownInteractionTarget"));
  InteractionContext ctx{r->place_id, as->actor_id, role, {}, {}};
  auto checked = validate_interaction(c, s, cat, a, actor, ctx);
  if (!checked.ok)
    return {false, {}, checked.error};
  auto ad = actor_dialogue(c, cat, as->actor_id);
  auto id =
      r->dialogue_id ? r->dialogue_id : (ad ? ad->dialogue_id : std::nullopt);
  auto d = id ? find_id(cat.dialogues, *id) : nullptr;
  if (!d || d->context_role != role)
    return error<InteractionQuote>(fail("NoDialogueForRole"));
  InteractionQuote q;
  q.dialogue_id = *id;
  q.context = ctx;
  return {true, q, {}};
}
// 事件语义有效性故意不检查busy，避免临时行动误取消排队事件。
Outcome<bool> event_available(const CommonRead &c, const InteractionState &s,
                              const InteractionCatalog &cat,
                              const InteractionAccess &a, const StableId &actor,
                              const EventDefinition &d) {
  auto p = c.actor(actor);
  if (!p || !p->alive || !p->available || place(a, c, actor) != d.place_id)
    return {true, false, {}};
  auto conditions = all_conditions(c, s, a, actor, d.conditions);
  if (!conditions.ok || !conditions.value)
    return conditions;
  auto dialogue = find_id(cat.dialogues, d.dialogue_id);
  if (!dialogue)
    return error<bool>(fail("InvalidSceneEvent"));
  auto r = role_def(cat, dialogue->context_role);
  auto assignment = a.role ? a.role(dialogue->context_role) : std::nullopt;
  if (!r || !assignment)
    return error<bool>(fail("UnknownInteractionTarget"));
  auto provider = c.actor(assignment->actor_id);
  if (!assignment->enabled || r->place_id != d.place_id ||
      assignment->actor_id == actor || !provider || !provider->alive ||
      !provider->available || place(a, c, provider->id) != d.place_id)
    return {true, false, {}};
  auto ad = actor_dialogue(c, cat, provider->id);
  auto id =
      r->dialogue_id ? r->dialogue_id : (ad ? ad->dialogue_id : std::nullopt);
  return {true, id && *id == d.dialogue_id, {}};
}
} // namespace
const EncounterRecord *pending_encounter(const InteractionState &s,
                                         const StableId &actor) {
  for (auto &e : s.encounters)
    if (e.status == "pending" && contains(e.participants, actor))
      return &e;
  return nullptr;
}
bool interaction_occupied(const InteractionState &s, const StableId &actor) {
  return session(s, actor) != nullptr;
}
std::optional<bool> flag_value(const InteractionState &s, const StableId &id) {
  auto f = find_id(s.flags, id);
  return f ? std::optional<bool>(f->value) : std::nullopt;
}
// 原Godot Vector2距离使用binary32；与地形binary64几何严格分开。
bool anchors_meet(const InteractionCatalog &c, const Location &a,
                  const Location &b) {
  auto wa = std::get_if<WildernessLocation>(&a);
  auto wb = std::get_if<WildernessLocation>(&b);
  if (wa || wb) {
    if (!wa || !wb || wa->map_id != wb->map_id)
      return false;
    float dx =
        static_cast<float>(wa->point.x) - static_cast<float>(wb->point.x);
    float dy =
        static_cast<float>(wa->point.y) - static_cast<float>(wb->point.y);
    float xx = dx * dx;
    float yy = dy * dy;
    return xx + yy <= 36.0f;
  }
  auto ra = std::get_if<OnRouteLocation>(&a);
  auto rb = std::get_if<OnRouteLocation>(&b);
  if (!ra || !rb)
    return false;
  auto ea = find_id(c.routes, ra->edge_id), eb = find_id(c.routes, rb->edge_id);
  if (!ea || !eb)
    return false;
  if (!((ea->origin == eb->origin && ea->destination == eb->destination) ||
        (ea->origin == eb->destination && ea->destination == eb->origin)) ||
      ea->line != eb->line)
    return false;
  Integer la = ea->duration_ticks * 100, lb = eb->duration_ticks * 100;
  Integer pa = ea->origin < ea->destination ? ra->progress : la - ra->progress;
  Integer pb = eb->origin < eb->destination ? rb->progress : lb - rb->progress;
  return std::abs(pa * lb - pb * la) < la + lb;
}
Outcome<InteractionContext>
validate_interaction(const CommonRead &c, const InteractionState &s,
                     const InteractionCatalog &cat, const InteractionAccess &a,
                     const StableId &actor, const InteractionContext &ctx,
                     const StableId &service, const StableId &own) {
  if (!a.battle_occupied || !a.resolve_location || !a.role)
    return error<InteractionContext>(fail("UnsupportedNativeCapability",
                                          ErrorKind::unsupported_capability,
                                          "interaction_access"));
  if (a.battle_occupied &&
      (a.battle_occupied(actor) || a.battle_occupied(ctx.provider_actor_id)))
    return error<InteractionContext>(fail("BattlePending"));
  if (ctx.input_error)
    return error<InteractionContext>(fail(*ctx.input_error));
  if (ctx.kind && *ctx.kind == "encounter") {
    if (!service.empty())
      return error<InteractionContext>(fail("RoadServiceDenied"));
    auto e =
        ctx.encounter_id ? find_id(s.encounters, *ctx.encounter_id) : nullptr;
    if (!e || e->status != "pending")
      return error<InteractionContext>(fail("EncounterResolved"));
    if (!ctx.place_id.empty() || actor == ctx.provider_actor_id ||
        !contains(e->participants, actor) ||
        !contains(e->participants, ctx.provider_actor_id))
      return error<InteractionContext>(fail("NotEncounterParticipant"));
    for (std::size_t i = 0; i < 2; ++i) {
      auto present = encounter_side(c, a, *e, i, true);
      if (!present.ok)
        return {false, {}, present.error};
    }
    auto ad = actor_dialogue(c, cat, ctx.provider_actor_id);
    auto d = ad && ad->road_dialogue_id
                 ? find_id(cat.dialogues, *ad->road_dialogue_id)
                 : nullptr;
    if (!d || d->context_role != ctx.role_id)
      return error<InteractionContext>(fail("NoRoadDialogue"));
    auto p = c.actor(actor), provider = c.actor(ctx.provider_actor_id);
    auto available = dialogue_available(c, a, actor, true);
    if (!available.ok)
      return {false, {}, available.error};
    if (!p || !provider || !available.value || !provider->alive)
      return error<InteractionContext>(fail("PersonAbsent"));
    return {true, ctx, {}};
  }
  // 普通人物的叙事上下文不继承任何岗位服务；实际同地、身份和占用仍逐项检查。
  if (ctx.kind && *ctx.kind == "narrative") {
    if (!service.empty() || !ctx.role_id.empty() || ctx.encounter_id)
      return error<InteractionContext>(fail("NarrativeServiceDenied"));
    bool born_bound = false;
    if (a.born_personal_session) {
      for (const auto &current : s.interactions)
        if (current.actor_id == actor &&
            current.context.provider_actor_id == ctx.provider_actor_id &&
            current.context.place_id == ctx.place_id) {
          auto bound = a.born_personal_session(current);
          if (!bound.ok)
            return {false, {}, bound.error};
          born_bound = bound.value;
        }
    }
    if (!born_bound && a.known_personal_session) {
      for (const auto &current : s.interactions)
        if (current.actor_id == actor &&
            current.context.provider_actor_id == ctx.provider_actor_id &&
            current.context.place_id == ctx.place_id) {
          auto bound = a.known_personal_session(current);
          if (!bound.ok)
            return {false, {}, bound.error};
          born_bound = bound.value;
        }
    }
    if (!cat.regional_enabled && !born_bound)
      return error<InteractionContext>(fail("NarrativeServiceDenied"));
    auto p = c.actor(actor), provider = c.actor(ctx.provider_actor_id);
    bool authored = false;
    for (const auto &topic : cat.regional_actor_topics)
      if (topic.actor_id == ctx.provider_actor_id)
        authored = true;
    authored = authored || born_bound;
    if (!p || !provider || !authored || actor == ctx.provider_actor_id)
      return error<InteractionContext>(fail("UnknownInteractionTarget"));
    if (place(a, c, actor) != ctx.place_id)
      return error<InteractionContext>(fail("NotAtPlace"));
    auto actor_available = dialogue_available(c, a, actor, true);
    if (!actor_available.ok)
      return {false, {}, actor_available.error};
    auto provider_available = dialogue_available(c, a, provider->id, true);
    if (!provider_available.ok)
      return {false, {}, provider_available.error};
    if (!actor_available.value || !provider_available.value ||
        place(a, c, provider->id) != ctx.place_id)
      return error<InteractionContext>(fail("PersonAbsent"));
    // 完工只允许这两人仍由同一原行动占用；新接触或其它行动不能借此绕过busy。
    if ((!p->action_id.empty() && p->action_id != own) ||
        (!provider->action_id.empty() && provider->action_id != own))
      return error<InteractionContext>(fail("ActorBusy"));
    return {true, ctx, {}};
  }
  auto p = c.actor(actor), provider = c.actor(ctx.provider_actor_id);
  auto r = role_def(cat, ctx.role_id);
  if (!p || !provider || !r)
    return error<InteractionContext>(fail("UnknownInteractionTarget"));
  if (place(a, c, actor) != ctx.place_id)
    return error<InteractionContext>(fail("NotAtPlace"));
  auto provider_available =
      dialogue_available(c, a, provider->id, service.empty());
  if (!provider_available.ok)
    return {false, {}, provider_available.error};
  if (!provider_available.value || place(a, c, provider->id) != ctx.place_id)
    return error<InteractionContext>(fail("PersonAbsent"));
  auto assignment = a.role ? a.role(ctx.role_id) : std::nullopt;
  if (!assignment || r->place_id != ctx.place_id ||
      assignment->actor_id != provider->id || !assignment->enabled ||
      (!service.empty() && !contains(r->services, service)))
    return error<InteractionContext>(fail("RoleNotAuthorized"));
  if (!service.empty() && a.service_admission) {
    auto allowed = a.service_admission(actor, ctx.role_id, service, own);
    if (!allowed.ok)
      return {false, {}, allowed.error};
  }
  if ((!p->action_id.empty() && p->action_id != own) ||
      (!provider->action_id.empty() && provider->action_id != own))
    return error<InteractionContext>(fail("ActorBusy"));
  auto actor_available = dialogue_available(c, a, actor, service.empty());
  if (!actor_available.ok)
    return {false, {}, actor_available.error};
  if (!actor_available.value || actor == provider->id)
    return error<InteractionContext>(fail("InvalidInteraction"));
  return {true, ctx, {}};
}
Outcome<InteractionQuote>
preview_interaction(const CommonRead &c, const InteractionState &s,
                    const InteractionCatalog &cat, const InteractionAccess &a,
                    const StableId &actor, const InteractionCommand &command) {
  if (is_interaction_extra(command))
    return preview_interaction_extra(c, s, cat, a, actor, command);
  if (std::holds_alternative<EndInteraction>(command))
    return {true, {}, {}};
  if (auto talk = std::get_if<Talk>(&command))
    return talk->topic_id.empty()
               ? can_begin(c, s, cat, a, actor, talk->role_id)
               : preview_regional_talk(c, s, cat, a, actor, talk->topic_id, {},
                                       talk->role_id);
  if (auto talk = std::get_if<TalkActor>(&command))
    return preview_regional_talk(c, s, cat, a, actor, talk->topic_id,
                                 talk->actor_id, {});
  if (auto event = std::get_if<OpenEvent>(&command)) {
    auto d = find_id(cat.events, event->event_id);
    if (!d)
      return error<InteractionQuote>(fail("UnknownEvent"));
    if (d->on_unavailable != "cancel")
      return error<InteractionQuote>(fail("UnsupportedEventPolicy"));
    const EventRecord *rec = nullptr;
    for (auto &v : s.events)
      if (v.key == actor + "|" + d->id)
        rec = &v;
    if (rec && rec->status == "cancelled")
      return error<InteractionQuote>(fail("EventCancelled"));
    auto p = c.actor(actor);
    auto loc = p ? std::get_if<SettlementLocation>(&p->location) : nullptr;
    if (!rec || rec->status != "queued" || !loc || loc->place_id != d->place_id)
      return error<InteractionQuote>(fail("EventNotHere"));
    auto cond = all_conditions(c, s, a, actor, d->conditions);
    if (!cond.ok)
      return {false, {}, cond.error};
    if (!cond.value)
      return error<InteractionQuote>(fail("EventUnavailable"));
    auto dialogue = find_id(cat.dialogues, d->dialogue_id);
    if (!dialogue)
      return error<InteractionQuote>(fail("InvalidSceneEvent"));
    auto q = can_begin(c, s, cat, a, actor, dialogue->context_role);
    if (q.ok && q.value.dialogue_id != d->dialogue_id)
      return error<InteractionQuote>(fail("EventParticipantChanged"));
    if (q.ok)
      q.value.event_id = d->id;
    return q;
  }
  const StableId *encounter = nullptr;
  if (auto x = std::get_if<EncounterTalk>(&command))
    encounter = &x->encounter_id;
  if (auto x = std::get_if<EncounterLeave>(&command))
    encounter = &x->encounter_id;
  if (encounter) {
    auto e = find_id(s.encounters, *encounter);
    if (!e)
      return error<InteractionQuote>(fail("UnknownEncounter"));
    if (e->status != "pending")
      return error<InteractionQuote>(fail("EncounterResolved"));
    auto p = c.actor(actor);
    const bool dialogue = std::holds_alternative<EncounterTalk>(command);
    auto available = dialogue_available(c, a, actor, dialogue);
    if (!available.ok)
      return {false, {}, available.error};
    if (!contains(e->participants, actor) || !p || !available.value)
      return error<InteractionQuote>(fail("NotEncounterParticipant"));
    for (std::size_t i = 0; i < 2; ++i) {
      auto present = encounter_side(c, a, *e, i, dialogue);
      if (!present.ok)
        return {false, {}, present.error};
    }
    InteractionQuote q;
    q.encounter_id = *encounter;
    if (std::holds_alternative<EncounterLeave>(command))
      return {true, q, {}};
    if (session(s, actor))
      return error<InteractionQuote>(fail("AlreadyTalking"));
    auto provider = actor == e->leaders[0] ? e->leaders[1] : e->leaders[0];
    auto ad = actor_dialogue(c, cat, provider);
    auto d = ad && ad->road_dialogue_id
                 ? find_id(cat.dialogues, *ad->road_dialogue_id)
                 : nullptr;
    if (!d)
      return error<InteractionQuote>(fail("NoRoadDialogue"));
    q.dialogue_id = d->id;
    q.context = InteractionContext{"", provider, d->context_role,
                                   StableId("encounter"), *encounter};
    return {true, q, {}};
  }
  auto se = session(s, actor);
  if (!se)
    return error<InteractionQuote>(fail("NoInteraction"));
  auto valid = validate_interaction(c, s, cat, a, actor, se->context);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (se->regional_binding && !se->regional_action_committed) {
    auto source = validate_regional_binding(c, cat, a, *se->regional_binding);
    if (!source.ok)
      return {false, {}, source.error};
  }
  auto n = node(cat, *se);
  if (!n)
    return error<InteractionQuote>(fail("DialogueNodeMissing"));
  InteractionQuote q;
  if (std::holds_alternative<DialogueNext>(command) && se->phase == "say") {
    q.next = n->next;
    return {true, q, {}};
  }
  if (auto choose = std::get_if<DialogueChoose>(&command);
      choose && se->phase == "choice")
    for (auto &o : n->options)
      if (o.id == choose->option_id) {
        auto cond = all_conditions(c, s, a, actor, o.conditions);
        if (!cond.ok)
          return {false, {}, cond.error};
        if (cond.value) {
          q.next = o.next;
          return {true, q, {}};
        }
      }
  return error<InteractionQuote>(fail("InvalidDialogueInput"));
}
Outcome<InteractionEffect>
apply_interaction(CommonWrite &c, InteractionState &s,
                  const InteractionCatalog &cat, const InteractionAccess &a,
                  const StableId &actor, const InteractionCommand &command,
                  const InteractionQuote &q, const WorldMoment *exact_at,
                  InteractionSelectedResponsePending *response_pending) {
  if (cat.response_sources_version)
    if (const auto *chosen = std::get_if<DialogueChoose>(&command)) {
      if (!response_pending || !exact_at)
        return error<InteractionEffect>(
            fail("InteractionOriginalResponseCaptureRequired",
                 ErrorKind::unsupported_capability));
      auto *store = dynamic_cast<CommonStore *>(&c);
      if (!store)
        return error<InteractionEffect>(
            fail("InteractionResponseCommonStoreRequired",
                 ErrorKind::unsupported_capability));
      // 复用原准入，不能用调用者传来的 next 跳到另一个作者节点。
      auto fresh = preview_interaction(c, s, cat, a, actor, command);
      if (!fresh.ok)
        return {false, {}, fresh.error};
      if (fresh.value.next != q.next)
        return error<InteractionEffect>(
            fail("StaleInteractionSelectedResponse"));
      auto *current = session(s, actor);
      if (!current || current->phase != "choice")
        return error<InteractionEffect>(
            fail("InteractionOriginalChoiceRequired"));
      auto prepared = prepare_interaction_selected_response_in_candidate(
          *store, s, cat, actor, current->id, current->node_id,
          chosen->option_id, q.next, *exact_at, *response_pending);
      if (!prepared.ok)
        return {false, {}, prepared.error};
      current->node_id = q.next;
      current->phase = "stepping";
      current->pending_command.reset();
      return {true, {}, {}};
    }
  if (is_interaction_extra(command)) {
    auto store = dynamic_cast<CommonStore *>(&c);
    if (!store)
      return error<InteractionEffect>(
          fail("InteractionCoordinationUnavailable"));
    return apply_interaction_extra(*store, s, cat, a, actor, command, q);
  }
  if (std::holds_alternative<EndInteraction>(command)) {
    auto before = capture_actor_social_occupancy(c, s, a, actor);
    if (!before.ok)
      return {false, {}, before.error};
    const auto *current = session(s, actor);
    const StableId original = current ? current->id : StableId{};
    s.interactions.erase(
        std::remove_if(s.interactions.begin(), s.interactions.end(),
                       [&](auto &v) { return v.actor_id == actor; }),
        s.interactions.end());
    if (!original.empty()) {
      auto recorded = record_captured_actor_social_occupancy(
          a, actor, before.value, ActorSocialOccupancyCause::end_interaction,
          original, actor);
      if (!recorded.ok)
        return {false, {}, recorded.error};
    }
    return {true, {}, {}};
  }
  if (std::holds_alternative<EncounterLeave>(command)) {
    auto e = find_id(s.encounters, q.encounter_id);
    if (!e)
      return error<InteractionEffect>(fail("UnknownEncounter"));
    if (!a.set_journey_encounter)
      return error<InteractionEffect>(fail("UnsupportedNativeCapability",
                                           ErrorKind::unsupported_capability,
                                           "encounter_leave"));
    for (std::size_t i = 0; i < 2; ++i) {
      auto present = encounter_side(c, a, *e, i);
      if (!present.ok)
        return {false, {}, present.error};
      if (present.value)
        continue;
      auto r =
          a.set_journey_encounter(e->action_ids[i], "", e->resume_paused[i]);
      if (!r.ok)
        return {false, {}, r.error};
    }
    for (std::size_t n = s.interactions.size(); n > 0; --n) {
      const auto &v = s.interactions[n - 1];
      if (!v.context.encounter_id || *v.context.encounter_id != e->id)
        continue;
      const auto closing_actor = v.actor_id, closing_session = v.id;
      auto before = capture_actor_social_occupancy(c, s, a, closing_actor);
      if (!before.ok)
        return {false, {}, before.error};
      s.interactions.erase(s.interactions.begin() +
                           static_cast<std::ptrdiff_t>(n - 1));
      auto recorded = record_captured_actor_social_occupancy(
          a, closing_actor, before.value,
          ActorSocialOccupancyCause::encounter_leave, closing_session, actor);
      if (!recorded.ok)
        return {false, {}, recorded.error};
    }
    if (e->exact_origin) {
      if (!exact_at)
        return error<InteractionEffect>(
            fail("ExactEncounterResolutionMomentRequired"));
      auto order =
          compare_world_moments(e->exact_origin->created_at, *exact_at);
      if (!order.ok)
        return {false, {}, order.error};
      if (order.value > 0)
        return error<InteractionEffect>(
            fail("ExactEncounterResolutionBeforeContact"));
    }
    auto id = c.next_id("result");
    if (!id.ok)
      return {false, {}, id.error};
    e->status = "resolved";
    e->resolved_tick = e->exact_origin ? exact_at->half_day_tick : c.tick();
    e->result_id = id.value;
    if (e->exact_origin) {
      e->resolved_at = *exact_at;
      auto registered =
          c.append_fact({e->result_id, "encounter_resolved", "interaction"});
      if (!registered.ok)
        return {false, {}, registered.error};
    }
    return {true, {e->id, e->result_id}, {}};
  }
  if (std::holds_alternative<EncounterTalk>(command) ||
      std::holds_alternative<Talk>(command) ||
      std::holds_alternative<TalkActor>(command) ||
      std::holds_alternative<OpenEvent>(command)) {
    if (std::holds_alternative<EncounterTalk>(command)) {
      auto e = find_id(s.encounters, q.encounter_id);
      if (!e)
        return error<InteractionEffect>(fail("UnknownEncounter"));
      e->talked = true;
    }
    auto r = begin(c, s, cat, a, actor, q);
    if (!r.ok)
      return {false, {}, r.error};
    if (!q.event_id.empty())
      for (auto &e : s.events)
        if (e.key == actor + "|" + q.event_id) {
          e.status = "shown";
          e.last_tick = c.tick();
        }
  } else {
    auto se = session(s, actor);
    if (!se)
      return error<InteractionEffect>(fail("NoInteraction"));
    se->node_id = q.next;
    se->phase = "stepping";
    se->pending_command.reset();
  }
  auto r = pump_dialogue(c, s, cat, a, actor);
  if (!r.ok)
    return {false, {}, r.error};
  if (s.face_to_face_contacts) {
    if (!exact_at)
      return error<InteractionEffect>(fail("ContactSourceMomentRequired"));
    auto *store = dynamic_cast<CommonStore *>(&c);
    if (!store)
      return error<InteractionEffect>(
          fail("InteractionCoordinationUnavailable"));
    auto contact = publish_face_to_face_contact_in_candidate(*store, s, cat, a,
                                                             actor, *exact_at);
    if (!contact.ok)
      return {false, {}, contact.error};
  }
  if (q.regional_binding) {
    auto exposed = expose_regional_binding(c, s, cat, a, *q.regional_binding);
    if (!exposed.ok)
      return {false, {}, exposed.error};
  }
  return {true, {}, {}};
}
// 只裁剪无永久货物来源引用的resolved；同tick比较字符串ID，保留被实际合同引用的原始锚点。
Outcome<StableId>
install_encounter(CommonWrite &c, InteractionState &s, const StableId &player,
                  const EncounterInstall &hit,
                  const std::vector<StableId> &retained_encounter_ids,
                  const ExactEncounterOrigin *exact_origin) {
  if (!hit.apply_motion)
    return error<StableId>(fail("UnsupportedNativeCapability",
                                ErrorKind::unsupported_capability,
                                "contact_install"));
  auto id = c.next_id("encounter");
  if (!id.ok)
    return id;
  EncounterRecord e;
  e.id = id.value;
  e.leaders = hit.leaders;
  e.anchors = hit.anchors;
  e.action_ids = hit.action_ids;
  e.resume_paused = hit.resume_paused;
  // 命中已携带原构造次序；沿用现有codec元数据，不以共同位置默认序覆盖道路锚点。
  e.anchor_field_orders = hit.anchor_field_orders;
  e.anchors_order = hit.anchors_order;
  e.action_ids_order = hit.action_ids_order;
  e.resume_paused_order = hit.resume_paused_order;
  for (auto &leader : hit.leaders) {
    auto people = c.members(leader);
    if (!people)
      return error<StableId>(fail("UnknownEncounterActor"));
    e.participants.insert(e.participants.end(), people->begin(), people->end());
  }
  bool interactive = contains(e.participants, player);
  if (exact_origin) {
    e.exact_origin = *exact_origin;
    e.created_tick = exact_origin->created_at.half_day_tick;
    if (!interactive)
      e.resolved_at = exact_origin->created_at;
  } else
    e.created_tick = c.tick() + 1;
  e.resolved_tick = interactive ? -1 : e.created_tick;
  e.status = interactive ? "pending" : "resolved";
  if (!interactive) {
    auto result = c.next_id("result");
    if (!result.ok)
      return {false, {}, result.error};
    e.result_id = result.value;
  }
  auto applied = hit.apply_motion(e.id, interactive);
  if (!applied.ok)
    return {false, {}, applied.error};
  s.encounters.push_back(e);
  std::vector<std::pair<Integer, StableId>> resolved;
  for (auto &v : s.encounters)
    if (v.status == "resolved" && !v.exact_origin &&
        !contains(retained_encounter_ids, v.id)) {
      bool original_session = false;
      if (s.social_occupancy && s.social_occupancy->version == 3)
        for (const auto &source : s.social_occupancy->transitions.rows())
          if (source.second.session_opening &&
              source.second.session_opening->context.encounter_id ==
                  std::optional<StableId>{v.id}) {
            original_session = true;
            break;
          }
      if (!original_session)
        resolved.emplace_back(v.created_tick, v.id);
    }
  std::sort(resolved.begin(), resolved.end());
  if (resolved.size() > 128) {
    std::set<StableId> erase;
    for (std::size_t i = 0; i < resolved.size() - 128; ++i)
      erase.insert(resolved[i].second);
    s.encounters.erase(
        std::remove_if(s.encounters.begin(), s.encounters.end(),
                       [&](auto &v) { return erase.count(v.id); }),
        s.encounters.end());
  }
  return id;
}
Status pump_dialogue_node(CommonWrite &c, InteractionState &s,
                          const InteractionCatalog &cat,
                          const InteractionAccess &a, const StableId &actor) {
  auto se = session(s, actor);
  if (!se)
    return fail("NoInteraction");
  if (se->phase == "waiting" || se->phase == "trade")
    return common_success();
  auto valid = validate_interaction(c, s, cat, a, actor, se->context);
  if (!valid.ok)
    return {false, {}, valid.error};
  for (int i = 0; i < 64; ++i) {
    auto n = node(cat, *se);
    if (!n)
      return fail("DialogueNodeMissing");
    if (n->kind == "branch") {
      if (!n->condition)
        return fail("DialogueNodeMissing");
      auto r = evaluate_condition(c, s, a, actor, *n->condition);
      if (!r.ok)
        return {false, {}, r.error};
      se->node_id = r.value ? n->if_true : n->if_false;
      continue;
    }
    if (n->kind == "say") {
      auto speaker = n->speaker == "$provider"
                         ? se->context.provider_actor_id
                         : (n->speaker == "$player" ? actor : n->speaker);
      auto p = c.actor(speaker);
      bool present = p && p->alive;
      if (se->context.kind && *se->context.kind == "encounter") {
        auto e = se->context.encounter_id
                     ? find_id(s.encounters, *se->context.encounter_id)
                     : nullptr;
        present = present && e && e->status == "pending" &&
                  contains(e->participants, speaker);
      } else
        present = present && place(a, c, speaker) == se->context.place_id;
      if (!present)
        return fail("SpeakerAbsent");
      se->phase = "say";
      return common_success();
    }
    if (n->kind == "choice") {
      se->phase = "choice";
      return common_success();
    }
    if (n->kind == "action") {
      if (!n->command)
        return fail("DialogueNodeMissing");
      se->phase = "action";
      se->pending_command = n->command;
      return common_success();
    }
    if (n->kind == "end") {
      auto before = capture_actor_social_occupancy(c, s, a, actor);
      if (!before.ok)
        return {false, {}, before.error};
      const auto original = se->id;
      s.interactions.erase(
          std::remove_if(s.interactions.begin(), s.interactions.end(),
                         [&](auto &v) { return v.actor_id == actor; }),
          s.interactions.end());
      return record_captured_actor_social_occupancy(
          a, actor, before.value, ActorSocialOccupancyCause::dialogue_end,
          original, actor);
    }
    return fail("UnsupportedDialogueNode");
  }
  return fail("DialogueStepLimit");
}
Status enter_place_events(const CommonRead &c, InteractionState &s,
                          const InteractionCatalog &cat,
                          const InteractionAccess &a, const StableId &actor) {
  auto p = c.actor(actor);
  if (!p)
    return fail("UnknownActor");
  auto loc = std::get_if<SettlementLocation>(&p->location);
  StableId here = loc ? loc->place_id : "";
  std::vector<const EventDefinition *> ordered;
  for (auto &v : cat.events)
    ordered.push_back(&v);
  std::sort(ordered.begin(), ordered.end(),
            [](auto x, auto y) { return x->id < y->id; });
  for (auto d : ordered) {
    if (d->trigger != "on_enter")
      continue;
    if (d->on_unavailable != "cancel")
      return fail("UnsupportedEventPolicy");
    if (d->place_id != here)
      continue;
    auto cond = all_conditions(c, s, a, actor, d->conditions);
    if (!cond.ok)
      return {false, {}, cond.error};
    if (!cond.value)
      continue;
    StableId key = actor + "|" + d->id;
    EventRecord *old = nullptr;
    for (auto &e : s.events)
      if (e.key == key)
        old = &e;
    if (old && (old->status == "queued" || d->once_per_actor ||
                c.tick() - old->last_tick < d->cooldown_ticks))
      continue;
    auto dialogue = find_id(cat.dialogues, d->dialogue_id);
    if (!dialogue)
      return fail("InvalidSceneEvent");
    auto q = can_begin(c, s, cat, a, actor, dialogue->context_role);
    if (!q.ok) {
      if (q.error.kind != ErrorKind::rule)
        return {false, {}, q.error};
      continue;
    }
    if (q.value.dialogue_id != d->dialogue_id)
      continue;
    EventRecord e{key, d->id, actor, here, "queued", c.tick()};
    if (old)
      *old = e;
    else
      s.events.push_back(e);
  }
  return common_success();
}
Status reconcile_events(const CommonRead &c, InteractionState &s,
                        const InteractionCatalog &cat,
                        const InteractionAccess &a) {
  std::vector<EventRecord *> ordered;
  for (auto &e : s.events)
    ordered.push_back(&e);
  std::sort(ordered.begin(), ordered.end(),
            [](auto x, auto y) { return x->key < y->key; });
  for (auto e : ordered) {
    if (e->status != "queued")
      continue;
    auto d = find_id(cat.events, e->event_id);
    if (!d)
      return fail("InvalidSceneEvent");
    if (d->on_unavailable != "cancel")
      return fail("UnsupportedEventPolicy");
    auto available = event_available(c, s, cat, a, e->actor_id, *d);
    if (!available.ok)
      return {false, {}, available.error};
    if (!available.value) {
      e->status = "cancelled";
      e->last_tick = c.tick();
    }
  }
  return common_success();
}
} // namespace hanmo::resident
