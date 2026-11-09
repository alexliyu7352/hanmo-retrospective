#include "resident/domains/interaction/interaction_source_projection.hpp"
// JSON共同头和活动params都来自独立询问原件；缺实体、错身份或额外字段不能由导入丢弃。
#include "contact_inquiry_detail.hpp"
#include "contact_inquiry_boundary.hpp"
namespace hanmo::resident {
Outcome<bool> emit_contact_inquiry_fact_payload_fields(ProjectionWriter& w,const ContactInquiryState* state,const FactRecord& h){const bool owned=h.kind==contact_inquiry_started_kind||h.kind==contact_inquiry_finished_kind;
        if(!owned&&h.domain!=contact_inquiry_module)return {true,false,{}};
        if(!owned||!state||h.domain!=contact_inquiry_module)return {false,{},common_failure("ContactInquiryFactMismatch",h.id).error};auto status=emit_contact_inquiry_fact_fields(w,*state,h.id,&h);if(!status.ok)return {false,{},status.error};return {w.status().ok,w.status().ok,w.status().error};}
Status emit_contact_inquiry_fact_payloads_fields(ProjectionWriter& w,const CommonStore& common,const ContactInquiryState* state){
    std::size_t count=0;
    for(const auto& row:common.state().facts.rows()){
        const auto& h=row.second;const bool owned=h.kind==contact_inquiry_started_kind||h.kind==contact_inquiry_finished_kind;
        if(!owned&&h.domain!=contact_inquiry_module)continue;
        if(!owned||!state||h.id!=row.first||h.domain!=contact_inquiry_module)return common_failure("ContactInquiryFactMismatch",row.first);
        auto checked=w.object(row.first,h.field_order,[&](auto&out){return emit_contact_inquiry_fact_fields(out,*state,row.first,&h);});if(!checked.ok)return checked;++count;
    }
    if(state&&count!=state->started.size()+state->finished.size())return common_failure("ContactInquiryFactCoverageMismatch");return w.status();
}
Outcome<BoundaryValue> contact_inquiry_fact_payloads(const CommonStore& common,const ContactInquiryState* state){BoundaryValue value;BoundaryProjectionWriter writer(value);auto checked=writer.object("",[&](auto&out){return emit_contact_inquiry_fact_payloads_fields(out,common,state);});if(checked.ok)checked=writer.finish();return checked.ok?Outcome<BoundaryValue>{true,std::move(value),{}}:Outcome<BoundaryValue>{false,{},checked.error};}
Outcome<bool> emit_contact_inquiry_action_payload_fields(ProjectionWriter& w,const ContactInquiryState* state,const ActionCoreView& action){
    if(action.kind!=contact_inquiry_action_kind)return {true,false,{}};if(!state)return {false,{},common_failure("ContactInquiryProfileRequired",action.id).error};const ContactInquiryStarted* original=nullptr;
    for(const auto& entry:state->started.rows())if(entry.second.action_id==action.id){if(original)return {false,{},common_failure("DuplicateContactInquiryAction",action.id).error};original=&entry.second;}
    if(!original||original->contact.tick!=action.start_tick||original->contact.actor_id!=action.actor_id||contact_inquiry_detail::finished(*state,original->fact_id))return {false,{},common_failure("ContactInquiryActionMismatch",action.id).error};
    auto status=emit_contact_inquiry_action_fields(w,*original);return {status.ok,status.ok,status.error};
}
Status emit_contact_inquiry_action_payloads_fields(ProjectionWriter& w,const CommonStore& common,const ContactInquiryState* state){
    
    for(const auto& row:common.state().actions.rows())if(row.second.kind==contact_inquiry_action_kind){
        if(!state)return common_failure("ContactInquiryProfileRequired",row.first);const ContactInquiryStarted* original=nullptr;
        for(const auto& entry:state->started.rows())if(entry.second.action_id==row.first){if(original)return common_failure("DuplicateContactInquiryAction",row.first);original=&entry.second;}
        if(!original||original->action_id!=row.second.id||original->contact.tick!=row.second.start_tick||original->contact.actor_id!=row.second.actor_id||contact_inquiry_detail::finished(*state,original->fact_id))return common_failure("ContactInquiryActionMismatch",row.first);auto emitted_action=w.object(row.first,[&](auto&out){return emit_contact_inquiry_action_fields(out,*original);});if(!emitted_action.ok)return emitted_action;
    }
    return w.status();
}
Outcome<BoundaryValue> contact_inquiry_action_payloads(const CommonStore& common,const ContactInquiryState* state){BoundaryValue value;BoundaryProjectionWriter writer(value);auto checked=writer.object("",[&](auto&out){return emit_contact_inquiry_action_payloads_fields(out,common,state);});if(checked.ok)checked=writer.finish();return checked.ok?Outcome<BoundaryValue>{true,std::move(value),{}}:Outcome<BoundaryValue>{false,{},checked.error};}
Status validate_contact_inquiry_json_payloads(const CommonStore& common,const ContactInquiryState* state,const BoundaryValue& source){
    try{using namespace boundary;auto facts=contact_inquiry_fact_payloads(common,state);if(!facts.ok)return {false,{},facts.error};for(const auto& row:object(facts.value,"contact_inquiries/facts")){const auto& actual=field(field(source,"facts","state"),row.first,"facts");if(!compare_boundary(actual,row.second).exact())return common_failure("ContactInquiryFactPayloadMismatch",row.first);}
        auto actions=contact_inquiry_action_payloads(common,state);if(!actions.ok)return {false,{},actions.error};for(const auto& row:object(actions.value,"contact_inquiries/actions")){const auto& actual=field(field(field(source,"actions","state"),row.first,"actions"),"params",row.first);if(!compare_boundary(actual,row.second).exact())return common_failure("ContactInquiryActionPayloadMismatch",row.first);}return common_success();
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
}
