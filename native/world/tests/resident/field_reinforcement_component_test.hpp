#pragma once
#include "hanmo/resident_world_api.hpp"
#include "field_custody_source_component_test.hpp"
#include <set>
#include <map>
#include <string>
namespace hanmo::resident {
struct WorldBundle;
// 分列纯几何、负写口与真正出现的源覆盖；零实际入场绝不包装为正写口通过。
struct FieldReinforcementComponentCoverage {
  FieldCustodySourceComponentCoverage custody_sources;
  Integer geometry_cases = 0, negative_contract_cases = 0, negative_time_cases = 0;
  Integer negative_winner_contract_cases = 0, negative_ordinary_loss_cases = 0;
  Integer ordinary_roster_departures = 0, ordinary_named_adoptions = 0;
  Integer finite_return_held_members = 0, finite_return_prior_exits = 0;
  Integer negative_finite_return_cases = 0, stable_return_sources = 0;
  Integer continuation_selection_cases = 0, continuation_codec_cases = 0;
  Integer result_owner_negative_cases = 0, result_owner_moment_cases = 0;
  Integer quiet_current_cases = 0, quiet_head_cases = 0, quiet_negative_cases = 0;
  Integer quiet_army_cases = 0;
  bool quiet_initial_checked = false, quiet_army_checked = false;
  std::set<StableId> result_owner_facts;
  std::map<StableId, Integer> result_owner_fact_kinds, result_owner_moment_kinds;
  bool continuation_selection_checked = false;
  bool initial_checked = false, entry_rejections_checked = false;
  bool ordinary_cross_world_checked = false;
  std::set<StableId> boundaries, queues, admissions, winner_continuations, ordinary_casualties, finite_returns;
  std::set<StableId> continuation_codec_rows;
};
// 只作用原 bank 的检查副本；不修改原 ResidentWorld、作者输入、命令或 stop。
void check_field_reinforcement_components(WorldBundle &, const Bytes &,
                                         FieldReinforcementComponentCoverage &);
std::string field_reinforcement_component_report(const FieldReinforcementComponentCoverage &);
} // namespace hanmo::resident
