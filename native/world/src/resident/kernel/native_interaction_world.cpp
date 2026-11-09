#include "native_interaction_world.hpp"
#include "native_exact_result_owners.hpp"
#include "world_bundle.hpp"
#include "native_actor_social_occupancy.hpp"
#include "native_task_execution_state.hpp"
#include "resident/domains/interaction/interaction_action_confirmation.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
[[noreturn]]void required(const char* code){throw Failure{common_failure(code,{},ErrorKind::unsupported_capability).error};}
[[noreturn]]void mismatch(const char* code){throw Failure{common_failure(code,{},ErrorKind::internal).error};}
template<class T,class F>Outcome<T> guard(F f){try{return {true,f(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("InteractionOwningResourceExhausted",{},ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("InteractionOwningResourceExhausted",{},ErrorKind::internal).error};}}
bool same_selection(const NativeInteractionSectionVersions&a,const NativeInteractionSectionVersions&b){return a.messages==b.messages&&a.personal==b.personal&&a.inquiry==b.inquiry&&a.invitations==b.invitations&&a.meetings==b.meetings&&a.receipts==b.receipts&&a.replies==b.replies;}
NativeInteractionSectionVersions selected(const WorldBundle&w,const NativeCharacterFamilyWorldRead&family){
    if(!w.catalog||!w.catalog->common||!w.catalog->interaction||!w.catalog->exact_time||!exact_complete_selected(*w.catalog->exact_time))
        required("InteractionActualOwningCatalogRequired");
    take(family.validate_bindings());const auto body=take(family.body().sources());
    if(&body.common!=&w.common||!w.actor_foundation||&body.state!=&*w.actor_foundation||!family.people().bound_to(w.common,w.family_life)||&family.control().common!=&w.common)
        mismatch("InteractionFamilyOwningCandidateMismatch");
    const auto s=take(selected_native_interaction_versions(w.catalog->identity));
    take(validate_native_interaction_response_layout(*w.catalog,w.interaction));
    // This is source construction, not profile admission. The separate full
    // catalog gate stays closed while any selected writer/life hook is missing.
    if((w.catalog->interaction->visit_messages?w.catalog->interaction->visit_messages->version:0)!=s.messages||
       (w.interaction.visit_messages?w.interaction.visit_messages->version:0)!=s.messages||
       (w.catalog->personal_appointments?w.catalog->personal_appointments->version:0)!=s.personal||
       (w.interaction.personal_appointments?w.interaction.personal_appointments->version:0)!=s.personal||
       (w.catalog->contact_inquiries?w.catalog->contact_inquiries->version:0)!=s.inquiry||
       (w.interaction.contact_inquiries?w.interaction.contact_inquiries->version:0)!=s.inquiry||
       (w.catalog->visit_appointment_invitations?w.catalog->visit_appointment_invitations->version:0)!=s.invitations||
       (w.interaction.visit_appointment_invitations?w.interaction.visit_appointment_invitations->version:0)!=s.invitations||
       (w.interaction.visit_appointment_meetings?w.interaction.visit_appointment_meetings->version:0)!=s.meetings||
       (w.catalog->visit_message_replies?w.catalog->visit_message_replies->version:0)!=s.replies||
       (w.interaction.visit_message_replies?w.interaction.visit_message_replies->version:0)!=s.replies||
       bool(s.receipts)!=w.catalog->visit_message_receipts_enabled||
       (w.interaction.visit_message_receipts?w.interaction.visit_message_receipts->version:0)!=s.receipts)
        mismatch("InteractionOwningSelectedRootMismatch");
    if((s.messages>=2&&s.replies&&s.replies<3)||(s.messages==1&&s.replies>=3))
        required("InteractionReplyOriginalLifeVersionRequired");
    if(w.catalog->visit_appointment_attendance&&(!w.catalog->director||!w.catalog->director->personal_appointment_attendance))
        required("InteractionOriginalAttendanceOwnerRequired");
    take(w.actual_phase_moment());return s;
}
}
NativeInteractionWorldRead::NativeInteractionWorldRead(const WorldBundle&w,const NativeCharacterFamilyWorldRead&family,NativeInteractionSectionVersions selection)
    :world_(w),family_(family),selected_(selection),clock_(w){
    const auto response=take(selected_native_interaction_response_version(w.catalog->identity));
    if(response){
        const auto body=take(family.body().sources());const auto*occupancy=w.services.actor_social_occupancy_world();
        if(!body.life_sources||!occupancy)required("InteractionSessionActualLifeOpeningOwnerRequired");
        if(!occupancy->bound_to(w.common,w.interaction))mismatch("InteractionSessionOpeningCandidateMismatch");
        study_terminals_=take(NativeInteractionStudyLifeRead::create(w,family.body()));
        if(interaction_action_confirmation_schema_version(response)){
            growth_terminals_=take(NativeInteractionGrowthLifeRead::create(w,family.body()));
            if(take(selected_native_task_execution_version(w.catalog->identity))==2)
                task_terminals_=take(NativeInteractionTaskLifeRead::create(w,family.body()));
        }
        session_life_.emplace(InteractionSessionLifeContext{w.common,w.interaction,*w.catalog->interaction,
            *w.catalog->family_life,w.family_life,*body.life_sources,clock_,occupancy->read(),study_terminals_.get(),growth_terminals_.get(),task_terminals_.get()});
    }
    if(selection.personal==2)personal_=take(NativePersonalAppointmentTime2Read::create(w,family.body()));
    if(selection.inquiry==2)inquiry_=take(NativeContactInquiryTime2Read::create(w,family.body(),personal_.get()));
    if(selection.meetings==2)visit_=take(NativeVisitAppointmentTime2Read::create(w,family.body(),family.people(),family.control()));
    if(selection.messages>=2)messages_=take(NativeVisitMessageLifeRead::create(w,family.body()));
    if(selection.replies>=3)replies_=take(NativeVisitReplyLifeRead::create(w,family.body(),family.people(),family.control()));
    if(selection.receipts==2)receipts_=take(NativeVisitMessageReceiptRead::create(w,family.control()));
    if(w.catalog->director&&w.catalog->director->personal_appointment_attendance){
        if(!personal_)required("InteractionActualPersonalAttendanceRequired");
        const VisitAppointmentMeetingTime2Context* written=nullptr;
        if(w.catalog->visit_appointment_attendance){if(!visit_)required("InteractionActualVisitAttendanceRequired");written=take(visit_->context());}
        attendance_=take(NativeDirectorAppointmentAttendanceRead2::create(w,*take(personal_->context()),written));
    }
    if(w.catalog->director&&w.catalog->director->visit_reception){
        if(!messages_)required("InteractionActualReceptionLifeOwnerRequired");
        reception_=take(VisitReceptionWorldAccess::create(w,family.body(),family.people(),family.control()));
    }
    if(messages_||replies_||reception_){
        VisitOriginalResultSources sources{w.common,messages_?take(messages_->context()):nullptr,replies_?take(replies_->context()):nullptr,
            reception_?&reception_->context():nullptr,receipts_?&*w.interaction.visit_message_receipts:nullptr,receipts_?take(receipts_->context()):nullptr};
        originals_=checkpoint_make_shared<NativeVisitOriginalResultRead>(sources);
    }
}
Outcome<std::unique_ptr<NativeInteractionWorldRead>> NativeInteractionWorldRead::create(const WorldBundle&w,const NativeCharacterFamilyWorldRead&family){return guard<std::unique_ptr<NativeInteractionWorldRead>>([&]{
    const auto selection=selected(w,family);
    auto owner=std::unique_ptr<NativeInteractionWorldRead>(new NativeInteractionWorldRead(w,family,selection));
    take(owner->validate_bindings());return owner;
});}
bool NativeInteractionWorldRead::bound_to(const WorldBundle&w,const CharacterConditionWorldRead&body,const FamilyContinuationPeopleRead4&people,const ProfileControlEligibilitySources&control)const noexcept{
    return &world_==&w&&&family_.body()==&body&&&family_.people()==&people&&&family_.control()==&control;
}
Status NativeInteractionWorldRead::validate_bindings()const{return guard<Done>([&]{
    const auto s=selected(world_,family_);
    if(!same_selection(s,selected_)||bool(personal_)!=(s.personal==2)||bool(inquiry_)!=(s.inquiry==2)||bool(visit_)!=(s.meetings==2)||
       bool(messages_)!=(s.messages>=2)||bool(replies_)!=(s.replies>=3)||bool(receipts_)!=(s.receipts==2)||
       bool(attendance_)!=bool(world_.catalog->director&&world_.catalog->director->personal_appointment_attendance)||
       bool(reception_)!=bool(world_.catalog->director&&world_.catalog->director->visit_reception)||
       bool(originals_)!=bool(messages_||replies_||reception_))mismatch("InteractionOwningGraphChanged");
    const auto response=take(selected_native_interaction_response_version(world_.catalog->identity));
    if(bool(session_life_)!=bool(response)||bool(study_terminals_)!=bool(response)||
       bool(growth_terminals_)!=bool(interaction_action_confirmation_schema_version(response))||
       bool(task_terminals_)!=bool(interaction_action_confirmation_schema_version(response)&&take(selected_native_task_execution_version(world_.catalog->identity))==2))
        mismatch("InteractionSessionLifeOwningGraphChanged");
    if(session_life_){const auto&x=*session_life_;const auto body=take(family_.body().sources());const auto*occupancy=world_.services.actor_social_occupancy_world();
        if(!occupancy||&x.common!=&world_.common||&x.state!=&world_.interaction||&x.catalog!=world_.catalog->interaction.get()||&x.life_policy!=world_.catalog->family_life.get()||&x.deaths!=&world_.family_life||&x.causes!=body.life_sources||&x.clock!=&clock_||&x.occupancy!=&occupancy->read()||!occupancy->bound_to(world_.common,world_.interaction)||x.study_terminals!=study_terminals_.get()||!study_terminals_->bound_to(world_.common,world_.interaction))mismatch("InteractionSessionLifeOwningCandidateMismatch");
        if(x.growth_terminals!=growth_terminals_.get()||
           (growth_terminals_&&!growth_terminals_->bound_to(world_.common,world_.interaction)))
            mismatch("InteractionSessionGrowthOwningCandidateMismatch");
        if(x.task_terminals!=task_terminals_.get()||
           (task_terminals_&&!task_terminals_->bound_to(world_.common,world_.interaction)))
            mismatch("InteractionSessionTaskOwningCandidateMismatch");
    }
    if(personal_){const auto*x=take(personal_->context());if(!personal_->actions().bound_to(world_.common,x->state))mismatch("InteractionPersonalActionOwnerMismatch");}
    if(inquiry_){const auto*x=take(inquiry_->context());if(!inquiry_->actions().bound_to(world_.common,x->state))mismatch("InteractionInquiryActionOwnerMismatch");}
    if(visit_){const auto*x=take(visit_->context());if(!visit_->actions().bound_to(world_.common,x->state))mismatch("InteractionVisitActionOwnerMismatch");}
    if(messages_)take(messages_->context());if(replies_)take(replies_->context());if(receipts_)take(receipts_->context());
    if(reception_){const auto&x=reception_->context();if(&x.common!=&world_.common||&x.interactions!=&world_.interaction||!x.actual_clock||!x.message_life||&x.message_life->common!=&world_.common)mismatch("InteractionReceptionOwningCandidateMismatch");if(take(compare_world_moments(take(x.actual_clock->read_current_moment()),take(world_.actual_phase_moment()))))mismatch("InteractionReceptionActualClockMismatch");}
    return Done{};
});}
Status NativeInteractionWorldRead::validate_original_sources()const{return guard<Done>([&]{
    take(validate_bindings());
    if(messages_)take(validate_visit_message_life_sources(*take(messages_->context())));
    if(replies_)take(validate_visit_reply_read(*take(replies_->context())));
    if(receipts_)take(validate_visit_message_actual_results(world_.common,*world_.interaction.visit_message_receipts,*take(receipts_->context())));
    if(personal_)take(validate_personal_appointment_time2(*take(personal_->context())));
    if(inquiry_)take(validate_contact_inquiry_time2(*take(inquiry_->context())));
    if(visit_)take(validate_visit_appointment_meeting_time2(*take(visit_->context())));
    if(reception_)take(validate_visit_reception_world(world_,reception_->context()));
    if(attendance_)take(attendance_->validate_original_sources());
    if(session_life_)take(validate_interaction_session_life_sources(*session_life_));
    return Done{};
});}
Status NativeInteractionWorldRead::visit_session_life_pins(InteractionSessionLifeSourcePinSink&sink)const{return guard<Done>([&]{
    take(validate_bindings());
    if(session_life_)take(visit_interaction_session_life_source_pins(*session_life_,sink));
    return Done{};
});}
Status NativeInteractionWorldRead::bind_result_sources(NativeExactResultAccess&access)const{return guard<Done>([&]{
    take(validate_bindings());
    if(&access.common!=&world_.common||&access.tasks!=&world_.tasks||&access.affairs!=&world_.affairs||&access.social!=&world_.social||
       access.interaction!=&world_.interaction||access.interaction_catalog!=world_.catalog->interaction.get())mismatch("InteractionResultOwningCandidateMismatch");
    const auto*personal=personal_?take(personal_->context()):nullptr;
    const auto*inquiry=inquiry_?take(inquiry_->context()):nullptr;
    const auto*visit=visit_?take(visit_->context()):nullptr;
    if((access.personal_appointments&&access.personal_appointments!=personal)||(access.contact_inquiries&&access.contact_inquiries!=inquiry)||
       (access.visit_appointments&&access.visit_appointments!=visit)||(access.visit_originals&&access.visit_originals.get()!=originals_.get()))
        mismatch("InteractionResultOwnerAlreadyBound");
    access.personal_appointments=personal;access.contact_inquiries=inquiry;access.visit_appointments=visit;access.visit_originals=originals_;
    // This only binds real readers. P10 capture belongs after the actual source
    // and command result have reached the owner's valid original cut.
    return Done{};
});}
Status NativeInteractionWorldRead::apply_actual_death_in_candidate(WorldBundle&w,const DeathRef&death,ExactPhaseWriteContext&writes)const{return guard<Done>([&]{
    if(&w!=&world_||w.active_exact_write_context()!=&writes||!w.common.actor_condition_capture_identity())
        mismatch("InteractionDeathActualCandidateRequired");
    take(validate_bindings());
    struct Changed {WorldBundle& world;~Changed(){world.note_write_boundary();}} changed{w};
    if(messages_){take(apply_visit_message_death_in_candidate(w.common,*w.interaction.visit_messages,*take(messages_->context()),death));w.note_write_boundary();}
    if(replies_){const auto*original=take(replies_->context());if(!original->life)required("VisitReplyOriginalLifeSourcesRequired");take(apply_visit_reply_death_in_candidate(w.common,*w.interaction.visit_message_replies,*original->life,death));w.note_write_boundary();}
    if(personal_){take(apply_personal_appointment_death_world_in_candidate(w,death,*take(personal_->context()),&writes));w.note_write_boundary();}
    if(inquiry_){take(apply_contact_inquiry_death_world_in_candidate(w,death,*take(inquiry_->context()),&writes));w.note_write_boundary();}
    if(visit_){take(apply_visit_appointment_death_world_in_candidate(w,death,*take(visit_->context()),&writes));w.note_write_boundary();}
    // Original Action owners run first. The S3 source still refuses a waiting
    // or currently occupied session unless its real Study/Growth/Task owner already
    // released it and supplies its actual source. This hook never releases an Action.
    if(session_life_){auto access=w.services.interaction_access();take(apply_interaction_session_death_in_candidate(w.common,w.interaction,*session_life_,access,death,&writes.pending_study_releases,writes.pending_interaction_deaths));w.note_write_boundary();}
    return Done{};
});}
Status NativeInteractionWorldRead::validate_session_life_in_capture(const ExactPhaseWriteContext&writes)const{return guard<Done>([&]{
    if(world_.active_exact_write_context()!=&writes||!world_.common.actor_condition_capture_identity()||
       writes.interaction_condition_consumer_active||writes.interaction_condition_cursor!=writes.condition_writes.size())
        mismatch("InteractionDeathCaptureNotDrained");
    take(validate_bindings());
    if(!session_life_){if(writes.pending_interaction_deaths)mismatch("InteractionDeathCaptureSelectionMismatch");return Done{};}
    if(!writes.pending_interaction_deaths)required("InteractionDeathTailActualCaptureRequired");
    take(validate_interaction_session_life_sources_in_capture(*session_life_,*writes.pending_interaction_deaths,&writes.pending_study_releases));
    return Done{};
});}
Status NativeInteractionWorldRead::bind_actual_death_result_in_candidate(WorldBundle&w,ExactPhaseWriteContext&writes,const SourceRef&result)const{return guard<Done>([&]{
    if(&w!=&world_||w.active_exact_write_context()!=&writes||!w.common.actor_condition_capture_identity()||
       writes.interaction_condition_consumer_active||writes.interaction_condition_cursor!=writes.condition_writes.size())
        mismatch("InteractionDeathCaptureNotDrained");
    take(validate_bindings());
    if(!session_life_){if(writes.pending_interaction_deaths)mismatch("InteractionDeathCaptureSelectionMismatch");return Done{};}
    if(!writes.pending_interaction_deaths)required("InteractionDeathTailActualCaptureRequired");
    take(bind_interaction_session_death_result_in_candidate(w.common,w.interaction,*session_life_,*writes.pending_interaction_deaths,result,&writes.pending_study_releases));
    w.note_write_boundary();return Done{};
});}
Outcome<const NativeInteractionWorldRead*> read_native_interaction_world(const WorldBundle&w,const CompleteExactWorldReadBindings&bindings){return guard<const NativeInteractionWorldRead*>([&]{
    if(!bindings.interaction_sources||!bindings.family_people4||!bindings.family_control_sources)required("InteractionCompleteOwningGroupRequired");
    if(!w.catalog||w.catalog->complete_exact_time_runtime.get()!=&bindings.runtime||
       !bindings.interaction_sources->bound_to(w,bindings.character_conditions,*bindings.family_people4,*bindings.family_control_sources))
        mismatch("InteractionCompleteOwningCandidateMismatch");
    take(bindings.interaction_sources->validate_bindings());return bindings.interaction_sources;
});}
}
