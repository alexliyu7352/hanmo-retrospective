// 报名只从当前教师本人say和同候选军事掌握读派生；长期授权仍由本次真实开课事实承担。
#pragma once
#include "growth_tactic_lessons.hpp"
#include "resident/domains/military/military_tactic_learning_contract.hpp"
namespace hanmo::resident {
struct GrowthTacticLessonSourceAccess {
    const GrowthSkillOriginContext& origins;
    const GrowthState& growth;
    const MilitaryTacticKnowledgeRead& military;
    // 当前/实际开课前的存在、在世及合法空闲来源；只读原Family/Life/Military，不回调本教学总验。
    std::function<Status(const StableId&,Integer,const StableId&)> person_before;
    std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
};
Status validate_growth_tactic_lesson_catalog(const CommonStore&,const InteractionCatalog&,const GrowthTacticLessonCatalog&,const MilitaryTacticKnowledgeRead&);
bool growth_tactic_lesson_agreement_equal(const GrowthTacticLessonAgreement&,const GrowthTacticLessonAgreement&);
Outcome<GrowthTacticLessonQuote> preview_growth_tactic_lesson(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const GrowthTacticLessonCatalog&,const GrowthTacticLessonSourceAccess&,const StableId& student,const StableId& definition);
Status revalidate_growth_tactic_lesson(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const GrowthTacticLessonCatalog&,const GrowthTacticLessonSourceAccess&,const GrowthTacticLessonQuote&);
// 开课头与其payload.tick由本域核；教师原取得必须严格在开课之前，不能借完成后学会倒签教师。
Status validate_growth_tactic_lesson_agreement_before_start(const CommonStore&,const InteractionCatalog&,const GrowthTacticLessonCatalog&,const GrowthTacticLessonSourceAccess&,const GrowthTacticLessonAgreement&,const StableId& actual_start_fact);
}
