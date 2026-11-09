// 治理页面只消费同世界合法ID索引；当面签发、本人受领与本人原任务各守自己的读界。
#include "governance_integration.hpp"
#include "resident/views/common_presentation.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V = BoundaryValue;
using A = V::Array;
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
// 索引只保留ID；代理签发的原首长不因公权归属自动获知远方新邀请。真实代理自己的签据在独立页。
void unique_actor(CheckpointMap<StableId, std::vector<StableId>> &rows,
                  const StableId &actor, const StableId &id) {
  auto &ids = rows[actor];
  if (std::find(ids.begin(), ids.end(), id) == ids.end())
    ids.push_back(id);
}
StableId named(const CommonCatalog &c, const StableId &key) {
  const auto *value = c.texts.find(key);
  return value ? *value : StableId{};
}
V source_identity(const CommonStore &c, const StableId &actor,
                  const StableId &site) {
  StableId id;
  const char *hex = "0123456789abcdef";
  for (auto b : c.state().identity.world_id) {
    id += hex[b >> 4];
    id += hex[b & 15];
  }
  return boundary::obj(
      {{"world_id", V{id}},
       {"execution_epoch", V{c.state().identity.execution_epoch}},
       {"revision", V{c.revision()}},
       {"tick", V{c.tick()}},
       {"observer_actor_id", V{actor}},
       {"place_id", V{site}}});
}
} // namespace
GovernanceViewIndex make_governance_view_index(
    const CommonStore &common, const PoliticsGovernanceState &governance,
    const TaskState &tasks, const GovernanceTaskState &task_governance) {
  GovernanceViewIndex index;
  index.identity = common.state().identity;
  index.authority_count = governance.authorities.size();
  index.task_count = task_governance.governance_contracts.size();
  std::set<StableId> accepted;
  for (const auto &row : task_governance.governance_contracts.rows()) {
    accepted.insert(row.second.acceptance.authority_source_fact_id);
    unique_actor(index.task_ids_by_actor,
                 row.second.acceptance.executor_actor_id, row.first);
  }
  for (auto it = governance.authorities.rows().rbegin();
       it != governance.authorities.rows().rend(); ++it) {
    const auto &a = it->second;
    if (common.tick() >= a.offer_expires_tick)
      continue;
    if (a.issuer_actor_id != a.executor_actor_id &&
        it->first.rfind("politics_governance_agency_authority:", 0) != 0)
      unique_actor(index.authority_ids_by_actor, a.issuer_actor_id, it->first);
    if (!accepted.count(it->first))
      unique_actor(index.authority_ids_by_actor, a.executor_actor_id,
                   it->first);
  }
  for (auto &row : index.task_ids_by_actor)
    std::stable_sort(
        row.second.begin(), row.second.end(),
        [&](const auto &a, const auto &b) {
          const auto *x = tasks.tasks.find(a);
          const auto *y = tasks.tasks.find(b);
          const bool xa = x && (x->status == "active" || x->status == "ready" ||
                                x->status == "settling"),
                     ya = y && (y->status == "active" || y->status == "ready" ||
                                y->status == "settling");
          return xa && !ya;
        });
  return index;
}
Outcome<BoundaryValue> query_governance_business(const GovernanceReadContext &c,
                                                 const StableId &observer,
                                                 const StableId &place_id) {
  if (!c.governance_catalog.enabled)
    return {false,
            {},
            common_failure("GovernanceNotEnabled", {},
                           ErrorKind::unsupported_capability)
                .error};
  if (observer != c.common.state().metadata.player_actor_id)
    return {false,
            {},
            common_failure("GovernanceObserverMismatch", observer).error};
  auto where = c.common.resolved_location(observer);
  const auto *at =
      where.ok ? std::get_if<SettlementLocation>(&where.value) : nullptr;
  if (!at || at->place_id != place_id)
    return {false, {}, common_failure("PlaceNotObservable", place_id).error};
  if (!c.view_index || !c.politics_index ||
      !same_identity(c.view_index->identity, c.common.state().identity) ||
      c.view_index->authority_count != c.governance.authorities.size() ||
      c.view_index->task_count !=
          c.governance_tasks.governance_contracts.size() ||
      !politics_view_index_matches(*c.politics_index, c.common, c.politics))
    return {false, {}, common_failure("StaleGovernanceViewIndex").error};
  const auto &index = *c.view_index;
  A rows;
  Integer inspected = 0;
  bool truncated = false;
  CommonPresentation presentation(c.common.state(), c.common_catalog);
  auto budget = [&]() {
    if (inspected >= 256 || rows.size() >= 64) {
      truncated = true;
      return false;
    }
    ++inspected;
    return true;
  };
  auto action = [&](const StableId &text, const GovernanceCommand &command) {
    auto quote = preview_governance_business(c, observer, command);
    return boundary::obj(
        {{"text", V{text}},
         {"command", V{governance_command_kind(command)}},
         {"params", encode_governance_command(command)},
         {"available", V{quote.ok}},
         {"code", V{quote.ok ? StableId{} : quote.error.original.code}}});
  };
  for (const auto &id :
       politics_index_rows(index.authority_ids_by_actor, observer)) {
    if (!budget())
      break;
    const auto *a = c.governance.authorities.find(id);
    if (!a ||
        (a->issuer_actor_id != observer && a->executor_actor_id != observer) ||
        (id.rfind("politics_governance_agency_authority:", 0) == 0 &&
         a->executor_actor_id != observer))
      return {false, {}, common_failure("StaleGovernanceViewIndex", id).error};
    A actions;
    if (a->executor_actor_id == observer) {
      GovernanceAcceptCommand command{id, c.common.revision()};
      auto quote = preview_governance_business(c, observer, command);
      if (quote.ok ||
          quote.error.original.code != "GovernanceAuthorityAlreadyAccepted")
        actions.push_back(action("受领安民", command));
    }
    auto executor = presentation.display_name(a->executor_actor_id);
    if (!executor.ok)
      return {false, {}, executor.error};
    const auto *rule_name = c.common_catalog.texts.find(a->rule.name_key);
    if (!rule_name)
      return {
          false,
          {},
          common_failure("UnknownGovernanceRuleText", a->rule.name_key).error};
    rows.push_back(
        boundary::obj({{"kind", V{"commission"}},
                       {"id", V{id}},
                       {"display_name", V{*rule_name}},
                       {"text", V{"执行人：" + executor.value + "。需劳动" +
                                  std::to_string(a->rule.duration_ticks) +
                                  "半日，受领时预留公钱" +
                                  std::to_string(a->rule.cash_amount) + "、粮" +
                                  std::to_string(a->rule.grain_amount) +
                                  "。劳动与实际分发完成后才结算民心。"}},
                       {"actions", V{std::move(actions)}}}));
  }
  for (const auto &id :
       politics_index_rows(index.task_ids_by_actor, observer)) {
    if (!budget())
      break;
    auto progress = query_governance_task_progress(c.common, c.tasks,
                                                   c.governance_tasks, id);
    if (!progress.ok)
      return {false, {}, progress.error};
    A actions;
    if (progress.value.status == "active" || progress.value.status == "ready")
      actions.push_back(
          action("取消本人安民责任",
                 GovernanceCancelCommand{id, c.common.revision()}));
    auto lines = governance_task_progress_lines(progress.value);
    StableId text;
    for (const auto &line : lines) {
      if (!text.empty())
        text += '\n';
      text += line;
    }
    rows.push_back(
        boundary::obj({{"kind", V{"governance_task"}},
                       {"id", V{id}},
                       {"display_name", V{progress.value.display_name}},
                       {"text", V{text}},
                       {"actions", V{std::move(actions)}}}));
  }
  // 本地公职只能以真实Office/Grant形成签发动作；不列其它辖区或远方人物候选。
  for (const auto &jurisdiction_id : politics_index_rows(
           c.politics_index->jurisdiction_ids_by_site, place_id)) {
    if (!budget())
      break;
    const auto *j = c.politics.jurisdictions.find(jurisdiction_id);
    if (!j || j->seat_place_id != place_id)
      return {false,
              {},
              common_failure("StalePoliticsViewIndex", jurisdiction_id).error};
    A actions;
    bool may_read = false;
    for (const auto &office_id :
         politics_index_rows(c.politics_index->office_ids_by_actor, observer)) {
      if (inspected >= 256 || actions.size() >= 32) {
        truncated = true;
        break;
      }
      ++inspected;
      const auto *office = c.organization.offices.find(office_id);
      if (!office || !office->holder_actor_id ||
          *office->holder_actor_id != observer)
        continue;
      const ActingAuthority current{office->organization_id, office_id,
                                    office->epoch, observer};
      auto allowed =
          check_authority(c.common, c.organization, c.organization_catalog,
                          current, "political_commission_governance",
                          {ScopeRef::Kind::site, place_id}, 0);
      if (!allowed.ok)
        continue;
      may_read = true;
      for (const auto &person :
           politics_index_rows(c.politics_index->actor_ids_by_site, place_id)) {
        if (inspected >= 256 || actions.size() >= 32) {
          truncated = true;
          break;
        }
        for (const auto &policy :
             c.governance_catalog.commission_policies.rows()) {
          if (inspected >= 256 || actions.size() >= 32) {
            truncated = true;
            break;
          }
          for (const auto &inventory : j->public_inventory_ids) {
            if (inspected >= 256 || actions.size() >= 32) {
              truncated = true;
              break;
            }
            ++inspected;
            PoliticalGovernanceCommissionCommand command{
                current,      person,    jurisdiction_id,
                policy.first, inventory, j->revision};
            auto preview = preview_governance_business(c, observer, command);
            if (!preview.ok)
              continue;
            auto name = presentation.display_name(person);
            if (!name.ok)
              return {false, {}, name.error};
            const auto *rule = c.governance_catalog.appeasement_rules.find(
                policy.second.rule_id);
            actions.push_back(
                action("委任" + name.value + "办理" +
                           named(c.common_catalog, rule->name_key),
                       command));
          }
        }
      }
    }
    if (!may_read)
      continue;
    const auto *civil = c.governance.civil.find(jurisdiction_id);
    if (!civil)
      return {
          false,
          {},
          common_failure("MissingPoliticalCivilState", jurisdiction_id).error};
    rows.push_back(boundary::obj(
        {{"kind", V{"governance_jurisdiction"}},
         {"id", V{jurisdiction_id}},
         {"display_name", V{named(c.common_catalog, j->name_key)}},
         {"text", V{"本辖区民心：" + std::to_string(civil->support) +
                    "。支出从授权公库预留并按既定家庭计划实际分发。"}},
         {"actions", V{std::move(actions)}}}));
  }
  return {true,
          boundary::obj({{"ok", V{true}},
                         {"source_identity",
                          source_identity(c.common, observer, place_id)},
                         {"rows", V{std::move(rows)}},
                         {"truncated", V{truncated}}}),
          {}};
}
} // namespace hanmo::resident
