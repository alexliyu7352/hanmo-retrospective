#include "native_public_asset_history.hpp"
#include "native_political_treaty_roots.hpp"
#include "resident/domains/politics/treaty/treaty_resource_state_read2.hpp"
#include "native_character_family_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_military_state.hpp"
#include "native_military_training_period.hpp"
#include "naval_gunnery_integration.hpp"
#include "naval_integration.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/military/military_roster_history.hpp"
#include "resident/domains/military/naval_asset_history.hpp"
#include "resident/domains/military/naval_battle_history.hpp"
#include "resident/domains/military/naval_codec.hpp"
#include "resident/domains/military/occupation_rules.hpp"
#include "resident/domains/military/occupation_time.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <algorithm>
#include <cstdint>
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
// 同步原来源失败保持原错误，不向调用者提供未经证明的产权。
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
template <class T, class F> Outcome<T> guarded(F &&fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {
        false, {}, common_failure("PublicAssetHistoryResourceExhausted").error};
  } catch (const std::length_error &) {
    return {
        false, {}, common_failure("PublicAssetHistoryResourceExhausted").error};
  }
}
// 原真实行必须唯一；用当前列表中的相同 ID 不能替代缺失的历史载荷。
template <class T>
const T *unique_id(const std::vector<T> &rows, const StableId &id) {
  const T *result = nullptr;
  for (const auto &row : rows)
    if (row.id == id) {
      if (result)
        fail("PublicAssetHistoryDuplicateSource", id);
      result = &row;
    }
  return result;
}
bool contains(const std::vector<StableId> &ids, const StableId &id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
const char *approval_kind(PublicBudgetDecisionKind kind) {
  switch (kind) {
  case PublicBudgetDecisionKind::authorize:
    return "public_budget_authorized";
  case PublicBudgetDecisionKind::delegate:
    return "public_budget_delegated";
  case PublicBudgetDecisionKind::amend:
    return "public_budget_amended";
  case PublicBudgetDecisionKind::close:
    return "public_budget_closed";
  }
  fail("PublicAssetBudgetDecisionRequired");
}
} // namespace

// 成员次序固定借用寿命；没有重新获取 Complete 或军事 phase 的空 remaining。
struct NativePublicAssetHistoryRead::Sources final : CheckpointAllocatedObject {
  const NativeCompleteWorldCoreSources &core;
  const WorldBundle &world;
  const PoliticalBudgetState *budgets;
  const TreatyResourceSourceLinks2 *treaty_links = nullptr;
  const DynamicStepInput *phase;
  WorldCurrentMomentRead clock;
  MilitaryAccess access;
  CharacterConditionSources body;
  NavalGunneryWorldRead gunnery;
  std::optional<MilitaryAuthoritySourceContext> authority;
  std::optional<NavalOriginalSourceContext> naval_original;
  std::optional<NavalHistoryAccess> naval_history;
  std::optional<ExactNavalAssetHistoryContext> naval_assets;

  Sources(const NativeCompleteWorldCoreSources &c,
          const PoliticalBudgetState *b, const DynamicStepInput *p,
          CharacterConditionSources actual_body)
      : core(c), world(c.characters.early.world), budgets(b), phase(p),
        clock(world), access(c.characters.early.military_access),
        body(std::move(actual_body)), gunnery(world) {}

