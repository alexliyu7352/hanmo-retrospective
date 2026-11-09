// 新教学尾永久绑定原实际request/result；不为原GrowthEffect增加字段或版本。
#pragma once
#include "growth_tactic_lesson_completion.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
struct GrowthTacticLessonNewSources {std::vector<StableId> starts,closures;};
// 调用方保存提交前完整本账，只认此次新增源；原未绑定/被改动源不能追认。
Outcome<GrowthTacticLessonNewSources> growth_tactic_lesson_new_sources(const GrowthTacticLessonState& before,const GrowthTacticLessonState& after);
// 同候选cache/history已生成、Plan reconcile前；from_tick只取提交前原World，不能由result.tick倒推。
// 从不自开Common保存点，失败不留半份绑定；原Personal/Done回执不因教学追加外层字段。
Status bind_growth_tactic_lesson_result_in_candidate(CommonStore&,GrowthTacticLessonState&,const GrowthTacticLessonNewSources&,const StableId& actor,Integer sequence,const CommitResult& actual_result,Integer from_tick);
Status validate_growth_tactic_lesson_result_bindings(const CommonStore&,const GrowthTacticLessonState&);
// actual_result必须为Common当前缓存里的同一不可变对象；不能用同ID重造DTO替代。
Outcome<std::vector<GrowthTacticLessonCompletion>> read_growth_tactic_lesson_completions_for_result(const CommonStore&,const GrowthTacticLessonState&,const CommitResult& actual_result);
}
