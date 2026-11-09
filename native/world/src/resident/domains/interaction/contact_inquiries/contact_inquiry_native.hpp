#pragma once
#include "contact_inquiry_time2.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_native.hpp"
namespace hanmo::resident {
class ContactInquiryExactReadLease {public:virtual ~ContactInquiryExactReadLease()=default;virtual Outcome<const ContactInquiryTime2Context*> context()const=0;};
class NativeContactInquiryTime2Read final:public ContactInquiryExactReadLease,public CheckpointAllocatedObject {
 const WorldBundle& world_;const CharacterConditionWorldRead& body_owner_;
 const NativePersonalAppointmentTime2Read* appointment_owner_;
 WorldCurrentMomentRead clock_;FamilyDeathOriginRead old_death_origins_;
 std::optional<AppointmentContactHistory> appointments_;std::optional<MessageContactHistory> messages_;
 KnownContactLocationAccess knowledge_;ContactInquiryTime2Context context_;ContactInquiryActionTimeRead actions_;
 NativeContactInquiryTime2Read(const WorldBundle&,const CharacterConditionWorldRead&,const CharacterConditionSources&,const NativePersonalAppointmentTime2Read*,const PersonalAppointmentTime2Context*);
public:
 static Outcome<std::unique_ptr<NativeContactInquiryTime2Read>> create(const WorldBundle&,const CharacterConditionWorldRead&,const NativePersonalAppointmentTime2Read* actual_appointments);
 Outcome<const ContactInquiryTime2Context*> context()const override;
 const ContactInquiryActionTimeRead& actions()const noexcept{return actions_;}
};
struct ContactInquiryExactWorldFactory {Outcome<std::shared_ptr<const ContactInquiryExactReadLease>>(*read)(const WorldBundle&,const WorldMoment&)=nullptr;};
ContactInquiryExactWorldFactory native_contact_inquiry_exact_world_factory();
ExactPhaseRegistration make_contact_inquiry_exact_phase_registration(ContactInquiryExactWorldFactory);
// 同一原终止算法的借用入口；先核候选与真实 M，完成结果后才释放唯一 Action。
Status apply_contact_inquiry_death_world_in_candidate(WorldBundle&,const DeathRef&,const ContactInquiryTime2Context&,ExactPhaseWriteContext* actual_writes=nullptr);
Status apply_contact_inquiry_death_world_in_candidate(WorldBundle&,const DeathRef&,ContactInquiryExactWorldFactory,ExactPhaseWriteContext* actual_writes=nullptr);
// Finite original callbacks shared by the one interaction provider.
Status contact_inquiry_exact_validate_author_selection(ContactInquiryExactWorldFactory,const WorldBundle&w);
Status contact_inquiry_exact_validate_world_sources(ContactInquiryExactWorldFactory,const WorldBundle&w,const WorldMoment&at);
Outcome<std::vector<ExactEventPrediction>> contact_inquiry_exact_predict(ContactInquiryExactWorldFactory,const WorldBundle&w,const WorldMoment&from,const WorldMoment&through);
Status contact_inquiry_exact_provide_interval(ContactInquiryExactWorldFactory,WorldBundle&w,const MomentSpan&span,ExactPhaseWriteContext&writes);
Outcome<ExactEventApplied> contact_inquiry_exact_apply_due(ContactInquiryExactWorldFactory,WorldBundle&w,const ExactEventPrediction&p,ExactPhaseWriteContext&writes);
Status contact_inquiry_exact_validate_applied_source(ContactInquiryExactWorldFactory,const WorldBundle&w,const ExactEventApplied&source);

}
