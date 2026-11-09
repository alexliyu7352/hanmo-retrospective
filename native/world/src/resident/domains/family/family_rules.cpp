#include "resident/storage/money_transfer_recipe.hpp"
#include "family_origin_v2.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "voluntary_succession.hpp"
#include "voluntary_family_readiness.hpp"
#include "family_legacy.hpp"
#include "adult_adoption.hpp"
// 原家事规则在唯一候选中运行；年龄、收养、退休和私产交接均不增加新的世界模拟机制。
#include "family_codec.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <tuple>
#include <picosha2.h>
namespace hanmo::resident {
namespace {
constexpr Integer maximum = 2147483647, year = 720;
// 规范实例号必须已由共同计数器分配，不接受前导零和未来来源。
Integer number(const StableId &id, const StableId &prefix, Integer next) {
  if (id.rfind(prefix + ":", 0) != 0)
    return -1;
  auto s = id.substr(prefix.size() + 1);
  Integer n = 0;
  auto r = std::from_chars(s.data(), s.data() + s.size(), n);
  return r.ec == std::errc{} && r.ptr == s.data() + s.size() && n > 0 &&
                 n < next && std::to_string(n) == s
             ? n
             : -1;
}
// UTF-8姓名按字符计上限，禁止控制字符和两端ASCII空白；不把中文字符计作三个名字字符。
bool valid_name(const std::string &name) {
  if (name.empty() || static_cast<unsigned char>(name.front()) <= 32 ||
      static_cast<unsigned char>(name.back()) <= 32)
    return false;
  std::size_t count = 0;
  for (std::size_t i = 0; i < name.size();) {
    const auto lead = static_cast<unsigned char>(name[i++]);
    std::uint32_t scalar = 0;
    unsigned trailing = 0;
    if (lead < 0x80)
      scalar = lead;
    else if (lead >= 0xc2 && lead <= 0xdf) {
      scalar = lead & 0x1f;
      trailing = 1;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      scalar = lead & 0x0f;
      trailing = 2;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      scalar = lead & 0x07;
      trailing = 3;
    } else
      return false;
    if (name.size() - i < trailing)
      return false;
    for (unsigned n = 0; n < trailing; ++n) {
      const auto byte = static_cast<unsigned char>(name[i++]);
      if ((byte & 0xc0) != 0x80)
        return false;
      scalar = (scalar << 6) | (byte & 0x3f);
    }
    if ((trailing == 1 && scalar < 0x80) || (trailing == 2 && scalar < 0x800) ||
        (trailing == 3 && scalar < 0x10000) || scalar > 0x10ffff ||
        (scalar >= 0xd800 && scalar <= 0xdfff))
      return false;
    if (scalar < 32 || scalar == 127 || ++count > 80)
      return false;
  }
  return true;
}
Owner actor_owner(const StableId &id) { return {Owner::Kind::actor, id, {}}; }
// 对某个历史半日算周岁，不从人物原型猜测缺失生日。
Integer age(const CommonStore &c, const StableId &id, Integer tick) {
  auto *a = c.state().actors.find(id);
  if (!a || !a->birth_tick || *a->birth_tick < -maximum ||
      *a->birth_tick > tick)
    return -1;
  return (tick - *a->birth_tick) / year;
}
template <class T> const T *fact(const FamilyState &s, const StableId &id) {
  auto *f = s.facts.find(id);
  return f ? std::get_if<T>(f) : nullptr;
}
template <class T> T *fact(FamilyState &s, const StableId &id) {
  auto *f = s.facts.find(id);
  return f ? std::get_if<T>(f) : nullptr;
}
// 上下文仅比原约束的地点/提供人/岗位，路遇不能获得室内事务权限。
bool context_matches(const InteractionContext &c, const AdoptionTerms &t) {
  return c.kind != StableId("encounter") &&
         c.provider_actor_id == t.provider_actor_id &&
         c.place_id == t.place_id && c.role_id == t.role_id;
}
// 当时庄园所有权沿永久继任记录计算，后续移交不得反向改变收款合法性。
Owner historical_wallet_owner(const CommonStore &c, const FamilyState &s,
                              const EstateCatalog &estates,
                              const StableId &wallet, Integer tick) {
  for (const auto &row : estates.estates.rows())
    if (row.second.wallet_id == wallet) {
      auto owner = row.second.owner_actor_id;
      for (const auto &id : s.successions) {
        auto *f = fact<FamilyChoice>(s, id);
        if (!f || f->tick > tick || !f->assets)
          continue;
        for (const auto &e : f->assets->estates)
          if (e.estate_id == row.first)
            owner = f->heir_actor_id;
      }
      return actor_owner(owner);
    }
  auto *w = c.wallet(wallet);
  return w ? w->owner : Owner{};
}
Status validate_terms(const CommonStore &c, const FamilyState &s,
                      const FamilyCatalog &catalog, const CommonCatalog &common,
                      const EstateCatalog &estates, const AdoptionTerms &t,
                      Integer start, bool historical) {
  if (start < 0 || start > c.tick())
    return common_failure("InvalidFamilyTerms");
  auto *guardian = c.state().actors.find(t.guardian_actor_id);
  auto *provider = c.actor(t.provider_actor_id);
  auto *role = common.roles.find(t.role_id);
  if (!guardian || !provider || t.guardian_actor_id == t.provider_actor_id ||
      !role || !common.places.find(t.place_id))
    return common_failure("InvalidFamilyReference");
  auto *config = catalog.roles.find(t.role_id);
  if (!config || !config->valid ||
      std::find(role->services.begin(), role->services.end(), "family_adopt") ==
          role->services.end() ||
      role->place_id != t.place_id)
    return common_failure("FamilyServiceNotOffered");
  if (t.policy.enable_travel_plan &&
      (!*t.policy.enable_travel_plan ||
       !config->policy.enable_travel_plan.value_or(false)))
    return common_failure("InvalidFamilyPolicy");
  if (t.policy.child_age_years != config->policy.child_age_years ||
      t.policy.guardian_min_age != config->policy.guardian_min_age ||
      t.policy.heir_min_age != config->policy.heir_min_age ||
      t.policy.retirement_min_age != config->policy.retirement_min_age)
    return common_failure("FamilyPolicyMismatch");
  if (t.prototype_id != config->prototype_id ||
      t.duration != config->adoption_ticks || t.fee != config->fee ||
      t.recipient_wallet_id != config->wallet_id ||
      t.birth_tick !=
          start + t.duration - config->policy.child_age_years * year ||
      !valid_name(t.display_name))
    return common_failure("FamilyTermsMismatch");
  const auto *prototype = catalog.prototypes.find(t.prototype_id);
  if (!prototype)
    return common_failure("UnknownActorPrototype");
  auto skills = family_prototype_skill_eligibility(catalog, *prototype);
  if (!skills.ok)
    return skills;
  if (start > maximum - t.duration)
    return common_failure("FamilyClockOverflow");
  if (age(c, t.guardian_actor_id, start) < config->policy.guardian_min_age ||
      !guardian->birth_tick || t.birth_tick - *guardian->birth_tick < 16 * year)
    return common_failure("FamilyGuardianAgeRequired");
  auto expected =
      role->organization_id
          ? Owner{Owner::Kind::organization, *role->organization_id, {}}
          : actor_owner(t.provider_actor_id);
  auto *payer = c.wallet(t.payer_wallet_id);
  auto paid = historical ? historical_wallet_owner(c, s, estates,
                                                   t.recipient_wallet_id, start)
                         : (c.wallet(t.recipient_wallet_id)
                                ? c.wallet(t.recipient_wallet_id)->owner
                                : Owner{});
  if (t.payer_wallet_id != guardian->wallet_id ||
      t.payer_wallet_id == t.recipient_wallet_id || !payer ||
      !owner_equal(payer->owner, actor_owner(guardian->id)) ||
      !owner_equal(paid, expected))
    return common_failure("FamilyPaymentAuthority");
  return common_success();
}
// 继任资格只来自该监护人实际完成的收养，不接受姓名、原型或别人的家庭来源。
Status eligible(const CommonStore &c, const FamilyState &s,
                const StableId &guardian, const StableId &heir, Integer tick,
                bool retire) {
  auto *source = s.children.find(heir);
  auto *f = source ? fact<AdoptionFact>(s, *source) : nullptr;
  if (source)
    if (auto *adult = fact<AdultAdoptionFact>(s, *source);
        adult && adult->agreement.adopter_actor_id == guardian &&
        adult->agreement.adoptee_actor_id == heir)
      return common_failure("FamilySuccessionConsentRequired", heir);
  if (guardian == heir || !c.actor(guardian) || !c.actor(heir) || !f ||
      f->kind != "family_adoption" || f->guardian_actor_id != guardian ||
      f->completed_tick > tick)
    return common_failure("FamilyNotOwnChild");
  if (age(c, heir, tick) < f->terms.policy.heir_min_age)
    return common_failure("FamilyHeirNotAdult");
  if (retire && age(c, guardian, tick) < f->terms.policy.retirement_min_age)
    return common_failure("FamilyRetirementAgeRequired");
  return common_success();
}
Status ready(const CommonStore &c, const EstateState &estates,
             const FamilyAccess &access, const StableId &actor,
             const StableId &own, const StableId &contact, bool retire) {
  auto *a = c.state().actors.find(actor);
  if (!a || !a->alive || !a->available || a->hp <= 0 || a->injury >= 3)
    return common_failure("ActorUnavailable");
  if (!a->action_id.empty() && a->action_id != own)
    return common_failure("ActorBusy");
  for (const auto &row : c.state().parties.rows())
    if (std::find(row.second.members.begin(), row.second.members.end(),
                  actor) != row.second.members.end())
      return common_failure("FamilyPartyPending");
  if (!access.interaction || !access.interaction_access || !access.tasks ||
      !access.military_or_office_pending)
    return common_failure("UnsupportedNativeCapability", "family_access",
                          ErrorKind::unsupported_capability);
  if (access.interaction_access->battle_occupied &&
      access.interaction_access->battle_occupied(actor))
    return common_failure("BattlePending");
  if (retire)
    for (const auto &row : access.tasks->tasks.rows())
      if (row.second.assignee == actor && row.second.status != "success" &&
          row.second.status != "failed" && row.second.status != "cancelled")
        return common_failure("FamilyContractPending");
  for (const auto &session : access.interaction->interactions)
    if ((session.actor_id == actor &&
         session.context.provider_actor_id != contact) ||
        (session.context.provider_actor_id == actor &&
         session.actor_id != contact))
      return common_failure("FamilyConversationPending");
  for (const auto &row : estates.estates.rows())
    if (row.second.detached.find(actor))
      return common_failure("FamilyMilitaryPending");
  if (access.military_or_office_pending(actor, retire))
    return common_failure("FamilyMilitaryOrOfficePending");
  return common_success();
}
bool retired(const FamilyState &s, const StableId &actor) {
  for (const auto &id : s.successions) {
    auto step = family_succession_step(s, id);
    if (step.ok && step.value.predecessor_actor_id == actor)
      return true;
  }
  return false;
}
// 排序仅用于原GD明确排序的私产计划；不改变共同资产表的插入顺序。
template <class T> std::vector<StableId> sorted(const OrderedTable<T> &t) {
  auto ids = t.order();
  std::sort(ids.begin(), ids.end());
  return ids;
}
Outcome<SuccessionAssets>
asset_plan(const CommonStore &c, const EstateState &estates,
           const EstateCatalog &ec, const CommonCatalog &common,
           const StableId &guardian, const StableId &heir) {
  auto owner = actor_owner(guardian), receiver = actor_owner(heir);
  auto *old = c.actor(guardian);
  auto *next = c.actor(heir);
  if (!c.wallet(next->wallet_id)->reserved.empty())
    return {false, {}, common_failure("FamilyAssetsReserved").error};
  SuccessionAssets plan;
  std::set<StableId> stores, cashboxes;
  for (const auto &id : sorted(estates.estates)) {
    auto &e = *estates.estates.find(id);
    if (e.owner_actor_id != guardian)
      continue;
    if (!e.detached.empty())
      return {false, {}, common_failure("FamilyEstateRetainersAway").error};
    if (estate_construction_pending(estates, id))
      return {
          false, {}, common_failure("FamilyEstateConstructionPending").error};
    auto *d = ec.estates.find(id);
    if (!d)
      return {false, {}, common_failure("UnknownEstate").error};
    stores.insert(d->inventory_id);
    cashboxes.insert(d->wallet_id);
    plan.estates.push_back({id, d->inventory_id, d->wallet_id, {}});
  }
  Integer total = 0;
  for (const auto &id : sorted(c.state().wallets)) {
    auto *w = c.wallet(id);
    if (!owner_equal(w->owner, owner))
      continue;
    if (!w->reserved.empty())
      return {false, {}, common_failure("FamilyAssetsReserved").error};
    if (cashboxes.count(id))
      continue;
    total += w->balance;
    plan.wallets.push_back({id, next->wallet_id, w->balance, {}});
  }
  if (c.wallet(next->wallet_id)->balance > maximum - total)
    return {false, {}, common_failure("MoneyOverflow").error};
  auto weight = c.inventory_weight(next->inventory_id);
  if (!weight.ok)
    return {false, {}, weight.error};
  Integer load = weight.value;
  for (const auto &id : sorted(c.state().inventories)) {
    auto *inv = c.inventory(id);
    for (const auto &lot : inv->lots) {
      if ((id == old->inventory_id && !owner_equal(lot.owner, owner)) ||
          (id == next->inventory_id && !owner_equal(lot.owner, receiver)))
        return {false, {}, common_failure("FamilyForeignCargo").error};
      if (id == next->inventory_id && !lot.reserved_by.empty())
        return {false, {}, common_failure("FamilyAssetsReserved").error};
      if (!owner_equal(lot.owner, owner))
        continue;
      if (!lot.reserved_by.empty())
        return {false, {}, common_failure("FamilyAssetsReserved").error};
      if (id != old->inventory_id && !stores.count(id))
        return {
            false, {}, common_failure("FamilyPrivateAssetsElsewhere").error};
      auto destination = id == old->inventory_id ? next->inventory_id : id;
      if (id == old->inventory_id)
        load += lot.quantity * common.items.find(lot.item_id)->weight;
      plan.goods.push_back(
          {id, destination, lot.id, lot.item_id, lot.quantity, {}});
    }
  }
  if (load > c.inventory(next->inventory_id)->capacity)
    return {false, {}, common_failure("FamilyHeirCapacityExceeded").error};
  for (const auto &id : sorted(c.state().roles)) {
    auto *role = c.state().roles.find(id);
    if (role->actor_id == guardian && role->enabled)
      plan.disabled_roles.push_back(id);
  }
  return {true, std::move(plan), {}};
}
// 新人物外貌沿原SHA256紧凑字符串数组协议；不推进world/event/battle随机流。
Outcome<std::shared_ptr<const AppearanceProfile>>
make_appearance(const CommonStore &c, const IdentityPrototype &p,
                const StableId &actor) {
  auto a = checkpoint_make_shared<AppearanceProfile>();
  a->generation_seed = "not-sampled";
  if (p.appearance_kind.empty()) {
    a->generator_version = "legacy-portrait-v1";
    a->attire_style = "legacy";
    a->source = "legacy";
    return {true, std::move(a), {}};
  }
  a->generator_version = "appearance-v1";
  a->attire_style = p.attire_style;
  a->base_features.present = true;
  if (p.appearance_kind == "fixed") {
    a->source = "fixed";
    a->base_features = p.fixed_features;
    return {true, std::move(a), {}};
  }
  auto *stream = c.state().rng.find("world");
  if (!stream)
    return {false, {}, common_failure("InvalidAppearanceSeed").error};
  a->generation_seed = std::to_string(static_cast<std::uint64_t>(stream->seed));
  a->source = "generated";
  for (auto entry : {std::pair<const char *, StableId *>{
                         "face_shape", &a->base_features.face_shape},
                     {"eyes", &a->base_features.eyes},
                     {"brows", &a->base_features.brows},
                     {"nose", &a->base_features.nose},
                     {"mouth", &a->base_features.mouth},
                     {"hair", &a->base_features.hair},
                     {"beard", &a->base_features.beard},
                     {"skin_palette", &a->base_features.skin_palette},
                     {"build", &a->base_features.build}}) {
    auto *choices = p.candidates.find(entry.first);
    if (!choices || choices->empty())
      return {false, {}, common_failure("InvalidAppearanceDefinition").error};
    auto values = *choices;
    std::sort(values.begin(), values.end());
    auto input = "[\"appearance-v1\",\"" + a->generation_seed + "\",\"" +
                 actor + "\",\"" + entry.first + "\"]";
    auto digest = picosha2::hash256_hex_string(input);
    std::uint32_t index = 0;
    auto r = std::from_chars(digest.data(), digest.data() + 8, index, 16);
    if (r.ec != std::errc{})
      return {false, {}, common_failure("InvalidAppearanceSeed").error};
    *entry.second = values[index % values.size()];
  }
  return {true, std::move(a), {}};
}
// 身份实体只通过CommonStore及资源recipe创建；knowledge和social在同候选建立空记录。
Outcome<StableId> instantiate(CommonStore &c, FamilyState &s,
                              InteractionState &interaction,
                              SocialState &social, const FamilyCatalog &catalog,
                              const CommonCatalog &common,
                              const AdoptionTerms &terms,
                              const StableId &source) {
  auto *prototype = catalog.prototypes.find(terms.prototype_id);
  if (!prototype)
    return {false, {}, common_failure("UnknownActorPrototype").error};
  auto qualified = family_prototype_skill_eligibility(catalog, *prototype);
  if (!qualified.ok)
    return {false, {}, qualified.error};
  if (!valid_name(terms.display_name))
    return {false, {}, common_failure("InvalidActorDisplayName").error};
  if (terms.birth_tick < -maximum || terms.birth_tick > c.tick())
    return {false, {}, common_failure("InvalidActorBirthTick").error};
  auto *place = common.places.find(terms.place_id);
  if (!place)
    return {false, {}, common_failure("InvalidActorCreationPlace").error};
  auto *adoption = fact<AdoptionFact>(s, source);
  if (!adoption || adoption->kind != "family_adoption")
    return {false, {}, common_failure("InvalidActorCreationSource").error};
  for (const auto &row : s.facts.rows())
    if (auto *f = std::get_if<IdentityCreationFact>(&row.second))
      if (f->source == source) {
        if (f->prototype_id != terms.prototype_id ||
            f->display_name != terms.display_name ||
            f->birth_tick != terms.birth_tick)
          return {
              false, {}, common_failure("ActorCreationSourceConflict").error};
        return {true, row.first, {}};
      }
  if (c.state().next_instance_id >= maximum)
    return {false, {}, common_failure("IdentityCounterExhausted").error};
  auto id = "actor:" + std::to_string(c.state().next_instance_id);
  auto wallet = "personal:" + id, bag = "bag:" + id;
  bool known = false;
  for (const auto &k : interaction.knowledge)
    if (k.actor_id == id)
      known = true;
  if (c.actor(id) || c.state().facts.find(id) || c.wallet(wallet) ||
      c.inventory(bag) || known || c.state().request_sequences.find(id))
    return {false, {}, common_failure("ActorIdentityCollision").error};
  auto appearance = make_appearance(c, *prototype, id);
  if (!appearance.ok)
    return {false, {}, appearance.error};
  auto next = c.next_id("actor");
  if (!next.ok)
    return {false, {}, next.error};
  auto owner = actor_owner(id);
  auto r = c.apply_resources({{CreateWallet{{wallet, owner, 0, {}, {}}},
                               CreateInventory{{bag, owner, 20, 0, {}, {}}}},
                              {}});
  if (!r.ok)
    return {false, {}, r.error};
  ActorRecord actor;
  actor.id = id;
  actor.prototype_id = terms.prototype_id;
  actor.display_name = terms.display_name;
  actor.birth_tick = terms.birth_tick;
  actor.created_tick = c.tick();
  actor.location = SettlementLocation{place->settlement_id, terms.place_id};
  actor.alive = true;
  actor.available = true;
  actor.hp = 100;
  actor.wallet_id = wallet;
  actor.inventory_id = bag;
  actor.attributes = prototype->attributes;
  actor.skills = prototype->skills;
  actor.attributes_order = prototype->attributes_order;
  actor.skills_order = prototype->skills_order;
  actor.appearance = appearance.value;
  actor.field_order = {
      "id",        "prototype_id", "display_name", "birth_tick", "created_tick",
      "location",  "alive",        "available",    "hp",         "fatigue",
      "merit",     "attributes",   "skills",       "wallet_id",  "inventory_id",
      "action_id", "injury",       "combat",       "appearance"};
  r = c.create_actor(actor);
  if (!r.ok)
    return {false, {}, r.error};
  interaction.knowledge.push_back({id, {}});
  social.memories.insert(id, {});
  r = c.append_fact({id, "actor_creation", "family"});
  if (!r.ok)
    return {false, {}, r.error};
  s.facts.insert(id, IdentityCreationFact{terms.prototype_id,
                                          id,
                                          c.tick(),
                                          terms.birth_tick,
                                          terms.display_name,
                                          source,
                                          {}});
  return {true, id, {}};
}
// 行动载荷和永久收养事实必须逐字段一致；完工只放宽remaining_ticks零值。
Status validate_action(const CommonStore &c, const FamilyState &s,
                       const FamilyCatalog &catalog,
                       const CommonCatalog &common,
                       const EstateCatalog &estates,
                       const ActionCoreView &action, bool finishing) {
  auto *p = s.actions.find(action.id);
  auto *f = fact<AdoptionFact>(s, action.id);
  if (action.kind != "family_adopt" ||
      number(action.id, "action", c.state().next_instance_id) < 1 || !p || !f)
    return common_failure("InvalidFamilyAction");
  bool historical =
      f->kind == "family_adoption" || f->kind == "family_adoption_failed";
  auto r = validate_terms(c, s, catalog, common, estates, p->training,
                          action.start_tick, historical);
  if (!r.ok)
    return r;
  if (!family_codec::same_terms(f->terms, p->training) ||
      !context_matches(f->context, p->training) ||
      !context_matches(p->context, p->training) ||
      f->guardian_actor_id != action.actor_id ||
      f->start_tick != action.start_tick ||
      action.participants !=
          std::vector<StableId>{action.actor_id, p->training.provider_actor_id})
    return common_failure("FamilyAdoptionFactMismatch");
  if (historical) {
    if (!finishing ||
        f->completed_tick != action.start_tick + p->training.duration ||
        f->completed_tick > c.tick())
      return common_failure("FamilyAdoptionAlreadyCompleted");
    auto *child = s.children.find(f->child_actor_id);
    if (f->success != (f->kind == "family_adoption") ||
        (f->success && (!child || *child != action.id)))
      return common_failure("FamilyAdoptionFactMismatch");
    return common_success();
  }
  if (f->kind != "family_adoption_pending" || f->success ||
      f->completed_tick != -1 || !f->child_actor_id.empty())
    return common_failure("InvalidFamilyAdoptionFact");
  if (action.remaining_ticks < (finishing ? 0 : 1) ||
      action.remaining_ticks > p->training.duration ||
      action.start_tick + p->training.duration - action.remaining_ticks !=
          c.tick())
    return common_failure("FamilyClockMismatch");
  auto *wallet = c.wallet(p->training.payer_wallet_id);
  Integer reserved = -1;
  if (wallet)
    for (const auto &entry : wallet->reserved)
      if (entry.action_id == action.id)
        reserved = entry.amount;
  if (!wallet || reserved != p->training.fee ||
      wallet->balance < p->training.fee)
    return common_failure("FamilyAdoptionEscrowMismatch");
  if (!finishing)
    for (const auto &person : action.participants)
      if (!c.actor(person) || c.actor(person)->action_id != action.id)
        return common_failure("FamilyOccupationMissing");
  return common_success();
}
} // namespace
// 旧原款与Known新来源共用这一实际准备门；false保原不移公职语义。
Status validate_voluntary_family_parties_ready(const CommonStore &c,
                                               const FamilyState &s,
                                               const EstateState &estates,
                                               const FamilyAccess &access,
                                               const StableId &predecessor,
                                               const StableId &successor) {
  for (const auto &person : {predecessor, successor}) {
    auto valid = ready(c, estates, access, person, {}, {}, false);
    if (!valid.ok)
      return valid;
    if (!access.tasks)
      return common_failure("UnsupportedNativeCapability", "family_tasks",
                            ErrorKind::unsupported_capability);
    for (const auto &row : access.tasks->tasks.rows())
      if (row.second.assignee == person && row.second.status != "success" &&
          row.second.status != "failed" && row.second.status != "cancelled")
        return common_failure("FamilyContractPending", person);
  }
  return retired(s, successor) ? common_failure("FamilyAlreadyRetired")
                               : common_success();
}
Outcome<FamilyQuote>
preview_family(const CommonStore &c, const FamilyState &s,
               const FamilyCatalog &catalog, const CommonCatalog &common,
               const EstateState &estates, const EstateCatalog &ec,
               const FamilyAccess &access, const StableId &actor,
               const FamilyCommand &command, const StableId &own) {
  // 新生前profile只准已接受的实际plan；旧单方designation不能降级成换控权。
  auto routed = validate_voluntary_family_command_route(catalog, command.kind);
  if (!routed.ok)
    return {false, {}, routed.error};
  if (command.kind == "family_agree_succession" ||
      command.kind == "family_cancel_succession" ||
      command.kind == "family_continue_as") {
    if (!catalog.enabled || !catalog.voluntary_succession ||
        !catalog.voluntary_succession->enabled || !access.interaction ||
        !access.interaction_catalog || !access.interaction_access)
      return {false,
              {},
              common_failure("VoluntarySuccessionCapabilityRequired").error};
    auto valid = validate_family(
        c, s, catalog, common, estates, ec, &access.estate_access,
        access.economy, access.interaction_catalog, access.legacy,
        access.biological_identity ? &access.biological_identity : nullptr,
        access.origin_v2, access.initial_age);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (actor != c.state().metadata.player_actor_id)
      return {false, {}, common_failure("NotPlayerActor", actor).error};
    FamilyQuote quote;
    if (command.kind == "family_agree_succession") {
      if (!command.request_id || !command.heir_actor_id)
        return {
            false, {}, common_failure("InvalidSuccessionConsentCommand").error};
      auto consent = preview_succession_consent(
          c, s, *access.interaction, *access.interaction_catalog,
          *access.interaction_access, *catalog.voluntary_succession, actor,
          *command.heir_actor_id, *command.request_id);
      if (!consent.ok)
        return {false, {}, consent.error};
      quote.succession_consent = std::move(consent.value);
      return {true, std::move(quote), {}};
    }
    if (!command.plan_id)
      return {false, {}, common_failure("SuccessionPlanRequired").error};
    const auto *plan = fact<SuccessionConsentFact>(s, *command.plan_id);
    if (!plan)
      return {false,
              {},
              common_failure("UnknownSuccessionPlan", *command.plan_id).error};
    if (plan->agreement.personal.predecessor_actor_id != actor)
      return {false, {}, common_failure("SuccessionPlanAuthorityDenied").error};
    quote.succession_plan_id = *command.plan_id;
    if (command.kind == "family_cancel_succession") {
      if (plan->conclusion && plan->conclusion->kind != "cancelled")
        return {
            false, {}, common_failure("SuccessionPlanAlreadyCompleted").error};
      return {true, std::move(quote), {}};
    }
    // 换控制人物前退出原会话，真实行动/军籍/未结受托责任须先处理；公职和未选资产仍归前代。
    valid = validate_voluntary_family_parties_ready(
        c, s, estates, access, actor,
        plan->agreement.personal.successor_actor_id);
    if (!valid.ok)
      return {false, {}, valid.error};
    auto assets = preview_accepted_succession_assets(
        c, s, common, *access.interaction_catalog,
        *catalog.voluntary_succession, *command.plan_id, access.legacy);
    if (!assets.ok)
      return {false, {}, assets.error};
    quote.succession_personal = std::move(assets.value);
    return {true, std::move(quote), {}};
  }
  if (command.kind == "family_adopt_existing") {
    if (!catalog.enabled || !catalog.adult_adoption ||
        !catalog.adult_adoption->enabled)
      return {
          false, {}, common_failure("AdultAdoptionCapabilityRequired").error};
    if (!access.interaction || !access.interaction_catalog ||
        !access.interaction_access)
      return {false,
              {},
              common_failure("UnsupportedNativeCapability",
                             "adult_adoption_source",
                             ErrorKind::unsupported_capability)
                  .error};
    if (!command.request_id || !command.adoptee_actor_id)
      return {false, {}, common_failure("InvalidAdultAdoptionCommand").error};
    auto valid = validate_family(
        c, s, catalog, common, estates, ec, &access.estate_access,
        access.economy, access.interaction_catalog, access.legacy,
        access.biological_identity ? &access.biological_identity : nullptr,
        access.origin_v2, access.initial_age);
    if (!valid.ok)
      return {false, {}, valid.error};
    auto current = preview_adult_adoption(
        c, s, *access.interaction, *access.interaction_catalog,
        *access.interaction_access, *catalog.adult_adoption, actor,
        *command.adoptee_actor_id, *command.request_id);
    if (!current.ok)
      return {false, {}, current.error};
    FamilyQuote quote;
    quote.adult_adoption = std::move(current.value);
    return {true, std::move(quote), {}};
  }
  if (command.kind != "family_adopt" && command.kind != "family_designate" &&
      command.kind != "family_retire")
    return {false, {}, common_failure("UnknownFamilyCommand").error};
  if (!catalog.enabled)
    return {false, {}, common_failure("FamilyUnavailable").error};
  auto valid = validate_family(
      c, s, catalog, common, estates, ec, &access.estate_access, access.economy,
      access.interaction_catalog, access.legacy,
      access.biological_identity ? &access.biological_identity : nullptr,
      access.origin_v2, access.initial_age);
  if (!valid.ok)
    return {false, {}, valid.error};
  bool retire = command.kind == "family_retire";
  if (retire)
    for (const auto &row : s.facts.rows())
      if (const auto *plan = std::get_if<SuccessionConsentFact>(&row.second);
          plan && !plan->conclusion &&
          plan->agreement.personal.predecessor_actor_id == actor)
        return {
            false,
            {},
            common_failure("SuccessionPlanAlreadyPending", row.first).error};
  auto r =
      ready(c, estates, access, actor, own,
            command.kind == "family_adopt" ? command.context.provider_actor_id
                                           : StableId{},
            retire);
  if (!r.ok)
    return {false, {}, r.error};
  if (retired(s, actor))
    return {false, {}, common_failure("FamilyAlreadyRetired").error};
  FamilyQuote quote;
  if (command.kind == "family_adopt") {
    if (!access.interaction_catalog)
      return {false,
              {},
              common_failure("UnsupportedNativeCapability", "family_context",
                             ErrorKind::unsupported_capability)
                  .error};
    auto checked = validate_interaction(c, *access.interaction,
                                        *access.interaction_catalog,
                                        *access.interaction_access, actor,
                                        command.context, "family_adopt", own);
    if (!checked.ok)
      return {false, {}, checked.error};
    r = ready(c, estates, access, checked.value.provider_actor_id, own, actor,
              false);
    if (!r.ok)
      return {false, {}, r.error};
    auto *config = catalog.roles.find(checked.value.role_id);
    if (!config || !config->valid)
      return {false, {}, common_failure("FamilyServiceNotOffered").error};
    auto *text = common.texts.find(config->child_name_key);
    auto name = command.display_name.value_or(text ? *text : StableId{});
    if (!valid_name(name))
      return {false, {}, common_failure("InvalidActorDisplayName").error};
    if (c.tick() > maximum - config->adoption_ticks)
      return {false, {}, common_failure("FamilyClockOverflow").error};
    AdoptionTerms terms;
    terms.guardian_actor_id = actor;
    terms.provider_actor_id = checked.value.provider_actor_id;
    terms.role_id = checked.value.role_id;
    terms.place_id = checked.value.place_id;
    terms.prototype_id = config->prototype_id;
    terms.display_name = name;
    terms.birth_tick = c.tick() + config->adoption_ticks -
                       config->policy.child_age_years * year;
    terms.duration = config->adoption_ticks;
    terms.fee = config->fee;
    terms.payer_wallet_id = c.actor(actor)->wallet_id;
    terms.recipient_wallet_id = config->wallet_id;
    terms.policy = config->policy;
    r = validate_terms(c, s, catalog, common, ec, terms, c.tick(), false);
    if (!r.ok)
      return {false, {}, r.error};
    auto money = c.spendable(terms.payer_wallet_id);
    if (!money.ok)
      return {false, {}, money.error};
    if (money.value < terms.fee)
      return {false, {}, common_failure("InsufficientFunds").error};
    if (c.wallet(terms.recipient_wallet_id)->balance > maximum - terms.fee)
      return {false, {}, common_failure("MoneyOverflow").error};
    quote.duration = terms.duration;
    quote.cost = terms.fee;
    quote.context = checked.value;
    quote.training = std::move(terms);
    return {true, std::move(quote), {}};
  }
  auto *designation = s.designations.find(actor);
  std::optional<StableId> heir = command.heir_actor_id;
  if (retire) {
    auto *choice = designation ? fact<FamilyChoice>(s, *designation) : nullptr;
    heir =
        choice ? std::optional<StableId>{choice->heir_actor_id} : std::nullopt;
  }
  if (!heir)
    return {false, {}, common_failure("FamilyHeirRequired").error};
  r = eligible(c, s, actor, *heir, c.tick(), retire);
  if (!r.ok)
    return {false, {}, r.error};
  r = ready(c, estates, access, *heir, {}, {}, retire);
  if (!r.ok)
    return {false, {}, r.error};
  auto here = c.resolved_location(actor), there = c.resolved_location(*heir);
  auto *a = here.ok ? std::get_if<SettlementLocation>(&here.value) : nullptr;
  auto *b = there.ok ? std::get_if<SettlementLocation>(&there.value) : nullptr;
  if (!a || !b || a->settlement_id != b->settlement_id ||
      a->place_id != b->place_id)
    return {false, {}, common_failure("FamilyNotTogether").error};
  FamilyChoice plan;
  plan.guardian_actor_id = actor;
  plan.heir_actor_id = *heir;
  plan.adoption_source = *s.children.find(*heir);
  plan.place_id = a->place_id;
  if (retire) {
    if (c.state().metadata.player_actor_id != actor)
      return {false, {}, common_failure("FamilyNotCurrentPlayer").error};
    auto assets = asset_plan(c, estates, ec, common, actor, *heir);
    if (!assets.ok)
      return {false, {}, assets.error};
    plan.designation_source = *designation;
    plan.assets = std::move(assets.value);
  }
  quote.family = std::move(plan);
  return {true, std::move(quote), {}};
}
Status
start_family_action(CommonStore &c, FamilyState &s,
                    const FamilyCatalog &catalog, const CommonCatalog &common,
                    const EstateState &estates, const EstateCatalog &ec,
                    const FamilyAccess &access, const ActionCoreView &action,
                    const FamilyCommand &command, const FamilyQuote &quote) {
  if (action.kind != "family_adopt" ||
      number(action.id, "action", c.state().next_instance_id) < 1)
    return common_failure("InvalidFamilyAction");
  if (c.state().facts.find(action.id))
    return common_failure("FamilyAdoptionAlreadyStarted");
  auto current = preview_family(c, s, catalog, common, estates, ec, access,
                                action.actor_id, command, action.id);
  if (!current.ok)
    return {false, {}, current.error};
  if (!quote.training || !quote.context || !current.value.training ||
      !family_codec::same_terms(*current.value.training, *quote.training) ||
      !context_matches(*quote.context, *current.value.training))
    return common_failure("StaleFamilyQuote");
  auto &terms = *current.value.training;
  if (action.start_tick != c.tick() ||
      action.remaining_ticks != current.value.duration ||
      action.participants !=
          std::vector<StableId>{action.actor_id,
                                current.value.context->provider_actor_id})
    return common_failure("InvalidFamilyAction");
  for (const auto &person : action.participants)
    if (c.actor(person)->action_id != action.id)
      return common_failure("FamilyOccupationMissing");
  s.actions.insert(action.id, {terms, *current.value.context, {}});
  auto r = c.apply_resources(
      {{SetReservation{terms.payer_wallet_id, action.id, terms.fee, false}},
       {}});
  if (!r.ok)
    return r;
  r = c.append_fact({action.id, "family_adoption_pending", "family"});
  if (!r.ok)
    return r;
  AdoptionFact fact;
  fact.guardian_actor_id = action.actor_id;
  fact.start_tick = c.tick();
  fact.terms = terms;
  fact.context = *current.value.context;
  s.facts.insert(action.id, std::move(fact));
  return common_success();
}
Outcome<FamilyEffect>
finish_family_action(CommonStore &c, FamilyState &s,
                     InteractionState &interaction, SocialState &social,
                     const FamilyCatalog &catalog, const CommonCatalog &common,
                     const EstateState &estates, const EstateCatalog &ec,
                     const FamilyAccess &access, const ActionCoreView &action) {
  auto valid = validate_action(c, s, catalog, common, ec, action, true);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto *f = fact<AdoptionFact>(s, action.id);
  FamilyEffect effect;
  effect.success = f->success;
  effect.actor_id = f->child_actor_id;
  if (f->kind != "family_adoption_pending") {
    effect.already_completed = true;
    return {true, std::move(effect), {}};
  }
  const auto terms = f->terms;
  const auto context = f->context;
  if (action.remaining_ticks != 0 ||
      c.tick() != action.start_tick + terms.duration)
    return {false, {}, common_failure("FamilyAdoptionNotReady").error};
  if (!access.interaction_catalog || !access.interaction_access)
    return {false,
            {},
            common_failure("UnsupportedNativeCapability", "family_context",
                           ErrorKind::unsupported_capability)
                .error};
  bool success =
      validate_interaction(c, interaction, *access.interaction_catalog,
                           *access.interaction_access, action.actor_id, context,
                           "family_adopt", action.id)
          .ok;
  for (const auto &person : action.participants)
    if (!ready(c, estates, access, person, action.id,
               person == action.actor_id ? terms.provider_actor_id
                                         : action.actor_id,
               false)
             .ok ||
        c.actor(person)->action_id != action.id)
      success = false;
  if (c.wallet(terms.recipient_wallet_id)->balance > maximum - terms.fee)
    success = false;
  f->completed_tick = c.tick();
  f->success = success;
  f->kind = success ? "family_adoption" : "family_adoption_failed";
  auto r = c.replace_fact_header({action.id, f->kind, "family"});
  if (!r.ok)
    return {false, {}, r.error};
  if (!success) {
    r = c.apply_resources({{ReleaseActionResources{action.id}}, {}});
    if (!r.ok)
      return {false, {}, r.error};
    effect.success = false;
    effect.code = "FamilyAdoptionInterrupted";
    effect.actor_id = StableId{};
    return {true, std::move(effect), {}};
  }
  auto made =
      instantiate(c, s, interaction, social, catalog, common, terms, action.id);
  if (!made.ok)
    return {false, {}, made.error};
  fact<AdoptionFact>(s, action.id)->child_actor_id = made.value;
  s.children.set(made.value, action.id);
  if (!access.initialize_schedule)
    return {false,
            {},
            common_failure("UnsupportedNativeCapability", "family_schedule",
                           ErrorKind::unsupported_capability)
                .error};
  r = access.initialize_schedule(
      made.value, terms.prototype_id,
      terms.policy.enable_travel_plan.value_or(false));
  if (!r.ok)
    return {false, {}, r.error};
  ResourceRecipe payment_recipe;
  r = resource_recipe::append_money_transfer(
      payment_recipe, terms.payer_wallet_id, terms.recipient_wallet_id,
      terms.fee, resource_recipe::MoneyTransferPolicy::original_balance);
  if (!r.ok)
    return {false, {}, r.error};
  payment_recipe.operations.push_back(ReleaseActionResources{action.id});
  r = c.apply_resources(payment_recipe);
  if (!r.ok)
    return {false, {}, r.error};
  effect.success = true;
  effect.actor_id = made.value;
  return {true, std::move(effect), {}};
}
Outcome<FamilyEffect>
apply_family_instant(CommonStore &c, FamilyState &s, EstateState &estates,
                     const FamilyCatalog &catalog, const CommonCatalog &common,
                     const EstateCatalog &ec, const FamilyAccess &access,
                     const StableId &actor, const FamilyCommand &command,
                     const FamilyQuote &quote) {
  if (command.kind == "family_agree_succession" ||
      command.kind == "family_cancel_succession" ||
      command.kind == "family_continue_as") {
    auto fresh = preview_family(c, s, catalog, common, estates, ec, access,
                                actor, command);
    if (!fresh.ok)
      return {false, {}, fresh.error};
    FamilyEffect effect;
    effect.success = true;
    if (command.kind == "family_agree_succession") {
      if (!quote.succession_consent || !fresh.value.succession_consent ||
          !succession_consent_source_equal(
              quote.succession_consent->agreement,
              fresh.value.succession_consent->agreement))
        return {false, {}, common_failure("StaleSuccessionConsent").error};
      auto accepted = accept_succession_plan(
          c, s, *access.interaction, *access.interaction_catalog,
          *access.interaction_access, *catalog.voluntary_succession,
          *quote.succession_consent, access.legacy);
      if (!accepted.ok)
        return {false, {}, accepted.error};
      effect.fact_id = accepted.value;
      effect.heir_actor_id =
          quote.succession_consent->agreement.personal.successor_actor_id;
    } else {
      if (!quote.succession_plan_id ||
          quote.succession_plan_id != fresh.value.succession_plan_id ||
          quote.succession_plan_id != command.plan_id)
        return {false, {}, common_failure("StaleSuccessionPlan").error};
      if (command.kind == "family_cancel_succession") {
        auto cancelled = cancel_succession_plan(c, s, actor, *command.plan_id);
        if (!cancelled.ok)
          return {false, {}, cancelled.error};
        effect.fact_id = *command.plan_id;
      } else {
        if (!quote.succession_personal || !fresh.value.succession_personal ||
            !succession_personal_terms_equal(
                quote.succession_personal->terms,
                fresh.value.succession_personal->terms))
          return {
              false, {}, common_failure("StaleSuccessionPersonalQuote").error};
        auto completed = complete_succession_plan(
            c, s, common, *access.interaction_catalog,
            *catalog.voluntary_succession, *command.plan_id,
            *quote.succession_personal, access.legacy);
        if (!completed.ok)
          return {false, {}, completed.error};
        effect.fact_id = completed.value;
        effect.heir_actor_id =
            quote.succession_personal->terms.successor_actor_id;
      }
    }
    auto valid = validate_family(
        c, s, catalog, common, estates, ec, &access.estate_access,
        access.economy, access.interaction_catalog, access.legacy,
        access.biological_identity ? &access.biological_identity : nullptr,
        access.origin_v2, access.initial_age);
    if (!valid.ok)
      return {false, {}, valid.error};
    effect.player_actor_id = c.state().metadata.player_actor_id;
    return {true, std::move(effect), {}};
  }
  if (command.kind == "family_adopt_existing") {
    if (!quote.adult_adoption || !command.request_id ||
        !command.adoptee_actor_id ||
        quote.adult_adoption->agreement.adopter_actor_id != actor ||
        quote.adult_adoption->agreement.adoptee_actor_id !=
            *command.adoptee_actor_id ||
        quote.adult_adoption->agreement.request_id != *command.request_id)
      return {false, {}, common_failure("StaleAdultAdoptionRequest").error};
    auto current = preview_family(c, s, catalog, common, estates, ec, access,
                                  actor, command);
    if (!current.ok)
      return {false, {}, current.error};
    if (!current.value.adult_adoption ||
        !adult_adoption_agreement_equal(current.value.adult_adoption->agreement,
                                        quote.adult_adoption->agreement))
      return {false, {}, common_failure("StaleAdultAdoptionRequest").error};
    auto accepted = accept_adult_adoption(
        c, s, *access.interaction, *access.interaction_catalog,
        *access.interaction_access, *catalog.adult_adoption,
        *quote.adult_adoption);
    if (!accepted.ok)
      return {false, {}, accepted.error};
    FamilyEffect effect;
    effect.success = true;
    effect.fact_id = accepted.value;
    effect.actor_id = *command.adoptee_actor_id;
    return {true, std::move(effect), {}};
  }
  if ((command.kind != "family_designate" && command.kind != "family_retire") ||
      !quote.family)
    return {false, {}, common_failure("InvalidFamilyInstant").error};
  FamilyCommand refresh = command;
  refresh.heir_actor_id = quote.family->heir_actor_id;
  auto current = preview_family(c, s, catalog, common, estates, ec, access,
                                actor, refresh);
  if (!current.ok)
    return {false, {}, current.error};
  if (!current.value.family ||
      !family_codec::same_choice(*current.value.family, *quote.family))
    return {false, {}, common_failure("StaleFamilyQuote").error};
  if (c.state().next_instance_id >= maximum)
    return {false, {}, common_failure("FamilyCounterExhausted").error};
  auto id = c.next_id("family");
  if (!id.ok)
    return {false, {}, id.error};
  auto plan = *current.value.family;
  plan.kind = command.kind == "family_designate" ? "family_designation"
                                                 : "family_succession";
  plan.tick = c.tick();
  if (command.kind == "family_retire") {
    ResourceRecipe recipe;
    auto owner = actor_owner(actor), receiver = actor_owner(plan.heir_actor_id);
    for (const auto &entry : plan.assets->wallets) {
      auto money = resource_recipe::append_money_transfer(
          recipe, entry.source_id, entry.destination_id, entry.amount,
          resource_recipe::MoneyTransferPolicy::original_balance);
      if (!money.ok)
        return {false, {}, money.error};
    }
    for (const auto &entry : plan.assets->goods) {
      if (entry.source_inventory_id == entry.destination_inventory_id)
        recipe.operations.push_back(SetLotOwner{entry.source_inventory_id,
                                                entry.lot_id, receiver, true});
      else
        recipe.operations.push_back(TransferLots{entry.source_inventory_id,
                                                 entry.destination_inventory_id,
                                                 entry.item_id,
                                                 entry.quantity,
                                                 {owner},
                                                 {},
                                                 receiver});
    }
    for (const auto &entry : plan.assets->estates) {
      recipe.operations.push_back(SetWalletOwner{entry.wallet_id, receiver});
      recipe.operations.push_back(
          SetInventoryOwner{entry.inventory_id, receiver});
    }
    auto r = c.apply_resources(recipe);
    if (!r.ok)
      return {false, {}, r.error};
    for (const auto &entry : plan.assets->estates)
      estates.estates.find(entry.estate_id)->owner_actor_id =
          plan.heir_actor_id;
    for (const auto &role : plan.assets->disabled_roles) {
      auto value = *c.state().roles.find(role);
      value.enabled = false;
      r = c.apply_role(role, value);
      if (!r.ok)
        return {false, {}, r.error};
    }
    s.successions.push_back(id.value);
    r = c.set_player_actor(plan.heir_actor_id);
    if (!r.ok)
      return {false, {}, r.error};
  } else
    s.designations.set(actor, id.value);
  auto r = c.append_fact({id.value, plan.kind, "family"});
  if (!r.ok)
    return {false, {}, r.error};
  s.facts.insert(id.value, plan);
  r = validate_family(c, s, catalog, common, estates, ec, &access.estate_access,
                      access.economy, access.interaction_catalog, access.legacy,
                      access.biological_identity ? &access.biological_identity
                                                 : nullptr,
                      access.origin_v2, access.initial_age);
  if (!r.ok)
    return {false, {}, r.error};
  FamilyEffect effect;
  effect.fact_id = id.value;
  effect.heir_actor_id = plan.heir_actor_id;
  effect.player_actor_id = c.state().metadata.player_actor_id;
  return {true, std::move(effect), {}};
}
// 动态人物只承认当前typed永久来源；此口不替代共同数值、外貌和容器验证。
Status validate_family_identity(const CommonStore &c, const FamilyState &s,
                                const FamilyCatalog &catalog,
                                const ActorRecord &actor) {
  if (number(actor.id, "actor", c.state().next_instance_id) < 1 ||
      !actor.prototype_id || !actor.display_name || !actor.created_tick ||
      !actor.birth_tick)
    return common_failure("InvalidDynamicActor");
  if (!catalog.prototypes.find(*actor.prototype_id) ||
      !valid_name(*actor.display_name) || *actor.created_tick < 0 ||
      *actor.created_tick > c.tick() || *actor.birth_tick < -maximum ||
      *actor.birth_tick > *actor.created_tick)
    return common_failure("InvalidDynamicActor");
  auto *f = fact<IdentityCreationFact>(s, actor.id);
  if (!f || f->actor_id != actor.id || f->prototype_id != *actor.prototype_id ||
      f->display_name != *actor.display_name ||
      f->birth_tick != *actor.birth_tick ||
      f->created_tick != *actor.created_tick)
    return common_failure("ActorCreationMismatch");
  auto *source = fact<AdoptionFact>(s, f->source);
  auto *created = fact<CharacterCreationFact>(s, f->source);
  if (created) {
    if (!catalog.character_creation ||
        created->kind != "family_character_creation" ||
        created->actor_id != actor.id ||
        created->prototype_actor_id != f->prototype_id ||
        created->created_tick != 0 || f->created_tick != 0 ||
        created->draft.display_name != f->display_name ||
        f->birth_tick != -created->draft.age_years * year ||
        catalog.character_creation->prototype_actor_id != f->prototype_id)
      return common_failure("InvalidActorCreationSource");
    auto draft = validate_character_creation_draft(created->draft,
                                                   catalog.extended_skills);
    if (!draft.ok)
      return draft;
  } else if (!source || source->kind != "family_adoption")
    return common_failure("InvalidActorCreationSource");
  for (const auto &row : s.facts.rows())
    if (row.first != actor.id)
      if (auto *other = std::get_if<IdentityCreationFact>(&row.second))
        if (other->source == f->source)
          return common_failure("InvalidActorCreationSource");
  if (actor.wallet_id != "personal:" + actor.id ||
      actor.inventory_id != "bag:" + actor.id)
    return common_failure("DynamicActorAssetsMismatch");
  return common_success();
}
// 休整候选沿家庭索引原序，只读来源监护人；在场、生死和粮食规则仍归services。
std::vector<StableId> family_children(const FamilyState &s,
                                      const StableId &guardian) {
  std::vector<StableId> children;
  for (const auto &row : s.children.rows()) {
    auto *source = fact<AdoptionFact>(s, row.second);
    if (source && source->guardian_actor_id == guardian)
      children.push_back(row.first);
  }
  return children;
}
// 行程授权必须同时具备本人创建、成功收养和显式policy，不能单凭原型有计划而放行。
bool family_travel_enabled(const CommonStore &c, const FamilyState &s,
                           const StableId &id) {
  auto *actor = c.state().actors.find(id);
  auto *creation = fact<IdentityCreationFact>(s, id);
  auto *child = s.children.find(id);
  auto *adoption = creation ? fact<AdoptionFact>(s, creation->source) : nullptr;
  return actor && creation && child && *child == creation->source && adoption &&
         adoption->kind == "family_adoption" && adoption->success &&
         adoption->child_actor_id == id &&
         adoption->terms.policy.enable_travel_plan.value_or(false) &&
         actor->prototype_id == creation->prototype_id &&
         adoption->terms.prototype_id == creation->prototype_id &&
         actor->created_tick == creation->created_tick &&
         adoption->completed_tick == creation->created_tick;
}
// 历史指定仍核当时资格，不用现在是否同地、是否有钱去推翻已经发生的事实。
Status validate_choice(const CommonStore &c, const FamilyState &s,
                       const CommonCatalog &common, const FamilyChoice &choice,
                       bool succession) {
  if (choice.tick < 0 || choice.tick > c.tick() ||
      !common.places.find(choice.place_id) ||
      (succession && (!choice.designation_source || !choice.assets)) ||
      (!succession && (choice.designation_source || choice.assets)))
    return common_failure("InvalidFamilyChoice");
  auto *source = s.children.find(choice.heir_actor_id);
  if (!source || *source != choice.adoption_source)
    return common_failure("FamilyAdoptionSourceMismatch");
  return eligible(c, s, choice.guardian_actor_id, choice.heir_actor_id,
                  choice.tick, succession);
}
// 历史移交不能通过伪造库存ID把组织资产写成私人遗产，原私袋与庄库分别核对。
Status validate_assets(const CommonStore &c, const CommonCatalog &common,
                       const EstateCatalog &ec, const FamilyChoice &choice,
                       OrderedTable<StableId> &owners) {
  const auto &assets = *choice.assets;
  auto *source = c.actor(choice.guardian_actor_id);
  auto *target = c.actor(choice.heir_actor_id);
  if (!source || !target)
    return common_failure("InvalidSuccessionAssets");
  std::vector<StableId> expected, received;
  std::set<StableId> stores, cashboxes;
  for (const auto &id : sorted(owners))
    if (*owners.find(id) == choice.guardian_actor_id)
      expected.push_back(id);
  for (const auto &entry : assets.estates) {
    auto *d = ec.estates.find(entry.estate_id);
    if (!d || entry.inventory_id != d->inventory_id ||
        entry.wallet_id != d->wallet_id)
      return common_failure("InvalidSuccessionEstate");
    received.push_back(entry.estate_id);
    stores.insert(entry.inventory_id);
    cashboxes.insert(entry.wallet_id);
  }
  if (received != expected)
    return common_failure("SuccessionEstateSourceMismatch");
  std::set<StableId> wallets, lots, roles;
  for (const auto &entry : assets.wallets) {
    if (entry.amount < 0 || entry.amount > maximum)
      return common_failure("InvalidSuccessionMoney");
    auto *w = c.wallet(entry.source_id);
    if (!wallets.insert(entry.source_id).second ||
        cashboxes.count(entry.source_id) ||
        entry.destination_id != target->wallet_id || !w ||
        !owner_equal(w->owner, actor_owner(choice.guardian_actor_id)))
      return common_failure("SuccessionMoneySourceMismatch");
  }
  if (!wallets.count(source->wallet_id))
    return common_failure("MissingSuccessionPersonalWallet");
  for (const auto &entry : assets.goods) {
    if (entry.quantity < 1 || entry.quantity > maximum ||
        !common.items.find(entry.item_id) ||
        !lots.insert(entry.lot_id).second ||
        number(entry.lot_id, "lot", c.state().next_instance_id) < 1)
      return common_failure("InvalidSuccessionGoods");
    if (entry.source_inventory_id == source->inventory_id) {
      if (entry.destination_inventory_id != target->inventory_id)
        return common_failure("SuccessionGoodsSourceMismatch");
    } else if (!stores.count(entry.source_inventory_id) ||
               entry.source_inventory_id != entry.destination_inventory_id)
      return common_failure("SuccessionGoodsSourceMismatch");
  }
  for (const auto &id : assets.disabled_roles)
    if (!common.roles.find(id) || !roles.insert(id).second)
      return common_failure("InvalidSuccessionRoles");
  for (const auto &id : expected)
    owners.set(id, choice.heir_actor_id);
  return common_success();
}
// 家庭验证沿身份→头→庄园→永久事实→控制链顺序，所有来源都落同一世界。
Status validate_family(
    const CommonStore &c, const FamilyState &s, const FamilyCatalog &catalog,
    const CommonCatalog &common, const EstateState &estates,
    const EstateCatalog &ec, const EstateAccess *estate_access,
    const LocalEconomyState *economy,
    const InteractionCatalog *adult_interaction,
    const FamilyLegacyState *legacy,
    const std::function<Status(const ActorRecord &)> *biological_identity,
    const FamilyOriginV2Sources *origin_v2, const LifeAgeSources *initial_age) {
  if (catalog.extended_skills != common.extended_skills)
    return common_failure("FamilySkillProfileMismatch");
  for (const auto &row : c.state().actors.rows())
    if (!catalog.prototypes.find(row.first)) {
      const auto *head = c.state().facts.find(row.first);
      const bool born = common.family_birth_identity_enabled && head &&
                        head->kind == "actor_creation" &&
                        head->domain == "family_birth";
      auto r = born ? (biological_identity && *biological_identity
                           ? (*biological_identity)(row.second)
                           : common_failure("BiologicalIdentityReadRequired",
                                            row.first))
               : origin_v2
                   ? validate_family_identity_v2(c, s, catalog, common,
                                                 row.second, *origin_v2)
                   : validate_family_identity(c, s, catalog, row.second);
      if (!r.ok)
        return r;
    }
  if (catalog.household_control) {
    if (!economy)
      return common_failure("HouseholdControlEconomyRequired");
    auto control = validate_household_control(
        c, s, common, *economy, &*catalog.household_control, initial_age);
    if (!control.ok)
      return control;
  } else {
    for (const auto &row : s.facts.rows())
      if (std::holds_alternative<HouseholdControlFact>(row.second))
        return common_failure("HouseholdControlCapabilityRequired", row.first);
    for (const auto &row : c.state().facts.rows())
      if (row.second.domain == "family" &&
          row.second.kind == "family_household_control")
        return common_failure("HouseholdControlCapabilityRequired", row.first);
  }
  if (catalog.adult_adoption) {
    if (!adult_interaction)
      return common_failure("AdultAdoptionInteractionRequired");
    auto adopted = validate_adult_adoptions(c, s, common, *adult_interaction,
                                            &*catalog.adult_adoption);
    if (!adopted.ok)
      return adopted;
  } else {
    for (const auto &row : s.facts.rows())
      if (std::holds_alternative<AdultAdoptionFact>(row.second))
        return common_failure("AdultAdoptionCapabilityRequired", row.first);
    for (const auto &row : c.state().facts.rows())
      if (row.second.domain == "family" &&
          row.second.kind == "family_adult_adoption")
        return common_failure("AdultAdoptionCapabilityRequired", row.first);
  }
  Status voluntary;
  if (catalog.voluntary_source_mode ==
      FamilyVoluntarySourceMode::living_paths) {
    if (!catalog.voluntary_succession)
      return common_failure("VoluntaryPathSuccessionPairRequired");
    // 真实Known/关系声明是严格后续晚门，不能在此反调用并循环证明自己的控制来源。
    voluntary = validate_voluntary_succession_structure(c, s);
  } else if (catalog.voluntary_source_mode ==
             FamilyVoluntarySourceMode::authored_roles)
    voluntary = validate_voluntary_succession_facts(
        c, s, common, adult_interaction,
        catalog.voluntary_succession ? &*catalog.voluntary_succession : nullptr,
        legacy);
  else
    return common_failure("UnsupportedFamilyVoluntarySourceMode");
  if (!voluntary.ok)
    return voluntary;
  auto created =
      origin_v2 ? validate_family_creation_sources_v2(c, s, catalog, common,
                                                      *origin_v2)
                : validate_character_creation_source(c, s, catalog, common);
  if (!created.ok)
    return created;
  // 初始名单模式不替代后续继任链；复用同一typed只读守卫。
  auto founder =
      origin_v2 ? validate_family_founder_v2(c, s, catalog, common, *origin_v2)
                : validate_family_founder(c.state(), s, catalog);
  if (!founder.ok)
    return founder;
  auto estate_status = validate_estate(c, estates, ec, common, estate_access);
  if (!estate_status.ok)
    return estate_status;
  OrderedTable<StableId> latest;
  std::set<StableId> succession_ids;
  for (const auto &id : c.state().facts.order()) {
    const auto *value = s.facts.find(id);
    if (!value)
      continue;
    if (auto *f = std::get_if<AdoptionFact>(value)) {
      if (number(id, "action", c.state().next_instance_id) < 1)
        return common_failure("InvalidFamilyAdoptionFact");
      auto r =
          validate_terms(c, s, catalog, common, ec, f->terms, f->start_tick,
                         f->kind != "family_adoption_pending");
      if (!r.ok)
        return r;
      if (f->guardian_actor_id != f->terms.guardian_actor_id ||
          !context_matches(f->context, f->terms))
        return common_failure("FamilyAdoptionFactMismatch");
      if (f->kind == "family_adoption_pending") {
        auto *action = c.action(id);
        if (!action)
          return common_failure("OrphanFamilyAdoption");
        r = validate_action(c, s, catalog, common, ec, *action, false);
        if (!r.ok)
          return r;
      } else if (f->completed_tick != f->start_tick + f->terms.duration ||
                 f->completed_tick > c.tick() ||
                 f->success != (f->kind == "family_adoption"))
        return common_failure("InvalidFamilyAdoptionDate");
      else if (f->success) {
        auto *child = s.children.find(f->child_actor_id);
        if (!child || *child != id)
          return common_failure("OrphanFamilyChild");
      } else if (!f->child_actor_id.empty())
        return common_failure("FailedAdoptionHasChild");
    } else if (auto *f = std::get_if<FamilyChoice>(value)) {
      if (f->kind == "family_designation") {
        auto r = validate_choice(c, s, common, *f, false);
        if (!r.ok || number(id, "family", c.state().next_instance_id) < 1)
          return common_failure("InvalidFamilyDesignation");
        auto *prior = latest.find(f->guardian_actor_id);
        if (!prior || number(id, "family", c.state().next_instance_id) >
                          number(*prior, "family", c.state().next_instance_id))
          latest.set(f->guardian_actor_id, id);
      } else if (f->kind == "family_succession")
        succession_ids.insert(id);
      else
        return common_failure("InvalidFamilyChoice");
    } else if (std::holds_alternative<VoluntarySuccessionFact>(*value))
      succession_ids.insert(id);
  }
  for (const auto &row : s.facts.rows()) {
    auto *h = c.state().facts.find(row.first);
    if (!h || h->domain != "family")
      return common_failure("InvalidFamilyFact", row.first);
    auto kind = std::visit(
        [](const auto &f) -> StableId {
          using T = std::decay_t<decltype(f)>;
          if constexpr (std::is_same_v<T, IdentityCreationFact>)
            return "actor_creation";
          else
            return f.kind;
        },
        row.second);
    if (h->kind != kind)
      return common_failure("InvalidFamilyFact", row.first);
    if (auto *creation = std::get_if<IdentityCreationFact>(&row.second)) {
      auto *actor = c.state().actors.find(row.first);
      if (!actor || creation->actor_id != row.first)
        return common_failure("OrphanActorCreation");
      auto r = validate_family_identity(c, s, catalog, *actor);
      if (!r.ok)
        return r;
    }
  }
  for (const auto &row : s.children.rows()) {
    if (auto *adult = fact<AdultAdoptionFact>(s, row.second)) {
      if (adult->agreement.adoptee_actor_id != row.first)
        return common_failure("AdultAdoptionRelationMismatch");
      continue;
    }
    auto *f = fact<AdoptionFact>(s, row.second);
    auto *creation = fact<IdentityCreationFact>(s, row.first);
    if (!f || f->kind != "family_adoption" || f->child_actor_id != row.first ||
        !f->success || !creation || creation->source != row.second)
      return common_failure("InvalidFamilyChild");
    if (creation->prototype_id != f->terms.prototype_id ||
        creation->birth_tick != f->terms.birth_tick ||
        creation->display_name != f->terms.display_name ||
        creation->created_tick != f->completed_tick)
      return common_failure("FamilyChildIdentityMismatch");
  }
  if (latest.size() != s.designations.size())
    return common_failure("FamilyDesignationIndexMismatch");
  for (const auto &row : latest.rows()) {
    auto *current = s.designations.find(row.first);
    if (!current || *current != row.second)
      return common_failure("FamilyDesignationIndexMismatch");
  }
  StableId current = s.founder_actor_id;
  std::set<StableId> seen{current};
  OrderedTable<StableId> owners;
  for (const auto &row : ec.estates.rows())
    owners.insert(row.first, row.second.owner_actor_id);
  Integer last_tick = 0;
  // 新域的零资产控制事实按Common原序插入；旧Family索引与产权回放仍逐项核对。
  std::vector<StableId> control_order;
  for (const auto &id : c.state().facts.order())
    if (succession_ids.count(id) || (legacy && legacy->activations.find(id)))
      control_order.push_back(id);
  std::vector<StableId> family_order;
  std::set<StableId> legacy_seen;
  for (const auto &id : control_order) {
    if (legacy)
      if (const auto *activated = legacy->activations.find(id)) {
        const auto *accepted =
            legacy->consents.find(activated->consent_fact_id);
        const auto *head = c.state().facts.find(id);
        if (!accepted || !head || head->domain != "family_legacy" ||
            head->kind != "family_legacy_control_activated" ||
            activated->kind != head->kind ||
            accepted->source.predecessor_actor_id != current ||
            !c.actor(accepted->source.successor_actor_id) ||
            !seen.insert(accepted->source.successor_actor_id).second ||
            activated->tick < last_tick || activated->tick > c.tick() ||
            !legacy_seen.insert(id).second)
          return common_failure("InvalidLegacyControlChain", id);
        current = accepted->source.successor_actor_id;
        last_tick = activated->tick;
        continue;
      }
    family_order.push_back(id);
    if (!succession_ids.count(id) ||
        number(id, "family", c.state().next_instance_id) < 1)
      return common_failure("InvalidSuccessionChain");
    if (fact<VoluntarySuccessionFact>(s, id)) {
      auto step = family_succession_step(s, id);
      if (!step.ok)
        return {false, {}, step.error};
      if (step.value.predecessor_actor_id != current ||
          seen.count(step.value.successor_actor_id) ||
          step.value.tick < last_tick)
        return common_failure("InvalidSuccessionChain");
      current = step.value.successor_actor_id;
      seen.insert(current);
      last_tick = step.value.tick;
      succession_ids.erase(id);
      continue;
    }
    auto *f = fact<FamilyChoice>(s, id);
    auto r = validate_choice(c, s, common, *f, true);
    if (!r.ok)
      return r;
    if (f->guardian_actor_id != current || seen.count(f->heir_actor_id) ||
        f->tick < last_tick)
      return common_failure("InvalidSuccessionChain");
    auto *choice = fact<FamilyChoice>(s, *f->designation_source);
    if (!choice || choice->kind != "family_designation" ||
        choice->guardian_actor_id != current ||
        choice->heir_actor_id != f->heir_actor_id ||
        choice->adoption_source != f->adoption_source ||
        choice->tick > f->tick ||
        number(*f->designation_source, "family", c.state().next_instance_id) >=
            number(id, "family", c.state().next_instance_id))
      return common_failure("SuccessionDesignationMismatch");
    for (const auto &later : s.facts.rows())
      if (auto *record = std::get_if<FamilyChoice>(&later.second))
        if (record->kind == "family_designation" &&
            record->guardian_actor_id == current &&
            number(later.first, "family", c.state().next_instance_id) >
                number(*f->designation_source, "family",
                       c.state().next_instance_id) &&
            number(later.first, "family", c.state().next_instance_id) <
                number(id, "family", c.state().next_instance_id))
          return common_failure("SuccessionDesignationSuperseded");
    r = validate_assets(c, common, ec, *f, owners);
    if (!r.ok)
      return r;
    current = f->heir_actor_id;
    seen.insert(current);
    last_tick = f->tick;
    succession_ids.erase(id);
  }
  if (family_order != s.successions ||
      (legacy && legacy_seen.size() != legacy->activations.size()) ||
      !succession_ids.empty() || c.state().metadata.player_actor_id != current)
    return common_failure("InvalidSuccessionPlayer");
  for (const auto &row : owners.rows()) {
    auto *e = estates.estates.find(row.first);
    if (!e || e->owner_actor_id != row.second)
      return common_failure("EstateSuccessionSourceMissing");
  }
  return common_success();
}
} // namespace hanmo::resident
