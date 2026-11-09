// 同教师原say是继续既存课程的唯一例外；下面只测此纯读，不冒World事务。
#include "resident/domains/player_plan/plan_binding.hpp"
#include <iostream>
using namespace hanmo::resident;
int main(){int checks=0;auto require=[&](bool value){++checks;if(!value){std::cerr<<"PLAN_TACTIC_LESSON_FAIL "<<checks<<'\n';std::exit(1);}};
 GrowthTacticLessonActionSource source;auto& a=source.agreement;a.student_id="student";a.teacher_id="teacher";a.dialogue_id="lesson";a.node_id="offer";a.session_id="session";a.place_id="school";a.context.provider_actor_id=a.teacher_id;a.context.place_id=a.place_id;a.context.role_id="teacher_role";
 InteractionSession session;session.id=a.session_id;session.actor_id=a.student_id;session.dialogue_id=a.dialogue_id;session.node_id=a.node_id;session.phase="say";session.context=a.context;
 require(player_plan_tactic_lesson_meeting_matches(source,session));
 auto changed=session;changed.actor_id="other";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 changed=session;changed.context.provider_actor_id="other";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 changed=session;changed.node_id="other";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 changed=session;changed.id="other";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 changed=session;changed.phase="choice";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 changed=session;changed.phase="waiting";changed.pending_action_id="same";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 changed=session;changed.context.role_id="other";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 changed=session;changed.context.place_id="other";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 changed=session;changed.context.kind="other";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 changed=session;changed.context.encounter_id="other";require(!player_plan_tactic_lesson_meeting_matches(source,changed));
 source.completion=GrowthTacticLessonCompletion{};require(!player_plan_tactic_lesson_meeting_matches(source,session));
 std::cout<<"PLAN_TACTIC_LESSON_PASS "<<checks<<"/0\n";
}
