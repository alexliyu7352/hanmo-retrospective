#include "personal_appointment_native.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/native_interaction_world.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
[[noreturn]]void fail(const char*code,ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,{},kind).error};}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
void selected(const WorldBundle&w){if(!w.catalog||!w.catalog->personal_appointments||!w.interaction.personal_appointments||w.catalog->personal_appointments->version!=2||w.interaction.personal_appointments->version!=2||!w.catalog->interaction||!w.catalog->family||!w.catalog->actor_foundation||!w.actor_foundation||!w.action_elapsed||!w.world_timeline||!w.catalog->family_life||!w.catalog->family_life->enabled||w.catalog->family_life->selected_version!=4)fail("PersonalAppointmentTime2OriginalRootsRequired",ErrorKind::unsupported_capability);}
void body_bound(const WorldBundle&w,const CharacterConditionSources&b){selected(w);if(&b.common!=&w.common||&b.state!=&*w.actor_foundation||&b.catalog!=w.catalog->actor_foundation.get()||b.deaths!=&w.family_life||b.family_catalog!=w.catalog->family.get()||b.shared!=w.catalog->common.get())fail("PersonalAppointmentBodyCandidateMismatch");if(!b.life_sources)fail("PersonalAppointmentLife4OwnerRequired",ErrorKind::unsupported_capability);if(take(compare_world_moments(b.actual_now,take(w.actual_phase_moment()))))fail("PersonalAppointmentActualPhaseMismatch");}
}
NativePersonalAppointmentTime2Read::NativePersonalAppointmentTime2Read(const WorldBundle&w,const CharacterConditionWorldRead&owner,const CharacterConditionSources&body):world_(w),body_owner_(owner),clock_(w),context_{w.common,w.interaction,*w.catalog->interaction,*w.catalog->personal_appointments,*w.interaction.personal_appointments,*w.action_elapsed,w.world_timeline->scale_ref,w.catalog->identity,clock_,*w.catalog->family_life,w.family_life,*body.life_sources,*w.actor_foundation,*w.catalog->actor_foundation,*w.catalog->family,body.born_sources},actions_(context_){}
Outcome<std::unique_ptr<NativePersonalAppointmentTime2Read>>NativePersonalAppointmentTime2Read::create(const WorldBundle&w,const CharacterConditionWorldRead&owner){return guard<std::unique_ptr<NativePersonalAppointmentTime2Read>>([&]{selected(w);auto body=take(owner.sources());body_bound(w,body);return std::unique_ptr<NativePersonalAppointmentTime2Read>(new NativePersonalAppointmentTime2Read(w,owner,body));});}
Outcome<const PersonalAppointmentTime2Context*>NativePersonalAppointmentTime2Read::context()const{return guard<const PersonalAppointmentTime2Context*>([&]{auto body=take(body_owner_.sources());body_bound(world_,body);if(body.born_sources!=context_.born_sources||body.life_sources!=&context_.life_causes||&context_.state!=&*world_.interaction.personal_appointments)fail("PersonalAppointmentOwningLeaseChanged");return &context_;});}
namespace {
class OwningRead final:public PersonalAppointmentExactReadLease,public CheckpointAllocatedObject {
 // The complete owner already holds this exact same-candidate original subgroup.
 // Destroying this borrower cannot destroy or rebuild a second native reader.
 std::unique_ptr<CompleteExactWorldBindingOwner> complete_;
 const NativePersonalAppointmentTime2Read* original_=nullptr;
public:
 OwningRead(const WorldBundle&w,std::unique_ptr<CompleteExactWorldBindingOwner>owner):complete_(std::move(owner)){
  if(!complete_)fail("PersonalAppointmentCompleteOwnerRequired",ErrorKind::unsupported_capability);
  const auto*group=take(read_native_interaction_world(w,complete_->reads()));
  if(!group||(original_=group->personal())==nullptr)fail("PersonalAppointmentNativeSubgroupRequired",ErrorKind::unsupported_capability);
  take(original_->context());
 }
 Outcome<const PersonalAppointmentTime2Context*>context()const override{return original_->context();}
};
Outcome<std::shared_ptr<const PersonalAppointmentExactReadLease>>read(const WorldBundle&w,const WorldMoment&at){return guard<std::shared_ptr<const PersonalAppointmentExactReadLease>>([&]{selected(w);if(take(compare_world_moments(at,take(w.actual_phase_moment()))))fail("PersonalAppointmentActualPhaseMismatch");return std::shared_ptr<const PersonalAppointmentExactReadLease>(checkpoint_make_shared<OwningRead>(w,take(w.complete_exact_time_bindings())));});}
}
PersonalAppointmentExactWorldFactory native_personal_appointment_exact_world_factory(){return {&read};}
}
