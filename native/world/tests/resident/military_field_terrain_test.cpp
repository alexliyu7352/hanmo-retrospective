// 直接消费原报价、AI和顺序轮；固定规则不靠测试重写另一套移动算法。
#define main prior_command_resource_component_main
#include "military_command_resources_test.cpp"
#undef main
#include "resident/domains/military/military_field_terrain.hpp"
#include "resident/domains/military/military_field_terrain_profile.hpp"
namespace {
// 原领域夹具仅配置受检作者数据，人员/入场属性继续沿既有构造，不能代World来源。
// 填入原军团地形测试数据；只改私有helper名称以避开生产terrain命名空间。
void configure_field_terrain(MilitaryBattleCatalog &c) {
  c.field_terrain_enabled = true;
  c.battlefields[0].field_terrain = MilitaryFieldTerrainRule{};
  auto &t = c.troop_types[0];
  t.movement = 3;
  t.movement_costs = {{"plain", 1}, {"road", 1},  {"forest", 1}, {"hill", 2},
                      {"mud", 2},   {"water", 0}, {"wall", 0}};
}
void surface(MilitaryBattleCatalog &c, HexPoint p, const StableId &terrain) {
  for (auto &row : c.battlefields[0].cells)
    if (row.q == p.q && row.r == p.r) {
      row.terrain = terrain;
      return;
    }
  throw std::runtime_error("fixture cell missing");
}
} // namespace
int main() {
  try {
    checks = 0;
    MilitaryFieldTerrainRule rule;
    for (const auto &category :
         {StableId("infantry"), StableId("archer"), StableId("cavalry")}) {
      check(take(military_field_terrain_cost(rule, category, "plain")) == 1 &&
                take(military_field_terrain_cost(rule, category, "road")) == 1,
            "plain road one");
      check(take(military_field_terrain_cost(rule, category, "hill")) == 2,
            "hill two");
      check(take(military_field_terrain_cost(rule, category, "mud")) ==
                (category == "cavalry" ? 4 : 2),
            "real mud cost");
      check(take(military_field_terrain_cost(rule, category, "forest")) ==
                (category == "infantry" ? 1
                 : category == "archer" ? 2
                                        : 3),
            "forest differentiated");
      check(take(military_field_terrain_cost(rule, category, "water")) < 0,
            "water is never land access");
    }
    check(!military_field_terrain_cost(rule, "ship", "road").ok &&
              !military_field_terrain_cost(rule, "infantry", "invented").ok,
          "unknown matrix source rejected");
    auto c = catalog();
    configure_field_terrain(c);
    auto b = frame();
    b.units[0].position = {2, 1};
    b.units[1].position = {3, 1};
    freeze(b);
    b.ai_plans = {{"enemy", order(b.units[1], "wait")}};
    surface(c, {1, 1}, "mud");
    auto retreat = order(b.units[0], "move");
    retreat.destination = {1, 1};
    auto q = take(preview_military_order(b, c, "unit", retreat));
    check(q.cost == 2 && q.path.size() == 2,
          "one hex retreat can pay actual mud cost two");
    auto far = retreat;
    far.destination = {0, 1};
    check(!preview_military_order(b, c, "unit", far).ok,
          "zone retreat never goes two hexes");
    auto bad = retreat;
    bad.action = "attack";
    bad.target_id = "enemy";
    check(!preview_military_order(b, c, "unit", bad).ok,
          "zone retreat cannot attack");
    auto old = c;
    old.field_terrain_enabled = false;
    old.battlefields[0].field_terrain.reset();
    check(!military_hex::validate(old.battlefields[0]).ok,
          "old board rejects new mud");
    surface(old, {1, 1}, "hill");
    check(!preview_military_order(b, old, "unit", retreat).ok,
          "old movement budget path retained");
    auto arch = c.troop_types[0];
    arch.id = "arch";
    arch.category = "archer";
    arch.movement = 2;
    arch.minimum_range = 2;
    arch.maximum_range = 4;
    arch.movement_costs["forest"] = 2;
    c.troop_types.push_back(arch);
    b.units[0].troop_type_id = "arch";
    b.units[0].position = {0, 0};
    b.units[1].position = {5, 0};
    freeze(b);
    for (auto &x : b.commanded_damage->units)
      if (x.unit_id == "unit")
        x.skill_id = "archery";
    surface(c, {0, 0}, "hill");
    auto arrow = order(b.units[0], "attack", "enemy");
    check(preview_military_order(b, c, "unit", arrow).ok,
          "hill actual archer reaches five");
    surface(c, {0, 0}, "plain");
    check(!preview_military_order(b, c, "unit", arrow).ok,
          "same original range four on plain");
    surface(c, {0, 0}, "hill");
    check(military_field_attack_maximum_range(c.battlefields[0], arch,
                                              {0, 0}) == 5,
          "hill cap exactly five");
    // 对方更早实际移入控制区时，不继续原长路并攻击；事件保明确取消来源。
    c = catalog();
    configure_field_terrain(c);
    b = frame();
    b.units[0].position = {2, 1};
    b.units[1].position = {5, 1};
    b.units.push_back(make_unit("blocker", 1, {5, 0}));
    freeze(b);
    for (auto &x : b.commanded_damage->units)
      if (x.unit_id == "blocker")
        x.leadership = 100;
    auto intercept = order(b.units[2], "move");
    intercept.destination = {3, 0};
    b.ai_plans = {{"enemy", order(b.units[1], "wait")}, {"blocker", intercept}};
    auto advance = order(b.units[0], "attack", "enemy");
    advance.destination = {4, 1};
    check(preview_military_order(b, c, "unit", advance).ok,
          "initial original path is legal before new enemy move");
    Integer draws = 0;
    auto random = [&](StableId &, StableId &, Integer,
                      Integer) -> Outcome<Integer> {
      ++draws;
      return {true, 10000, {}};
    };
    auto next = take(resolve_military_round(b, c, b.controller_actor_id,
                                            {{"unit", advance}}, random));
    bool cancelled = false;
    for (const auto &event : next.last_events)
      if (event.kind == "attack" && event.unit_id == "unit" && event.code &&
          *event.code == "MilitaryZoneRetreatCannotAttack")
        cancelled = true;
    check(cancelled, "actual new zone cancels original attack");
    for (const auto &u : next.units)
      check(total(u) == 100 && u.dead.empty() && u.wounded.empty(),
            "cancelled attack conserves every roster");
    check(draws == 0, "terrain introduces no RNG");
    check(make_military_ai(next, c).ok, "AI uses same new geometry");
    check(!validate_military_field_terrain_rule({"unknown"}).ok,
          "unknown rule refused");
    std::cout << "MILITARY_FIELD_TERRAIN=" << checks
              << " FAILURES=0 SCOPE=PURE_QUOTE_ROUND\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "MILITARY_FIELD_TERRAIN_FAIL " << checks << " " << e.what()
              << '\n';
    return 1;
  }
}