  // 原 Complete 前序已经持有 formation/return/period；此处只组合 source-local
  // 读口。
  void bind_naval() {
    const auto &w = world;
    if (!w.catalog->naval)
      return;
    const auto selected =
        take(selected_native_military_state_versions(w.catalog->identity));
    const auto &early = core.characters.early;
    if (selected.military_action_time != 1 ||
        selected.military_authority_time != 1 || !w.military_action_time ||
        !w.military_exact || !w.action_elapsed || !early.formations ||
        !early.returns || !early.periods || !w.production_cycles ||
        !body.scale || !w.catalog->travel || !w.catalog->family ||
        !w.catalog->growth)
      fail("PublicAssetNavalOriginalRootsRequired");
    authority.emplace(MilitaryAuthoritySourceContext{
        w.common, w.military, *w.military_action_time, *w.action_elapsed,
        ExactOfficeSourceReadContext{w.common,
                                     w.military,
                                     *w.military_action_time,
                                     *w.action_elapsed,
                                     *w.catalog->military,
                                     access,
                                     *body.scale,
                                     clock,
                                     *w.catalog,
                                     w.organization,
                                     w.family,
                                     body,
                                     {},
                                     2},
        selected.military_authority_time, nullptr});
    naval_original.emplace(NavalOriginalSourceContext{
        w.common, w.military, *w.military_exact, *w.military_action_time,
        *w.action_elapsed, *authority, *early.formations, *early.returns,
        early.periods, nullptr});
    naval_history.emplace(naval_history_access(w));
    naval_history->original_sources = &*naval_original;
    naval_history->gunnery_skills =
        w.naval_gunnery_enabled() ? &gunnery : nullptr;
    naval_assets.emplace(ExactNavalAssetHistoryContext{
        *w.military_action_time, *w.action_elapsed,
        ExactNavalRepairSourceContext{*w.catalog->military,
                                      access,
                                      *w.catalog->naval,
                                      *body.scale,
                                      clock,
                                      {}},
        &*naval_original});
    take(validate_naval_original_source_context(
        *naval_original, w.common, w.military, *w.catalog->military, access));
  }

  // 真世界/来源/时钟逐次核对；不因目录选中而自动视为历史成功。
  void validate() const {
    const auto &w = world;
    const auto &early = core.characters.early;
    if (&early.world != &w || !w.catalog || !w.catalog->common ||
        !w.catalog->military || !w.catalog->actor_foundation ||
        !w.actor_foundation || !w.world_timeline ||
        (budgets && (!w.political_budgets || &*w.political_budgets != budgets ||
                     budgets->schema_version != 2)) ||
        (!budgets && !treaty_links) ||
        access.common_catalog != w.catalog->common.get() ||
        &body.common != &w.common || &body.state != &*w.actor_foundation ||
        &body.catalog != w.catalog->actor_foundation.get() ||
        body.military != &w.military)
      fail("PublicAssetHistoryWorldBindingMismatch");
    const auto now = take(clock.read_current_moment());
    if (compare(now, take(early.clock.read_current_moment())) ||
        compare(now, body.actual_now))
      fail("PublicAssetHistoryMomentChanged");
    if (treaty_links) {
      take(validate_native_political_treaty_root_layout(w));
      if (!w.political_treaty_terms || !w.catalog->political_treaty_terms)
        fail("PublicAssetTreatyOriginalRootsRequired");
      take(validate_political_treaty_resource_body_bindings2(
          *w.political_treaty_terms, *treaty_links,
          *w.catalog->political_treaty_terms, w.catalog->identity));
      const auto &r = treaty_links->revisions;
      if (&r.common != &w.common || &r.organization != &w.organization ||
          !w.catalog->organization ||
          &r.organization_catalog != w.catalog->organization.get() ||
          compare(now, take(r.clock.read_organization_authority_now())))
        fail("PublicAssetTreatyOriginalRootsMismatch");
    }
    if (phase && (phase->from_tick != w.common.tick() || phase->from_tick < 0 ||
                  phase->from_tick == INT64_MAX ||
                  phase->to_tick != phase->from_tick + 1 ||
                  now.half_day_tick != phase->to_tick))
      fail("PublicAssetHistoryActualPhaseMismatch");
    if (bool(w.catalog->naval) != bool(naval_original) ||
        bool(naval_original) != bool(naval_history) ||
        bool(naval_original) != bool(naval_assets))
      fail("PublicAssetNavalSelectionMismatch");
    if (naval_original) {
      if (!early.formations->bound_to(w.common.state(), w.military) ||
          !early.returns->bound_to(w.common, w.military) ||
          !early.periods->bound_to(w.common, *w.production_cycles) ||
          naval_history->original_sources != &*naval_original ||
          naval_assets->original_sources != &*naval_original ||
          naval_history->gunnery_skills !=
              (w.naval_gunnery_enabled() ? &gunnery : nullptr))
        fail("PublicAssetNavalOriginalBindingMismatch");
      take(validate_naval_original_source_context(
          *naval_original, w.common, w.military, *w.catalog->military, access));
    }
  }

