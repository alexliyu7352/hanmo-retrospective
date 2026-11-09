// 仅核原会面say的人工决定边界；真实Action/结果由书面会面原领域门核，不冒完整World运行。
#include "resident/domains/player_plan/plan_binding.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
}
int main(){try{
 VisitAppointmentMeetingActionSource source;source.status="active";source.actor_id="person";auto& c=source.contact;c.actor_id="person";c.session_id="interaction:1";c.dialogue_id="private_talk";c.node_id="meet";c.provider_actor_id="host";c.role_id="host_role";c.place_id="hall";
 InteractionSession session;session.id=c.session_id;session.actor_id=c.actor_id;session.dialogue_id=c.dialogue_id;session.node_id=c.node_id;session.phase="say";session.context.provider_actor_id=c.provider_actor_id;session.context.role_id=c.role_id;session.context.place_id=c.place_id;
 require(player_plan_visit_appointment_meeting_matches(source,session),"same original active meeting say can continue");
 auto changed=session;changed.id="interaction:2";require(!player_plan_visit_appointment_meeting_matches(source,changed),"another session remains a manual decision");
 changed=session;changed.actor_id="other";require(!player_plan_visit_appointment_meeting_matches(source,changed),"another initiator rejected");
 changed=session;changed.dialogue_id="another_talk";require(!player_plan_visit_appointment_meeting_matches(source,changed),"another dialogue rejected");
 changed=session;changed.node_id="cancel";require(!player_plan_visit_appointment_meeting_matches(source,changed),"cancellation requires actual user decision");
 changed=session;changed.phase="choice";require(!player_plan_visit_appointment_meeting_matches(source,changed),"choice is never automatically answered");
 changed=session;changed.context.provider_actor_id="other";require(!player_plan_visit_appointment_meeting_matches(source,changed),"another provider rejected");
 changed=session;changed.context.role_id="other";require(!player_plan_visit_appointment_meeting_matches(source,changed),"another role rejected");
 changed=session;changed.context.place_id="elsewhere";require(!player_plan_visit_appointment_meeting_matches(source,changed),"another place rejected");
 changed=session;changed.pending_action_id="action:2";require(!player_plan_visit_appointment_meeting_matches(source,changed),"unrelated pending action rejected");
 changed=session;changed.context.kind="encounter";require(!player_plan_visit_appointment_meeting_matches(source,changed),"encounter does not become private meeting");
 changed=session;changed.context.encounter_id="encounter:1";require(!player_plan_visit_appointment_meeting_matches(source,changed),"encounter reference rejected");
 source.status="completed";require(!player_plan_visit_appointment_meeting_matches(source,session),"completion never reopens original dialogue");
 source.status="active";source.actor_id="other";require(!player_plan_visit_appointment_meeting_matches(source,session),"original source actor must match its contact");
 std::cout<<"PLAYER_PLAN_VISIT_APPOINTMENT "<<checks<<"/0 same-say boundary only; World deferred\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
