#include "resident/domains/interaction/interaction_source_projection.hpp"
// JSON共同头和活动params都来自独立预约原件；缺实体、错身份或额外字段不能由导入丢弃。
#include "personal_appointment_detail.hpp"
namespace hanmo::resident {
Outcome<bool> emit_personal_appointment_fact_payload_fields(ProjectionWriter& w,const PersonalAppointmentState* state,const FactRecord& h){const bool owned=h.kind==personal_appointment_accepted_kind||h.kind==personal_appointment_cancelled_kind||h.kind==personal_appointment_started_kind||h.kind==personal_appointment_finished_kind;
        if(!owned&&h.domain!=personal_appointment_module)return {true,false,{}};
        if(!owned||!state||h.domain!=personal_appointment_module)return {false,{},common_failure("PersonalAppointmentFactMismatch",h.id).error};auto status=emit_personal_appointment_fact_fields(w,*state,h.id,&h);if(!status.ok)return {false,{},status.error};return {w.status().ok,w.status().ok,w.status().error};}
Status emit_personal_appointment_fact_payloads_fields(ProjectionWriter& w,const CommonStore& common,const PersonalAppointmentState* state){
    std::size_t count=0;
    for(const auto& row:common.state().facts.rows()){
        const auto& h=row.second;const bool owned=h.kind==personal_appointment_accepted_kind||h.kind==personal_appointment_cancelled_kind||h.kind==personal_appointment_started_kind||h.kind==personal_appointment_finished_kind;
        if(!owned&&h.domain!=personal_appointment_module)continue;
        if(!owned||!state||h.id!=row.first||h.domain!=personal_appointment_module)return common_failure("PersonalAppointmentFactMismatch",row.first);
        auto checked=w.object(row.first,h.field_order,[&](auto&out){return emit_personal_appointment_fact_fields(out,*state,row.first,&h);});if(!checked.ok)return checked;++count;
    }
    if(state&&count!=state->accepted.size()+state->cancelled.size()+state->started.size()+state->finished.size())return common_failure("PersonalAppointmentFactCoverageMismatch");return w.status();
}
Outcome<BoundaryValue> personal_appointment_fact_payloads(const CommonStore& common,const PersonalAppointmentState* state){BoundaryValue value;BoundaryProjectionWriter writer(value);auto checked=writer.object("",[&](auto&out){return emit_personal_appointment_fact_payloads_fields(out,common,state);});if(checked.ok)checked=writer.finish();return checked.ok?Outcome<BoundaryValue>{true,std::move(value),{}}:Outcome<BoundaryValue>{false,{},checked.error};}
Outcome<bool> emit_personal_appointment_action_payload_fields(ProjectionWriter& w,const PersonalAppointmentState* state,const ActionCoreView& action){
    if(action.kind!=personal_appointment_action_kind)return {true,false,{}};if(!state)return {false,{},common_failure("PersonalAppointmentProfileRequired",action.id).error};const PersonalAppointmentStarted* original=nullptr;
    for(const auto& entry:state->started.rows())if(entry.second.action_id==action.id){if(original)return {false,{},common_failure("DuplicatePersonalAppointmentAction",action.id).error};original=&entry.second;}
    if(!original||original->contact.tick!=action.start_tick||original->contact.actor_id!=action.actor_id||appointment_detail::finish(*state,original->appointment_id))return {false,{},common_failure("PersonalAppointmentActionMismatch",action.id).error};
    auto status=emit_personal_appointment_action_fields(w,*original);return {status.ok,status.ok,status.error};
}
Status emit_personal_appointment_action_payloads_fields(ProjectionWriter& w,const CommonStore& common,const PersonalAppointmentState* state){
    
    for(const auto& row:common.state().actions.rows())if(row.second.kind==personal_appointment_action_kind){
        if(!state)return common_failure("PersonalAppointmentProfileRequired",row.first);const PersonalAppointmentStarted* original=nullptr;
        for(const auto& entry:state->started.rows())if(entry.second.action_id==row.first){if(original)return common_failure("DuplicatePersonalAppointmentAction",row.first);original=&entry.second;}
        if(!original||original->action_id!=row.second.id||original->contact.tick!=row.second.start_tick||original->contact.actor_id!=row.second.actor_id||appointment_detail::finish(*state,original->appointment_id))return common_failure("PersonalAppointmentActionMismatch",row.first);auto emitted_action=w.object(row.first,[&](auto&out){return emit_personal_appointment_action_fields(out,*original);});if(!emitted_action.ok)return emitted_action;
    }
    return w.status();
}
Outcome<BoundaryValue> personal_appointment_action_payloads(const CommonStore& common,const PersonalAppointmentState* state){BoundaryValue value;BoundaryProjectionWriter writer(value);auto checked=writer.object("",[&](auto&out){return emit_personal_appointment_action_payloads_fields(out,common,state);});if(checked.ok)checked=writer.finish();return checked.ok?Outcome<BoundaryValue>{true,std::move(value),{}}:Outcome<BoundaryValue>{false,{},checked.error};}
Status validate_personal_appointment_json_payloads(const CommonStore& common,const PersonalAppointmentState* state,const BoundaryValue& source){
    try{using namespace boundary;auto facts=personal_appointment_fact_payloads(common,state);if(!facts.ok)return {false,{},facts.error};for(const auto& row:object(facts.value,"personal_appointments/facts")){const auto& actual=field(field(source,"facts","state"),row.first,"facts");if(!compare_boundary(actual,row.second).exact())return common_failure("PersonalAppointmentFactPayloadMismatch",row.first);}
        auto actions=personal_appointment_action_payloads(common,state);if(!actions.ok)return {false,{},actions.error};for(const auto& row:object(actions.value,"personal_appointments/actions")){const auto& actual=field(field(field(source,"actions","state"),row.first,"actions"),"params",row.first);if(!compare_boundary(actual,row.second).exact())return common_failure("PersonalAppointmentActionPayloadMismatch",row.first);}return common_success();
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
}