  // 公库不是 Estate 私产；双向反扫全部原产权 writer，不能删载荷后冒未曾转移。
  void exclude_private_and_check_coverage(const ResourceKey &key) const {
    const auto &w = world;
    const bool inventory = key.kind == ResourceKey::Kind::inventory;
    if (w.catalog->estate)
      for (const auto &row : w.catalog->estate->estates.rows())
        if ((inventory ? row.second.inventory_id : row.second.wallet_id) ==
            key.id)
          fail("PublicAssetEstateTitleRouteRequired", key.id);
    for (const auto &row : w.family.facts.rows()) {
      const auto *f = std::get_if<FamilyChoice>(&row.second);
      if (!f || f->kind != "family_succession")
        continue;
      take(read_actual_common_fact_position(
          w.common,
          SourceRefBuildView{"family", "family_succession", row.first}));
      if (!f->assets || std::count(w.family.successions.begin(),
                                   w.family.successions.end(), row.first) != 1)
        fail("PublicAssetSuccessionPayloadRequired", row.first);
      for (const auto &asset : f->assets->estates)
        if ((inventory ? asset.inventory_id : asset.wallet_id) == key.id)
          fail("PublicAssetEstateTitleRouteRequired", key.id);
    }
    for (const auto &id : w.family.successions) {
      const auto *row = w.family.facts.find(id);
      const auto *f = row ? std::get_if<FamilyChoice>(row) : nullptr;
      if (!f || f->kind != "family_succession" || !f->assets)
        fail("PublicAssetSuccessionPayloadRequired", id);
    }
    for (const auto &event : w.military.occupation_facts)
      take(read_actual_common_fact_position(
          w.common,
          SourceRefBuildView{"military", "military_occupation", event.id}));
    for (const auto &event : w.naval.battle.captures) {
      take(read_actual_common_fact_position(
          w.common,
          SourceRefBuildView{"military_naval", "naval_capture", event.id}));
      if (!w.catalog->naval ||
          !unique_id(w.catalog->naval->vessels, event.vessel_id))
        fail("PublicAssetNavalOriginalVesselRequired", event.id);
    }
    for (const auto &row : w.common.state().facts.rows()) {
      const auto &h = row.second;
      if (h.domain == "family" && h.kind == "family_succession") {
        const auto *value = w.family.facts.find(h.id);
        const auto *f = value ? std::get_if<FamilyChoice>(value) : nullptr;
        if (!f || f->kind != h.kind || !f->assets)
          fail("PublicAssetOriginalTitlePayloadMissing", h.id);
      }
      if (h.domain == "military" && h.kind == "military_occupation" &&
          !unique_id(w.military.occupation_facts, h.id))
        fail("PublicAssetOriginalTitlePayloadMissing", h.id);
      if (h.domain == "military_naval" && h.kind == "naval_capture" &&
          !unique_id(w.naval.battle.captures, h.id))
        fail("PublicAssetOriginalTitlePayloadMissing", h.id);
    }
  }

