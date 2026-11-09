#include "director_actor_read.hpp"
#include "director_subject2.hpp"
namespace hanmo::resident {
std::optional<DirectorActorRegistrationView> director_actor_registration(const DirectorState&s,const StableId&actor){
 if(s.subject2){const auto*r=s.subject2->registrations.find(director_subject_key2({SubjectKind::actor,actor}));if(!r||r->subject.kind!=SubjectKind::actor||r->subject.id!=actor)return {};return DirectorActorRegistrationView{r->epoch,r->obligations_only,&r->policy_id,r->lifecycle};}
 const auto*r=s.registrations.find(actor);if(!r||r->actor_id!=actor)return {};return DirectorActorRegistrationView{r->epoch,r->obligations_only,&r->policy_id,r->lifecycle};
}
const ReviewSchedule* director_actor_schedule(const DirectorState&s,const StableId&actor){const auto*r=s.schedules.find(s.subject2?director_subject_key2({SubjectKind::actor,actor}):actor);return r&&r->subject.kind==SubjectKind::actor&&r->subject.id==actor?r:nullptr;}
Outcome<const FinitePlan*> read_director_actor_plan(const DirectorState&s,const StableId&actor){
 if(!s.subject2)return {true,s.plans.find(actor),{}};
 const auto*schedule=director_actor_schedule(s,actor);if(!schedule)return {false,nullptr,common_failure("DirectorActorScheduleMissing2",actor).error};
 const DirectorAdoptionResult2*latest=nullptr;
 for(const auto&row:s.subject2->adoption_results.rows()){const auto&r=row.second;if(r.subject.kind!=SubjectKind::actor||r.subject.id!=actor)continue;if(r.decision_seq<1||r.decision_seq>schedule->decision_seq||!r.plan_id||row.first!=director_adoption_key2(r.subject,r.registration_epoch,r.decision_seq))return {false,nullptr,common_failure("DirectorActorDecisionSourceMismatch2",actor).error};if(latest&&latest->decision_seq==r.decision_seq)return {false,nullptr,common_failure("DuplicateDirectorDecisionSequence2",actor).error};if(!latest||latest->decision_seq<r.decision_seq)latest=&r;}
 const FinitePlan*plan=latest?s.plans.find(*latest->plan_id):nullptr;
 if(latest&&!plan){const auto*archived=s.archived_plans.find(*latest->plan_id);if(!archived||archived->subject.kind!=SubjectKind::actor||archived->subject.id!=actor||archived->phase!=PlanPhase::completed)return {false,nullptr,common_failure("DirectorActorPlanSourceMissing2",actor).error};}
 if(plan&&(plan->id!=*latest->plan_id||plan->subject.kind!=SubjectKind::actor||plan->subject.id!=actor))return {false,nullptr,common_failure("DirectorActorPlanOwnerMismatch2",actor).error};
 for(const auto&row:s.plans.rows())if(row.second.subject.kind==SubjectKind::actor&&row.second.subject.id==actor&&&row.second!=plan){if(!latest||row.second.phase!=PlanPhase::completed)return {false,nullptr,common_failure("AmbiguousActiveDirectorActorPlan2",actor).error};}
 return {true,plan,{}};
}
}
