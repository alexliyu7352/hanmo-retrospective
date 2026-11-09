// 公务入口保留当面人物和治所语境；远方任命只能显示本人亲历或已送达的历史。
#include "politics_views.hpp"
#include "politics_internal.hpp"
#include "politics_navigation.hpp"
#include "politics_view_index.hpp"
#include "resident/views/common_presentation.hpp"
#include <set>
namespace hanmo::resident {
using namespace politics_detail;
namespace {
using V = BoundaryValue;
using A = V::Array;
// 中文呈现明确区分受任和到任，不向玩家显示调试评分。
StableId status_text(AppointmentStatus status) {
  switch (status) {
  case AppointmentStatus::offered:
    return "任用已当面提出，等本人答复。";
  case AppointmentStatus::accepted:
    return "已接受任用，须实际到治所完成到任，职权才生效。";
  case AppointmentStatus::declined:
    return "本人已婉拒，原有生活与责任继续。";
  case AppointmentStatus::withdrawn:
    return "任用已撤回。";
  case AppointmentStatus::expired:
    return "任用时限已过。";
  case AppointmentStatus::arrived:
    return "已实际到任，可以行使列明的公职权限。";
  case AppointmentStatus::invalidated:
    return "任用的权源或本人处境已变化，需重新办理。";
  }
  return {};
}
// 名称只来自同一作者文本目录，职责正文由真实Grant逐项组成。
StableId named(const CommonCatalog &catalog, const StableId &key) {
  const auto *value = catalog.texts.find(key);
  return value ? *value : StableId{};
}
// 任用前展示完整职权及可再委任边界，不把公库余额或尚未收到的远方报告塞进条款。
StableId office_terms(const Jurisdiction &j, const CommonCatalog &cc,
                      const PoliticsCatalog &pc, const OrganizationState &org,
                      const OrganizationCatalog &oc) {
  StableId result = "任职：" + named(cc, j.office_name_key) + "；辖区：" +
                    named(cc, j.name_key) + "。\n";
  const auto *office = org.offices.find(j.office_id);
  if (!office)
    return result;
  for (const auto &id : office->grant_ids) {
    const auto *grant = oc.grants.find(id);
    if (!grant)
      continue;
    result += "授权事项：";
    bool first = true;
    for (const auto &kind : grant->allowed_command_kinds) {
      if (!first)
        result += "、";
      first = false;
      const auto *key = pc.command_name_keys.find(kind);
      result += key ? named(cc, *key) : StableId{};
    }
    result += "。适用范围：";
    first = true;
    for (const auto &scope : grant->scopes) {
      if (!first)
        result += "、";
      first = false;
      result += scope.kind == ScopeRef::Kind::site
                    ? named(cc, j.name_key) + "治所"
                    : "本辖区指定公库";
    }
    result += "。单次授权额度上限" + std::to_string(grant->amount_limit) +
              "，实际办理仍须核对资源与责任。";
    result += grant->may_delegate ? "可在上述范围内再委任。" : "不得再委任。";
    result += "授权自半日" + std::to_string(grant->valid_from) + "起";
    result += grant->valid_until
                  ? "，至半日" + std::to_string(*grant->valid_until) + "止。\n"
                  : "，至合法撤销或公权失效时止。\n";
  }
  result += "公库归组织所有，任职不改变本人私产、亲缘和旧责任。接受后须在期限内"
            "实际到治所，到任前不得行使新职权。";
  return result;
}
// 世界身份由共同权威表派生，旧控件不得跨角色、世界或执行代继续使用。
V identity(const CommonStore &c, const StableId &actor, const StableId &site) {
  StableId hex;
  const char *digits = "0123456789abcdef";
  for (auto byte : c.state().identity.world_id) {
    hex += digits[byte >> 4];
    hex += digits[byte & 15];
  }
  return boundary::obj(
      {{"world_id", V{hex}},
       {"execution_epoch", V{c.state().identity.execution_epoch}},
       {"revision", V{c.revision()}},
       {"tick", V{c.tick()}},
       {"observer_actor_id", V{actor}},
       {"place_id", V{site}}});
}
} // namespace
bool political_fact_known(const CommonStore &c, const PoliticsState &state,
                          const SocialState &social, const StableId &actor,
                          const StableId &id) {
  const auto *fact = state.facts.find(id);
  if (!fact || fact->tick > c.tick() || !c.state().facts.find(id))
    return false;
  if (std::find(fact->witness_actor_ids.begin(), fact->witness_actor_ids.end(),
                actor) != fact->witness_actor_ids.end())
    return true;
  if (!social.dynamic)
    return false;
  const auto *known = social.dynamic->known_reports.find("0:" + actor);
  if (!known)
    return false;
  const auto report_id = "report:" + id;
  const auto *row = known->find(report_id);
  const auto *report = social.dynamic->reports.find(report_id);
  return row && report && report->fact_id == id &&
         report->observation_tick <= c.tick() &&
         row->learned_tick <= c.tick() &&
         row->receiver.kind == SubjectKind::actor && row->receiver.id == actor;
}
Outcome<bool>
read_political_fact_known(const CommonStore &c, const PoliticsState &state,
                          const SocialState &social, const StableId &actor,
                          const StableId &id,
                          const OrganizationAuthorityClock *clock) {
  if (!clock)
    return {true, political_fact_known(c, state, social, actor, id), {}};
  auto now = read_organization_authority_now(c, clock);
  if (!now.ok)
    return {false, false, now.error};
  const auto *fact = state.facts.find(id);
  const auto *header = c.state().facts.find(id);
  if (!fact || !header)
    return {true, false, {}};
  if (fact->id != id || header->id != id || header->domain != "politics" ||
      header->kind != fact->kind || !fact->occurred_at ||
      fact->occurred_at->half_day_tick != fact->tick)
    return {false, false,
            common_failure("PoliticalKnowledgeSourceMismatch", id).error};
  auto order = compare_world_moments(*fact->occurred_at, now.value);
  if (!order.ok)
    return {false, false, order.error};
  if (order.value > 0)
    return {true, false, {}};
  if (std::find(fact->witness_actor_ids.begin(), fact->witness_actor_ids.end(),
                actor) != fact->witness_actor_ids.end())
    return {true, true, {}};
  if (!social.dynamic)
    return {true, false, {}};
  const auto *known = social.dynamic->known_reports.find("0:" + actor);
  if (!known)
    return {true, false, {}};
  const auto report_id = "report:" + id;
  const auto *row = known->find(report_id);
  const auto *report = social.dynamic->reports.find(report_id);
  // 报告仍沿原 Social 送达证据，不因同刻读取而补造远端知识。
  return {true,
          row && report && report->fact_id == id &&
              report->observation_tick <= now.value.half_day_tick &&
              row->learned_tick <= now.value.half_day_tick &&
              row->receiver.kind == SubjectKind::actor &&
              row->receiver.id == actor,
          {}};
}
Outcome<BoundaryValue> query_politics_entries(
    const CommonStore &c, const CommonCatalog &cc, const PoliticsState &state,
    const PoliticsCatalog &catalog, const OrganizationState &org,
    const OrganizationCatalog &oc, const LocalEconomyState &economy,
    const SocialState &social, const ActivityRead &activity,
    const StableId &observer, const StableId &site,
    const PoliticalNavigationRead *navigation,
    const PoliticsViewIndex *provided_index,
    const PoliticalDismissalPreviewRead &dismissal_preview,
    const PoliticalOrganizationRuntimeReadContext *runtime) {
  if (!catalog.enabled)
    return {false,
            {},
            common_failure("PoliticsNotEnabled", {},
                           ErrorKind::unsupported_capability)
                .error};
  // 完整任职读口须沿实际来源预检，不能将缺源伪装为没有可用命令。
  if (oc.authority_lifecycle_enabled && !runtime)
    return {false,
            {},
            common_failure("PoliticalRuntimeRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  const auto *clock = runtime ? &runtime->clock : nullptr;
  const auto here = place(c, observer);
  if (!here || here->place_id != site)
    return {false, {}, common_failure("PlaceNotObservable", site).error};
  std::optional<PoliticsViewIndex> rebuilt;
  if (runtime) {
    // 完整阶段的事实和当前 Grant 可能已更新，不能复用已提交时刻的缓存。
    auto current = make_politics_current_view_index(c, state, org, oc, social,
                                                    runtime->clock);
    if (!current.ok)
      return {false, {}, current.error};
    rebuilt.emplace(std::move(current.value));
  } else if (!provided_index)
    rebuilt = make_politics_view_index(c, state, org, oc, social);
  const auto &index = rebuilt ? *rebuilt : *provided_index;
  if (!politics_view_index_matches(index, c, state))
    return {false, {}, common_failure("StalePoliticsViewIndex").error};
  CommonPresentation presentation(c.state(), cc);
  A rows;
  Integer inspected = 0;
  bool truncated = false;
  auto action = [&](const StableId &label,
                    const PoliticsCommand &command) -> V {
    auto preview =
        preview_politics_command(c, state, catalog, org, oc, economy, activity,
                                 observer, command, runtime);
    if (preview.ok && dismissal_preview)
      if (const auto *dismiss = std::get_if<PoliticalDismissCommand>(&command))
        preview = dismissal_preview(observer, *dismiss);
    return boundary::obj(
        {{"text", V{label}},
         {"command", V{politics_command_kind(command)}},
         {"params", encode_politics_command(command)},
         {"available", V{preview.ok}},
         {"code", V{preview.ok ? StableId{} : preview.error.original.code}}});
  };
  // 每份任命从最新且合法已知的不可变事实投影；当前远地状态不能倒灌旧报告。
  std::set<StableId> shown;
  for (const auto &fact_id :
       politics_index_rows(index.known_appointment_facts_by_actor, observer)) {
    if (++inspected > 256 || rows.size() >= 64) {
      truncated = true;
      break;
    }
    const auto *source = state.facts.find(fact_id);
    if (!source)
      return {false, {}, common_failure("StalePoliticsViewIndex").error};
    const auto &fact = *source;
    if (fact.appointment_id.empty() || shown.count(fact.appointment_id))
      continue;
    auto known =
        read_political_fact_known(c, state, social, observer, fact.id, clock);
    if (!known.ok)
      return {false, {}, known.error};
    if (!known.value)
      continue;
    const auto *p = state.appointments.find(fact.appointment_id);
    if (!p || (p->actor_id != observer && p->issuer_actor_id != observer))
      continue;
    shown.insert(p->id);
    const auto *j = state.jurisdictions.find(p->jurisdiction_id);
    A actions;
    if (fact.id == p->last_fact_id && p->actor_id == observer) {
      if (fact.status == AppointmentStatus::offered) {
        actions.push_back(action(
            "接受任用", PoliticalAnswerAppointmentCommand{
                            observer, p->id, p->revision, true, fact.id}));
        actions.push_back(action(
            "婉拒任用", PoliticalAnswerAppointmentCommand{
                            observer, p->id, p->revision, false, fact.id}));
      } else if (fact.status == AppointmentStatus::accepted) {
        actions.push_back(action(
            "在治所办理到任",
            PoliticalArriveCommand{observer, p->id, p->revision, fact.id}));
        if (navigation) {
          auto step = plan_political_arrival(
              c, state, catalog, org, oc, economy, social, activity,
              *navigation, observer, p->id, p->revision, fact.id, runtime);
          if (step.ok && step.value.command) {
            V::Dict params;
            StableId kind, label = "前往治所（下一段）";
            if (const auto *move =
                    std::get_if<MoveLocalCommand>(&*step.value.command)) {
              kind = "move_local";
              params.emplace_back("place_id", V{move->place_id});
            } else {
              kind = "travel";
              const auto &travel = std::get<TravelCommand>(*step.value.command);
              V destination;
              if (const auto *place_id =
                      std::get_if<StableId>(&travel.destination))
                destination = V{*place_id};
              else {
                const auto point = std::get<PointI>(travel.destination);
                destination = V{A{V{point.x}, V{point.y}}};
              }
              params.emplace_back("destination", std::move(destination));
              if (travel.has_via)
                params.emplace_back("via", boundary::string_array(travel.via));
              if (step.value.travel_quote)
                label +=
                    "，私钱" + std::to_string(step.value.travel_quote->cost);
            }
            actions.push_back(boundary::obj({{"text", V{std::move(label)}},
                                             {"command", V{kind}},
                                             {"params", V{std::move(params)}},
                                             {"available", V{true}},
                                             {"code", V{StableId{}}}}));
          } else if (!step.ok) {
            auto denied = action(
                "此时无法前往治所",
                PoliticalArriveCommand{observer, p->id, p->revision, fact.id});
            for (auto &field : std::get<V::Dict>(denied.data)) {
              if (field.first == "available")
                field.second = V{false};
              if (field.first == "code")
                field.second = V{step.error.original.code};
            }
            actions.push_back(std::move(denied));
          }
        }
      }
    }
    if (fact.id == p->last_fact_id && p->issuer_actor_id == observer &&
        pending(fact.status)) {
      const auto *office = org.offices.find(p->issuer_office_id);
      if (office)
        actions.push_back(
            action("撤回任用",
                   PoliticalWithdrawCommand{{p->organization_id, office->id,
                                             office->epoch, observer},
                                            p->id,
                                            p->revision}));
    }
    auto name = presentation.display_name(p->actor_id);
    if (!name.ok)
      return {false, {}, name.error};
    rows.push_back(boundary::obj(
        {{"kind", V{"appointment"}},
         {"id", V{p->id}},
         {"display_name", V{name.value}},
         {"text", V{status_text(fact.status)}},
         {"jurisdiction_id", V{j->id}},
         {"seat_place_id", V{j->seat_place_id}},
         {"office_id", V{j->office_id}},
         {"source_fact_id", V{fact.id}},
         {"observed_tick", V{fact.tick}},
         {"expires_tick",
          V{fact.status == AppointmentStatus::accepted
                ? fact.tick + catalog.appointment_policies.find(p->policy_id)
                                  ->arrival_ticks
                : p->created_tick +
                      catalog.appointment_policies.find(p->policy_id)
                          ->response_ticks}},
         {"terms_text", V{office_terms(*j, cc, catalog, org, oc)}},
         {"actions", V{std::move(actions)}}}));
  }
  // 本地现职只在实际治所显示；签发新任命仍可针对授权辖区的当面候选。
  std::vector<StableId> jurisdiction_ids;
  for (const auto *ids :
       {&politics_index_rows(index.jurisdiction_ids_by_site, site),
        &politics_index_rows(index.jurisdiction_ids_by_issuer, observer)})
    for (const auto &id : *ids) {
      if (jurisdiction_ids.size() >= 256) {
        truncated = true;
        break;
      }
      if (std::find(jurisdiction_ids.begin(), jurisdiction_ids.end(), id) ==
          jurisdiction_ids.end())
        jurisdiction_ids.push_back(id);
    }
  for (const auto &jurisdiction_id : jurisdiction_ids) {
    if (++inspected > 256 || rows.size() >= 64) {
      truncated = true;
      break;
    }
    const auto *selected = state.jurisdictions.find(jurisdiction_id);
    if (!selected)
      return {false, {}, common_failure("StalePoliticsViewIndex").error};
    const auto &j = *selected;
    const auto *office = org.offices.find(j.office_id);
    if (!office)
      continue;
    A actions;
    for (const auto &office_id :
         politics_index_rows(index.office_ids_by_actor, observer)) {
      if (++inspected > 256) {
        truncated = true;
        break;
      }
      const auto *held = org.offices.find(office_id);
      if (!held)
        continue;
      const auto &issuer = *held;
      if (issuer.holder_actor_id != std::optional<StableId>(observer) ||
          issuer.status != OfficeStatus::active)
        continue;
      const ActingAuthority authority{issuer.organization_id, issuer.id,
                                      issuer.epoch, observer};
      if (dismissal_preview && office->holder_actor_id) {
        auto permit =
            political_authority(c, org, oc, authority, "political_dismiss",
                                {ScopeRef::Kind::site, j.seat_place_id}, clock);
        if (permit.ok && j.seat_place_id == site)
          actions.push_back(action(
              "办理撤职交接", PoliticalDismissCommand{
                                  authority, j.id, j.revision, office->epoch}));
        continue;
      }
      auto allowed =
          political_authority(c, org, oc, authority, "political_appoint",
                              {ScopeRef::Kind::site, j.seat_place_id}, clock);
      if (!allowed.ok)
        continue;
      if (office->holder_actor_id) {
        if (j.seat_place_id == site)
          actions.push_back(action(
              "办理撤职交接", PoliticalDismissCommand{
                                  authority, j.id, j.revision, office->epoch}));
        continue;
      }
      for (const auto &person :
           politics_index_rows(index.actor_ids_by_site, site)) {
        if (++inspected > 256 || actions.size() >= 32) {
          truncated = true;
          break;
        }
        const auto there = place(c, person);
        if (!there || there->place_id != site ||
            there->settlement_id != here->settlement_id)
          continue;
        for (const auto &policy : catalog.appointment_policies.rows()) {
          if (++inspected > 256 || actions.size() >= 32) {
            truncated = true;
            break;
          }
          PoliticalAppointCommand cmd{authority,    j.id,       person,
                                      policy.first, j.revision, office->epoch};
          auto possible =
              preview_politics_command(c, state, catalog, org, oc, economy,
                                       activity, observer, cmd, runtime);
          if (!possible.ok)
            continue;
          auto name = presentation.display_name(person);
          if (!name.ok)
            return {false, {}, name.error};
          actions.push_back(action("向" + name.value + "提出任用（" +
                                       named(cc, policy.second.name_key) + "）",
                                   cmd));
        }
      }
    }
    if (j.seat_place_id != site && actions.empty())
      continue;
    StableId text = "可向当面候选提出本辖区任用。";
    if (j.seat_place_id == site) {
      if (office->holder_actor_id) {
        auto name = presentation.display_name(*office->holder_actor_id);
        if (!name.ok)
          return {false, {}, name.error};
        text = "本地现任：" + name.value + "。公物和私人家产分别办理。";
      } else
        text = "本地职位暂缺，须由有权者提出任用并完成到任。";
    }
    rows.push_back(
        boundary::obj({{"kind", V{"jurisdiction"}},
                       {"id", V{j.id}},
                       {"display_name", V{named(cc, j.name_key)}},
                       {"text", V{text}},
                       {"seat_place_id", V{j.seat_place_id}},
                       {"office_id", V{j.office_id}},
                       {"terms_text", V{office_terms(j, cc, catalog, org, oc)}},
                       {"actions", V{std::move(actions)}}}));
  }
  return {true,
          boundary::obj({{"ok", V{true}},
                         {"source_identity", identity(c, observer, site)},
                         {"rows", V{std::move(rows)}},
                         {"truncated", V{truncated}}}),
          {}};
}
} // namespace hanmo::resident