  // 只有下方具名原业务读口能选择当前前缀或真实事实切口；产权算法仍唯一。
  Owner read_at(const ResourceKey &key,
                std::optional<SourceRefBuildView> actual_boundary,
                const WorldMoment &at) const {
    validate();
    const auto &w = world;
    if (key.id.empty() || (key.kind != ResourceKey::Kind::wallet &&
                           key.kind != ResourceKey::Kind::inventory))
      fail("PublicAssetOriginalContainerRequired", key.id);
    const auto now = take(clock.read_current_moment());
    if (compare(at, now) > 0 || (!actual_boundary && compare(at, now)))
      fail("PublicAssetActualBoundaryMomentRequired", key.id);
    const auto borrowed_prefix =
        actual_boundary ? take(read_actual_common_fact_prefix_before(
                              w.common, *actual_boundary))
                        : take(read_current_common_fact_prefix(w.common));
    NavalActualPrefixScratch prefix(borrowed_prefix);
    const MilitaryActualSourceWindow actual_window{prefix.prefix, at};
    const bool inventory = key.kind == ResourceKey::Kind::inventory;
    const auto *initial =
        inventory ? w.catalog->common->required_inventories.find(key.id)
                  : w.catalog->common->required_wallets.find(key.id);
    const auto *store = inventory ? w.common.inventory(key.id) : nullptr;
    const auto *wallet = inventory ? nullptr : w.common.wallet(key.id);
    if (!initial || (!store && !wallet) ||
        initial->owner.kind != Owner::Kind::organization ||
        initial->owner.id.empty())
      fail("PublicAssetAuthoredOrganizationTitleRequired", key.id);
    exclude_private_and_check_coverage(key);
    const StrongholdDefinition *fort = nullptr;
    for (const auto &candidate : w.catalog->military->strongholds)
      if (contains(inventory ? candidate.public_inventory_ids
                             : candidate.public_wallet_ids,
                   key.id)) {
        if (fort)
          fail("PublicAssetAmbiguousStronghold", key.id);
        fort = &candidate;
      }
    const NavalVesselDefinition *vessel = nullptr;
    if (inventory && w.catalog->naval)
      for (const auto &candidate : w.catalog->naval->vessels)
        if (candidate.cargo_inventory_id == key.id) {
          if (vessel)
            fail("PublicAssetAmbiguousVessel", key.id);
          vessel = &candidate;
        }
    if (fort && vessel)
      fail("PublicAssetCompetingOriginalTitleRoutes", key.id);
    // 每个涉及本资产的原占领都必须属于同一个作者 stronghold，不能另造移交链。
    for (const auto &event : w.military.occupation_facts) {
      bool manifest_has = false;
      if (event.asset_manifest) {
        if (inventory) {
          for (const auto &row : event.asset_manifest->inventories)
            manifest_has = manifest_has || row.first == key.id;
        } else {
          for (const auto &row : event.asset_manifest->wallets)
            manifest_has = manifest_has || row.first == key.id;
        }
      }
      if (!contains(inventory ? event.public_inventory_ids
                              : event.public_wallet_ids,
                    key.id) &&
          !manifest_has)
        continue;
      if (!fort || event.stronghold_id != fort->id ||
          event.garrison_id != fort->garrison_id)
        fail("PublicAssetOccupationRouteMismatch", event.id);
      take(phase ? validate_applied_occupation_manifest(
                       w.common, w.military, *w.catalog->military, access,
                       event.id, *phase)
                 : validate_applied_occupation_manifest(w.common, w.military,
                                                        *w.catalog->military,
                                                        access, event.id));
    }
    Owner result = initial->owner;
    if (fort) {
      auto original = take(read_military_garrison_owner_before_actual(
          w.common, w.military, *w.catalog->military, access, fort->garrison_id,
          actual_boundary, at, now, access.occupation_source_schema, {},
          &actual_window));
      auto current = take(read_military_garrison_owner_before_actual(
          w.common, w.military, *w.catalog->military, access, fort->garrison_id,
          std::nullopt, now, now, access.occupation_source_schema, {},
          &actual_window));
      if (!original.original_garrison ||
          initial->owner.id != original.original_garrison->organization_id ||
          !owner_equal(
              inventory ? store->owner : wallet->owner,
              Owner{Owner::Kind::organization, current.organization_id(), {}}))
        fail("PublicAssetOccupationCurrentTitleMismatch", key.id);
      result = Owner{Owner::Kind::organization, original.organization_id(), {}};
    } else if (vessel) {
      if (!naval_assets || !naval_history ||
          !owner_equal(vessel->owner, initial->owner))
        fail("PublicAssetNavalOriginalTitleRequired", key.id);
      bool observed = false;
      auto snapshot = [&](const NavalAssetState &assets) -> Status {
        const auto *actual = unique_id(assets.vessels, vessel->id);
        if (observed || !actual || actual->cargo_inventory_id != key.id)
          return common_failure("PublicAssetNavalPrefixMismatch", key.id);
        observed = true;
        result = actual->owner;
        return common_success();
      };
      // 前像查询不代替稳定全史；先核完整实船/战斗尾，再借原实际切点。
      if (w.naval_combat_enabled()) {
        take(validate_naval_battle_history_with_exact_repairs(
            w.common, w.military, *w.catalog->military, access, w.travel,
            w.naval, *w.catalog->naval, *naval_history, *naval_assets));
        take(visit_naval_history_at_actual_prefix(
            w.common, w.military, *w.catalog->military, access, w.travel,
            w.naval, *w.catalog->naval, *naval_history, *naval_assets,
            prefix.prefix, at,
            [&](const NavalAssetState &assets, const NavalBattleState &) {
              return snapshot(assets);
            }));
      } else {
        take(validate_naval_asset_history_exact(
            w.common, w.military, *w.catalog->military, access, w.naval,
            *w.catalog->naval, *naval_assets, {}, &*naval_history));
        take(visit_naval_asset_events_at_actual_prefix(
            w.common, w.military, *w.catalog->military, access, w.naval,
            *w.catalog->naval, *naval_assets, prefix.prefix, at, {}, snapshot,
            &*naval_history));
      }
      const auto *current = unique_id(w.naval.vessels, vessel->id);
      if (!observed || !current || current->cargo_inventory_id != key.id ||
          !owner_equal(current->owner, store->owner))
        fail("PublicAssetNavalCurrentTitleMismatch", key.id);
    } else {
      // 排除所有原产权变更路线后，作者值才是未转移证据；不是只比当前值跳过历史。
      for (const auto &event : w.naval.battle.captures) {
        const auto *actual =
            unique_id(w.catalog->naval->vessels, event.vessel_id);
        if (inventory && actual->cargo_inventory_id == key.id)
          fail("PublicAssetNavalRouteRequired", key.id);
      }
      if (!owner_equal(initial->owner,
                       inventory ? store->owner : wallet->owner))
        fail("PublicAssetUnexplainedCurrentTitle", key.id);
    }
    if (result.kind != Owner::Kind::organization || result.id.empty() ||
        compare(now, take(clock.read_current_moment())))
      fail("PublicAssetHistoryResultMismatch", key.id);
    return result;
  }

