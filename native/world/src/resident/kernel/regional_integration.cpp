// 有限生活历史在同候选回收，活动、责任和消息引用先固定，失败由外层事务整体放弃。
#include "regional_integration.hpp"
#include "native_household_report_source_pins.hpp"
#include "native_private_shop_custody_source_pins.hpp"
#include "world_read_stamp.hpp"
#include "resident/domains/commerce/private_shop_trade_pins.hpp"
#include "resident/domains/growth/growth_market_trade_pins.hpp"
#include "resident/domains/growth/growth_life_results.hpp"
#include "resident/views/regional_life_view.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
// 亲历知识不会无限保留全部历史；额外分享只钉住每位观察者最近16条，真实未完责任没有数量截断。
std::vector<StableId> collect_regional_life_pins(const WorldBundle &w) {
  std::set<StableId> pins;
  const auto pin = [&](const StableId &id) {
    if (w.growth.life_activities.find(id))
      pins.insert(id);
  };
  const auto report = [&](const StableId &id) {
    if (w.social.dynamic)
      if (const auto *r = w.social.dynamic->reports.find(id))
        pin(r->fact_id);
  };
  for (const auto &row : w.growth.life_activities.rows())
    if (row.second.view.phase == LifeActivityPhase::active)
      pin(row.first);
  for (const auto &row : w.director.plans.rows()) {
    const auto &p = row.second;
    if (p.current_action_id)
      pin(*p.current_action_id);
    if (p.pending_life_action_id)
      pin(*p.pending_life_action_id);
    if (p.last_life_fact_id)
      pin(*p.last_life_fact_id);
    for (const auto &id : p.known_report_ids)
      report(id);
  }
  for (const auto &row : w.organization.activities.rows()) {
    for (const auto &id : row.second.result_fact_ids)
      pin(id);
    if (row.second.execution.last_receipt_id)
      pin(*row.second.execution.last_receipt_id);
    for (const auto &participant : row.second.participation)
      pin(participant.binding_id);
  }
  for (const auto &row : w.organization.facts.rows())
    for (const auto &id : row.second.cause_fact_ids)
      pin(id);
  for (const auto &row : w.tasks.transfers.rows())
    pin(row.second.source_action_id);
  for (const auto &row : w.tasks.deliveries.rows())
    pin(row.second.source_action_id);
  for (const auto &row : w.tasks.shipments.rows())
    for (const auto &id : row.second.receipt_ids)
      pin(id);
  for (const auto &session : w.interaction.interactions)
    if (session.regional_binding)
      pin(session.regional_binding->source.fact_id);
  if (w.social.dynamic) {
    const auto &social = *w.social.dynamic;
    for (const auto &row : social.pending_messages.rows())
      if (row.second.status == MessageStatus::pending)
        report(row.second.report_id);
    for (const auto &receiver : social.known_reports.rows()) {
      std::vector<const KnownReport *> shared;
      for (const auto &row : receiver.second.rows())
        if (const auto *source = social.reports.find(row.first))
          if (const auto *life =
                  w.growth.life_activities.find(source->fact_id)) {
            const auto &known = row.second;
            const auto &people = life->view.participant_actor_ids;
            if (known.receiver.kind != SubjectKind::actor ||
                std::find(people.begin(), people.end(), known.receiver.id) ==
                    people.end())
              shared.push_back(&known);
          }
      std::sort(shared.begin(), shared.end(), [](const auto *a, const auto *b) {
        return a->learned_tick != b->learned_tick
                   ? a->learned_tick > b->learned_tick
                   : a->report_id > b->report_id;
      });
      for (std::size_t i = 0; i < std::min<std::size_t>(16, shared.size()); ++i)
        report(shared[i]->report_id);
    }
  }
  return {pins.begin(), pins.end()};
}
// 先由Growth确认可删集合，再同步清除其派生知识与曝光；不删除正在展示或等待投递的来源。
Status prune_regional_life(WorldBundle &w) {
  if (!w.regional_life_enabled())
    return common_success();
  std::vector<StableId> before;
  for (const auto &row : w.growth.life_activities.rows())
    before.push_back(row.first);
  auto pins = collect_regional_life_pins(w);
  if (w.private_shops_enabled()) {
    auto added =
        read_private_shop_trade_pins(w.common, w.private_shops, w.growth);
    if (!added.ok)
      return {false, {}, added.error};
    pins.insert(pins.end(), added.value.begin(), added.value.end());
  }
  if (w.growth_market_history_enabled()) {
    const auto &shops = w.catalog->growth_market_history->shop_ids;
    auto added = read_growth_market_trade_pins(w.common, w.growth,
                                               *w.catalog->growth, shops);
    if (!added.ok)
      return {false, {}, added.error};
    pins.insert(pins.end(), added.value.begin(), added.value.end());
  }
  if (w.personal_recovery_enabled())
    for (const auto &row : w.personal_recovery.services.rows()) {
      if (!w.growth.life_activities.find(row.first))
        return common_failure("GrowthRecoveryServiceSourceMissing", row.first);
      pins.push_back(row.first);
    }
  // Face3 真实引用在首次删除前收齐；拥有型结果不保留借读图，不反向影响旧 owner
  // 规则。
  auto report_pins = read_native_household_report_growth_pins(w, pins);
  if (!report_pins.ok)
    return {false, {}, std::move(report_pins.error)};
  const auto &report_retained =
      report_pins.value ? report_pins.value->ids : pins;
  // 明确托管的原营业引用也在首次删除前收齐；不是用整 Common 永久禁止回收。
  auto custody_pins =
      read_native_private_shop_custody_growth_pins(w, report_retained);
  if (!custody_pins.ok)
    return {false, {}, std::move(custody_pins.error)};
  const auto &retained_pins =
      custody_pins.value ? custody_pins.value->ids : report_retained;
  auto result = prune_life_evidence(w.common, w.growth, retained_pins);
  if (!result.ok)
    return result;
  std::vector<StableId> removed, retained;
  for (const auto &id : before)
    if (!w.growth.life_activities.find(id))
      removed.push_back(id);
    else
      retained.push_back(id);
  result = prune_life_reports(w.social, removed);
  if (!result.ok)
    return result;
  prune_regional_exposures(w.interaction, retained);
  return common_success();
}
// 计划选中的商店与已知供给必须属于同一真实库存；恢复不能替换来源或借别处商店套用旧消息。
Status validate_regional_world(const WorldBundle &w) {
  if (!w.regional_life_enabled())
    return common_success();
  for (const auto &row : (w.catalog->director->dynamic_subjects
                              ? w.catalog->director->policies.rows()
                              : w.catalog->director->subjects.rows()))
    if (row.second.private_livelihood) {
      const auto &policy = *row.second.private_livelihood;
      if (!w.catalog->travel->places.find(policy.return_place_id))
        return common_failure("InvalidLivelihoodDestination", row.first);
      for (const auto &id : policy.known_service_role_ids) {
        const auto *role = w.catalog->common->roles.find(id);
        const auto *service = w.catalog->growth->roles.find(id);
        if (!role || !service ||
            !w.catalog->travel->places.find(role->place_id) ||
            (service->activities.find("work") == nullptr &&
             service->activities.find("rest") == nullptr))
          return common_failure("InvalidKnownService", row.first);
      }
    }
  for (const auto &row : w.director.plans.rows())
    if (row.second.livelihood) {
      if (row.second.subject.kind != SubjectKind::actor)
        return common_failure("LivelihoodDirectorActorSubjectRequired",
                              row.first);
      const auto &intent = *row.second.livelihood;
      if (intent.selected_shop_id.empty() != intent.selected_source_id.empty())
        return common_failure("InvalidLivelihoodSource", row.first);
      if (!intent.selected_shop_id.empty()) {
        const auto *shop =
            w.catalog->growth->shops.find(intent.selected_shop_id);
        const auto *source = w.catalog->local_economy->supply_sources.find(
            intent.selected_source_id);
        if (!shop || !source || shop->inventory_id != source->inventory_id ||
            source->item_id != intent.item_id)
          return common_failure("InvalidLivelihoodSource", row.first);
      }
    }
  WorldCurrentMomentRead actual_clock(w);
  return validate_regional_life_sources(w.read(), *w.catalog, w.services,
                                        w.tasks.exact_quantities ? &actual_clock
                                                                 : nullptr);
}
} // namespace hanmo::resident
