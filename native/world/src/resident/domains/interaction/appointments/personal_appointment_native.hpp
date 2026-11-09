// Same-candidate original S3 owning read. The enclosing complete owner keeps
// CharacterConditionWorldRead alive after this borrower; no world is copied.
#pragma once
#include "personal_appointment_time2.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
class PersonalAppointmentExactReadLease {public:virtual ~PersonalAppointmentExactReadLease()=default;virtual Outcome<const PersonalAppointmentTime2Context*> context()const=0;};
class NativePersonalAppointmentTime2Read final:public PersonalAppointmentExactReadLease,public CheckpointAllocatedObject {
 const WorldBundle& world_;const CharacterConditionWorldRead& body_owner_;
 WorldCurrentMomentRead clock_;PersonalAppointmentTime2Context context_;
 PersonalAppointmentActionTimeRead actions_;
 NativePersonalAppointmentTime2Read(const WorldBundle&,const CharacterConditionWorldRead&,const CharacterConditionSources&);
public:
 static Outcome<std::unique_ptr<NativePersonalAppointmentTime2Read>> create(const WorldBundle&,const CharacterConditionWorldRead&);
 Outcome<const PersonalAppointmentTime2Context*> context()const override;
 const PersonalAppointmentActionTimeRead& actions()const noexcept{return actions_;}
};
struct PersonalAppointmentExactWorldFactory {
 Outcome<std::shared_ptr<const PersonalAppointmentExactReadLease>> (*read)(const WorldBundle&,const WorldMoment&)=nullptr;
};
// This outer entry obtains the real complete lease, then constructs the borrower.
// Complete-owner construction itself uses Native...::create(body), never this entry.
PersonalAppointmentExactWorldFactory native_personal_appointment_exact_world_factory();
ExactPhaseRegistration make_personal_appointment_exact_phase_registration(PersonalAppointmentExactWorldFactory);
// 同一原终止算法的借用入口；先核候选与真实 M，完成结果后才释放唯一 Action。
Status apply_personal_appointment_death_world_in_candidate(WorldBundle&,const DeathRef&,const PersonalAppointmentTime2Context&,ExactPhaseWriteContext* actual_writes=nullptr);
Status apply_personal_appointment_death_world_in_candidate(WorldBundle&,const DeathRef&,PersonalAppointmentExactWorldFactory,ExactPhaseWriteContext* actual_writes=nullptr);
// Finite original callbacks shared by the one interaction provider.
Status personal_appointment_exact_validate_author_selection(PersonalAppointmentExactWorldFactory,const WorldBundle&w);
Status personal_appointment_exact_validate_world_sources(PersonalAppointmentExactWorldFactory,const WorldBundle&w,const WorldMoment&at);
Outcome<std::vector<ExactEventPrediction>> personal_appointment_exact_predict(PersonalAppointmentExactWorldFactory,const WorldBundle&w,const WorldMoment&from,const WorldMoment&through);
Status personal_appointment_exact_provide_interval(PersonalAppointmentExactWorldFactory,WorldBundle&w,const MomentSpan&span,ExactPhaseWriteContext&writes);
Outcome<ExactEventApplied> personal_appointment_exact_apply_due(PersonalAppointmentExactWorldFactory,WorldBundle&w,const ExactEventPrediction&p,ExactPhaseWriteContext&writes);
Status personal_appointment_exact_validate_applied_source(PersonalAppointmentExactWorldFactory,const WorldBundle&w,const ExactEventApplied&source);

}
