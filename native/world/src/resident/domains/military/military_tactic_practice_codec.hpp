// 沿原MilitaryCommand/Effect外壳，不给旧receipt追加字段；新命令必须由16完整profile选择。
#pragma once
#include "military_tactic_practice.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident::military_tactic_practice_codec {
// 原receipt字段壳的精确存在性门；实际权源/实例区间另由World冷绑定。
bool effect_shape(const MilitaryEffect&);
Outcome<MilitaryTacticPracticeClaim> decode_command(const BoundaryValue&);
BoundaryValue encode_quote(const MilitaryTacticPracticeCompletion&);
Status validate_cached_effect(const CommonStore&,const MilitaryTacticLearningState&,const StableId& cache_key,const StableId& kind,const StableId& result_id,Integer result_tick,const MilitaryEffect&);
}
