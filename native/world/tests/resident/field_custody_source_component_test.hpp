// 原 Field target 的只读具名组件；只计真实 bank 中已存在的 Field capacity 来源。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <set>
#include <string>
namespace hanmo::resident {
struct CharacterConditionSources;
struct GroupMovementExecutionState;
struct FieldCustodySourceComponentCoverage {
  std::set<StableId> checked_transitions;
  Integer positive_cases = 0, negative_cases = 0;
  std::set<StableId> checked_military_openings;
  Integer military_head_positive_cases = 0, military_head_negative_cases = 0;
  Integer military_head_stale_cases = 0;
};
// 不写候选/作者输入/原断言/stop；无真实行时保 NOT_EXERCISED，不造正向来源。
void check_field_custody_source_components(
    const CharacterConditionSources &, FieldCustodySourceComponentCoverage &);
// 真实 military opening 的已保存条件头，逐原冻结时刻验证；不新造开程。
void check_field_military_head_source_components(
    const CharacterConditionSources &, const GroupMovementExecutionState &,
    FieldCustodySourceComponentCoverage &);
std::string field_custody_source_component_report(const FieldCustodySourceComponentCoverage &);
} // namespace hanmo::resident
