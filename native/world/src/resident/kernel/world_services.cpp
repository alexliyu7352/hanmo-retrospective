#include "native_public_supply_inspection.hpp"
#include "exact_time_profile.hpp"
#include "native_military_custody_command.hpp"
#include "world_read_stamp.hpp"
#include "resident/domains/family/family_origin_v2.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "naval_navigation_integration.hpp"
// 唯一候选的跨域只读接线；仅借用实际typed域，不保留旧世界引用或复制权威资产。
#include "world_bundle.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "political_takeover_execution_world.hpp"
#include "treaty_passage_integration.hpp"
#include "resident/domains/military/naval_casualties.hpp"
#include "resident/domains/military/naval_world.hpp"
#include "resident/domains/military/naval_manifest_history.hpp"
#include "resident/domains/military/naval_contact_install.hpp"
#include "resident/domains/tasks/task_offer_revisit.hpp"
#include "resident/views/regional_life_view.hpp"
#include "resident/domains/interaction/regional_outcome_dialogue.hpp"
#include "resident/domains/interaction/interaction_access.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/military_lifecycle.hpp"
#include <algorithm>
#include <stdexcept>

namespace hanmo::resident {
namespace {
// 原Location.resolve错误结果没有有效place；受控回调以空地点表达同样的条件不匹配。
Location unresolved_location() { return SettlementLocation{}; }
// 同候选按原向量顺序定位军队单位，不生成缺失人员或替代单位。
const MilitaryUnitRecord *find_unit(const MilitaryState &state,
                                    const StableId &id) {
  for (const auto &unit : state.military_units)
    if (unit.id == id)
      return &unit;
  return nullptr;
}
} // namespace

// 构造不提前读取世界；WorldBundle完成域安装后才绑定CommonStore成员解析。
BundleServices::BundleServices(WorldBundle &world)
    : world_(world), occupation_clock_(world), military_appointments_(world),
      field_appointments_(world) {
  if (world.growth_market_history_enabled() && world.catalog->growth &&
      world.catalog->local_economy)
    market_history_ = std::make_unique<GrowthMarketHistoryWorldAccess>(
        GrowthMarketHistoryWorldContext{
            world.common, *world.catalog->common, *world.catalog->growth,
            world.growth, *world.catalog->local_economy, world.local_economy,
            *world.catalog->growth_market_history, world.growth_market_history,
            world.growth_market_capture,
            world.political_takeover_enabled()
                ? &world.economic_takeover_execution
                : nullptr,
            &world});
  if (world.private_shops_enabled())
    shop_workers_.emplace(world.common, world.private_shops);
  if (world.catalog->actor_social_occupancy_selected &&
      world.catalog->actor_social_occupancy_initial && world.actor_foundation &&
      world.catalog->actor_foundation && world.catalog->common)
    social_occupancy_.reset(new NativeActorSocialOccupancyWorld(world));
}
// 复用C在役成员判断，军俘不取得行动成员身份。
const std::vector<StableId> *
BundleServices::active_army_members(const StableId &actor) const {
  return MilitaryMembership(world_.military).active_army_members(actor);
}
// 返回候选军队记录内的稳定字段借用，不返回适配器临时对象的内存。
const StableId *BundleServices::army_leader(const StableId &actor) const {
  const auto *person = world_.common.actor(actor);
  if (person)
    if (const auto *location = std::get_if<InArmyLocation>(&person->location)) {
      for (const auto &army : world_.military.armies)
        if (army.id == location->army_id && army.status == "active") {
          const bool member = std::find(army.member_actor_ids.begin(),
                                        army.member_actor_ids.end(),
                                        actor) != army.member_actor_ids.end();
          const bool prisoner =
              std::find(army.prisoner_ids.begin(), army.prisoner_ids.end(),
                        actor) != army.prisoner_ids.end();
          return member || prisoner ? &army.commander_actor_id : nullptr;
        }
      return nullptr;
    }
  return MilitaryMembership(world_.military).army_leader(actor);
}
// 战斗占用读真实C列表；不因P1目录通常为空就安装成功空函数。
Outcome<bool> BundleServices::field_battle_bound(const StableId &actor) const {
  auto layout = validate_field_battle_layout(
      world_.catalog->identity, world_.catalog->field_battle.get(),
      world_.field_battle ? &*world_.field_battle : nullptr);
  if (!layout.ok)
    return {false, false, layout.error};
  if (!world_.field_battle)
    return {true, false, {}};
  auto held = read_field_battle_held_actors(world_.common, world_.military,
                                            *world_.field_battle);
  if (!held.ok)
    return {false, false, held.error};
  return {true,
          std::find(held.value.begin(), held.value.end(), actor) !=
              held.value.end(),
          {}};
}
Outcome<std::optional<FieldBattleSpatialView>>
BundleServices::field_battle_position(const StableId &actor) const {
  auto layout = validate_field_battle_layout(
      world_.catalog->identity, world_.catalog->field_battle.get(),
      world_.field_battle ? &*world_.field_battle : nullptr);
  if (!layout.ok)
    return {false, {}, layout.error};
  if (!world_.field_battle)
    return {true, std::nullopt, {}};
  return read_field_battle_actor_position(world_.common, world_.military,
                                          *world_.field_battle, actor);
}
bool BundleServices::in_open_battle(const StableId &actor) const {
  auto field = field_battle_bound(actor);
  if (!field.ok || field.value)
    return true;
  if (world_.naval_combat_enabled()) {
    auto held = naval_battle_held_actors(world_.naval, world_.military);
    if (std::find(held.begin(), held.end(), actor) != held.end())
      return true;
  }
  return personal_occupied(world_.personal, actor) ||
         military_actor_in_battle(world_.military, actor);
}
// 管理命令允许排除本人会话，但provider与别人的会话仍真实占用。
bool BundleServices::in_interaction(const StableId &actor,
                                    const StableId &allowed_speaker) const {
  return InteractionActivity(world_.military, world_.interaction)
      .in_interaction(actor, allowed_speaker);
}
// A解析队伍及军团唯一领队位置，失败仍保留显式Outcome给B领域。
Outcome<Location>
BundleServices::resolved_location(const StableId &actor) const {
  return world_.common.resolved_location(actor);
}
// 原第一匹配队伍优先，不能把成员表排序或把军团误当party。
StableId BundleServices::party_id(const StableId &actor) const {
  for (const auto &row : world_.common.state().parties.rows()) {
    const auto &party = row.second;
    if (std::find(party.members.begin(), party.members.end(), actor) !=
        party.members.end())
      return row.first;
  }
  return {};
}
// 同行条款按原序求实际附加费，独行与仅属军团者没有隐含party费用。
Integer BundleServices::party_extra_cost(const StableId &actor) const {
  const auto id = party_id(actor);
  const auto *party = world_.common.state().parties.find(id);
  Integer total = 0;
  if (party)
    for (const auto &row : party->terms.rows())
      total += row.second.cost_per_tick;
  return total;
}
// 军团旅行只借C公柜和真实fit名册；负载及去重人数仍由B唯一算法计算。
Outcome<std::optional<ArmyTravelView>>
BundleServices::active_army(const StableId &actor) const {
  const auto *army = active_army_for_actor(world_.military, actor);
  if (!army)
    return {true, std::nullopt, {}};
  ArmyTravelView view;
  view.commander_actor_id = army->commander_actor_id;
  view.wallet_id = army->wallet_id;
  view.inventory_id = army->inventory_id;
  view.member_actor_ids = army->member_actor_ids;
  for (const auto &id : army->unit_ids) {
    const auto *unit = find_unit(world_.military, id);
    if (!unit)
      return {false,
              {},
              {{"UnknownMilitaryUnit", id},
               ErrorKind::internal,
               {},
               "military",
               "travel",
               army->id}};
    view.fit_actor_ids.insert(view.fit_actor_ids.end(), unit->fit.begin(),
                              unit->fit.end());
  }
  return {true, std::move(view), {}};
}
// 舟行证明来自同一Naval真实船、原人员和Common载重，每次报价/半日都重核。
// 同一实船账只读占用，伤亡/撤职不会隐式释放船只或把人转为陆军。
bool BundleServices::naval_berth(const StableId &place) const {
  if (!world_.naval_enabled())
    return false;
  for (const auto &b : world_.catalog->naval->berths)
    if (b.place_id == place)
      return true;
  return false;
}
bool BundleServices::army_embarked(const StableId &army) const {
  if (!world_.naval_enabled())
    return false;
  for (const auto &f : world_.naval.fleets)
    if (f.army_id == army && f.status == "active")
      return true;
  return false;
}
// 纯位置门借用军事原编队事实；不调用出航报价，不从Action.mode反推船只。
Outcome<std::optional<VesselFleetLocationView>>
BundleServices::vessel_location(const StableId &actor) const {
  if (!world_.naval_enabled())
    return {true, std::nullopt, {}};
  auto source =
      query_naval_location_source(world_.common, world_.military, world_.naval,
                                  *world_.catalog->naval, actor);
  if (!source.ok)
    return {false, {}, source.error};
  if (!source.value)
    return {true, std::nullopt, {}};
  const auto &p = *source.value;
  return {true,
          VesselFleetLocationView{p.fleet_id, p.army_id, p.commander_actor_id,
                                  p.formation_source_id, p.world_map_id},
          {}};
}
// 遗体位置只认原致死轮次锚点；战船以后易主或移动不搬走原私人资产。
Outcome<std::optional<VesselRemainsLocationView>>
BundleServices::vessel_remains(const StableId &actor) const {
  if (!world_.naval_combat_enabled())
    return {true, std::nullopt, {}};
  auto source = query_naval_remains_location(world_.common, world_.military,
                                             world_.naval, actor);
  if (!source.ok)
    return {false, {}, source.error};
  if (!source.value)
    return {true, std::nullopt, {}};
  const auto &p = *source.value;
  return {true,
          VesselRemainsLocationView{p.actor_id, p.fleet_id, p.army_id,
                                    p.round_id, p.casualty_id, p.anchor},
          {}};
}
// 冷准入只读真实原旅程，不改变热出航/续航资格；战斗hold和合法船损不损坏旧行动。
Outcome<std::optional<VesselFleetTravelView>>
BundleServices::vessel_journey(const StableId &actor, const StableId &action,
                               const StableId &plan) const {
  if (!world_.naval_enabled())
    return {true, std::nullopt, {}};
  auto source = query_naval_journey_source(
      world_.common, world_.military, world_.naval, *world_.catalog->naval,
      world_.travel, actor, action, plan,
      world_.naval_combat_enabled() ? &world_.naval.battle : nullptr);
  if (!source.ok)
    return {false, {}, source.error};
  if (!source.value)
    return {true, std::nullopt, {}};
  const auto &p = *source.value;
  return {true,
          VesselFleetTravelView{p.fleet_id, p.army_id, p.commander_actor_id,
                                p.payer_wallet_id, p.travel_budget},
          {}};
}
// 同一当前军旅报价，不从未来行程或当前冷档损伤补写资格。
Status BundleServices::naval_voyage_allowed(const StableId &actor,
                                            const TravelQuote &quote) const {
  auto prepared = prepare_naval_voyage_world(world_, actor, quote);
  return {prepared.ok, {}, prepared.error};
}
Outcome<std::optional<VesselFleetTravelView>>
BundleServices::vessel_fleet(const StableId &actor) const {
  if (!world_.naval_enabled())
    return {true, std::nullopt, {}};
  auto access = military_access();
  auto proof = preview_naval_travel(world_.common, world_.military,
                                    *world_.catalog->military, access,
                                    world_.naval, *world_.catalog->naval, actor,
                                    world_.naval_combat_enabled());
  if (!proof.ok)
    return {false, {}, proof.error};
  if (!proof.value)
    return {true, std::nullopt, {}};
  const auto &p = *proof.value;
  return {true,
          VesselFleetTravelView{p.fleet_id, p.army_id, p.commander_actor_id,
                                p.payer_wallet_id, p.travel_budget},
          {}};
}
// 行旅排程既检查会话owner也检查provider，复用C唯一占用定义。
bool BundleServices::talking(const StableId &actor) const {
  return in_interaction(actor);
}
// 准入读取C目录保存的真实条件，空数组经过正式all_conditions得到允许。
Outcome<bool> BundleServices::admission_allowed(const StableId &actor,
                                                const StableId &place) const {
  for (const auto &admission : world_.catalog->interaction->admissions)
    if (admission.id == place) {
      const auto access = interaction_access();
      return all_conditions(world_.common, world_.interaction, access, actor,
                            admission.conditions);
    }
  return {false,
          false,
          {{"UnknownPlace", place},
           ErrorKind::rule,
           {},
           "travel",
           "move_local",
           actor}};
}
// flags_仅为本次只读查询重建的加速视图，权威bool始终在C候选里。
const terrain::Flags &BundleServices::terrain_flags() const {
  if (checkpoint_read_cache_frozen_)
    return flags_;
  CheckpointMemoryCharge charge;
  terrain::Flags next;
  for (const auto &flag : world_.interaction.flags)
    if (next.find(flag.id) == next.end()) {


      (charge).grow(0);
      next.emplace(StableId(flag.id), flag.value);
    }
  flags_.swap(next);
  flags_memory_.swap(charge);
  return flags_;
}
// seal只在原world所属线程、任何worker lease交出前执行，不更改任何业务flag。
void BundleServices::prepare_checkpoint_read_cache() {
  if (!checkpoint_read_cache_frozen_) {
    terrain_flags();
    checkpoint_read_cache_frozen_ = true;
  }
}
void BundleServices::retain_checkpoint_read_cache(
    CheckpointRetentionCensus &census) const {
  census.owner(flags_.get_allocator().allocation_owner());
  census.resource(flags_memory_.resource());
}
// 动态行程只从原型配置和本人已完成收养来源授权，不复制另一份作者计划。
const TravelScheduleDefinition *
BundleServices::schedule(const StableId &actor) const {
  const auto *person = world_.common.state().actors.find(actor);
  if (!person)
    return nullptr;
  if (const auto *authored = world_.catalog->travel->authored_plans.find(actor))
    return authored;
  if (!person->prototype_id ||
      !family_travel_enabled(world_.common, world_.family, actor))
    return nullptr;
  return world_.catalog->travel->authored_plans.find(*person->prototype_id);
}
// 玩家身份只从唯一共同metadata借用，不从展示或缓存推断。
const StableId &BundleServices::player_actor_id() const {
  return world_.common.state().metadata.player_actor_id;
}
// 亲历只来自原明确参与者：已完成武学训练及个人战，加既有社会事件；课程和休整不扩散全局知闻。
std::optional<SocialObservedFact>
BundleServices::social_fact(const StableId &id) const {
  if (const auto *event = world_.social.events.find(id)) {
    if (event->kind == "aid" || event->kind == "theft" ||
        event->kind == "refusal")
      return SocialObservedFact{id,
                                event->kind,
                                event->actor_id,
                                event->tick,
                                {event->actor_id, event->target_actor_id}};
    return std::nullopt;
  }
  if (const auto *value = world_.growth.facts.find(id)) {
    if (const auto *fact = std::get_if<TrainingFact>(value);
        fact && fact->phase == "completed") {
      std::vector<StableId> participants{fact->training.student_id};
      if (!fact->training.teacher_id.empty())
        participants.push_back(fact->training.teacher_id);
      return SocialObservedFact{id, "training", fact->actor_id,
                                fact->completed_tick, std::move(participants)};
    }
    return std::nullopt;
  }
  if (const auto *fact = world_.personal.facts.find(id)) {
    const auto *battle = world_.personal.battles.find(fact->battle_id);
    if (!battle)
      return std::nullopt;
    return SocialObservedFact{id, "personal_battle", battle->controller_id,
                              fact->tick, fact->participants};
  }
  return std::nullopt;
}
bool BundleServices::task_exists(const StableId &id) const {
  return world_.tasks.tasks.find(id) != nullptr;
}
// 庄务只借同候选军团成员；军事永久muster来源由该领域的后继绑定提供。
EstateAccess BundleServices::estate_access() const {
  EstateAccess access;
  access.private_shop_staff = shop_workers_ ? &*shop_workers_ : nullptr;
  access.military_muster_estate = [this](const StableId &source) {
    return hanmo::resident::military_muster_estate(world_.military, source);
  };
  access.army_id = [this](const StableId &actor) {
    const auto *army = active_army_for_actor(world_.military, actor);
    return army ? army->id : StableId{};
  };
  return access;
}
// 共同validator只借当前家庭事实来证明动态来源，字段范围继续由共同store核对。
Status BundleServices::validate_dynamic_actor(const CommonRead &common,
                                              const ActorRecord &actor) const {
  if (&common != &world_.common)
    return common_failure("WrongFamilyCandidate", actor.id,
                          ErrorKind::internal);
  const auto *birth = world_.common.state().facts.find(actor.id);
  if (world_.family_birth_enabled() && birth &&
      birth->kind == "actor_creation" && birth->domain == "family_birth")
    return validate_biological_actor_identity(
        world_.common, *world_.catalog->common, *world_.catalog->family,
        *world_.catalog->family_lineage, world_.family_birth, actor);
  if (world_.catalog->exact_time &&
      exact_complete_selected(*world_.catalog->exact_time)) {
    if (!world_.actor_foundation || !world_.catalog->actor_foundation)
      return common_failure("CompleteFamilyOriginalSourcesRequired", actor.id);
    WorldCurrentMomentRead clock(world_);
    FamilyOriginV2Sources sources{*world_.actor_foundation,
                                  *world_.catalog->actor_foundation, clock};
    return validate_family_identity_v2(world_.common, world_.family,
                                       *world_.catalog->family,
                                       *world_.catalog->common, actor, sources);
  }
  return validate_family_identity(world_.common, world_.family,
                                  *world_.catalog->family, actor);
}
FamilyAccess
BundleServices::family_access(const InteractionAccess &interaction,
                              const FamilyOriginV2Sources *origins,
                              const LifeAgeSources *initial_age) const {
  FamilyAccess access;
  access.origin_v2 = origins;
  access.initial_age = initial_age;
  if (world_.family_birth_enabled())
    access.biological_identity = [this](const ActorRecord &actor) {
      return validate_biological_actor_identity(
          world_.common, *world_.catalog->common, *world_.catalog->family,
          *world_.catalog->family_lineage, world_.family_birth, actor);
    };
  access.interaction = &world_.interaction;
  access.interaction_catalog = world_.catalog->interaction.get();
  access.interaction_access = &interaction;
  access.tasks = &world_.tasks;
  access.estate_access = estate_access();
  access.economy = world_.dynamic_enabled() ? &world_.local_economy : nullptr;
  access.legacy =
      world_.family_legacy_enabled() ? &world_.family_legacy : nullptr;
  access.military_or_office_pending = [this](const StableId &actor,
                                             bool retirement) {
    if (retirement)
      for (const auto &office : world_.military.offices)
        if (office.enabled && office.holder_actor_id == actor)
          return true;
    for (const auto &army : world_.military.armies) {
      if (army.status == "released" || army.status == "closed" ||
          army.status == "disbanded" || army.status == "resigned")
        continue;
      if (army.commander_actor_id == actor ||
          std::find(army.member_actor_ids.begin(), army.member_actor_ids.end(),
                    actor) != army.member_actor_ids.end())
        return true;
    }
    return false;
  };
  access.initialize_schedule = [this](const StableId &actor,
                                      const StableId &prototype, bool enabled) {
    const auto *plan = enabled ? schedule(actor) : nullptr;
    auto *prior = world_.travel.itineraries.find(actor);
    if (!plan)
      return prior ? common_failure("UnexpectedItinerary", actor)
                   : common_success();
    const auto *person = world_.common.state().actors.find(actor);
    if (!person || !person->prototype_id ||
        *person->prototype_id != prototype || !person->created_tick ||
        *person->created_tick < 0 ||
        *person->created_tick > world_.common.tick())
      return common_failure("InvalidActorCreationTick", actor);
    if (plan->destinations.empty() || plan->start_tick < 0 ||
        plan->start_tick > 2147483647)
      return common_failure("InvalidItinerary", actor);
    if (prior)
      return prior->index >= 0 &&
                     static_cast<std::size_t>(prior->index) <
                         plan->destinations.size() &&
                     prior->ready_tick >= 0 && prior->ready_tick <= 2147483647
                 ? common_success()
                 : common_failure("InvalidItinerary", actor);
    world_.travel.itineraries.insert(
        actor,
        {0, std::max(*person->created_tick, plan->start_tick), false, {}});
    return common_success();
  };
  return access;
}
// 每次返回的闭包只借当前Bundle，不能保存到另一候选或跨恢复代次。
// 借当前冻结合同证明仍需收货的既有责任，不创造承诺，也不恢复离职或撤销的职责。
bool BundleServices::has_existing_contract_service(
    const StableId &provider, const StableId &role_id) const {
  if (!world_.dynamic_enabled())
    return false;
  const auto *role = world_.common.state().roles.find(role_id);
  const auto *person = world_.common.actor(provider);
  if (!role || role->actor_id != provider || !person || !person->alive ||
      !person->available)
    return false;
  for (const auto &duty : world_.organization.duties.rows())
    if (duty.second.active && duty.second.actor_id == provider &&
        duty.second.role_id == role_id) {
      auto member = organization_access().member_view(
          provider, duty.second.organization_id);
      if (!member || member->response == "departed")
        continue;
      for (const auto &entry : world_.tasks.contracts.rows()) {
        const auto &contract = entry.second;
        if (contract.status == ContractStatus::settled ||
            contract.issuer.kind != Owner::Kind::organization ||
            contract.issuer.id != duty.second.organization_id ||
            !contract.current_task_id ||
            contract.frozen_terms.definition.report_role != role_id)
          continue;
        const auto *task = world_.tasks.tasks.find(*contract.current_task_id);
        if (task && task->contract_id == std::optional<StableId>(contract.id))
          return true;
      }
    }
  return false;
}
InteractionAccess BundleServices::interaction_access(
    const CompleteExactWorldReadBindings *actual) const {
  // 只绑定当前实际session；完整出生/控制来源由World晚门核验。

  InteractionAccess access;
  // 回调只借当前 World；每次反读真实看管，不把 unavailable 本身当放行依据。
  access.custody_dialogue_available = [this, actual](const StableId &actor) {
    return read_native_military_custody_permission(
        world_, actor, NamedMilitaryCustodyCommandKind::co_located_dialogue, actual);
  };
  const auto custody = access.custody_dialogue_available;
  access.supply_inspection_selected =
      bool(world_.catalog->public_supply_inspection);
  if (access.supply_inspection_selected) {
    access.inspect_supply_preview = [this, actual](const StableId &actor,
                                                   const StableId &prop) {
      return preview_native_public_supply_inspection(world_, actual, actor,
                                                     prop);
    };
    access.inspect_supply_apply = [this, actual](const StableId &actor,
                                                 const StableId &prop) {
      return apply_native_public_supply_inspection(world_, actual, actor, prop);
    };
  }
  access.social_occupancy_selected =
      world_.catalog->actor_social_occupancy_selected;
  access.social_occupancy =
      social_occupancy_ ? &social_occupancy_->write() : nullptr;
  access.born_personal_session = [this](const InteractionSession &session) {
    return match_born_personal_dialogue_session_world(world_, session);
  };
  access.known_personal_session = [this](const InteractionSession &session) {
    return match_known_personal_dialogue_session_world(world_, session);
  };
  if (world_.regional_life_enabled()) {
    const auto categories = [this](const StableId &speaker) {
      std::vector<StableId> out;
      for (const auto &topic :
           world_.catalog->interaction->regional_actor_topics)
        if (topic.actor_id == speaker &&
            std::find(out.begin(), out.end(), topic.activity) == out.end())
          out.push_back(topic.activity);
      return out;
    };
    access.news_preview = [this, categories, custody](const StableId &listener,
                                                      const StableId &speaker)
        -> Outcome<std::shared_ptr<const SocialNewsQuote>> {
      auto result = preview_social_news(
          world_.common, world_.social, *world_.catalog->common, listener,
          speaker, categories(speaker), this, custody);
      if (!result.ok)
        return {false, {}, result.error};
      return {true,
              std::make_shared<const SocialNewsQuote>(std::move(result.value)),
              {}};
    };
    access.news_apply =
        [this, categories, custody](
            const StableId &listener, const StableId &speaker,
            const SocialNewsQuote &quote) -> Outcome<std::vector<StableId>> {
      auto result = apply_social_news(
          world_.common, world_.social, *world_.catalog->common, listener,
          speaker, quote, categories(speaker), this, custody);
      if (!result.ok)
        return {false, {}, result.error};
      return {true, std::move(result.value.events), {}};
    };
    access.regional_candidates = [this, custody](const StableId &observer,
                                                 const StableId &place) {
      auto issued = PoliticalTakeoverExecutionWorldAccess::create(world_);
      if (!issued.ok)
        return Outcome<std::vector<RegionalOpportunityBinding>>{
            false, {}, issued.error};
      auto authority = organization_access();
      RegionalSharePreview share =
          [this, custody](const StableId &report, const StableId &speaker,
                          const StableId &listener,
                          const std::vector<StableId> &categories) {
            return preview_share_life_report(world_.common, world_.social,
                                             report, speaker, listener,
                                             categories, custody);
          };
      return resolve_regional_life_candidates(
          world_.read(), *world_.catalog, authority, *this, share, observer,
          place, issued.value->issued_supply(),
          world_.tasks.exact_quantities ? &occupation_clock_ : nullptr);
    };
    access.regional_outcome_text =
        [this](const RegionalOpportunityBinding &binding) {
          return regional_outcome_text(
              world_.common, world_.tasks, *world_.catalog->tasks, binding,
              world_.tasks.exact_quantities ? &occupation_clock_ : nullptr);
        };
    access.regional_share =
        [this, custody](const RegionalOpportunityBinding &binding) {
          for (const auto &topic :
               world_.catalog->interaction->regional_actor_topics)
            if (topic.id == binding.topic_id &&
                topic.actor_id == binding.provider_actor_id)
              return share_life_report(
                  world_.common, world_.social, binding.source.report_id,
                  binding.provider_actor_id, binding.observer_actor_id,
                  {topic.activity}, custody);
          return common_failure("RegionalSharingNotAllowed");
        };
  }
  access.resolve_location = [this](const StableId &actor) {
    const auto location = resolved_location(actor);
    return location.ok ? location.value : unresolved_location();
  };
  access.party_contains = [this](const StableId &actor,
                                 const StableId &member) {
    const auto id = party_id(actor);
    const auto *party = world_.common.state().parties.find(id);
    return party && std::find(party->members.begin(), party->members.end(),
                              member) != party->members.end();
  };
  access.task_status = [this](const StableId &actor, const StableId &offer) {
    return task_condition_status(world_.common, world_.tasks,
                                 world_.interaction, actor, offer);
  };
  access.offer_declined_before =
      [this](const StableId &actor, const StableId &offer) -> Outcome<bool> {
    if (!world_.catalog->task_offer_responses || !world_.tasks.offer_responses)
      return {false, false,
              common_failure("TaskOfferResponseProfileRequired").error};
    const auto meeting = interaction_access();
    return evaluate_task_offer_revisit_condition(
        world_.common, *world_.catalog->tasks, world_.interaction,
        *world_.catalog->interaction, meeting, *world_.tasks.offer_responses,
        actor, {offer, true});
  };
  access.treaty_personal_passage = [this](const StableId &actor,
                                          const StableId &place,
                                          const StableId &terms) {
    return treaty_passage_world_admission(world_, actor, place, terms);
  };
  access.relation_trust = [this](const StableId &from,
                                 const StableId &to) -> std::optional<Integer> {
    for (const auto &row : world_.social.relations.rows())
      if (row.second.from_actor == from && row.second.to_actor == to)
        return row.second.trust;
    return std::nullopt;
  };
  // 欠薪拒绝只阻止新服务；匹配真实参与行动的教师仍完成已经接受的课时。
  access.service_admission = [this](const StableId &requester,
                                    const StableId &role_id,
                                    const StableId &kind,
                                    const StableId &own_action) {
    if (!world_.dynamic_enabled())
      return common_success();
    for (const auto &row : world_.organization.duties.rows())
      if (row.second.active && row.second.role_id == role_id) {
        auto member = organization_access().member_view(
            row.second.actor_id, row.second.organization_id);
        if (!member || member->accepts_new_duty)
          continue;
        if (kind == "report_offer" &&
            has_existing_contract_service(row.second.actor_id, role_id)) {
          bool existing = false;
          for (const auto &entry : world_.tasks.contracts.rows()) {
            const auto &contract = entry.second;
            if (contract.status == ContractStatus::settled ||
                !contract.current_task_id ||
                contract.issuer.kind != Owner::Kind::organization ||
                contract.issuer.id != row.second.organization_id ||
                contract.frozen_terms.definition.report_role != role_id)
              continue;
            const auto *task =
                world_.tasks.tasks.find(*contract.current_task_id);
            if (task && task->assignee == requester &&
                task->contract_id == std::optional<StableId>(contract.id)) {
              existing = true;
              break;
            }
          }
          if (existing)
            continue;
        }
        const auto *action =
            own_action.empty() ? nullptr : world_.common.action(own_action);
        const auto *actor = world_.common.actor(row.second.actor_id);
        if (action && actor && actor->action_id == own_action &&
            std::find(action->participants.begin(), action->participants.end(),
                      actor->id) != action->participants.end())
          continue;
        return common_failure("MemberDeclinesNewDuty", row.second.actor_id);
      }
    return common_success();
  };
  access.role =
      [this](const StableId &id) -> std::optional<RoleAssignmentView> {
    const auto *role = world_.common.state().roles.find(id);
    if (!role)
      return std::nullopt;
    return RoleAssignmentView{role->actor_id, role->enabled};
  };
  access.battle_occupied = [this](const StableId &actor) {
    return in_open_battle(actor);
  };
  access.terrain_point_valid = [this](const StableId &map_id,
                                      const PointI &point) {
    const auto *map = world_.catalog->travel->maps.find(map_id);
    return map &&
           terrain::point_valid_prepared(*map, point, terrain::Mode::foot, {});
  };
  access.journey_encounter =
      [this](const StableId &id) -> Outcome<std::pair<StableId, bool>> {
    const auto *action = world_.common.action(id);
    if (!action || action->kind != "travel")
      return {false, {}, common_failure("EncounterWithoutJourney", id).error};
    return travel_encounter(world_.travel, id);
  };
  access.set_journey_encounter =
      [this](const StableId &id, const StableId &encounter, bool paused) {
        // 单一安装入口负责非空会面ID；此回调只用于C道别恢复原暂停位。
        if (!encounter.empty())
          return common_failure("UnsupportedNativeCapability", id,
                                ErrorKind::unsupported_capability);
        return resume_after_encounter(world_.travel, id, paused);
      };
  // 同行仍按原在役真人名册读军队归属，军俘不会被当作可邀请的行动成员。
  access.army_id = [this](const StableId &actor) {
    const auto *army = active_army_for_actor(world_.military, actor);
    return army ? army->id : StableId{};
  };
  // 店铺条款仅借增长/经济域原目录；岗位许可由Interaction.validate先行检查。
  access.shop_exists = [this](const StableId &shop) {
    return world_.catalog->growth &&
           world_.catalog->growth->shops.find(shop) != nullptr;
  };
  access.shop_representative = [this](const StableId &shop,
                                      const InteractionContext &context) {
    const auto *definition = world_.catalog->growth
                                 ? world_.catalog->growth->shops.find(shop)
                                 : nullptr;
    return definition && definition->place_id == context.place_id &&
           std::find(definition->representative_roles.begin(),
                     definition->representative_roles.end(),
                     context.role_id) != definition->representative_roles.end();
  };
  // 同行增删唯一落共同party primitive，完整保留事务回滚和成员读版本失效。
  access.write_party = [this](const StableId &id,
                              const std::optional<PartyRecord> &party) {
    return world_.common.apply_party(id, party);
  };
  // 自动发现没有特权：仍复用原Battle/Siege/Encounter/人物忙碌守卫。
  access.automatic_discovery_guard = [this](const StableId &actor) {
    return command_common_guard(world_, actor, "discover_offer");
  };
  // 归营庄卫只能凭真实永久回执和仍有效的借调链重续；此读取不释放借调或改军队。
  access.returned_retainer_context =
      [this](const StableId &actor, const StableId &target,
             const StableId &receipt) -> Outcome<InteractionContext> {
    const auto *header = world_.common.state().facts.find(receipt);
    const MilitaryReturnFact *fact = nullptr;
    for (const auto &candidate : world_.military.return_facts)
      if (candidate.id == receipt)
        fact = &candidate;
    const auto *person = world_.common.actor(target);
    if (!header || header->kind != "military_return" || !fact || !person ||
        target == actor ||
        std::find(fact->returned_retainers.begin(),
                  fact->returned_retainers.end(),
                  target) == fact->returned_retainers.end())
      return {false, {}, common_failure("RetainerReturnReceiptRequired").error};
    const ArmyRecord *army = nullptr;
    for (const auto &candidate : world_.military.armies)
      if (candidate.id == fact->army_id)
        army = &candidate;
    const RetainerRecord *pledge = nullptr;
    if (army)
      for (const auto &candidate : army->retainers)
        if (candidate.actor_id == target)
          pledge = &candidate;
    const auto *estate =
        pledge ? world_.estate.estates.find(pledge->estate_id) : nullptr;
    const auto *source = estate ? estate->detached.find(target) : nullptr;
    if (!army || army->status != "closed" || !estate ||
        estate->owner_actor_id != actor || !source || !pledge ||
        *source != pledge->source_id)
      return {false, {}, common_failure("EstateDetachmentMismatch").error};
    const auto actor_location = resolved_location(actor);
    const auto target_location = resolved_location(target);
    const auto *actor_place =
        actor_location.ok
            ? std::get_if<SettlementLocation>(&actor_location.value)
            : nullptr;
    const auto *target_place =
        target_location.ok
            ? std::get_if<SettlementLocation>(&target_location.value)
            : nullptr;
    if (!person->alive || !person->available || !person->action_id.empty() ||
        !actor_place || !target_place ||
        actor_place->settlement_id != target_place->settlement_id ||
        actor_place->place_id != target_place->place_id)
      return {false, {}, common_failure("PersonAbsent").error};
    for (const auto &row : world_.common.state().roles.rows()) {
      const auto *definition = world_.catalog->common->roles.find(row.first);
      if (!row.second.enabled || row.second.actor_id != target || !definition ||
          std::find(definition->services.begin(), definition->services.end(),
                    "join_party") == definition->services.end())
        continue;
      InteractionContext context;
      context.place_id = actor_place->place_id;
      context.provider_actor_id = target;
      context.role_id = row.first;
      context.field_order = {"place_id", "provider_actor_id", "role_id"};
      return {true, std::move(context), {}};
    }
    return {false, {}, common_failure("ServiceNotOffered").error};
  };

  return access;
}
} // namespace hanmo::resident
