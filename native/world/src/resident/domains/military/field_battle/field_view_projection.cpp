// 热 Read/preview 共用同一只写字段口；不序列化内部序号来源、随机流或真实成员
// ID。
#include "field_view_projection.hpp"
#include <type_traits>
namespace hanmo::resident {
namespace {
// 有限标签只按合同枚举映射；无效 typed 负例不能产生看似合法的默认标签。
template <class E, std::size_t N>
Status tag(ProjectionWriter &w, std::string_view key, E e,
           const char *const (&names)[N]) {
  auto i = static_cast<std::size_t>(e);
  return i < N ? w.text(key, names[i]) : w.reject("InvalidFieldBattleTag", key);
}
constexpr const char *states[]{"Deploying", "Running", "Ended", "Council"};
constexpr const char *stances[]{"cavalry", "aggressive", "cunning", "cautious",
                                "steady"};
constexpr const char *roles[]{"commander", "strategist", "officer"};
constexpr const char *layouts[]{"hold", "line", "flank", "ambush"};
constexpr const char *categories[]{"Infantry", "Archer", "Cavalry"};
constexpr const char *requests[]{"vanguard", "flank", "reserve", "guard"};
constexpr const char *decisions[]{"pending", "granted", "denied"};
constexpr const char *strategies[]{"ambush", "feint_strength"};
Status time(ProjectionWriter &w, const FieldBattleTime &t) {
  w.field("tick", t.tick);
  return w.field("t", t.t);
}
Status point(ProjectionWriter &w, PointI p) {
  w.field("x", p.x);
  return w.field("y", p.y);
}
Status wave(ProjectionWriter &w, FieldCouncilWave value) {
  if (value == FieldCouncilWave::Reserve)
    return w.text("wave", "reserve");
  if (value == FieldCouncilWave::First || value == FieldCouncilWave::Second)
    return w.field("wave",
                   value == FieldCouncilWave::First ? Integer{1} : Integer{2});
  return w.reject("InvalidFieldBattleTag", "wave");
}
// 公开 task 只含当前分支字段，不用冷 codec 的 variant tag/record 包装。
Status mission(ProjectionWriter &w, const FieldMissionView &m) {
  w.field("kind", m.kind);
  if (m.point_dm)
    w.object("point_dm", [&](auto &c) { return point(c, *m.point_dm); });
  if (m.facing_mdeg)
    w.field("facing_mdeg", *m.facing_mdeg);
  if (m.target_formation_id)
    w.field("target_formation_id", *m.target_formation_id);
  if (m.exit_side)
    w.field("exit_side", *m.exit_side);
  return w.status();
}
Status mission(ProjectionWriter &w, const FieldMission &m) {
  return std::visit(
      [&](const auto &v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, FieldMoveTo> ||
                      std::is_same_v<T, FieldHold>) {
          w.text("kind", std::is_same_v<T, FieldMoveTo> ? "MoveTo" : "Hold");
          w.object("point_dm", [&](auto &c) { return point(c, v.point_dm); });
          return w.field("facing_mdeg", v.facing_mdeg);
        } else if constexpr (std::is_same_v<T, FieldEngage>) {
          w.text("kind", "Engage");
          return w.field("target_formation_id", v.target_formation_id);
        } else if constexpr (std::is_same_v<T, FieldSupport>) {
          w.text("kind", "Support");
          return w.field("target_formation_id", v.ally_formation_id);
        } else {
          w.text("kind", "Withdraw");
          return w.field("exit_side", v.exit_side);
        }
      },
      m);
}
Status plan(ProjectionWriter &w, const FieldCouncilPlan &p) {
  tag(w, "layout", p.layout, layouts);
  w.sequence("assignments", p.assignments, [](auto &list, const auto &a) {
    return list.object("", [&](auto &c) {
      c.field("formation_id", a.formation_id);
      c.object("mission", [&](auto &x) { return mission(x, a.mission); });
      return wave(c, a.wave);
    });
  });
  return w.sequence("stratagems", p.stratagems, [](auto &list, const auto &s) {
    return list.object("", [&](auto &c) {
      tag(c, "kind", s.kind, strategies);
      if (s.formation_id)
        c.field("formation_id", *s.formation_id);
      if (s.area_dm)
        c.object("area", [&](auto &x) { return point(x, *s.area_dm); });
      return c.status();
    });
  });
}
Status council(ProjectionWriter &w, const FieldCouncilView &v) {
  w.sequence("participants", v.participants, [](auto &list, const auto &p) {
    return list.object("", [&](auto &c) {
      c.field("actor_id", p.actor_id);
      tag(c, "role", p.role, roles);
      c.field("formation_id", p.formation_id);
      return tag(c, "stance", p.stance, stances);
    });
  });
  w.object("intel", [&](auto &c) {
    const auto &i = v.intel;
    c.field("level", i.level);
    c.sequence("total_estimate", i.total_estimate,
               [](auto &x, Integer n) { return x.field("", n); });
    if (i.formations)
      c.sequence("formations", *i.formations, [](auto &list, const auto &f) {
        return list.object("", [&](auto &x) {
          x.field("formation_id", f.formation_id);
          tag(x, "category", f.category, categories);
          return x.field("fit", f.fit);
        });
      });
    if (i.commanders)
      c.sequence("commanders", *i.commanders, [](auto &list, const auto &a) {
        return list.object("", [&](auto &x) {
          x.field("actor_id", a.actor_id);
          return x.object("attributes", [&](auto &y) {
            y.field("leadership", a.leadership);
            y.field("martial", a.martial);
            y.field("intellect", a.intellect);
            y.field("administration", a.administration);
            return y.field("charisma", a.charisma);
          });
        });
      });
    if (i.enemy_plan)
      c.object("enemy_plan", [&](auto &x) { return plan(x, *i.enemy_plan); });
    return c.status();
  });
  w.sequence("proposals", v.proposals, [](auto &list, const auto &p) {
    return list.object("", [&](auto &c) {
      c.field("proposal_id", p.proposal_id);
      c.field("speaker_actor_id", p.speaker_actor_id);
      tag(c, "stance", p.stance, stances);
      c.object("plan", [&](auto &x) { return plan(x, p.plan); });
      return c.field("text_key", p.text_key);
    });
  });
  w.sequence("requests", v.requests, [](auto &list, const auto &r) {
    return list.object("", [&](auto &c) {
      c.field("actor_id", r.actor_id);
      std::visit(
          [&](const auto &request) {
            using T = std::decay_t<decltype(request)>;
            if constexpr (std::is_same_v<T, FieldCouncilRoleRequest>) {
              c.text("kind", "role");
              return tag(c, "value", request.role, requests);
            } else {
              c.text("kind", "target");
              return c.field("value", request.target_formation_id);
            }
          },
          r.value);
      tag(c, "status", r.status, decisions);
      return c.field("reason", r.reason);
    });
  });
  if (v.adopted_proposal_id)
    w.field("adopted_proposal_id", *v.adopted_proposal_id);
  return w.field("closed", v.closed);
}
Status event(ProjectionWriter &w, const FieldBattleEventView &e) {
  w.field("sequence", e.sequence);
  w.object("at", [&](auto &c) { return time(c, e.at); });
  w.field("kind", e.kind);
  if (e.formation_id)
    w.field("formation_id", *e.formation_id);
  if (e.target_formation_id)
    w.field("target_formation_id", *e.target_formation_id);
  if (e.count)
    w.field("count", *e.count);
  if (e.tactic)
    w.field("tactic", *e.tactic);
  if (e.stratagem)
    w.field("stratagem", *e.stratagem);
  if (e.success)
    w.field("success", *e.success);
  return w.status();
}
} // namespace
Status emit_field_battle_view_fields(ProjectionWriter &w,
                                     const FieldBattleView &v) {
  w.object("identity", [&](auto &c) {
    c.field("battle_id", v.identity.battle_id);
    c.field("segment", v.identity.segment);
    c.object("now", [&](auto &x) { return time(x, v.identity.now); });
    tag(c, "state", v.identity.state, states);
    c.field("observer_actor_id", v.identity.observer_actor_id);
    return c.field("revision", v.identity.revision);
  });
  w.object("field", [&](auto &c) {
    c.object("anchor", [&](auto &x) {
      x.field("route_id", v.field.anchor.route_id);
      x.object("point_m",
               [&](auto &y) { return point(y, v.field.anchor.point_m); });
      return x.object(
          "tangent", [&](auto &y) { return point(y, v.field.anchor.tangent); });
    });
    c.field("field_hash", v.field.field_hash);
    c.field("length_m", v.field.length_m);
    c.field("width_m", v.field.width_m);
    c.field("cell_m", v.field.cell_m);
    if (v.field.cells)
      c.sequence("cells", *v.field.cells, [](auto &list, const auto &cell) {
        return list.object("", [&](auto &x) {
          x.field("terrain", cell.terrain);
          return x.field("height_dm", cell.height_dm);
        });
      });
    return c.status();
  });
  w.sequence("sides", v.sides, [](auto &list, const auto &s) {
    return list.object("", [&](auto &c) {
      c.field("side", s.side);
      c.field("display_name", s.display_name);
      c.field("faction_id", s.faction_id);
      c.field("is_observer_side", s.is_observer_side);
      c.field("controller", s.controller);
      c.sequence("deploy_zone", s.deploy_zone, [](auto &x, std::size_t i) {
        return x.unsigned_field("", i);
      });
      return c.sequence("exits", s.exits, [](auto &x, std::size_t i) {
        return x.unsigned_field("", i);
      });
    });
  });
  w.sequence("formations", v.formations, [](auto &list, const auto &f) {
    return list.object("", [&](auto &c) {
      c.field("formation_id", f.formation_id);
      c.field("side", f.side);
      c.field("army_id", f.army_id);
      c.field("display_name", f.display_name);
      c.field("category", f.category);
      c.field("troop_type_id", f.troop_type_id);
      c.object("position_dm", [&](auto &x) { return point(x, f.position_dm); });
      c.field("facing_mdeg", f.facing_mdeg);
      c.field("width_dm", f.width_dm);
      c.field("depth_dm", f.depth_dm);
      c.field("state", f.state);
      c.field("control", f.control);
      c.object("mission", [&](auto &x) { return mission(x, f.mission); });
      c.field("fit", f.fit);
      c.field("wounded", f.wounded);
      c.field("dead", f.dead);
      c.field("captured", f.captured);
      c.field("deserted", f.deserted);
      c.field("morale", f.morale);
      c.object("commander", [&](auto &x) {
        x.field("actor_id", f.commander.actor_id);
        x.field("display_name", f.commander.display_name);
        return x.field("state", f.commander.state);
      });
      if (f.deputy_actor_id)
        c.field("deputy_actor_id", *f.deputy_actor_id);
      c.sequence("traits", f.traits,
                 [](auto &x, const auto &t) { return x.field("", t); });
      c.sequence("tactics", f.tactics, [](auto &items, const auto &t) {
        return items.object("", [&](auto &x) {
          x.field("tactic_id", t.tactic_id);
          x.field("kind", t.kind);
          x.field("point_cost", t.point_cost);
          if (t.ready_at)
            x.object("ready_at", [&](auto &y) { return time(y, *t.ready_at); });
          return x.status();
        });
      });
      c.field("tactic_points", f.tactic_points);
      if (f.wave)
        wave(c, *f.wave);
      return c.sequence("statuses", f.statuses, [](auto &items, const auto &s) {
        return items.object("", [&](auto &x) {
          x.field("kind", s.kind);
          if (s.until)
            x.object("until", [&](auto &y) { return time(y, *s.until); });
          return x.status();
        });
      });
    });
  });
  if (v.council)
    w.object("council", [&](auto &c) { return council(c, *v.council); });
  w.sequence("orders", v.orders, [](auto &list, const auto &o) {
    return list.object("", [&](auto &c) {
      c.field("order_id", o.order_id);
      c.field("recipient", o.recipient);
      if (o.mission)
        c.object("mission", [&](auto &x) { return mission(x, *o.mission); });
      if (o.tactic_id)
        c.field("tactic_id", *o.tactic_id);
      c.object("issued", [&](auto &x) { return time(x, o.issued); });
      c.field("status", o.status);
      if (o.eta_range)
        c.sequence("eta_range", *o.eta_range, [](auto &items, const auto &t) {
          return items.object("", [&](auto &x) { return time(x, t); });
        });
      return c.status();
    });
  });
  w.sequence("events", v.events, [](auto &list, const auto &e) {
    return list.object("", [&](auto &c) { return event(c, e); });
  });
  if (v.result)
    w.object("result", [&](auto &c) {
      c.field("winner_side", v.result->winner_side);
      c.field("reason", v.result->reason);
      return c.object("ended",
                      [&](auto &x) { return time(x, v.result->ended); });
    });
  return w.status();
}
Outcome<BoundaryValue>
encode_field_battle_public_view(const FieldBattleView &view) {
  BoundaryValue result;
  BoundaryProjectionWriter writer(result);
  auto emitted = emit_field_battle_view_fields(writer, view);
  if (!emitted.ok)
    return {false, {}, emitted.error};
  auto finished = writer.finish();
  if (!finished.ok)
    return {false, {}, finished.error};
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
