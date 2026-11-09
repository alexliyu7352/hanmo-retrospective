// 索引在候选发布/恢复时重建；热页面只读本人合法ID和实际当前场所对应的条目。
#include "politics_view_index.hpp"
#include "politics_fiscal.hpp"
#include "politics_views.hpp"
#include <set>
namespace hanmo::resident {
namespace {
Outcome<PoliticsViewIndex> build_politics_view_index(
    const CommonStore &c, const PoliticsState &state,
    const OrganizationState &org, const OrganizationCatalog &oc,
    const SocialState &social, const PoliticalFiscalState *fiscal,
    const OrganizationAuthorityClock *clock) {
  PoliticsViewIndex index;
  index.identity = c.state().identity;
  index.fact_count = state.facts.size();
  index.appointment_count = state.appointments.size();
  index.policy_count = fiscal ? fiscal->policies.size() : 0;
  std::map<StableId, std::vector<StableId>> historical;
  std::set<std::pair<StableId, StableId>> seen;
  for (auto row = state.facts.rows().rbegin(); row != state.facts.rows().rend();
       ++row) {
    const auto &fact = row->second;
    const auto *appointment = state.appointments.find(fact.appointment_id);
    if (!appointment)
      continue;
    for (const auto &observer :
         {appointment->actor_id, appointment->issuer_actor_id}) {
      const auto key = std::make_pair(observer, appointment->id);
      if (seen.count(key))
        continue;
      auto known =
          read_political_fact_known(c, state, social, observer, fact.id, clock);
      if (!known.ok)
        return {false, {}, known.error};
      if (!known.value)
        continue;
      seen.insert(key);
      // 排序依据本人最后已知事实，绝不把尚未送达的真实状态倒灌为可见信息。
      if (fact.status == AppointmentStatus::offered ||
          fact.status == AppointmentStatus::accepted)
        index.known_appointment_facts_by_actor[observer].push_back(fact.id);
      else
        historical[observer].push_back(fact.id);
    }
  }
  for (const auto &row : historical) {
    auto &target = index.known_appointment_facts_by_actor[row.first];
    target.insert(target.end(), row.second.begin(), row.second.end());
  }
  for (const auto &row : state.jurisdictions.rows())
    index.jurisdiction_ids_by_site[row.second.seat_place_id].push_back(
        row.first);
  for (const auto &row : org.offices.rows())
    if (row.second.holder_actor_id) {
      const auto &actor = *row.second.holder_actor_id;
      index.office_ids_by_actor[actor].push_back(row.first);
      for (const auto &grant_id : row.second.grant_ids) {
        // 只收实际当前 Grant 的候选范围；最终权限仍走原真钟和完整父链预览。
        const Grant *grant = nullptr;
        if (oc.authority_lifecycle_enabled) {
          const auto *current = org.current_grants.find(grant_id);
          if (current && !current->revoked)
            grant = &current->terms;
        } else
          grant = oc.grants.find(grant_id);
        if (!grant || std::find(grant->allowed_command_kinds.begin(),
                                grant->allowed_command_kinds.end(),
                                "political_appoint") ==
                          grant->allowed_command_kinds.end())
          continue;
        for (const auto &scope : grant->scopes)
          if (scope.kind == ScopeRef::Kind::site)
            for (const auto &id : politics_index_rows(
                     index.jurisdiction_ids_by_site, scope.id)) {
              auto &values = index.jurisdiction_ids_by_issuer[actor];
              if (std::find(values.begin(), values.end(), id) == values.end())
                values.push_back(id);
            }
      }
    }
  for (const auto &actor : c.actor_order()) {
    auto location = c.resolved_location(actor);
    if (location.ok)
      if (const auto *at = std::get_if<SettlementLocation>(&location.value))
        index.actor_ids_by_site[at->place_id].push_back(actor);
  }
  if (fiscal)
    for (const auto &row : fiscal->policies.rows()) {
      const auto &policy = row.second.snapshot;
      auto found =
          index.latest_policy_by_jurisdiction.find(policy.jurisdiction_id);
      const auto *previous = found == index.latest_policy_by_jurisdiction.end()
                                 ? nullptr
                                 : fiscal->policies.find(found->second);
      if (!previous ||
          previous->snapshot.policy_revision < policy.policy_revision)
        index.latest_policy_by_jurisdiction[policy.jurisdiction_id] = row.first;
    }
  return {true, std::move(index), {}};
}
} // namespace
PoliticsViewIndex make_politics_view_index(const CommonStore &c,
                                           const PoliticsState &state,
                                           const OrganizationState &org,
                                           const OrganizationCatalog &oc,
                                           const SocialState &social,
                                           const PoliticalFiscalState *fiscal) {
  // 旧知识路径只返回布尔判断，无新增可失败源；公开签名保持原行为。
  return std::move(
      build_politics_view_index(c, state, org, oc, social, fiscal, nullptr)
          .value);
}
Outcome<PoliticsViewIndex> make_politics_current_view_index(
    const CommonStore &c, const PoliticsState &state,
    const OrganizationState &org, const OrganizationCatalog &oc,
    const SocialState &social, const OrganizationAuthorityClock &clock,
    const PoliticalFiscalState *fiscal) {
  return build_politics_view_index(c, state, org, oc, social, fiscal, &clock);
}
bool politics_view_index_matches(const PoliticsViewIndex &index,
                                 const CommonStore &c,
                                 const PoliticsState &state) {
  const auto &a = index.identity;
  const auto &b = c.state().identity;
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick &&
         index.fact_count == state.facts.size() &&
         index.appointment_count == state.appointments.size();
}
} // namespace hanmo::resident
