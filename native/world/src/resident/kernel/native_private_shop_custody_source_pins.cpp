// 同源接受、产权历史与原营业依赖的有限中央映射；未知根不按名称猜 schema。
#include "native_private_shop_custody_source_pins.hpp"
#include "common_command_sources.hpp"
#include "exact_complete_world.hpp"
#include "native_character_family_world.hpp"
#include "native_complete_world_sources.hpp"
#include "native_private_shop_household_custody.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include "resident/domains/commerce/private_shop_age3.hpp"
#include "resident/domains/commerce/private_shop_capability.hpp"
#include "resident/domains/commerce/private_shop_opening_provenance.hpp"
#include "resident/domains/commerce/private_shop_operation_sources.hpp"
#include "resident/domains/family/life_age.hpp"
#include "resident/domains/growth/growth_skill_origin.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_exact2.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_original_provenance.hpp"
#include "resident/domains/politics/takeover/politics_takeover_state.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff_state.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_catalog.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_state.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_catalog.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_state.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_state.hpp"
#include "resident/domains/tasks/static_assets/task_static_asset_state.hpp"
#include "resident/domains/tasks/task_public_account_state.hpp"
#include "resident/domains/tasks/task_quantity_time2.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <algorithm>
#include <limits>
#include <type_traits>
namespace hanmo::resident {
namespace {
using Slot = NativePrivateShopCustodyPinSlot;
using RowOwner = NativePrivateShopCustodyRowOwner;
using ASlot = CharacterAvailabilityProvenanceSlot;
// 这里只报真实根/借址错误，不把失败变成空保留集合。
Status bad(const char *code) {
  return common_failure(code, {}, ErrorKind::internal);
}
bool same_rule(const ActorFoundationRuleRef &a,
               const ActorFoundationRuleRef &b) {
  return a.id == b.id && a.version == b.version &&
         a.catalog_digest == b.catalog_digest;
}
// 表键直接借原 OrderedTable；不得保留临时 source DTO 中的字符串地址。
template <class T>
const StableId *key_for(const OrderedTable<T> &table, const T *row) {
  for (const auto &r : table.rows())
    if (&r.second == row)
      return &r.first;
  return nullptr;
}
} // namespace
class NativePrivateShopCustodyPinCollector final
    : public PrivateShopHouseholdCustodyPinSink,
      public PrivateShopOperationSourcePinSink,
      public PrivateShopOpeningProvenanceSink,
      public PrivateShopAgeBasisSink3,
      public PoliticalShopLicensingOriginalSink,
      public CharacterAvailabilityProvenanceSink {
  const WorldBundle &w_;
  const PrivateShopHouseholdCustodyContext &x_;
  const CharacterAvailabilityRetentionContext &availability_owned_;
  const CharacterAvailabilitySourceRetentionContext &availability_sources_;
  NativePrivateShopCustodySourcePins out_;
  const PrivateShopHouseholdCustodyAcceptedSource *consumer_ = nullptr;
  std::size_t cut_ = 0;
  const FactRecord *accepted_head_ = nullptr;
  const PrivateShopRecord *opening_ = nullptr;
  const FactRecord *opening_head_ = nullptr;
  std::size_t opening_cut_ = 0;
  const FactRecord *permission_head_ = nullptr;
  std::size_t permission_cut_ = 0;
  bool opening_slot(Slot slot) const noexcept {
    return slot == Slot::opening || slot == Slot::opening_catalog ||
           slot == Slot::opening_growth || slot == Slot::opening_task ||
           slot == Slot::opening_permission ||
           slot == Slot::opening_availability;
  }
  std::size_t cut(Slot slot) const noexcept {
    if (slot == Slot::opening_permission || slot == Slot::opening_availability)
      return permission_cut_;
    return opening_slot(slot) ? opening_cut_ : cut_;
  }
  Status status_ = common_success();
  // 域 visitor 的首错必须保留，不能靠后续空槽覆盖。
  Status reject(Status v) {
    if (status_.ok && !v.ok)
      status_ = std::move(v);
    return status_;
  }
  Status append(Slot slot, NativePrivateShopCustodyDependency pin,
                bool before = true) {
    if (!status_.ok)
      return status_;
    if (!consumer_ || x_.state.accepted.find(consumer_->id) != consumer_)
      return reject(bad("CustodyPinActualConsumerRequired"));
    const auto *boundary =
        slot == Slot::opening_permission || slot == Slot::opening_availability
            ? permission_head_
        : opening_slot(slot) ? opening_head_
                             : accepted_head_;
    if (!boundary || w_.common.state().facts.find(boundary->id) != boundary)
      return reject(bad("CustodyPinActualCutRequired"));
    out_.pins_.push_back({consumer_, slot, boundary, before, std::move(pin)});
    return common_success();
  }
  // 每个调用处明确给原表；此处再核 key 对应的实际行地址。
  template <class T>
  Status table(Slot slot, RowOwner owner, const OrderedTable<T> &root,
               const T *row, bool before = true) {
    const auto *key = row ? key_for(root, row) : nullptr;
    if (!key || root.find(*key) != row)
      return reject(bad("CustodyPinActualTableRowRequired"));
    return append(
        slot, NativePrivateShopCustodyOwnedRowPin{owner, &root, row, key, 0},
        before);
  }
  template <class V>
  Status vector_row(Slot slot, RowOwner owner, const V &root, std::size_t n,
                    bool before = true) {
    if (n >= root.size())
      return reject(bad("CustodyPinActualVectorRowRequired"));
    return append(
        slot,
        NativePrivateShopCustodyOwnedRowPin{owner, &root, &root[n], nullptr, n},
        before);
  }
  // 反查真实 Common 头；是否在旧 cut 之后单独标记而不丢失已检查 suffix。
  Status head(Slot slot, SourceRefBuildView ref) {
    // 借原视图核唯一真实头与追加序；再借原表键，不构造临时来源。
    auto p = read_actual_common_fact_position(w_.common, ref);
    if (!p.ok)
      return reject({false, {}, std::move(p.error)});
    const auto &facts = w_.common.state().facts;
    const auto *h = facts.find(facts.order()[p.value]);
    if (!h || h->id != ref.id || h->domain != ref.domain ||
        h->kind != ref.kind)
      return reject(bad("CustodyPinActualCommonHeadRequired"));
    return append(slot,
                  NativePrivateShopCustodyCommonPin{&w_.common, h, p.value},
                  p.value < cut(slot));
  }
  // 初始人物源是原拥有对象，不把 construction/registration 结果变成 Common
  // 事实。
  // 原表别名与历史是否在 cut 前是两件事；后缀仍保留但显式标 false。
  Outcome<bool> before(SourceRefBuildView ref) const {
    auto position = read_actual_common_fact_position(w_.common, ref);
    if (!position.ok)
      return {false, false, std::move(position.error)};
    return {true, position.value < cut_, {}};
  }
  Status original_actor_rows(const StableId &id,
                             const StableId *construction = nullptr) {
    if (!w_.actor_foundation || !w_.catalog->actor_foundation)
      return reject(bad("CustodyPinOriginalActorRootsRequired"));
    const auto &f = *w_.actor_foundation;
    const auto &cat = *w_.catalog->actor_foundation;
    const auto *origin = f.character_origin_records.origins.find(id);
    if (!origin)
      return reject(bad("CustodyPinCharacterOriginRequired"));
    auto v = table(Slot::initial_origin, RowOwner::character_origin,
                   f.character_origin_records.origins, origin);
    if (!v.ok)
      return v;
    const auto *seed = f.actor_start_context.initial_condition_seeds.find(
        origin->initial_condition_seed_ref.seed_key);
    v = table(Slot::initial_origin, RowOwner::condition_seed,
              f.actor_start_context.initial_condition_seeds, seed);
    if (!v.ok)
      return v;
    if (origin->origin_kind == CharacterOriginRecordV2::Kind::created) {
      const auto *source = std::get_if<CreatedCharacterSource>(&origin->source);
      const auto *r =
          source ? f.character_origin_records.registration_sources.find(
                       source->creation_result_ref)
                 : nullptr;
      v = table(Slot::initial_origin, RowOwner::registration,
                f.character_origin_records.registration_sources, r);
      if (!v.ok)
        return v;
      return head(Slot::initial_origin, {"family", "actor_creation", id});
    }
    if (origin->origin_kind != CharacterOriginRecordV2::Kind::authored)
      return reject(bad("CustodyPinInitialOriginRouteRequired"));
    const auto *definition = cat.actors.find(id);
    if (!definition)
      return reject(bad("CustodyPinAuthoredDefinitionRequired"));
    v = table(Slot::initial_origin, RowOwner::authored_actor, cat.actors,
              definition);
    if (!v.ok)
      return v;
    std::optional<std::size_t> n;
    for (std::size_t i = 0; i < f.actor_start_context.scenario_origins.size();
         ++i)
      if (actor_foundation_origin_equal(
              f.actor_start_context.scenario_origins[i].ref,
              definition->scenario_origin_ref)) {
        if (n)
          return reject(bad("CustodyPinDuplicateScenarioOrigin"));
        n = i;
      }
    if (!n)
      return reject(bad("CustodyPinScenarioOriginRequired"));
    v = vector_row(Slot::initial_origin, RowOwner::scenario_origin,
                   f.actor_start_context.scenario_origins, *n);
    if (!v.ok)
      return v;
    const ActorConstructionReceipt *receipt = nullptr;
    for (const auto &r : f.actor_start_context.construction_receipts.rows())
      if (actor_foundation_origin_equal(r.second.scenario_origin_ref,
                                        definition->scenario_origin_ref) &&
          std::find(r.second.constructed_actor_ids.begin(),
                    r.second.constructed_actor_ids.end(),
                    id) != r.second.constructed_actor_ids.end()) {
        if (receipt || r.first != r.second.initialization_result_ref ||
            (construction && *construction != r.first))
          return reject(bad("CustodyPinConstructionIdentityRequired"));
        receipt = &r.second;
      }
    v = table(Slot::initial_origin, RowOwner::construction_receipt,
              f.actor_start_context.construction_receipts, receipt);
    if (!v.ok)
      return v;
    v = table(Slot::initial_origin, RowOwner::actor_age_basis,
              f.actor_start_context.actor_age_bases,
              f.actor_start_context.actor_age_bases.find(id));
    if (!v.ok)
      return v;
    return table(Slot::initial_origin, RowOwner::actor_domain_binding,
                 f.actor_start_context.actor_domain_bindings,
                 f.actor_start_context.actor_domain_bindings.find(id));
  }
  // 原 Estate 当前与历史链会读所有 succession；只 pin cut 前列表会漏掉真实
  // suffix。
  Status title_history() {
    const auto &inventory = consumer_->accepted_service.custody_inventory_id;
    bool estate = false;
    for (const auto &r : x_.market.inventory_titles.estates.estates.rows())
      if (r.second.inventory_id == inventory) {
        if (estate)
          return reject(bad("CustodyPinEstateAlias"));
        estate = true;
        auto v = table(Slot::title_checked_history, RowOwner::estate_definition,
                       x_.market.inventory_titles.estates.estates, &r.second);
        if (!v.ok)
          return v;
        v = table(Slot::title_checked_history, RowOwner::estate_current,
                  w_.estate.estates, w_.estate.estates.find(r.first));
        if (!v.ok)
          return v;
        // 原 current Estate title 同时检查真实钱包产权，不能只保库存。
        v = table(Slot::title_checked_history, RowOwner::wallet,
                  w_.common.state().wallets,
                  w_.common.wallet(r.second.wallet_id));
        if (!v.ok)
          return v;
        v = original_actor_rows(r.second.owner_actor_id);
        if (!v.ok)
          return v;
      }
    for (std::size_t i = 0; i < w_.family.successions.size(); ++i) {
      const auto &id = w_.family.successions[i];
      auto applied = before({"family", "family_succession", id});
      if (!applied.ok)
        return reject({false, {}, std::move(applied.error)});
      auto v = vector_row(Slot::title_checked_history,
                          RowOwner::family_succession_order, w_.family.successions,
                          i, applied.value);
      if (!v.ok)
        return v;
      v = head(Slot::title_checked_history,
               {"family", "family_succession", id});
      if (!v.ok)
        return v;
      const auto *r = w_.family.facts.find(id);
      v = table(Slot::title_checked_history, RowOwner::family_fact,
                w_.family.facts, r, applied.value);
      if (!v.ok)
        return v;
      if (estate) {
        const auto *f = r ? std::get_if<FamilyChoice>(r) : nullptr;
        if (!f || !f->designation_source)
          return reject(bad("CustodyPinEstateDesignationRequired"));
        const auto *d = w_.family.facts.find(*f->designation_source);
        auto adopted =
            before({"family", "family_designation", *f->designation_source});
        if (!adopted.ok)
          return reject({false, {}, std::move(adopted.error)});
        v = table(Slot::title_checked_history, RowOwner::family_fact,
                  w_.family.facts, d, adopted.value);
        if (!v.ok)
          return v;
        v = head(Slot::title_checked_history,
                 {"family", "family_designation", *f->designation_source});
        if (!v.ok)
          return v;
      }
    }
    for (std::size_t i = 0; i < w_.military.occupation_facts.size(); ++i) {
      const auto &f = w_.military.occupation_facts[i];
      auto applied = before({"military", "military_occupation", f.id});
      if (!applied.ok)
        return reject({false, {}, std::move(applied.error)});
      auto v =
          vector_row(Slot::title_checked_history, RowOwner::military_occupation,
                     w_.military.occupation_facts, i, applied.value);
      if (!v.ok)
        return v;
      v = head(Slot::title_checked_history,
               {"military", "military_occupation", f.id});
      if (!v.ok)
        return v;
    }
    for (std::size_t i = 0; i < w_.naval.battle.captures.size(); ++i) {
      const auto &f = w_.naval.battle.captures[i];
      auto applied = before({"military_naval", "naval_capture", f.id});
      if (!applied.ok)
        return reject({false, {}, std::move(applied.error)});
      auto v = vector_row(Slot::title_checked_history, RowOwner::naval_capture,
                          w_.naval.battle.captures, i, applied.value);
      if (!v.ok)
        return v;
      v = head(Slot::title_checked_history,
               {"military_naval", "naval_capture", f.id});
      if (!v.ok)
        return v;
      bool found = false;
      for (std::size_t n = 0;
           n < x_.market.inventory_titles.naval_catalog.vessels.size(); ++n)
        if (x_.market.inventory_titles.naval_catalog.vessels[n].id ==
            f.vessel_id) {
          if (found)
            return reject(bad("CustodyPinDuplicateVessel"));
          found = true;
          v = vector_row(Slot::title_checked_history, RowOwner::naval_vessel,
                         x_.market.inventory_titles.naval_catalog.vessels, n);
          if (!v.ok)
            return v;
        }
      if (!found)
        return reject(bad("CustodyPinVesselRequired"));
    }
    return common_success();
  }

public:
  NativePrivateShopCustodyPinCollector(
      const WorldBundle &w, const PrivateShopHouseholdCustodyContext &x,
      const CharacterAvailabilityRetentionContext &owned,
      const CharacterAvailabilitySourceRetentionContext &sources)
      : w_(w), x_(x), availability_owned_(owned),
        availability_sources_(sources) {
    out_.world_ = &w;
  }
  // 每次切到一个真实接受行，绝不把相同槽下多个 consumer 合成一个键。
  Status collect(const PrivateShopHouseholdCustodyAcceptedSource &r) {
    consumer_ = &r;
    auto p = read_actual_common_fact_position(
        w_.common,
        SourceRefBuildView{"local_economy",
                           "private_shop_household_custody_accepted", r.id});
    if (!p.ok)
      return reject({false, {}, std::move(p.error)});
    cut_ = p.value;
    accepted_head_ = w_.common.state().facts.find(r.id);
    opening_ = nullptr;
    opening_head_ = nullptr;
    permission_head_ = nullptr;
    auto v = append(Slot::accepted, NativePrivateShopCustodyCommonBoundaryPin{
                                        &w_.common, &w_.common.state().identity,
                                        &w_.common.state().next_instance_id});
    if (!v.ok)
      return v;
    v = table(Slot::accepted, RowOwner::accepted, x_.state.accepted, &r);
    if (!v.ok)
      return v;
    v = head(Slot::accepted, {"local_economy",
                              "private_shop_household_custody_accepted", r.id});
    if (!v.ok)
      return v;


    auto memory = CheckpointMemoryCharge(0);
    const MomentOwnerSource actual{{"local_economy",
                                    "private_shop_household_custody_accepted",
                                    checkpoint_fresh_string_copy(r.id)},
                                   1};
    v = visit_private_shop_household_custody_pins(x_, actual, *this);
    if (!v.ok)
      return reject(std::move(v));
    v = title_history();
    if (!v.ok)
      return v;
    return status_;
  }
  NativePrivateShopCustodySourcePins finish() { return std::move(out_); }
  // 原明确托管 policy 只映同一锁目录中的实际版本，绝不查名称相似的替身。
  Status policy(const ActorFoundationRuleRef &r) override {
    const auto *p = x_.catalog.policies.find(r.id);
    if (!p || !same_rule(p->policy_ref, r))
      return reject(bad("CustodyPinPolicyMismatch"));
    auto v = table(Slot::policy, RowOwner::policy, x_.catalog.policies, p);
    if (!v.ok)
      return v;
    return table(
        Slot::policy, RowOwner::premises,
        x_.market.operation.shops.catalog.premises,
        x_.market.operation.shops.catalog.premises.find(p->premises_id));
  }
  Status handling_rule(const StableId &id, Integer version) override {
    const auto *r = x_.travel.cargo_handling_rules.find(id);
    if (!r || r->version != version)
      return reject(bad("CustodyPinHandlingRuleMismatch"));
    return table(Slot::handling_rule, RowOwner::handling_rule,
                 x_.travel.cargo_handling_rules, r);
  }
  Status actor(const StableId &id) override {
    if (id != consumer_->proprietor_actor_id)
      return reject(bad("CustodyPinActorMismatch"));
    return table(Slot::actor, RowOwner::actor, w_.common.state().actors,
                 w_.common.state().actors.find(id));
  }
  Status inventory(const StableId &id) override {
    if (id != consumer_->accepted_service.custody_inventory_id)
      return reject(bad("CustodyPinInventoryMismatch"));
    return table(Slot::inventory, RowOwner::inventory,
                 w_.common.state().inventories, w_.common.inventory(id));
  }
  Status inventory_initial_origin(
      const PrivateInventoryInitialOrigin &origin) override {
    if (const auto *r = std::get_if<RequiredPrivateInventoryOrigin>(&origin)) {
      if (r->original_catalog_source.domain != "common_catalog" ||
          r->original_catalog_source.kind != "inventory" ||
          r->original_catalog_source.id !=
              consumer_->accepted_service.custody_inventory_id)
        return reject(bad("CustodyPinRequiredInventoryMismatch"));
      return table(Slot::initial_origin, RowOwner::required_inventory,
                   w_.catalog->common->required_inventories,
                   w_.catalog->common->required_inventories.find(
                       r->original_catalog_source.id));
    }
    if (const auto *r = std::get_if<AuthoredPersonalInventoryOrigin>(&origin))
      return original_actor_rows(r->actor_id, &r->initialization_result_ref);
    const auto &r = std::get<CreatedPersonalInventoryOrigin>(origin);
    if (r.source_schema != 2 || r.actual_creation_source.domain != "family" ||
        r.actual_creation_source.kind != "actor_creation" ||
        r.actual_creation_source.id != r.actor_id)
      return reject(bad("CustodyPinCreatedInventoryMismatch"));
    const auto *origin_row =
        w_.actor_foundation
            ? w_.actor_foundation->character_origin_records.origins.find(
                  r.actor_id)
            : nullptr;
    const auto *created =
        origin_row ? std::get_if<CreatedCharacterSource>(&origin_row->source)
                   : nullptr;
    if (!created || created->creation_result_ref != r.registration_result_ref)
      return reject(bad("CustodyPinCreatedRegistrationMismatch"));
    return original_actor_rows(r.actor_id);
  }
  // 无 schema 的原开店/产权引用以真实 table+Common 身份保留，不猜默认 /1。
  Status original_source(std::string_view slot, std::size_t ordinal,
                         const SourceRef &ref,
                         std::optional<Integer> schema) override {
    if (slot == "operation_acceptance") {
      if (ordinal || schema != std::optional<Integer>{2} ||
          ref.id != consumer_->operation_id)
        return reject(bad("CustodyPinOperationSourceMismatch"));
      const auto &o = x_.market.operation;
      auto actual =
          read_original_private_shop_operation_acceptance(o, {ref, 2});
      if (!actual.ok)
        return reject({false, {}, std::move(actual.error)});
      auto v = head(Slot::operation, {ref.domain, ref.kind, ref.id});
      if (!v.ok)
        return v;
      v = table(Slot::operation, RowOwner::operation, w_.private_shops.operations,
                w_.private_shops.operations.find(ref.id));
      if (!v.ok)
        return v;
      bool found = false;
      for (std::size_t i = 0; i < o.sources.accepted_operations.size(); ++i)
        if (&o.sources.accepted_operations[i] == actual.value) {
          if (found)
            return reject(bad("CustodyPinOperationAlias"));
          found = true;
          v = vector_row(Slot::operation, RowOwner::operation_acceptance,
                         o.sources.accepted_operations, i);
          if (!v.ok)
            return v;
        }
      if (!found)
        return reject(bad("CustodyPinOperationOwnerRequired"));
      found = false;
      for (std::size_t i = 0; i < o.capabilities.operation_profiles.size(); ++i)
        if (o.capabilities.operation_profiles[i].operation_ref == ref.id) {
          if (found)
            return reject(bad("CustodyPinOperationProfileAlias"));
          found = true;
          v = vector_row(Slot::operation, RowOwner::operation_profile,
                         o.capabilities.operation_profiles, i);
          if (!v.ok)
            return v;
        }
      if (!found)
        return reject(bad("CustodyPinOperationProfileRequired"));
      v = visit_private_shop_operation_acceptance_pins(o, {ref, 2}, *this);
      return v.ok ? status_ : reject(std::move(v));
    }
    if (slot == "original_shop_epoch") {
      if (ordinal || schema)
        return reject(bad("CustodyPinOpeningSourceMismatch"));
      auto v = visit_original_private_shop_opening_provenance(
          x_.market.operation.shops, ref, *this);
      return v.ok ? status_ : reject(std::move(v));
    }
    if (slot == "inventory_title_application") {
      if (schema || ref.domain != "family" || ref.kind != "family_succession")
        return reject(bad("CustodyPinTitleSourceMismatch"));
      auto v = table(Slot::title_application, RowOwner::family_fact,
                     w_.family.facts, w_.family.facts.find(ref.id));
      if (!v.ok)
        return v;
      return head(Slot::title_application, {ref.domain, ref.kind, ref.id});
    }
    return reject(bad("CustodyPinUnknownOriginalSlot"));
  }
  // 开设 source 的完整 reader 成功后才接此原根；边界仍是该开设头，不是今日。
  Status opening(const PrivateShopContext &x,
                 const PrivateShopRecord &r) override {
    if (&x != &x_.market.operation.shops || &x.common != &w_.common ||
        &x.common_catalog != w_.catalog->common.get() ||
        &x.family_catalog != w_.catalog->family.get() ||
        &x.economy != &w_.local_economy ||
        &x.economy_catalog != w_.catalog->local_economy.get() ||
        &x.catalog != w_.catalog->private_shops.get() ||
        &x.interaction_catalog != w_.catalog->interaction.get() ||
        x.markets != w_.catalog->growth.get() ||
        w_.private_shops.shops.find(r.id) != &r)
      return reject(bad("CustodyOpeningSameWorldRootsRequired"));
    opening_ = &r;
    opening_head_ = w_.common.state().facts.find(r.id);
    if (!opening_head_ || opening_head_->domain != private_shop_module ||
        opening_head_->kind != private_shop_open_kind)
      return reject(bad("CustodyOpeningActualHeaderRequired"));
    auto pos = read_actual_common_fact_position(w_.common, *opening_head_);
    if (!pos.ok)
      return reject({false, {}, std::move(pos.error)});
    opening_cut_ = pos.value;
    auto v = table(Slot::opening, RowOwner::shop, w_.private_shops.shops, &r);
    if (!v.ok)
      return v;
    v = head(Slot::opening,
             {private_shop_module, private_shop_open_kind, r.id});
    if (!v.ok)
      return v;
    // 原 catalog validator 核所有铺位，不只本次选择的一行。
    for (const auto &row : x.catalog.premises.rows()) {
      const auto &p = row.second;
      v = table(Slot::opening_catalog, RowOwner::premises, x.catalog.premises, &p);
      if (!v.ok)
        return v;
      v = table(Slot::opening_catalog, RowOwner::place, x.common_catalog.places,
                x.common_catalog.places.find(p.place_id));
      if (!v.ok)
        return v;
      v = table(Slot::opening_catalog, RowOwner::role_definition,
                x.common_catalog.roles,
                x.common_catalog.roles.find(p.lessor_role_id));
      if (!v.ok)
        return v;
      v = table(Slot::opening_catalog, RowOwner::actor, w_.common.state().actors,
                w_.common.state().actors.find(p.lessor_actor_id));
      if (!v.ok)
        return v;
      v = table(Slot::opening_catalog, RowOwner::wallet, w_.common.state().wallets,
                w_.common.wallet(p.lessor_wallet_id));
      if (!v.ok)
        return v;
      v = table(Slot::opening_catalog, RowOwner::rent_rate,
                x.economy_catalog.rates,
                x.economy_catalog.rates.find(p.rent_rate_profile));
      if (!v.ok)
        return v;
      v = table(Slot::opening_catalog, RowOwner::original_market, x.markets->shops,
                x.markets->shops.find(p.market_shop_id));
      if (!v.ok)
        return v;
      bool found = false;
      for (std::size_t i = 0; i < x.interaction_catalog.dialogues.size(); ++i) {
        const auto &d = x.interaction_catalog.dialogues[i];
        if (d.id != p.agreement_dialogue_id)
          continue;
        for (std::size_t n = 0; n < d.nodes.size(); ++n) {
          if (d.nodes[n].id != p.agreement_node_id)
            continue;
          if (found)
            return reject(bad("CustodyOpeningDialogueAlias"));
          found = true;
          v = vector_row(Slot::opening_catalog, RowOwner::dialogue,
                         x.interaction_catalog.dialogues, i);
          if (!v.ok)
            return v;
          v = vector_row(Slot::opening_catalog, RowOwner::dialogue_node, d.nodes,
                         n);
          if (!v.ok)
            return v;
        }
      }
      if (!found)
        return reject(bad("CustodyOpeningDialogueRequired"));
    }
    return table(Slot::opening_catalog, RowOwner::actor, w_.common.state().actors,
                 w_.common.state().actors.find(r.terms.proprietor_actor_id));
  }
  // 只收原 commerce Study
  // 全相关行，含已检查的开设后后缀；不把计入XP列表当全量。
  Status growth_study(const GrowthState &g,
                      const GrowthSkillOriginContext *origins) override {
    if (!opening_ || &g != &w_.growth ||
        (origins && (&origins->common != w_.catalog->common.get() ||
                     &origins->family_catalog != w_.catalog->family.get() ||
                     &origins->family != &w_.family ||
                     (origins->births && origins->births != &w_.family_birth))))
      return reject(bad("CustodyOpeningGrowthRootsRequired"));
    const auto &id = opening_->terms.proprietor_actor_id;
    for (const auto &row : g.facts.rows()) {
      const auto *study = std::get_if<StudyFact>(&row.second);
      if (!study || study->terms.student_id != id ||
          study->terms.skill_id != "commerce")
        continue;
      auto pos = read_actual_common_fact_position(
          w_.common,
          SourceRefBuildView{"cultivation", "skill_study", row.first});
      if (!pos.ok)
        return reject({false, {}, std::move(pos.error)});
      auto v = table(Slot::opening_growth, RowOwner::growth_study, g.facts,
                     &row.second, pos.value < opening_cut_);
      if (!v.ok)
        return v;
      v = head(Slot::opening_growth, {"cultivation", "skill_study", row.first});
      if (!v.ok)
        return v;
    }
    // 年龄只保原年龄 reader 实际读取的行；出租人不因商术的 prototype
    // 要求而新增资格。真正商术初态另在下方只对 proprietor 反查。
    const auto authored = [&](const StableId &actor) {
      const auto &ids = w_.catalog->common->actor_ids;
      return std::find(ids.begin(), ids.end(), actor) != ids.end();
    };
    const auto birth_rows = [&](const StableId &actor) -> Status {
      if (!origins || !origins->births)
        return reject(bad("CustodyOpeningBirthRootRequired"));
      const FamilyBirthRecord *birth = nullptr;
      const StableId *key = nullptr;
      for (const auto &row : origins->births->births.rows()) {
        if (row.second.actor_id != actor)
          continue;
        if (birth)
          return reject(bad("CustodyOpeningBirthAlias"));
        birth = &row.second;
        key = &row.first;
      }
      auto v = table(Slot::opening_growth, RowOwner::birth,
                     origins->births->births, birth);
      if (!v.ok)
        return v;
      v = head(Slot::opening_growth,
               {"family_birth", "family_birth_recorded", *key});
      if (!v.ok)
        return v;
      return head(Slot::opening_growth,
                  {"family_birth", "actor_creation", actor});
    };
    const auto &shops = x_.market.operation.shops;
    if (shops.catalog.version == 3 || shops.state.version == 3) {
      for (const auto *actor_id : {&opening_->terms.proprietor_actor_id,
                                   &opening_->terms.lessor_actor_id}) {
        auto v = visit_private_shop_age_basis_before_fact3(
            shops, *actor_id, opening_->opened_tick, opening_->id, *this);
        if (!v.ok)
          return reject(std::move(v));
      }
    } else {
      for (const auto *actor_id : {&opening_->terms.proprietor_actor_id,
                                   &opening_->terms.lessor_actor_id}) {
        const auto *actor = w_.common.state().actors.find(*actor_id);
        auto v = table(Slot::opening_growth, RowOwner::actor,
                       w_.common.state().actors, actor);
        if (!v.ok)
          return v;
        if (authored(*actor_id)) {
          const auto &members = w_.catalog->common->actor_ids;
          for (std::size_t n = 0; n < members.size(); ++n)
            if (members[n] == *actor_id) {
              v = vector_row(Slot::opening_growth, RowOwner::authored_actor_id,
                             members, n);
              if (!v.ok)
                return v;
            }
          continue;
        }
        const auto *creation = w_.common.state().facts.find(*actor_id);
        if (!creation || creation->kind != "actor_creation")
          return reject(bad("CustodyOpeningAgeCreationRequired"));
        if (creation->domain == "family_birth" && origins && origins->births) {
          v = birth_rows(*actor_id);
        } else {
          // 原 legacy age 只核真实创建头与当前不可变生日；不读 Family payload。
          v = head(Slot::opening_growth,
                   {creation->domain, creation->kind, creation->id});
        }
        if (!v.ok)
          return v;
      }
    }
    // 既有 GrowthSkillOrigin 的每个已准入分支都先核原 prototype；
    // 不从出租人的今日属性猜，也不把新的 Origin2 注册伪装为旧 Family 事实。
    const auto *actor = w_.common.state().actors.find(id);
    if (!actor)
      return reject(bad("CustodyOpeningOriginalPersonRequired"));
    const bool initial = authored(id);
    if (!initial && !actor->prototype_id)
      return reject(bad("CustodyOpeningSkillPrototypeRequired"));
    const auto &prototype = initial ? id : *actor->prototype_id;
    auto v = table(Slot::opening_growth, RowOwner::family_prototype,
                   w_.catalog->family->prototypes,
                   w_.catalog->family->prototypes.find(prototype));
    if (!v.ok)
      return v;
    if (initial) {
      return table(Slot::opening_growth, RowOwner::actor_initial_birth,
                   w_.catalog->common->actor_initial_birth,
                   w_.catalog->common->actor_initial_birth.find(id));
    }
    if (!origins)
      return reject(bad("CustodyOpeningSkillOriginRootRequired"));
    const auto *identity = w_.family.facts.find(id);
    if (!identity)
      return birth_rows(id);
    const auto *created = std::get_if<IdentityCreationFact>(identity);
    if (!created)
      return reject(bad("CustodyOpeningIdentityFactRequired"));
    v = table(Slot::opening_growth, RowOwner::family_fact, w_.family.facts,
              identity);
    if (!v.ok)
      return v;
    const auto *source = w_.family.facts.find(created->source);
    v = table(Slot::opening_growth, RowOwner::family_fact, w_.family.facts,
              source);
    if (!v.ok)
      return v;
    v = head(Slot::opening_growth, {"family", "actor_creation", id});
    if (!v.ok)
      return v;
    const auto *h = w_.common.state().facts.find(created->source);
    if (!h)
      return reject(bad("CustodyOpeningIdentityHeaderRequired"));
    return head(Slot::opening_growth, {h->domain, h->kind, h->id});
  }

  // Same validated age reader, only original owner identities; no age
  // calculation.
  Status basis(const LifeAgeSources &age, const StableId &actor,
               const FactRecord &actual,
               const LifeAgeBasisSnapshot &basis) override {
    const auto &shops = x_.market.operation.shops;
    if (!opening_ || !opening_head_ || &actual != opening_head_ ||
        w_.common.state().facts.find(actual.id) != &actual ||
        !shops.age_sources3 || &shops.age_sources3->basis != &age ||
        &shops.age_sources3->clock != &x_.market.operation.clock ||
        &age.common != &w_.common || !w_.actor_foundation ||
        &age.foundation != &*w_.actor_foundation ||
        &age.foundation_catalog != w_.catalog->actor_foundation.get() ||
        &age.family != &w_.family || &age.births != &w_.family_birth ||
        &age.control.catalog != w_.catalog->family.get() ||
        &age.control.shared != w_.catalog->common.get() ||
        (actor != opening_->terms.proprietor_actor_id &&
         actor != opening_->terms.lessor_actor_id))
      return reject(bad("CustodyOpeningAgeBasisRootsRequired"));
    // Immutable catalog objects are their actual owning definitions, not a
    // temporary calendar provider or an invented persistent calendar fact.
    auto immutable = [&](RowOwner owner, const auto *root) -> Status {
      if (!root)
        return reject(bad("CustodyOpeningAgeCatalogRequired"));
      return append(Slot::opening_growth, NativePrivateShopCustodyOwnedRowPin{
                                              owner, root, root, nullptr, 0});
    };
    auto v = immutable(RowOwner::foundation_catalog_definition,
                       w_.catalog->actor_foundation.get());
    if (!v.ok)
      return v;
    v = immutable(RowOwner::common_catalog_definition, w_.catalog->common.get());
    if (!v.ok)
      return v;
    v = table(Slot::opening_growth, RowOwner::actor, w_.common.state().actors,
              w_.common.state().actors.find(actor));
    if (!v.ok)
      return v;
    const auto &f = age.foundation;
    const auto &catalog = age.foundation_catalog;
    if (const auto *declared = std::get_if<DeclaredAgeBasisRef>(&basis.ref)) {
      if (declared->actor_id != actor)
        return reject(bad("CustodyOpeningAgeBasisActorMismatch"));
      if (declared->kind == DeclaredAgeBasisRef::Kind::actor_start) {
        if (declared->source_ref != actor)
          return reject(bad("CustodyOpeningAgeAuthoredRefMismatch"));
        const auto *origin = f.character_origin_records.origins.find(actor);
        if (!origin ||
            origin->origin_kind != CharacterOriginRecordV2::Kind::authored)
          return reject(bad("CustodyOpeningAgeAuthoredOriginRequired"));
        v = table(Slot::opening_growth, RowOwner::character_origin,
                  f.character_origin_records.origins, origin);
        if (!v.ok)
          return v;
        v = table(Slot::opening_growth, RowOwner::condition_seed,
                  f.actor_start_context.initial_condition_seeds,
                  f.actor_start_context.initial_condition_seeds.find(
                      origin->initial_condition_seed_ref.seed_key));
        if (!v.ok)
          return v;
        v = table(Slot::opening_growth, RowOwner::authored_actor, catalog.actors,
                  catalog.actors.find(actor));
        if (!v.ok)
          return v;
        // read_life_age_basis reads this exact row. The single-origin reader
        // does not traverse construction/scenario rows; do not invent such
        // gates.
        return table(Slot::opening_growth, RowOwner::actor_age_basis,
                     f.actor_start_context.actor_age_bases,
                     f.actor_start_context.actor_age_bases.find(actor));
      }
      if (declared->kind != DeclaredAgeBasisRef::Kind::creation_registration)
        return reject(bad("CustodyOpeningAgeBasisKindUnsupported"));
      v = immutable(RowOwner::family_catalog_definition, w_.catalog->family.get());
      if (!v.ok)
        return v;
      const auto *origin = f.character_origin_records.origins.find(actor);
      if (origin &&
          origin->origin_kind == CharacterOriginRecordV2::Kind::created) {
        const auto *source =
            std::get_if<CreatedCharacterSource>(&origin->source);
        const auto *registration =
            f.character_origin_records.registration_sources.find(
                declared->source_ref);
        if (!source || !registration ||
            source->creation_result_ref != declared->source_ref ||
            registration->actor_id != actor)
          return reject(bad("CustodyOpeningAgeRegistrationRequired"));
        v = table(Slot::opening_growth, RowOwner::character_origin,
                  f.character_origin_records.origins, origin);
        if (!v.ok)
          return v;
        v = table(Slot::opening_growth, RowOwner::registration,
                  f.character_origin_records.registration_sources,
                  registration);
        if (!v.ok)
          return v;
        v = table(Slot::opening_growth, RowOwner::condition_seed,
                  f.actor_start_context.initial_condition_seeds,
                  f.actor_start_context.initial_condition_seeds.find(
                      origin->initial_condition_seed_ref.seed_key));
        if (!v.ok)
          return v;
        // The real Family catalog owns the entire appearance choice collection
        // scanned for uniqueness by read_created_character_origin_v2.
        return head(Slot::opening_growth, {"family", "actor_creation", actor});
      }
      // Legacy source-body validation checks ALL CharacterCreationFact rows,
      // their actor/Identity rows and reverse Common headers, not only one cut
      // ID.
      const auto *selected = age.family.facts.find(declared->source_ref);
      const auto *selected_creation =
          selected ? std::get_if<CharacterCreationFact>(selected) : nullptr;
      if (!selected_creation || selected_creation->actor_id != actor)
        return reject(bad("CustodyOpeningAgeLegacyCreationRequired"));
      v = append(Slot::opening_growth, NativePrivateShopCustodyOwnedRowPin{
                                           RowOwner::family_state_identity,
                                           &w_.family, &w_.family, nullptr, 0});
      if (!v.ok)
        return v;
      for (const auto &entry : age.family.facts.rows()) {
        const auto *creation =
            std::get_if<CharacterCreationFact>(&entry.second);
        if (!creation)
          continue;
        auto position = read_actual_common_fact_position(
            w_.common, SourceRefBuildView{"family", "family_character_creation",
                                          entry.first});
        if (!position.ok)
          return reject({false, {}, std::move(position.error)});
        v = table(Slot::opening_growth, RowOwner::family_fact, age.family.facts,
                  &entry.second, position.value < opening_cut_);
        if (!v.ok)
          return v;
        v = head(Slot::opening_growth,
                 {"family", "family_character_creation", entry.first});
        if (!v.ok)
          return v;
        v = table(Slot::opening_growth, RowOwner::family_fact, age.family.facts,
                  age.family.facts.find(creation->actor_id));
        if (!v.ok)
          return v;
        v = table(Slot::opening_growth, RowOwner::actor, w_.common.state().actors,
                  w_.common.state().actors.find(creation->actor_id));
        if (!v.ok)
          return v;
        v = head(Slot::opening_growth,
                 {"family", "actor_creation", creation->actor_id});
        if (!v.ok)
          return v;
      }
      for (const auto &entry : w_.common.state().facts.rows())
        if (entry.second.domain == "family" &&
            entry.second.kind == "family_character_creation") {
          v = head(Slot::opening_growth,
                   {entry.second.domain, entry.second.kind, entry.second.id});
          if (!v.ok)
            return v;
        }
      return common_success();
    }
    const auto *birth_ref = std::get_if<BirthFactRef>(&basis.ref);
    if (!birth_ref)
      return reject(bad("CustodyOpeningAgeBasisKindUnsupported"));
    // The Birth4 abstract reader internally validates scheduled-arrangement,
    // historical presence and all original Birth structure. Its current view
    // does not expose that checked set; four shallow pointers are not closure.
    for (const auto &module : catalog.identity.module_versions)
      if (module.first == "family_birth" && module.second == "4")
        return reject(bad("CustodyOpeningBirth4AgeProvenanceRequired"));
    const auto *birth = age.births.births.find(birth_ref->birth_fact_id);
    if (!birth || birth->actor_id != actor)
      return reject(bad("CustodyOpeningAgeBirthRequired"));
    v = table(Slot::opening_growth, RowOwner::birth, age.births.births, birth);
    if (!v.ok)
      return v;
    v = table(Slot::opening_growth, RowOwner::family_birth_reply,
              age.births.replies, age.births.replies.find(birth->accepted_id));
    if (!v.ok)
      return v;
    return head(Slot::opening_growth, {"family_birth", "family_birth_recorded",
                                       birth_ref->birth_fact_id});
  }

  // Task 深源由其原同一次 reader 的有限 typed 出口提供；粗context只核同根。
  Status task_clients(const TaskFulfilledClientContext &x,
                      const StableId &actor,
                      const TaskFulfilledClientBoundary &b) override {
    if (!opening_ || actor != opening_->terms.proprietor_actor_id ||
        b.opening_fact_id != opening_->id ||
        b.opening_tick != opening_->opened_tick)
      return reject(bad("CustodyOpeningTaskBoundaryRequired"));
    return task_roots(x);
  }
  Status task_roots(const TaskFulfilledClientContext &x) {
    if (&x.common != &w_.common || &x.tasks != &w_.tasks ||
        &x.catalog != w_.catalog->tasks.get() ||
        &x.common_catalog != w_.catalog->common.get() ||
        x.payments != (w_.tasks.takeover_execution
                           ? &*w_.tasks.takeover_execution
                           : nullptr) ||
        x.static_assets !=
            (w_.tasks.static_assets ? &*w_.tasks.static_assets : nullptr) ||
        x.messages !=
            (w_.tasks.message_handoffs ? &*w_.tasks.message_handoffs : nullptr))
      return reject(bad("CustodyOpeningTaskRootsRequired"));
    return common_success();
  }
  Status task_source(const TaskFulfilledClientContext &,
                     const TaskFulfilledClientCheckedSource &) override;
  // 许可原full读覆盖六表后缀；本回调只按真实表/头保留，不给予新许可。
  Status permission(const PoliticalShopLicensingContext &x,
                    const PrivateShopPermissionRequest &request,
                    const StableId &id, Integer tick) override {
    if (!opening_ || id != opening_->id || tick != opening_->opened_tick ||
        request.actor_id != opening_->terms.proprietor_actor_id ||
        request.premises_id != opening_->terms.premises_id ||
        request.place_id != opening_->terms.place_id ||
        &x.common != &w_.common ||
        &x.common_catalog != w_.catalog->common.get() ||
        &x.state != &w_.political_shop_licensing ||
        &x.catalog != w_.catalog->political_shop_licensing.get() ||
        &x.politics != &w_.politics ||
        &x.politics_catalog != w_.catalog->politics.get() ||
        &x.organizations != &w_.organization ||
        &x.organization_catalog != w_.catalog->organization.get() ||
        &x.economy != &w_.local_economy || &x.interactions != &w_.interaction ||
        &x.interaction_catalog != w_.catalog->interaction.get() ||
        &x.premises != &w_.catalog->private_shops->premises)
      return reject(bad("CustodyOpeningPermissionRootsRequired"));
    if (x.state.version != 2)
      return reject(common_failure("CustodyOpeningPermissionExactPinsRequired",
                                   {}, ErrorKind::unsupported_capability));
    permission_head_ = opening_head_;
    permission_cut_ = opening_cut_;
    auto roots = permission_roots(x);
    if (!roots.ok)
      return roots;
    auto rows = [&](RowOwner owner, const auto &root, const char *kind) -> Status {
      for (const auto &row : root.rows()) {
        auto p = read_actual_common_fact_position(
            w_.common, SourceRefBuildView{political_shop_licensing_module, kind,
                                          row.first});
        if (!p.ok)
          return reject({false, {}, std::move(p.error)});
        auto v = table(Slot::opening_permission, owner, root, &row.second,
                       p.value < opening_cut_);
        if (!v.ok)
          return v;
        v = head(Slot::opening_permission,
                 {political_shop_licensing_module, kind, row.first});
        if (!v.ok)
          return v;
      }
      return common_success();
    };
    auto v = rows(RowOwner::licensing_registry, x.state.registries,
                  "political_shop_registry_initialized");
    if (!v.ok)
      return v;
    v = rows(RowOwner::licensing_warrant, x.state.warrants,
             "political_wanted_registered");
    if (!v.ok)
      return v;
    v = rows(RowOwner::licensing_release, x.state.releases,
             "political_wanted_released");
    if (!v.ok)
      return v;
    v = rows(RowOwner::licensing_clearance, x.state.clearances,
             political_shop_clearance_kind);
    if (!v.ok)
      return v;
    v = rows(RowOwner::licensing_permit, x.state.permits,
             political_shop_permit_kind);
    if (!v.ok)
      return v;
    v = rows(RowOwner::licensing_revocation, x.state.revocations,
             "political_shop_permit_revoked");
    if (!v.ok)
      return v;
    for (const auto &row : x.catalog.courts.rows()) {
      v = table(Slot::opening_permission, RowOwner::licensing_court,
                x.catalog.courts, &row.second);
      if (!v.ok)
        return v;
    }
    // 仍由同一原full门执行；visitor首错丢整组，不以已有行冒完整来源。
    v = visit_political_shop_permission_originals2(x, request, id, tick, *this);
    return v.ok ? status_ : reject(std::move(v));
  }

  Status permission_roots(const PoliticalShopLicensingContext &);
  Status boundary(const PoliticalShopLicensingContext &, const FactRecord &,
                  const WorldMoment &) override;
  Status court(const PoliticalShopLicensingContext &,
               const PoliticalShopCourt &) override;
  Status office(const PoliticalShopLicensingContext &, const OfficeState &,
                const OfficeState &) override;
  Status organization_fact(const PoliticalShopLicensingContext &,
                           const FactRecord &,
                           const OrganizationFact &) override;
  Status member(const PoliticalShopLicensingContext &, const OrgMemberState *,
                const OrgMemberState &, const MemberPolicy &) override;
  Status membership(const PoliticalShopLicensingContext &,
                    const MembershipOffer &, const MembershipDecision &,
                    const MembershipTerms &) override;
  Status initial_grant(const PoliticalShopLicensingContext &,
                       const Grant &) override;
  Status grant_policy(const PoliticalShopLicensingContext &,
                      const GrantPolicy &) override;
  Status current_grant(const PoliticalShopLicensingContext &,
                       const GrantRuntimeRecord &) override;
  Status control(const PoliticalShopLicensingContext &,
                 const PoliticalShopCourt &,
                 const PoliticalShopControlContext &,
                 const PoliticalTakeoverRule &,
                 const PoliticalTakeoverRegistration &, const FactRecord &,
                 const MilitaryOccupationFact &, const FactRecord &) override;
  Outcome<CharacterAvailabilityProvenanceSink *>
  availability(const PoliticalShopLicensingContext &, const StableId &,
               const FactRecord &, const WorldMoment &) override;
  Status append(ASlot, SourceRefBuildView, Integer, bool) override;
  template <class T>
  Status availability_owned(ASlot slot, const T &r, bool before) {
    bool registered = false;
    for (const auto &s : character_availability_provenance_slots)
      if (s.slot == slot && !s.established_source_identity)
        registered = true;
    if (!registered)
      return reject(bad("CustodyPermissionTypedSlotRequired"));
    auto actual =
        resolve_character_availability_retention_ref(availability_owned_, r);
    if (!actual.ok)
      return reject({false, {}, std::move(actual.error)});
    return append(
        Slot::opening_availability,
        NativePrivateShopCustodyAvailabilityPin{slot, std::move(actual.value)},
        before);
  }
  Status original_origin(const CharacterOriginRecordV2 &r, bool b) override {
    return availability_owned(ASlot::original_origin, r, b);
  }
  Status created_registration(const RegistrationSourceV2 &r, bool b) override {
    return availability_owned(ASlot::created_registration, r, b);
  }
  Status birth_allocation(const InitialCapabilityAllocation &r,
                          bool b) override {
    return availability_owned(ASlot::birth_allocation, r, b);
  }
  Status original_birth(const BirthFactRef &r, bool b) override {
    return availability_owned(ASlot::original_birth, r, b);
  }
  Status born_creation(const FactRecord &r, bool b) override {
    return availability_owned(ASlot::born_creation, r, b);
  }

  // 原 source-local visitor 已完整验证实际
  // source/schema，此处仅拥有化真实引用。
  Status source(const MomentOwnerSource &r) override {
    if (r.source_schema < 1 || r.source_ref.domain.empty() ||
        r.source_ref.kind.empty() || r.source_ref.id.empty())
      return reject(bad("CustodyPinOperationInputRequired"));


    (out_.memory_).grow(0);
    NativePrivateShopCustodyNamedPin pin{
        {checkpoint_fresh_string_copy(r.source_ref.domain),
         checkpoint_fresh_string_copy(r.source_ref.kind),
         checkpoint_fresh_string_copy(r.source_ref.id)},
        r.source_schema};
    return append(Slot::operation_input, std::move(pin));
  }
  Status locked_rule(const ActorFoundationRuleRef &r) override {
    bool found = false;
    for (std::size_t i = 0;
         i < x_.market.operation.markets.market_service_definitions.size();
         ++i) {
      const auto &d = x_.market.operation.markets.market_service_definitions[i];
      if (same_rule(d.definition_ref, r)) {
        if (found)
          return reject(bad("CustodyPinLockedRuleAlias"));
        found = true;
        auto v = vector_row(
            Slot::operation_input, RowOwner::market_definition,
            x_.market.operation.markets.market_service_definitions, i);
        if (!v.ok)
          return v;
      }
    }
    const auto &catalog = x_.market.operation.body.catalog;
    for (const auto &row : catalog.capability_purpose_bindings.rows())
      if (same_rule(row.second.purpose_ref, r)) {
        if (found)
          return reject(bad("CustodyPinLockedRuleAlias"));
        found = true;
        auto v = table(Slot::operation_input, RowOwner::capability_purpose,
                       catalog.capability_purpose_bindings, &row.second);
        if (!v.ok)
          return v;
      }
    return found ? common_success()
                 : reject(bad("CustodyPinOriginalLockedRuleRequired"));
  }
  Status fixed_transition(const StableId &) override {
    return reject(bad("CustodyPinUnexpectedAcceptanceTransition"));
  }
};
// 原许可每组真实cut独立标注；不把旧开店头当所有Grant/生命截面。
Status NativePrivateShopCustodyPinCollector::permission_roots(
    const PoliticalShopLicensingContext &x) {
  const auto &body = availability_owned_.condition;
  if (!w_.catalog || &x.common != &w_.common ||
      &x.common_catalog != w_.catalog->common.get() ||
      &x.state != &w_.political_shop_licensing ||
      &x.catalog != w_.catalog->political_shop_licensing.get() ||
      &x.politics != &w_.politics ||
      &x.politics_catalog != w_.catalog->politics.get() ||
      &x.organizations != &w_.organization ||
      &x.organization_catalog != w_.catalog->organization.get() ||
      &x.economy != &w_.local_economy || &x.interactions != &w_.interaction ||
      &x.interaction_catalog != w_.catalog->interaction.get() ||
      &x.premises != &w_.catalog->private_shops->premises ||
      x.state.version != 2 || !x.character_sources || !x.moment_clock ||
      x.moment_clock != &x_.market.operation.clock ||
      &x.character_sources->common != &body.common ||
      &x.character_sources->state != &body.state ||
      &x.character_sources->catalog != &body.catalog)
    return reject(bad("CustodyPermissionActualRootsRequired"));
  auto same =
      compare_world_moments(x.character_sources->actual_now, body.actual_now);
  if (!same.ok)
    return reject({false, {}, std::move(same.error)});
  return same.value == 0 ? common_success()
                         : reject(bad("CustodyPermissionBodyMomentMismatch"));
}
Status NativePrivateShopCustodyPinCollector::boundary(
    const PoliticalShopLicensingContext &x, const FactRecord &actual,
    const WorldMoment &at) {
  auto valid = permission_roots(x);
  if (!valid.ok)
    return valid;
  if (w_.common.state().facts.find(actual.id) != &actual)
    return reject(bad("CustodyPermissionActualCutRequired"));
  const WorldMoment *original = nullptr;
  if (actual.domain == "organization") {
    const auto *row = x.organizations.facts.find(actual.id);
    if (!row || row->id != actual.id || row->kind != actual.kind ||
        !row->grant_change_payload)
      return reject(bad("CustodyPermissionGrantCutRequired"));
    original = &row->grant_change_payload->effective_moment;
  } else {


    auto memory = CheckpointMemoryCharge(0);
    const SourceRef reference{checkpoint_fresh_string_copy(actual.domain),
                              checkpoint_fresh_string_copy(actual.kind),
                              checkpoint_fresh_string_copy(actual.id)};
    auto source = read_political_shop_permission_boundary_moment2(x, reference);
    if (!source.ok)
      return reject({false, {}, std::move(source.error)});
    original = source.value;
  }
  if (!original)
    return reject(bad("CustodyPermissionOriginalMomentRequired"));
  auto same = compare_world_moments(*original, at);
  if (!same.ok)
    return reject({false, {}, std::move(same.error)});
  if (same.value)
    return reject(bad("CustodyPermissionOriginalMomentMismatch"));
  auto position = read_actual_common_fact_position(w_.common, actual);
  if (!position.ok)
    return reject({false, {}, std::move(position.error)});
  permission_head_ = &actual;
  permission_cut_ = position.value;
  return head(Slot::opening_permission,
              {actual.domain, actual.kind, actual.id});
}
Status NativePrivateShopCustodyPinCollector::court(
    const PoliticalShopLicensingContext &x, const PoliticalShopCourt &r) {
  auto v = permission_roots(x);
  if (!v.ok)
    return v;
  v = table(Slot::opening_permission, RowOwner::licensing_court, x.catalog.courts,
            &r);
  if (!v.ok)
    return v;
  const auto *j = x.politics_catalog.jurisdictions.find(r.jurisdiction_id);
  v = table(Slot::opening_permission, RowOwner::jurisdiction_initial,
            x.politics_catalog.jurisdictions, j);
  if (!v.ok)
    return v;
  v = table(Slot::opening_permission, RowOwner::jurisdiction_current,
            x.politics.jurisdictions,
            x.politics.jurisdictions.find(r.jurisdiction_id), false);
  if (!v.ok)
    return v;
  const auto *o = x.organization_catalog.offices.find(r.office_id);
  v = table(Slot::opening_permission, RowOwner::office_initial,
            x.organization_catalog.offices, o);
  if (!v.ok)
    return v;
  for (const auto &id : o->grant_ids) {
    // 原catalog门检查实际存在的候选Grant；不凭名字创造缺失行。
    if (const auto *g = x.organization_catalog.grants.find(id)) {
      v = table(Slot::opening_permission, RowOwner::grant_initial,
                x.organization_catalog.grants, g);
      if (!v.ok)
        return v;
    }
  }
  v = table(Slot::opening_permission, RowOwner::role_definition,
            x.common_catalog.roles, x.common_catalog.roles.find(r.role_id));
  if (!v.ok)
    return v;
  v = table(Slot::opening_permission, RowOwner::place, x.common_catalog.places,
            x.common_catalog.places.find(j->seat_place_id));
  if (!v.ok)
    return v;
  auto actor_id = [&](const StableId &id) -> Status {
    const auto &ids = x.common_catalog.actor_ids;
    auto i = std::find(ids.begin(), ids.end(), id);
    if (i == ids.end())
      return reject(bad("CustodyPermissionAuthoredActorRequired"));
    return vector_row(Slot::opening_permission, RowOwner::authored_actor_id, ids,
                      static_cast<std::size_t>(i - ids.begin()));
  };
  for (const auto &id : r.premises_ids) {
    const auto *p = x.premises.find(id);
    v = table(Slot::opening_permission, RowOwner::premises, x.premises, p);
    if (!v.ok)
      return v;
    v = table(Slot::opening_permission, RowOwner::place, x.common_catalog.places,
              x.common_catalog.places.find(p->place_id));
    if (!v.ok)
      return v;
    v = actor_id(p->lessor_actor_id);
    if (!v.ok)
      return v;
  }
  for (const auto &seed : r.initial_wanted) {
    v = actor_id(seed.actor_id);
    if (!v.ok)
      return v;
  }
  bool found = false;
  for (std::size_t d = 0; d < x.interaction_catalog.dialogues.size(); ++d) {
    const auto &dialogue = x.interaction_catalog.dialogues[d];
    if (dialogue.id != r.dialogue_id)
      continue;
    for (std::size_t n = 0; n < dialogue.nodes.size(); ++n)
      if (dialogue.nodes[n].id == r.request_node_id) {
        if (found)
          return reject(bad("CustodyPermissionDialogueAlias"));
        found = true;
        v = vector_row(Slot::opening_permission, RowOwner::dialogue,
                       x.interaction_catalog.dialogues, d);
        if (!v.ok)
          return v;
        v = vector_row(Slot::opening_permission, RowOwner::dialogue_node,
                       dialogue.nodes, n);
        if (!v.ok)
          return v;
      }
  }
  return found ? common_success()
               : reject(bad("CustodyPermissionDialogueRequired"));
}
Status NativePrivateShopCustodyPinCollector::office(
    const PoliticalShopLicensingContext &x, const OfficeState &initial,
    const OfficeState &current) {
  auto v = permission_roots(x);
  if (!v.ok)
    return v;
  v = table(Slot::opening_permission, RowOwner::office_initial,
            x.organization_catalog.offices, &initial);
  if (!v.ok)
    return v;
  if (initial.id != current.id)
    return reject(bad("CustodyPermissionOfficeIdentityRequired"));
  // 当前投影的真实拥有身份，不冒曾经cut上的Office值。
  return table(Slot::opening_permission, RowOwner::office_current,
               x.organizations.offices, &current, false);
}
Status NativePrivateShopCustodyPinCollector::organization_fact(
    const PoliticalShopLicensingContext &x, const FactRecord &actual,
    const OrganizationFact &r) {
  auto v = permission_roots(x);
  if (!v.ok)
    return v;
  if (actual.id != r.id || actual.kind != r.kind ||
      actual.domain != "organization" ||
      w_.common.state().facts.find(actual.id) != &actual)
    return reject(bad("CustodyPermissionOrganizationHeadRequired"));
  auto position = read_actual_common_fact_position(w_.common, actual);
  if (!position.ok)
    return reject({false, {}, std::move(position.error)});
  v = table(Slot::opening_permission, RowOwner::organization_fact,
            x.organizations.facts, &r, position.value < permission_cut_);
  if (!v.ok)
    return v;
  v = head(Slot::opening_permission, {actual.domain, actual.kind, actual.id});
  if (!v.ok)
    return v;
  if (r.office_change_payload) {
    // 原Office历史只读这个真实cause头/顺序；不冒整lifecycle来源已验。
    return std::visit(
        [&](const auto &cause) -> Status {
          using T = std::decay_t<decltype(cause)>;
          if constexpr (std::is_same_v<T, ByOfficeTransition>)
            return head(Slot::opening_permission,
                        {cause.transition_source_ref.domain,
                         cause.transition_source_ref.kind,
                         cause.transition_source_ref.id});
          else
            return head(Slot::opening_permission,
                        {cause.death_ref.domain, cause.death_ref.kind,
                         cause.death_ref.id});
        },
        r.office_change_payload->cause);
  }
  // Member历史只检查join
  // decision；其它cause可为arrear原ID，不能猜Common/schema。
  // Grant原全门的真实变更/父链行由同次visitor完整发出，不另造金融依赖。
  return common_success();
}
Status NativePrivateShopCustodyPinCollector::member(
    const PoliticalShopLicensingContext &x, const OrgMemberState *initial,
    const OrgMemberState &current, const MemberPolicy &policy) {
  auto v = permission_roots(x);
  if (!v.ok)
    return v;
  if (initial) {
    v = table(Slot::opening_permission, RowOwner::member_initial,
              x.organization_catalog.members, initial);
    if (!v.ok)
      return v;
    if (initial->id != current.id)
      return reject(bad("CustodyPermissionMemberIdentityRequired"));
  }
  v = table(Slot::opening_permission, RowOwner::member_current,
            x.organizations.members, &current, false);
  if (!v.ok)
    return v;
  v = table(Slot::opening_permission, RowOwner::member_policy,
            x.organization_catalog.member_policies, &policy);
  if (!v.ok)
    return v;
  return table(Slot::opening_permission, RowOwner::actor, w_.common.state().actors,
               w_.common.state().actors.find(current.actor_id));
}
Status NativePrivateShopCustodyPinCollector::membership(
    const PoliticalShopLicensingContext &x, const MembershipOffer &offer,
    const MembershipDecision &decision, const MembershipTerms &terms) {
  auto v = permission_roots(x);
  if (!v.ok)
    return v;
  v = table(Slot::opening_permission, RowOwner::membership_offer,
            x.organizations.membership_offers, &offer, false);
  if (!v.ok)
    return v;
  auto position = read_actual_common_fact_position(
      w_.common,
      SourceRefBuildView{"organization", "membership_decided", decision.id});
  if (!position.ok)
    return reject({false, {}, std::move(position.error)});
  v = table(Slot::opening_permission, RowOwner::membership_decision,
            x.organizations.membership_decisions, &decision,
            position.value < permission_cut_);
  if (!v.ok)
    return v;
  v = table(Slot::opening_permission, RowOwner::membership_terms,
            x.organization_catalog.membership_terms, &terms);
  if (!v.ok)
    return v;
  return head(Slot::opening_permission,
              {"organization", "membership_decided", decision.id});
}
Status NativePrivateShopCustodyPinCollector::initial_grant(
    const PoliticalShopLicensingContext &x, const Grant &r) {
  auto v = permission_roots(x);
  return v.ok ? table(Slot::opening_permission, RowOwner::grant_initial,
                      x.organization_catalog.grants, &r)
              : v;
}
Status NativePrivateShopCustodyPinCollector::grant_policy(
    const PoliticalShopLicensingContext &x, const GrantPolicy &r) {
  auto v = permission_roots(x);
  return v.ok ? table(Slot::opening_permission, RowOwner::grant_policy,
                      x.organization_catalog.grant_policies, &r)
              : v;
}
Status NativePrivateShopCustodyPinCollector::current_grant(
    const PoliticalShopLicensingContext &x, const GrantRuntimeRecord &r) {
  auto v = permission_roots(x);
  return v.ok ? table(Slot::opening_permission, RowOwner::grant_current,
                      x.organizations.current_grants, &r, false)
              : v;
}
Status NativePrivateShopCustodyPinCollector::control(
    const PoliticalShopLicensingContext &x, const PoliticalShopCourt &court,
    const PoliticalShopControlContext &control,
    const PoliticalTakeoverRule &rule,
    const PoliticalTakeoverRegistration &registration,
    const FactRecord &registered, const MilitaryOccupationFact &occupation,
    const FactRecord &occupied) {
  auto v = permission_roots(x);
  if (!v.ok)
    return v;
  if (x.takeover != &control || &control.common != &w_.common ||
      &control.catalog != w_.catalog->political_takeover.get() ||
      &control.state != &w_.political_takeover ||
      &control.military != &w_.military ||
      &control.military_catalog != w_.catalog->military.get() ||
      control.military_access.common_catalog != w_.catalog->common.get() ||
      x.catalog.courts.find(court.id) != &court ||
      w_.common.state().facts.find(registered.id) != &registered ||
      w_.common.state().facts.find(occupied.id) != &occupied ||
      registered.id != registration.id || occupied.id != occupation.id)
    return reject(bad("CustodyPermissionControlRootsRequired"));
  auto p = read_actual_common_fact_position(w_.common, registered);
  if (!p.ok)
    return reject({false, {}, std::move(p.error)});
  v = table(Slot::opening_permission, RowOwner::takeover_rule,
            control.catalog.rules, &rule);
  if (!v.ok)
    return v;
  v = table(Slot::opening_permission, RowOwner::takeover_registration,
            control.state.registrations, &registration,
            p.value < permission_cut_);
  if (!v.ok)
    return v;
  v = head(Slot::opening_permission,
           {registered.domain, registered.kind, registered.id});
  if (!v.ok)
    return v;
  p = read_actual_common_fact_position(w_.common, occupied);
  if (!p.ok)
    return reject({false, {}, std::move(p.error)});
  bool found = false;
  for (std::size_t n = 0; n < w_.military.occupation_facts.size(); ++n)
    if (&w_.military.occupation_facts[n] == &occupation) {
      found = true;
      v = vector_row(Slot::opening_permission, RowOwner::military_occupation,
                     w_.military.occupation_facts, n,
                     p.value < permission_cut_);
      if (!v.ok)
        return v;
    }
  if (!found)
    return reject(bad("CustodyPermissionActualOccupationRequired"));
  v = head(Slot::opening_permission,
           {occupied.domain, occupied.kind, occupied.id});
  if (!v.ok)
    return v;
  // 原validate_applied_occupation_manifest实际读到的receipt、目录与manifest成员。
  auto by_id = [&](RowOwner owner, const auto &rows, const StableId &id,
                   bool before = false) -> Status {
    std::optional<std::size_t> matched;
    for (std::size_t n = 0; n < rows.size(); ++n)
      if (rows[n].id == id) {
        if (matched)
          return reject(bad("CustodyPermissionMilitaryRowAlias"));
        matched = n;
      }
    if (!matched)
      return reject(bad("CustodyPermissionMilitaryRowRequired"));
    return vector_row(Slot::opening_permission, owner, rows, *matched, before);
  };
  const auto &cat = control.military_catalog;
  v = by_id(RowOwner::military_stronghold_definition, cat.strongholds,
            occupation.stronghold_id, true);
  if (!v.ok)
    return v;
  v = by_id(RowOwner::military_garrison_definition, cat.garrisons,
            occupation.garrison_id, true);
  if (!v.ok)
    return v;
  v = by_id(RowOwner::military_siege, w_.military.sieges, occupation.siege_id);
  if (!v.ok)
    return v;
  v = by_id(RowOwner::military_army, w_.military.armies, occupation.army_id);
  if (!v.ok)
    return v;
  v = by_id(RowOwner::military_campaign, w_.military.campaigns,
            occupation.campaign_id);
  if (!v.ok)
    return v;
  for (const auto &campaign : w_.military.campaigns)
    if (campaign.id == occupation.campaign_id) {
      v = by_id(RowOwner::military_order_definition, cat.orders,
                campaign.template_id, true);
      if (!v.ok)
        return v;
    }
  for (const auto *id :
       {&occupation.from_organization_id, &occupation.to_organization_id}) {
    const auto &ids = x.common_catalog.organization_ids;
    const auto i = std::find(ids.begin(), ids.end(), *id);
    if (i == ids.end())
      return reject(bad("CustodyPermissionOccupationOrganizationRequired"));
    v = vector_row(Slot::opening_permission, RowOwner::organization_id, ids,
                   static_cast<std::size_t>(i - ids.begin()));
    if (!v.ok)
      return v;
  }
  if (!occupation.asset_manifest)
    return reject(bad("CustodyPermissionOccupationManifestRequired"));
  auto item = [&](const StableId &id) -> Status {
    const auto i = std::find(cat.item_ids.begin(), cat.item_ids.end(), id);
    if (i == cat.item_ids.end())
      return reject(bad("CustodyPermissionOccupationItemRequired"));
    return vector_row(Slot::opening_permission, RowOwner::military_item_id,
                      cat.item_ids,
                      static_cast<std::size_t>(i - cat.item_ids.begin()));
  };
  for (const auto &inventory : occupation.asset_manifest->inventories) {
    for (const auto &lot : inventory.second.public_lots) {
      v = item(lot.item_id);
      if (!v.ok)
        return v;
    }
    for (const auto &lot : inventory.second.protected_lots) {
      v = item(lot.item_id);
      if (!v.ok)
        return v;
    }
  }
  return common_success();
}
Outcome<CharacterAvailabilityProvenanceSink *>
NativePrivateShopCustodyPinCollector::availability(
    const PoliticalShopLicensingContext &x, const StableId &actor,
    const FactRecord &actual, const WorldMoment &at) {
  auto v = boundary(x, actual, at);
  if (!v.ok)
    return {false, nullptr, std::move(v.error)};
  v = table(Slot::opening_availability, RowOwner::actor, w_.common.state().actors,
            w_.common.state().actors.find(actor));
  if (!v.ok)
    return {false, nullptr, std::move(v.error)};
  return {true, this, {}};
}
Status NativePrivateShopCustodyPinCollector::append(ASlot slot,
                                                    SourceRefBuildView ref,
                                                    Integer schema,
                                                    bool applied) {
  bool registered = false;
  for (const auto &s : character_availability_provenance_slots)
    if (s.slot == slot && s.established_source_identity)
      registered = true;
  if (!registered)
    return reject(bad("CustodyPermissionEstablishedSlotRequired"));
  auto actual = resolve_character_availability_source_retention_ref(
      availability_sources_, slot, ref, schema);
  if (!actual.ok)
    return reject({false, {}, std::move(actual.error)});
  return append(
      Slot::opening_availability,
      NativePrivateShopCustodyAvailabilityPin{slot, std::move(actual.value)},
      applied);
}

// 逐个有限 Task 真来源映射；短栈 completion/index view 本身绝不逃逸。
Status NativePrivateShopCustodyPinCollector::task_source(
    const TaskFulfilledClientContext &x,
    const TaskFulfilledClientCheckedSource &source) {
  if (!opening_)
    return reject(bad("CustodyOpeningTaskCutRequired"));
  auto valid = task_roots(x);
  if (!valid.ok)
    return valid;
  // 有原 Common 头的行按真实位置标记后缀；无事实头的当前行不冒旧 cut 值。
  auto fact_row = [&](RowOwner owner, const auto &root, const auto *row,
                      const char *domain, const char *kind) -> Status {
    const auto *key = row ? key_for(root, row) : nullptr;
    if (!key)
      return reject(bad("CustodyOpeningTaskRowRequired"));
    auto position = read_actual_common_fact_position(
        w_.common, SourceRefBuildView{domain, kind, *key});
    if (!position.ok)
      return reject({false, {}, std::move(position.error)});
    auto v = table(Slot::opening_task, owner, root, row,
                   position.value < opening_cut_);
    if (!v.ok)
      return v;
    return head(Slot::opening_task, {domain, kind, *key});
  };
  auto task = [&](const StableId &id) -> Status {
    return table(Slot::opening_task, RowOwner::task, w_.tasks.tasks,
                 w_.tasks.tasks.find(id), false);
  };
  auto actor = [&](const StableId &id) -> Status {
    return table(Slot::opening_task, RowOwner::actor, w_.common.state().actors,
                 w_.common.state().actors.find(id), false);
  };
  return std::visit(
      [&](const auto *r) -> Status {
        using T = std::remove_cv_t<std::remove_pointer_t<decltype(r)>>;
        if (!r)
          return reject(bad("CustodyOpeningTaskSourceRequired"));
        if constexpr (std::is_same_v<T, TaskRecord>) {
          return table(Slot::opening_task, RowOwner::task, w_.tasks.tasks, r,
                       false);
        } else if constexpr (std::is_same_v<T, ContractRecord>) {
          return table(Slot::opening_task, RowOwner::task_contract,
                       w_.tasks.contracts, r, false);
        } else if constexpr (std::is_same_v<T, TaskDefinition>) {
          if (key_for(x.catalog.offers, r))
            return table(Slot::opening_task, RowOwner::task_definition,
                         x.catalog.offers, r);
          // 动态条款归真实 Contract，而非 Prepared offers 中同名的定义。
          for (const auto &row : w_.tasks.contracts.rows())
            if (&row.second.frozen_terms.definition == r)
              return append(Slot::opening_task,
                            NativePrivateShopCustodyOwnedRowPin{
                                RowOwner::task_frozen_definition,
                                &w_.tasks.contracts, r, &row.first, 0});
          return reject(bad("CustodyOpeningTaskDefinitionOwnerRequired"));
        } else if constexpr (std::is_same_v<T, TaskTransferFact>) {
          return fact_row(RowOwner::task_transfer, w_.tasks.transfers, r, "tasks",
                          "task_transfer");
        } else if constexpr (std::is_same_v<T, DeliveryFact>) {
          return fact_row(RowOwner::task_delivery, w_.tasks.deliveries, r, "tasks",
                          "task_delivery");
        } else if constexpr (std::is_same_v<T, EscortArrivalFact>) {
          return fact_row(RowOwner::task_escort, w_.tasks.facts, r, "tasks",
                          "escort_arrival");
        } else if constexpr (std::is_same_v<T, TaskStaticPaymentSource>) {
          if (!w_.tasks.takeover_execution)
            return reject(bad("CustodyOpeningTaskPaymentRootRequired"));
          return fact_row(RowOwner::task_static_payment,
                          w_.tasks.takeover_execution->static_payments, r,
                          "tasks", task_static_payment_fact_kind);
        } else if constexpr (std::is_same_v<T, TaskStaticAssetReceipt>) {
          if (!w_.tasks.static_assets)
            return reject(bad("CustodyOpeningTaskAssetRootRequired"));
          return fact_row(RowOwner::task_static_asset,
                          w_.tasks.static_assets->receipts, r, "tasks",
                          task_static_asset_fact_kind);
        } else if constexpr (std::is_same_v<T, TaskMessageHandoff>) {
          if (!w_.tasks.message_handoffs)
            return reject(bad("CustodyOpeningTaskMessageRootRequired"));
          return fact_row(RowOwner::task_message_handoff,
                          w_.tasks.message_handoffs->receipts, r, "tasks",
                          task_message_handoff_kind);
        } else if constexpr (std::is_same_v<T, TaskMessageReport>) {
          if (!w_.tasks.message_handoffs)
            return reject(bad("CustodyOpeningTaskMessageRootRequired"));
          return fact_row(RowOwner::task_message_report,
                          w_.tasks.message_handoffs->reports, r, "tasks",
                          task_message_report_kind);
        } else if constexpr (std::is_same_v<T, ExactQuantityActionCompletion>) {
          if (!w_.tasks.exact_execution || !r->task || !r->contract ||
              !r->start || !r->terminal || !r->committed_result)
            return reject(bad("CustodyOpeningTaskExactSourceRequired"));
          auto v = table(Slot::opening_task, RowOwner::task, w_.tasks.tasks,
                         r->task, false);
          if (!v.ok)
            return v;
          v = table(Slot::opening_task, RowOwner::task_contract,
                    w_.tasks.contracts, r->contract, false);
          if (!v.ok)
            return v;
          v = fact_row(RowOwner::task_exact_start,
                       w_.tasks.exact_execution->starts, r->start, "tasks",
                       "task_action_started");
          if (!v.ok)
            return v;
          v = fact_row(RowOwner::task_exact_terminal,
                       w_.tasks.exact_execution->terminals, r->terminal,
                       "tasks", "task_action_finished");
          if (!v.ok)
            return v;
          auto original = read_common_command_source(
              w_.common, std::string_view(r->committed_result->result_id));
          if (!original.ok)
            return reject({false, {}, std::move(original.error)});
          if (original.value.result != r->committed_result)
            return reject(bad("CustodyOpeningTaskActualResultRequired"));
          // 原 quantity reader 必需此实际 seal；不是所有 Common results 的永久
          // PIN。
          return append(
              Slot::opening_task,
              NativePrivateShopCustodyCommandPin{
                  &w_.common, original.value.result, original.value.history},
              false);
        } else if constexpr (std::is_same_v<T, TaskShopOrderBinding>) {
          if (!w_.tasks.shop_orders)
            return reject(bad("CustodyOpeningTaskShopRootRequired"));
          auto v = fact_row(
              RowOwner::task_shop_binding, w_.tasks.shop_orders->bindings, r,
              task_shop_order_domain, task_shop_order_accepted_kind);
          if (!v.ok)
            return v;
          v = head(Slot::opening_task,
                   {private_shop_module, "private_shop_order_issued",
                    r->order_issue_fact_id});
          if (!v.ok)
            return v;
          v = task(r->task_id);
          if (!v.ok)
            return v;
          v = actor(r->terms.proprietor_actor_id);
          if (!v.ok)
            return v;
          v = actor(r->terms.original_client.id);
          if (!v.ok)
            return v;
          return table(Slot::opening_task, RowOwner::wallet,
                       w_.common.state().wallets,
                       w_.common.wallet(r->terms.client_wallet_id), false);
        } else if constexpr (std::is_same_v<T, TaskShopOrderSettlement>) {
          if (!w_.tasks.shop_orders)
            return reject(bad("CustodyOpeningTaskShopRootRequired"));
          auto v = fact_row(
              RowOwner::task_shop_settlement, w_.tasks.shop_orders->settlements, r,
              task_shop_order_domain, task_shop_order_settled_kind);
          return v.ok ? task(r->task_id) : v;
        } else if constexpr (std::is_same_v<T, TaskShopOrderAction>) {
          if (!w_.tasks.shop_orders)
            return reject(bad("CustodyOpeningTaskShopRootRequired"));
          auto v = table(Slot::opening_task, RowOwner::task_shop_action,
                         w_.tasks.shop_orders->actions, r, false);
          if (!v.ok)
            return v;
          v = task(r->task_id);
          if (!v.ok)
            return v;
          const auto *original = w_.common.action(r->action_id);
          v = table(Slot::opening_task, RowOwner::common_action,
                    w_.common.state().actions, original, false);
          if (!v.ok)
            return v;
          for (const auto &id : original->participants) {
            v = actor(id);
            if (!v.ok)
              return v;
          }
          return common_success();
        } else if constexpr (std::is_same_v<T, TaskShopOrderInterruption>) {
          if (!w_.tasks.shop_orders)
            return reject(bad("CustodyOpeningTaskShopRootRequired"));
          auto v = fact_row(RowOwner::task_shop_interruption,
                            w_.tasks.shop_orders->interruptions, r,
                            task_shop_order_domain,
                            task_shop_order_interrupted_kind);
          if (!v.ok)
            return v;
          // 当次完成 phase 仍在的原 Action 也是被原 ledger 实际检查的行。
          if (const auto *original = w_.common.action(r->source.action_id)) {
            v = table(Slot::opening_task, RowOwner::common_action,
                      w_.common.state().actions, original, false);
            if (!v.ok)
              return v;
          }
          return common_success();
        } else if constexpr (std::is_same_v<T,
                                            TaskShopOrderEstateCancellation>) {
          if (!w_.tasks.shop_orders ||
              !w_.tasks.shop_orders->estate_cancellations)
            return reject(bad("CustodyOpeningTaskEstateRootRequired"));
          auto v = fact_row(
              RowOwner::task_shop_estate_cancellation,
              w_.tasks.shop_orders->estate_cancellations->cancellations, r,
              task_shop_order_estate_module,
              task_shop_order_estate_cancelled_kind);
          if (!v.ok)
            return v;
          // 这里只读永久身份；原 ledger 不要求已合法淘汰的 cached result
          // 再出现。
          return actor(r->request.executor_actor_id);
        } else if constexpr (std::is_same_v<T, TaskPersonalMessageStart>) {
          if (!w_.tasks.personal_messages)
            return reject(bad("CustodyOpeningPersonalMessageRootRequired"));
          return fact_row(RowOwner::task_personal_message_start,
                          w_.tasks.personal_messages->starts, r,
                          task_personal_message_module,
                          task_personal_message_started_kind);
        } else if constexpr (std::is_same_v<T, TaskPersonalMessageEnd>) {
          if (!w_.tasks.personal_messages)
            return reject(bad("CustodyOpeningPersonalMessageRootRequired"));
          return fact_row(RowOwner::task_personal_message_end,
                          w_.tasks.personal_messages->endings, r,
                          task_personal_message_module,
                          r->success ? task_personal_message_delivered_kind
                                     : task_personal_message_interrupted_kind);
        } else if constexpr (std::is_same_v<T, TaskPersonalReportStart>) {
          if (!w_.tasks.personal_reports)
            return reject(bad("CustodyOpeningPersonalReportRootRequired"));
          return fact_row(RowOwner::task_personal_report_start,
                          w_.tasks.personal_reports->starts, r,
                          task_personal_report_module,
                          task_personal_report_started_kind);
        } else if constexpr (std::is_same_v<T, TaskPersonalReportEnd>) {
          if (!w_.tasks.personal_reports)
            return reject(bad("CustodyOpeningPersonalReportRootRequired"));
          return fact_row(RowOwner::task_personal_report_end,
                          w_.tasks.personal_reports->endings, r,
                          task_personal_report_module,
                          r->success ? task_personal_report_completed_kind
                                     : task_personal_report_interrupted_kind);
        } else if constexpr (std::is_same_v<T, TaskMessageObserverProvenance>) {
          if (!w_.tasks.message_handoffs ||
              r->state != &*w_.tasks.message_handoffs ||
              !r->actual_observer_key || !r->actual_index)
            return reject(bad("CustodyOpeningMessageIndexRootRequired"));
          const auto &root =
              r->report ? r->state->reports_observed_by : r->state->observed_by;
          const auto at = root.find(*r->actual_observer_key);
          if (at == root.end() || &at->first != r->actual_observer_key ||
              &at->second != r->actual_index)
            return reject(bad("CustodyOpeningMessageIndexRowRequired"));
          return append(Slot::opening_task,
                        NativePrivateShopCustodyOwnedRowPin{
                            RowOwner::task_observer_index, &root, &at->second,
                            &at->first, 0},
                        false);
        } else if constexpr (std::is_same_v<T, ActorRecord>) {
          return table(Slot::opening_task, RowOwner::actor,
                       w_.common.state().actors, r, false);
        } else if constexpr (std::is_same_v<
                                 T, TaskOriginalOrganizationClientProvenance>) {
          if (r->catalog != w_.catalog->common.get() ||
              !r->actual_organization_id)
            return reject(bad("CustodyOpeningTaskOrganizationRootRequired"));
          const auto &root = r->catalog->organization_ids;
          for (std::size_t i = 0; i < root.size(); ++i)
            if (&root[i] == r->actual_organization_id)
              return vector_row(Slot::opening_task, RowOwner::organization_id,
                                root, i);
          return reject(bad("CustodyOpeningTaskOrganizationRowRequired"));
        } else {
          static_assert(!std::is_same_v<T, T>,
                        "新增 Task 源必须由真实 owner 显式映射");
        }
      },
      source);
}

// 同 World 每个原根与同 foundation 自身绑定先核；collector 不接受影子 context。
Outcome<NativePrivateShopCustodySourcePins>
collect_native_private_shop_custody_source_pins(
    const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    const NativeHouseholdFoundationLease &foundation) {
  auto valid = validate_native_private_shop_household_custody_layout(w);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  const auto *x = foundation.private_custody();
  if (!w.catalog || &core.characters.early.world != &w ||
      !w.private_shop_household_custody || !w.private_shop_operation_sources ||
      !w.private_shop_operation_capabilities || !w.actor_foundation ||
      !w.action_elapsed || !w.household_purchases ||
      !w.household_retail_settlements || !x)
    return {false, {}, bad("CustodyPinOriginalWorldRootsRequired").error};
  valid = foundation.validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  const auto &t = x->market.inventory_titles;
  const auto &o = x->market.operation;
  if (!foundation.bound_to(w.common, w.local_economy, *w.household_purchases,
                           w.private_shops, *w.household_retail_settlements,
                           *w.action_elapsed) ||
      &x->state != &*w.private_shop_household_custody ||
      &x->catalog != w.catalog->private_shop_household_custody.get() ||
      &x->travel != w.catalog->travel.get() ||
      &o.shops.state != &w.private_shops ||
      &o.sources != &*w.private_shop_operation_sources ||
      &o.capabilities != &*w.private_shop_operation_capabilities ||
      &t.common != &w.common || &t.catalog != w.catalog->common.get() ||
      &t.family != &w.family || &t.military != &w.military ||
      &t.naval != &w.naval || &t.naval_catalog != w.catalog->naval.get() ||
      &t.estates != w.catalog->estate.get() ||
      t.foundation != &*w.actor_foundation ||
      t.foundation_catalog != w.catalog->actor_foundation.get() ||
      t.family_catalog != w.catalog->family.get())
    return {false, {}, bad("CustodyPinSameWorldGraphRequired").error};
  valid = validate_private_shop_household_custody_sources(*x);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  const auto &body = o.body;
  if (&body.common != &w.common || &body.state != &*w.actor_foundation ||
      &body.catalog != w.catalog->actor_foundation.get())
    return {false, {}, bad("CustodyPermissionConditionOwnerMismatch").error};
  const auto *age = core.characters.family.life_age();
  if (age) {
    const auto bindings = age->bindings();
    if (!w.life_age_checks || !w.catalog->life_age_checks ||
        &bindings.state != &*w.life_age_checks ||
        &bindings.catalog != w.catalog->life_age_checks.get() ||
        &bindings.sources.common != &w.common ||
        &bindings.sources.foundation != &*w.actor_foundation ||
        &bindings.sources.foundation_catalog !=
            w.catalog->actor_foundation.get())
      return {false, {}, bad("CustodyPermissionAgeOwnerMismatch").error};
  }
  CharacterAvailabilityRetentionContext owned{body, &w.family_birth};
  CharacterAvailabilitySourceRetentionContext sources{
      body, age, w.life_age_checks ? &*w.life_age_checks : nullptr,
      w.catalog->life_age_checks.get()};
  NativePrivateShopCustodyPinCollector sink(w, *x, owned, sources);
  for (const auto &row : x->state.accepted.rows()) {
    valid = sink.collect(row.second);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
  }
  return {true, sink.finish(), {}};
}
bool NativePrivateShopCustodySourcePins::contains_source(
    SourceRefBuildView ref) const noexcept {
  for (const auto &pin : pins_)
    if (std::visit(
            [&](const auto &d) {
              using T = std::decay_t<decltype(d)>;
              if constexpr (std::is_same_v<T,
                                           NativePrivateShopCustodyCommonPin>)
                return d.row && d.row->domain == ref.domain &&
                       d.row->kind == ref.kind && d.row->id == ref.id;
              else if constexpr (std::is_same_v<
                                     T, NativePrivateShopCustodyNamedPin>)
                return d.source.domain == ref.domain &&
                       d.source.kind == ref.kind && d.source.id == ref.id;
              else if constexpr (std::is_same_v<
                                     T, NativePrivateShopCustodyCommandPin>)
                return d.result && ref.domain == "common" &&
                       ref.kind == "command_result" &&
                       d.result->result_id == ref.id;
              else if constexpr (std::is_same_v<
                                     T,
                                     NativePrivateShopCustodyAvailabilityPin>)
                return std::visit(
                    [&](const auto &source) -> bool {
                      using S = std::decay_t<decltype(source)>;
                      if constexpr (
                          std::is_same_v<
                              S, CharacterAvailabilitySourceRetentionRef>)
                        return source.source.domain == ref.domain &&
                               source.source.kind == ref.kind &&
                               source.source.id == ref.id;
                      else
                        return std::visit(
                            [&](const auto &owned) -> bool {
                              using O = std::decay_t<decltype(owned)>;
                              if constexpr (
                                  std::is_same_v<
                                      O,
                                      CharacterOriginalBirthOwnedRetentionRef> ||
                                  std::is_same_v<
                                      O,
                                      CharacterBornCreationOwnedRetentionRef>)
                                return owned.header &&
                                       owned.header->domain == ref.domain &&
                                       owned.header->kind == ref.kind &&
                                       owned.header->id == ref.id;
                              else
                                return false;
                            },
                            source.identity);
                    },
                    d.source);
              else
                return false;
            },
            pin.dependency))
      return true;
  return false;
}
// 首次删除前完整构造、收齐并投影为拥有型 IDs，所有借 owner 在返回前销毁。
Outcome<std::optional<NativePrivateShopCustodyGrowthPins>>
read_native_private_shop_custody_growth_pins(
    const WorldBundle &w, const std::vector<StableId> &existing) {
  auto valid = validate_native_private_shop_household_custody_layout(w);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  if (!w.private_shop_household_custody)
    return {true, std::nullopt, {}};
  if (!w.read_stamps || !w.world_timeline)
    return {false, {}, bad("CustodyPinActualClockRequired").error};
  auto at = w.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  std::unique_ptr<PhaseCurrentReadScope> phase;
  if (!w.has_active_read_phase()) {


    auto memory = CheckpointMemoryCharge(0);
    const StableId name{"private_shop_custody_retention"},
        provider{"local_economy"};
    auto entered = w.enter_phase_read_context(name, provider,
                                              {at.value, at.value}, at.value);
    if (!entered.ok)
      return {false, {}, std::move(entered.error)};
    phase = std::move(entered.value);
  }
  auto owner = w.complete_exact_time_bindings();
  if (!owner.ok)
    return {false, {}, std::move(owner.error)};
  if (!owner.value || !owner.value->reads().household_foundation ||
      !owner.value->reads().original_core)
    return {false, {}, bad("CustodyPinOriginalFoundationRequired").error};
  auto pins = collect_native_private_shop_custody_source_pins(
      w, *owner.value->reads().original_core,
      *owner.value->reads().household_foundation);
  if (!pins.ok)
    return {false, {}, std::move(pins.error)};
  CheckpointVector<const StableId *> additional;
  for (const auto &row : w.growth.life_activities.rows())
    if (pins.value.contains_source({"growth", "life_activity", row.first}) &&
        std::find(existing.begin(), existing.end(), row.first) ==
            existing.end())
      additional.push_back(&row.first);
  if (additional.size() >
      std::numeric_limits<std::size_t>::max() - existing.size())
    throw std::bad_alloc();
  const auto count = existing.size() + additional.size();


  NativePrivateShopCustodyGrowthPins result;
  (result.memory).grow(0);
  result.ids.reserve(count);
  for (const auto &id : existing)
    result.ids.push_back(checkpoint_fresh_string_copy(id));
  for (const auto *id : additional)
    result.ids.push_back(checkpoint_fresh_string_copy(*id));
  return {true,
          std::optional<NativePrivateShopCustodyGrowthPins>{std::move(result)},
          {}};
}
} // namespace hanmo::resident