  // 旧预算路径保持原实际批准、版本与容器范围检查，不借条约接口放宽。
  Owner read(const BudgetAuthorizationSource &approval,
             const ResourceKey &key) const {
    validate();
    if (!budgets || !contains(approval.container_refs, key.id) ||
        find_budget_authorization(*budgets, approval.budget_id,
                                  approval.budget_revision) != &approval ||
        approval.approval_source.domain != "politics" ||
        approval.approval_source.kind != approval_kind(approval.decision))
      fail("PublicAssetActualBudgetApprovalRequired", key.id);
    take(
        validate_budget_authorization_shape(approval, budgets->schema_version));
    take(read_actual_common_fact_position(world.common,
                                          approval.approval_source));
    return read_at(key,
                   SourceRefBuildView{approval.approval_source.domain,
                                      approval.approval_source.kind,
                                      approval.approval_source.id},
                   approval.approved_at);
  }

  // 读口只服务本 owning graph 的不可变原表；raw links 不回完整权源或 S5。
  void validate_treaty(const TreatyResourceSourceLinks2 &links) const {
    validate();
    if (treaty_links != &links)
      fail("PublicAssetTreatySourceOwnerMismatch");
  }

  // 容器必须来自原提案的有限实选来源，不允许任意 getter 冒付款权源。
  bool selected_source(const TreatyAidResourceSelection2 &selection,
                       const ResourceKey &key) const {
    if (key.kind == ResourceKey::Kind::wallet)
      return (selection.aid.money_route &&
              selection.aid.money_route->source_wallet_id == key.id) ||
             (selection.compensation &&
              selection.compensation->money_route.source_wallet_id == key.id);
    if (key.kind != ResourceKey::Kind::inventory)
      return false;
    for (const auto &route : selection.aid.item_routes)
      if (route.source_inventory_id == key.id)
        return true;
    return false;
  }
  void require_selected_source(const TreatyAidResourceProposalSource2 &proposal,
                               const ResourceKey &key,
                               const StableId *term = nullptr) const {
    for (const auto &choice : proposal.selections)
      if ((!term || choice.aid.term_id == *term) &&
          selected_source(choice, key))
        return;
    fail("PublicAssetTreatySelectedSourceRequired", key.id);
  }
};

