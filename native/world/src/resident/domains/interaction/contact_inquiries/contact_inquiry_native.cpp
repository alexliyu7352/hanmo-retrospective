#include "contact_inquiry_native.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/native_interaction_world.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
[[noreturn]]void fail(const char*c,ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(c,{},kind).error};}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
void selected(const WorldBundle&w){if(!w.catalog||!w.catalog->contact_inquiries||!w.interaction.contact_inquiries||w.catalog->contact_inquiries->version!=2||w.interaction.contact_inquiries->version!=2||!w.catalog->interaction||!w.catalog->common||!w.catalog->family||!w.catalog->actor_foundation||!w.actor_foundation||!w.action_elapsed||!w.world_timeline||!w.catalog->family_life||!w.catalog->family_life->enabled||w.catalog->family_life->selected_version!=4)fail("ContactInquiryTime2OriginalRootsRequired",ErrorKind::unsupported_capability);if(!w.catalog->personal_appointments&&!w.catalog->task_message_handoffs_enabled)fail("ContactInquiryOriginalKnowledgeRequired",ErrorKind::unsupported_capability);if(w.catalog->task_message_handoffs_enabled&&(!w.catalog->tasks||!w.tasks.message_handoffs))fail("ContactInquiryMessageOwnerRequired",ErrorKind::unsupported_capability);}
void body_bound(const WorldBundle&w,const CharacterConditionSources&b){selected(w);if(&b.common!=&w.common||&b.state!=&*w.actor_foundation||&b.catalog!=w.catalog->actor_foundation.get()||b.deaths!=&w.family_life||b.family_catalog!=w.catalog->family.get()||b.shared!=w.catalog->common.get())fail("ContactInquiryBodyCandidateMismatch");if(!b.life_sources)fail("ContactInquiryLife4OwnerRequired",ErrorKind::unsupported_capability);if(take(compare_world_moments(b.actual_now,take(w.actual_phase_moment()))))fail("ContactInquiryActualPhaseMismatch");}
const PersonalAppointmentTime2Context*appointments(const WorldBundle&w,const NativePersonalAppointmentTime2Read*a){if(!w.catalog->personal_appointments){if(a)fail("ContactInquiryUnselectedAppointmentOwner");return nullptr;}if(!a)fail("ContactInquiryAppointmentOwnerRequired",ErrorKind::unsupported_capability);const auto*x=take(a->context());if(!x||&x->common!=&w.common||&x->interaction!=&w.interaction||!w.interaction.personal_appointments||&x->state!=&*w.interaction.personal_appointments)fail("ContactInquiryAppointmentCandidateMismatch");return x;}
}
NativeContactInquiryTime2Read::NativeContactInquiryTime2Read(const WorldBundle&w,const CharacterConditionWorldRead&owner,const CharacterConditionSources&body,const NativePersonalAppointmentTime2Read*appointment_owner,const PersonalAppointmentTime2Context*a):world_(w),body_owner_(owner),appointment_owner_(appointment_owner),clock_(w),old_death_origins_(w.family_death_read()),knowledge_{w.common,*w.catalog->common,nullptr,nullptr,a,&clock_},context_{w.common,w.interaction,*w.catalog->interaction,*w.catalog->contact_inquiries,*w.interaction.contact_inquiries,knowledge_,*w.action_elapsed,w.world_timeline->scale_ref,w.catalog->identity,clock_,*w.catalog->family_life,w.family_life,*body.life_sources,*w.actor_foundation,*w.catalog->actor_foundation,*w.catalog->family,body.born_sources},actions_(context_){if(a){appointments_.emplace(AppointmentContactHistory{w.interaction,*w.catalog->interaction,*w.catalog->personal_appointments,*w.interaction.personal_appointments,old_death_origins_});knowledge_.appointments=&*appointments_;}if(w.catalog->task_message_handoffs_enabled){messages_.emplace(MessageContactHistory{w.tasks,*w.catalog->tasks,*w.tasks.message_handoffs});knowledge_.messages=&*messages_;}}
Outcome<std::unique_ptr<NativeContactInquiryTime2Read>>NativeContactInquiryTime2Read::create(const WorldBundle&w,const CharacterConditionWorldRead&owner,const NativePersonalAppointmentTime2Read*a){return guard<std::unique_ptr<NativeContactInquiryTime2Read>>([&]{selected(w);auto body=take(owner.sources());body_bound(w,body);auto original=appointments(w,a);return std::unique_ptr<NativeContactInquiryTime2Read>(new NativeContactInquiryTime2Read(w,owner,body,a,original));});}
Outcome<const ContactInquiryTime2Context*>NativeContactInquiryTime2Read::context()const{return guard<const ContactInquiryTime2Context*>([&]{auto body=take(body_owner_.sources());body_bound(world_,body);if(body.born_sources!=context_.born_sources||body.life_sources!=&context_.life_causes||appointments(world_,appointment_owner_)!=knowledge_.exact_appointments||&context_.state!=&*world_.interaction.contact_inquiries)fail("ContactInquiryOwningLeaseChanged");return &context_;});}
namespace {
class OwningRead final:public ContactInquiryExactReadLease,public CheckpointAllocatedObject {
 // The complete owner already holds this exact same-candidate original subgroup.
 // Destroying this borrower cannot destroy or rebuild a second native reader.
 std::unique_ptr<CompleteExactWorldBindingOwner> complete_;
 const NativeContactInquiryTime2Read* original_=nullptr;
public:
 OwningRead(const WorldBundle&w,std::unique_ptr<CompleteExactWorldBindingOwner>owner):complete_(std::move(owner)){
  if(!complete_)fail("ContactInquiryCompleteOwnerRequired",ErrorKind::unsupported_capability);
  const auto*group=take(read_native_interaction_world(w,complete_->reads()));
  if(!group||(original_=group->inquiry())==nullptr)fail("ContactInquiryNativeSubgroupRequired",ErrorKind::unsupported_capability);
  take(original_->context());
 }
 Outcome<const ContactInquiryTime2Context*>context()const override{return original_->context();}
};
Outcome<std::shared_ptr<const ContactInquiryExactReadLease>>read(const WorldBundle&w,const WorldMoment&at){return guard<std::shared_ptr<const ContactInquiryExactReadLease>>([&]{selected(w);if(take(compare_world_moments(at,take(w.actual_phase_moment()))))fail("ContactInquiryActualPhaseMismatch");return std::shared_ptr<const ContactInquiryExactReadLease>(checkpoint_make_shared<OwningRead>(w,take(w.complete_exact_time_bindings())));});}
}
ContactInquiryExactWorldFactory native_contact_inquiry_exact_world_factory(){return {&read};}
}
