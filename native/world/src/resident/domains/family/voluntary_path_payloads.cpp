// JSON必须逐值/字段序匹配typed来源；模块关闭、孤头、遗漏载荷都拒绝，不补写历史。
#include "voluntary_path_payloads.hpp"
#include "family_source_projection.hpp"
namespace hanmo::resident {
Status emit_voluntary_path_fact_payloads_fields(ProjectionWriter& out,const CommonStore& c,const VoluntaryPathState* state){
 if(state){auto shape=validate_voluntary_path_state_shape(*state);if(!shape.ok)return shape;out.validate_detached([&](auto& w){return emit_voluntary_path_state_fields(w,*state);});if(!out.status().ok)return out.status();
 for(const auto& row:state->cancellations.rows()){const auto* head=c.state().facts.find(row.first);if(!head||head->id!=row.first||head->kind!=voluntary_path_cancelled_kind||head->domain!=voluntary_path_module||row.first.rfind("voluntary_path:",0)!=0)return common_failure("VoluntaryPathPayloadHeaderMismatch",row.first);out.object(row.first,[&](auto& w){auto status=emit_voluntary_path_cancellation_fields(w,row.second);if(!status.ok)return status;w.text("domain",voluntary_path_module);return w.order_fields(head->field_order);});if(!out.status().ok)return out.status();}}
 std::size_t wanted=0;bool order_mismatch=false;for(const auto& row:c.state().facts.rows())if(row.second.domain==voluntary_path_module||row.second.kind==voluntary_path_cancelled_kind||row.first.rfind("voluntary_path:",0)==0){if(!state||!state->cancellations.find(row.first))return common_failure("VoluntaryPathPayloadSourceRequired",row.first);if(wanted>=state->cancellations.order().size()||state->cancellations.order()[wanted]!=row.first)order_mismatch=true;++wanted;}if(state&&(order_mismatch||wanted!=state->cancellations.size()))return common_failure("VoluntaryPathPayloadOrderMismatch");return out.status();
}
Outcome<BoundaryValue> voluntary_path_fact_payloads(const CommonStore& c,const VoluntaryPathState* state){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_voluntary_path_fact_payloads_fields(out,c,state);});if(status.ok)status=writer.finish();return {status.ok,std::move(value),status.error};}
Status validate_voluntary_path_json_payloads(const CommonStore& c,const VoluntaryPathState* state,const BoundaryValue& full){try{
 auto projected=voluntary_path_fact_payloads(c,state);if(!projected.ok)return {false,{},projected.error};const auto& facts=boundary::field(full,"facts","state");
 for(const auto& row:boundary::object(projected.value,voluntary_path_module)){const auto* actual=facts.find(row.first);if(!actual||!compare_boundary(*actual,row.second).exact())return common_failure("VoluntaryPathPayloadMismatch",row.first);}return common_success();
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_voluntary_path_quote(const VoluntaryPathQuote& q){
 BoundaryValue::Dict out{{"ok",BoundaryValue{true}},{"duration",BoundaryValue{Integer{0}}}};
 if(q.consent){out.emplace_back("succession_consent",encode_succession_consent_source(q.consent->agreement));out.emplace_back("living_source",encode_voluntary_path_source(q.consent->path));}
 if(q.personal)out.emplace_back("succession_personal",encode_succession_personal_terms(q.personal->terms));
 if(q.withdrawal)out.emplace_back("withdrawal",encode_known_personal_dialogue_proof(*q.withdrawal));
 if(!q.plan_id.empty())out.emplace_back("succession_plan_id",BoundaryValue{q.plan_id});return BoundaryValue{std::move(out)};
}
}