NativePublicAssetHistoryRead::NativePublicAssetHistoryRead(
    std::unique_ptr<Sources> sources)
    : sources_(std::move(sources)) {}
NativePublicAssetHistoryRead::~NativePublicAssetHistoryRead() = default;
Outcome<std::unique_ptr<NativePublicAssetHistoryRead>>
NativePublicAssetHistoryRead::create(const NativeCompleteWorldCoreSources &core,
                                     const PoliticalBudgetState &budgets,
                                     const DynamicStepInput *phase) {
  return guarded<std::unique_ptr<NativePublicAssetHistoryRead>>([&] {
    const auto &w = core.characters.early.world;
    if (!w.catalog || !w.catalog->common || !w.catalog->military ||
        !w.catalog->actor_foundation || !w.actor_foundation ||
        !w.world_timeline)
      fail("PublicAssetHistoryOriginalRootsRequired");
    take(core.characters.family.validate_bindings());
    auto sources = std::unique_ptr<Sources>(new Sources(
        core, &budgets, phase, take(core.characters.family.body().sources())));
    sources->bind_naval();
    sources->validate();
    return std::unique_ptr<NativePublicAssetHistoryRead>(
        new NativePublicAssetHistoryRead(std::move(sources)));
  });
}
// 与预算构造分开；保存真实条约 graph 借用，外层必须保活其各原表及当前阶段。
Outcome<std::unique_ptr<NativePublicAssetHistoryRead>>
NativePublicAssetHistoryRead::create_for_treaty(
    const NativeCompleteWorldCoreSources &core,
    const TreatyResourceSourceLinks2 &links, const DynamicStepInput *phase) {
  return guarded<std::unique_ptr<NativePublicAssetHistoryRead>>([&] {
    const auto &w = core.characters.early.world;
    if (!w.catalog || !w.catalog->common || !w.catalog->military ||
        !w.catalog->actor_foundation || !w.actor_foundation ||
        !w.world_timeline || &links.revisions.common != &w.common)
      fail("PublicAssetTreatyOriginalRootsRequired");
    take(core.characters.family.validate_bindings());
    auto sources = std::unique_ptr<Sources>(new Sources(
        core, nullptr, phase, take(core.characters.family.body().sources())));
    sources->treaty_links = &links;
    sources->bind_naval();
    sources->validate();
    return std::unique_ptr<NativePublicAssetHistoryRead>(
        new NativePublicAssetHistoryRead(std::move(sources)));
  });
}
bool NativePublicAssetHistoryRead::bound_to(
    const CommonStore &common) const noexcept {
  return &sources_->world.common == &common;
}
bool NativePublicAssetHistoryRead::bound_to(
    const WorldBundle &world) const noexcept {
  return &sources_->world == &world;
}
Status NativePublicAssetHistoryRead::validate_bindings() const {
  auto valid = guarded<bool>([&] {
    sources_->validate();
    return true;
  });
  return {valid.ok, {}, std::move(valid.error)};
}
Outcome<Owner>
NativePublicAssetHistoryRead::read_public_budget_asset_owner_before_approval(
    const PoliticalBudgetState &budgets,
    const BudgetAuthorizationSource &source, const ResourceKey &key) const {
  return guarded<Owner>([&] {
    if (&budgets != sources_->budgets)
      fail("PublicAssetBudgetOwnerMismatch", key.id);
    return sources_->read(source, key);
  });
}
// 热签署只读真正当前前缀；较早提案的 proposed_at 不能替代本次签署 M。
Outcome<Owner>
NativePublicAssetHistoryRead::read_current_owner_for_ratification(
    const TreatyResourceSourceLinks2 &links,
    const TreatyAidResourceProposalSource2 &proposal, const ResourceKey &key,
    const WorldMoment &actual_now) const {
  return guarded<Owner>([&] {
    sources_->validate_treaty(links);
    const auto *actual = take(read_treaty_aid_resource_proposal2_links(
        links, {treaty_aid_resource_proposal2_source(proposal), 2}));
    if (actual != &proposal ||
        compare(actual_now, take(sources_->clock.read_current_moment())))
      fail("PublicAssetActualTreatyProposalRequired", proposal.id);
    sources_->require_selected_source(proposal, key);
    return sources_->read_at(key, std::nullopt, actual_now);
  });
}
// 冷签署先核原表身份和原 Common 头，只借严格前缀；不回完整签署权源递归。
Outcome<Owner> NativePublicAssetHistoryRead::read_owner_before_signature(
    const TreatyResourceSourceLinks2 &links,
    const TreatyResourceSignatureSource2 &signature,
    const ResourceKey &key) const {
  return guarded<Owner>([&] {
    sources_->validate_treaty(links);
    const auto ref = treaty_signature_source(signature.signature);
    const auto *actual =
        take(read_treaty_resource_signature2_links(links, {ref, 2}));
    if (actual != &signature || !signature.actual_aid_resource_proposal)
      fail("PublicAssetActualTreatySignatureRequired", ref.id);
    const auto *proposal = take(read_treaty_aid_resource_proposal2_links(
        links, *signature.actual_aid_resource_proposal));
    sources_->require_selected_source(*proposal, key);
    return sources_->read_at(key,
                             SourceRefBuildView{ref.domain, ref.kind, ref.id},
                             signature.signature.signed_at);
  });
}
// 最终接受必须重核当时产权；同 M 的 consent/activation/accepted 仍按真实头序。
Outcome<Owner> NativePublicAssetHistoryRead::read_owner_before_aid_acceptance(
    const TreatyResourceSourceLinks2 &links,
    const TreatyAidAcceptedResourceSource2 &accepted,
    const ResourceKey &key) const {
  return guarded<Owner>([&] {
    sources_->validate_treaty(links);
    const auto ref = treaty_aid_accepted_resource_source2(accepted);
    const auto *actual =
        take(read_treaty_aid_accepted_resource2_links(links, {ref, 2}));
    if (actual != &accepted)
      fail("PublicAssetActualTreatyAcceptanceRequired", ref.id);
    const auto *proposal = take(
        read_treaty_aid_resource_proposal2_links(links, accepted.proposal));
    sources_->require_selected_source(*proposal, key, &accepted.term_id);
    return sources_->read_at(key,
                             SourceRefBuildView{ref.domain, ref.kind, ref.id},
                             accepted.accepted_at);
  });
}
} // namespace hanmo::resident
