// Same-candidate borrower; the complete world owns body, people and control
// before this object and destroys them after it. No second Family state.
#pragma once
#include "visit_appointment_meeting_time2.hpp"
#include "visit_appointment_authored.hpp"
#include "resident/domains/interaction/visit_messages/visit_authored_command.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
class VisitAppointmentExactReadLease {public:virtual ~VisitAppointmentExactReadLease()=default;virtual Outcome<const VisitAppointmentMeetingTime2Context*> context()const=0;};
class NativeVisitAppointmentTime2Read final:public VisitAppointmentExactReadLease,public CheckpointAllocatedObject {
 const WorldBundle& world_;const CharacterConditionWorldRead& body_owner_;
 const FamilyContinuationPeopleRead4& people_;const ProfileControlEligibilitySources& controls_;
 WorldCurrentMomentRead clock_;std::optional<VisitAuthoredCommandRead> commands_;std::optional<VisitAppointmentAuthorshipRead> authorship_;
 VisitAppointmentLifeContext life_;VisitAppointmentHistoricalSources history_;
 VisitAppointmentMeetingTime2Context context_;VisitAppointmentMeetingActionTimeRead actions_;
 NativeVisitAppointmentTime2Read(const WorldBundle&,const CharacterConditionWorldRead&,const CharacterConditionSources&,const FamilyContinuationPeopleRead4&,const ProfileControlEligibilitySources&);
public:
 static Outcome<std::unique_ptr<NativeVisitAppointmentTime2Read>> create(const WorldBundle&,const CharacterConditionWorldRead&,const FamilyContinuationPeopleRead4&,const ProfileControlEligibilitySources&);
 NativeVisitAppointmentTime2Read(const NativeVisitAppointmentTime2Read&)=delete;
 NativeVisitAppointmentTime2Read& operator=(const NativeVisitAppointmentTime2Read&)=delete;
 Outcome<const VisitAppointmentMeetingTime2Context*> context()const override;
 const VisitAppointmentMeetingActionTimeRead& actions()const noexcept{return actions_;}
};
struct VisitAppointmentExactWorldFactory {Outcome<std::shared_ptr<const VisitAppointmentExactReadLease>> (*read)(const WorldBundle&,const WorldMoment&)=nullptr;};
VisitAppointmentExactWorldFactory native_visit_appointment_exact_world_factory();
ExactPhaseRegistration make_visit_appointment_exact_phase_registration(VisitAppointmentExactWorldFactory);
// Meeting termination first, then original invitation life result, then the
// unique outer Action release. Each source is adopted once in this candidate.
// 同一原终止算法的借用入口；先核候选与真实 M，完成结果后才释放唯一 Action。
Status apply_visit_appointment_death_world_in_candidate(WorldBundle&,const DeathRef&,const VisitAppointmentMeetingTime2Context&,ExactPhaseWriteContext* actual_writes=nullptr);
Status apply_visit_appointment_death_world_in_candidate(WorldBundle&,const DeathRef&,VisitAppointmentExactWorldFactory,ExactPhaseWriteContext* actual_writes=nullptr);
Outcome<VisitAppointmentMeetingState> make_initial_visit_appointment_meeting_state(const VisitAppointmentCatalog&);
// Finite original callbacks shared by the one interaction provider.
Status visit_appointment_exact_validate_author_selection(VisitAppointmentExactWorldFactory,const WorldBundle&w);
Status visit_appointment_exact_validate_world_sources(VisitAppointmentExactWorldFactory,const WorldBundle&w,const WorldMoment&at);
Outcome<std::vector<ExactEventPrediction>> visit_appointment_exact_predict(VisitAppointmentExactWorldFactory,const WorldBundle&w,const WorldMoment&from,const WorldMoment&through);
Status visit_appointment_exact_provide_interval(VisitAppointmentExactWorldFactory,WorldBundle&w,const MomentSpan&span,ExactPhaseWriteContext&writes);
Outcome<ExactEventApplied> visit_appointment_exact_apply_due(VisitAppointmentExactWorldFactory,WorldBundle&w,const ExactEventPrediction&p,ExactPhaseWriteContext&writes);
Status visit_appointment_exact_validate_applied_source(VisitAppointmentExactWorldFactory,const WorldBundle&w,const ExactEventApplied&source);

}
