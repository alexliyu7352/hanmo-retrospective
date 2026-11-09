// 原RoadContact的有理数交会检测；道路形状只用于确认同一条线，不创造地形规则或随机会面。
#include "road_motion.hpp"
#include "route_geometry.hpp"
#include "travel.hpp"
#include <algorithm>
#include <cstdlib>
namespace hanmo::resident {
namespace {
// 缺少权威来源是接口故障，不能悄悄当作没有接触。
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code, {}, ErrorKind::internal).error};
}
} // namespace
// 使用同一个端点衔接器预演一半日的整数进度；不复制或修改共同资源。
Outcome<std::optional<RoadTravelMotion>> original_road_travel_motion(
    const CommonRead &c, const TravelCatalog &catalog,
    const TravelServices &services, const ActionCoreView &action,
    const RoadTravelParams &p, const std::vector<StableId> &hold) {
  auto actor = c.actor(action.actor_id);
  if (!actor)
    return fail<std::optional<RoadTravelMotion>>("UnknownActor");
  if (!std::holds_alternative<OnRouteLocation>(actor->location) ||
      !actor->alive || !actor->available)
    return {true, std::nullopt, {}};
  bool moving = false;
  if (std::find(hold.begin(), hold.end(), action.actor_id) == hold.end()) {
    auto allowed = can_step_road_travel(c, catalog, services, action, p);
    if (!allowed.ok && allowed.error.kind != ErrorKind::rule)
      return {false, {}, allowed.error};
    moving = allowed.ok;
  }
  auto work = p;
  Location start = actor->location;
  if (moving) {
    auto finished = finish_road_leg(catalog, c.tick(), work, start);
    if (!finished.ok)
      return {false, {}, finished.error};
    if (finished.value)
      return {true, std::nullopt, {}};
  }
  auto location = std::get_if<OnRouteLocation>(&start);
  if (!location)
    return {true, std::nullopt, {}};
  auto edge = catalog.roads.find(location->edge_id);
  if (!edge)
    return fail<std::optional<RoadTravelMotion>>("InvalidTravelPath");
  RoadTravelMotion trace;
  trace.actor_id = action.actor_id;
  trace.action_id = action.id;
  trace.edge_id = location->edge_id;
  trace.origin = location->origin;
  trace.start = trace.end = location->progress;
  trace.moving = moving;
  trace.params = std::move(work);
  if (moving)
    trace.end = trace.params.returning
                    ? std::max<Integer>(0, trace.start - 100)
                    : std::min(edge->duration_ticks * 100, trace.start + 100);
  return {true, std::move(trace), {}};
}
// 检测只产生候选；统一terrain/road的排序和人物占用选择在旅行入口完成。
Outcome<std::vector<ContactHit>> detect_road_contacts(
    const CommonRead &c, const TravelState &state, const TravelCatalog &catalog,
    const TravelServices &services, const std::vector<StableId> &hold) {
  std::vector<RoadTravelMotion> traces;
  auto ids = c.action_order();
  std::sort(ids.begin(), ids.end());
  for (const auto &id : ids) {
    auto action = c.action(id);
    if (!action)
      return fail<std::vector<ContactHit>>("MissingAction");
    if (action->kind != "travel")
      continue;
    auto p = state.road_actions.find(id);
    if (!p || !p->encounter_id.empty())
      continue;
    auto army = services.active_army(action->actor_id);
    if (!army.ok)
      return {false, {}, army.error};
    if (army.value)
      continue;
    auto trace =
        original_road_travel_motion(c, catalog, services, *action, *p, hold);
    if (!trace.ok)
      return {false, {}, trace.error};
    if (trace.value)
      traces.push_back(std::move(*trace.value));
  }
  std::vector<ContactHit> hits;
  for (std::size_t i = 0; i < traces.size(); ++i)
    for (std::size_t j = i + 1; j < traces.size(); ++j) {
      const auto &a = traces[i];
      const auto &b = traces[j];
      if (std::find(a.params.met_actions.begin(), a.params.met_actions.end(),
                    b.action_id) != a.params.met_actions.end() ||
          std::find(b.params.met_actions.begin(), b.params.met_actions.end(),
                    a.action_id) != b.params.met_actions.end())
        continue;
      auto hit = original_road_contact_intersection(catalog, a, b);
      if (hit)
        hits.push_back(std::move(*hit));
    }
  return {true, std::move(hits), {}};
}
// 截断只恢复行动前快照与交会锚点，已扣本半日费用/疲劳不退款也不再收取。
Status apply_road_contact_truncation(CommonWrite &c, TravelState &state,
                                     const ContactHit &hit,
                                     const StableId &encounter,
                                     bool interactive) {
  if (!hit.road)
    return common_failure("InvalidRoadContact", {}, ErrorKind::internal);
  for (const auto *endpoint : {&hit.road->first, &hit.road->second}) {
    const auto &trace = endpoint->trace;
    auto action = c.action(trace.action_id);
    if (!action || action->kind != "travel" ||
        action->actor_id != endpoint->actor_id ||
        !state.road_actions.find(trace.action_id))
      return common_failure("EncounterWithoutJourney", {}, ErrorKind::internal);
    auto p = trace.params;
    if (!p.has_paused && !p.field_order.empty())
      p.field_order.push_back("paused");
    p.has_paused = true;
    p.paused = interactive ? true : trace.params.paused;
    if (!p.has_encounter_id && !p.field_order.empty())
      p.field_order.push_back("encounter_id");
    if (!p.has_met_actions && !p.field_order.empty())
      p.field_order.push_back("met_actions");
    p.has_encounter_id = p.has_met_actions = true;
    p.encounter_id = interactive ? encounter : StableId{};
    p.met_actions.push_back(endpoint == &hit.road->first
                                ? hit.road->second.trace.action_id
                                : hit.road->first.trace.action_id);
    state.road_actions.set(trace.action_id, std::move(p));
    ActorMutation m;
    m.actor_id = endpoint->actor_id;
    m.location = endpoint->anchor;
    m.location_mode = ActorMutation::LocationMode::route_contact;
    auto moved = c.apply_actor(m);
    if (!moved.ok)
      return moved;
  }
  return common_success();
}
} // namespace hanmo::resident
