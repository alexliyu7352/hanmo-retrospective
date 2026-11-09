#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "craft_job_codec.hpp"
#include "craft_codec_archive.hpp"
namespace hanmo::resident {
namespace {
// The sole finite Common-header/table pairing for these original S9 sources.
template<class F> Status craft_original_table_members(F&&f){
 Status out=common_success();
 auto table=[&](const char*kind,const auto&rows){if(out.ok)out=f(kind,rows);};
 table("craft_job",&CraftJobsState::jobs);table("craft_service_span",&CraftJobsState::service_spans);
 table("craft_stage_result",&CraftJobsState::stages);table("craft_work_gate",&CraftJobsState::gates);
 table("craft_quality_result",&CraftJobsState::qualities);table("craft_completed",&CraftJobsState::completions);
 table("craft_pace_changed",&CraftJobsState::pace_changes);table("craft_control",&CraftJobsState::controls);
 table("craft_cancellation_requested",&CraftJobsState::cancellations);table("craft_cancelled",&CraftJobsState::cancellation_results);
 table("craft_facility_boundary",&CraftJobsState::facility_boundaries);table("craft_facility_occupancy",&CraftJobsState::facility_occupancy_events);
 return out;
}
template<class F>Status craft_original_tables(const CraftJobsState&s,F&&f){return craft_original_table_members([&](const char*kind,auto member){return f(kind,s.*member);});}
template<class T>const StableId& craft_original_id(const T&row){if constexpr(std::is_same_v<T,CraftFacilityOccupancyEvent>)return row.event_id;else return row.id;}
}
bool is_craft_original_fact_kind(std::string_view kind){
 bool found=false;craft_original_table_members([&](const char*name,auto){if(kind==name)found=true;return common_success();});return found;
}
Outcome<Integer> read_craft_original_fact_payload_schema(const CommonStore&common,const CraftJobsState&state,const FactRecord&head){
 const auto*actual=common.state().facts.find(head.id);
 if(state.schema_version!=1||head.id.empty()||!actual||actual->id!=head.id||actual->domain!=head.domain||actual->kind!=head.kind||head.domain!="crafting")return {false,0,common_failure("CraftOriginalHeaderRequired",head.id).error};
 if(!is_craft_original_fact_kind(head.kind))return {false,0,common_failure("UnsupportedCraftSourceKind",head.id).error};
 std::size_t matched=0;
 const auto checked=craft_original_tables(state,[&](const char*kind,const auto&rows){
  for(const auto&row:rows.rows())if(row.first==head.id||craft_original_id(row.second)==head.id){
   if(row.first!=head.id||craft_original_id(row.second)!=head.id||head.kind!=kind)return common_failure("CraftOriginalPayloadIdentityMismatch",head.id);
   ++matched;
  }
  return common_success();
 });
 if(!checked.ok)return {false,0,checked.error};
 if(matched!=1)return {false,0,common_failure("CraftOriginalPayloadRequired",head.id).error};
 return {true,1,{}};
}
Status visit_craft_original_fact_headers(const CommonStore&common,const CraftJobsState&state,CraftOriginalFactHeaderSink&sink){
 if(state.schema_version!=1)return common_failure("UnsupportedCraftJobsVersion");
 auto checked=craft_original_tables(state,[&](const char*kind,const auto&rows){
  for(const auto&row:rows.rows()){
   const auto*head=common.state().facts.find(row.first);
   if(!head||head->id!=row.first||head->domain!="crafting"||head->kind!=kind||craft_original_id(row.second)!=row.first)return common_failure("CraftOriginalHeaderRequired",row.first);
   auto schema=read_craft_original_fact_payload_schema(common,state,*head);if(!schema.ok)return Status{false,{},schema.error};
  }
  return common_success();
 });
 if(!checked.ok)return checked;
 // Inverse actual Common-header coverage is checked before emission.
 for(const auto&row:common.state().facts.rows())if(row.second.domain=="crafting"&&is_craft_original_fact_kind(row.second.kind)){
  if(row.first!=row.second.id)return common_failure("CraftOriginalHeaderIdentityMismatch",row.first);
  auto schema=read_craft_original_fact_payload_schema(common,state,row.second);if(!schema.ok)return {false,{},schema.error};
 }
 for(const auto&row:common.state().facts.rows())if(row.second.domain=="crafting"&&is_craft_original_fact_kind(row.second.kind)){checked=sink.append(row.second,1);if(!checked.ok)return checked;}
 return common_success();
}
Outcome<CraftJobsState> decode_craft_jobs(const BoundaryValue&input){try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<CraftJobsState>();CraftJobsState out;craft_codec::decode(input,out,"/craft_jobs");if(out.schema_version!=1)return {false,{},common_failure("UnsupportedCraftJobsVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Outcome<const CraftJob*> read_craft_original_action_owner_bootstrap(const CommonStore&common,const CraftJobsState&state,const StableId&action_id){
 auto fail=[&](const char*code){return Outcome<const CraftJob*>{false,nullptr,common_failure(code,action_id).error};};
 const auto*action=common.action(action_id);
 if(state.schema_version!=1||action_id.empty()||!action||action->id!=action_id||action->kind!="craft_job")return fail("CraftOriginalActionRequired");
 const CraftJob*job=nullptr;
 for(const auto&row:state.jobs.rows())if(row.second.execution_binding.action_id==action_id){
  if(job)return fail("DuplicateCraftActionOwner");
  if(row.first!=row.second.id)return fail("CraftJobIdentityMismatch");
  job=&row.second;
 }
 if(!job)return fail("CraftActionOwnerRequired");
 const auto&terms=job->terms;const auto&binding=job->execution_binding;
 const auto*head=common.state().facts.find(job->id);
 if(!head||head->kind!="craft_job"||terms.job_id!=job->id||terms.acceptance_ref.domain!="crafting"||terms.acceptance_ref.kind!="craft_job"||terms.acceptance_ref.id!=job->id||binding.action_kind!="craft_job"||!same_source_ref(binding.source_owner_ref,terms.acceptance_ref))return fail("CraftAcceptanceSourceMismatch");
 const auto schema=read_craft_original_fact_payload_schema(common,state,*head);if(!schema.ok)return {false,nullptr,schema.error};
 if(job->phase!=CraftJobPhase::working&&job->phase!=CraftJobPhase::paused&&job->phase!=CraftJobPhase::awaiting_output_space&&job->phase!=CraftJobPhase::settling)return fail("CraftOriginalActionPhaseMismatch");
 if(terms.maker.kind!=SubjectKind::actor||terms.maker.id.empty()||action->actor_id!=terms.maker.id||action->start_tick!=terms.accepted_at.half_day_tick||action->participants.size()!=terms.accepted_helpers.size()+1||action->participants.front()!=terms.maker.id)return fail("CraftOriginalActionIdentityMismatch");
 for(std::size_t n=0;n<action->participants.size();++n){
  const auto&id=action->participants[n];if(id.empty()||(n&&id!=terms.accepted_helpers[n-1]))return fail("CraftOriginalActionParticipantMismatch");
  for(std::size_t prior=0;prior<n;++prior)if(action->participants[prior]==id)return fail("CraftOriginalActionParticipantMismatch");
  const auto*actor=common.actor(id);if(!actor||actor->action_id!=action_id)return fail("CraftOriginalActionParticipantBindingMismatch");
 }
 return {true,job,{}};
}
Status validate_craft_original_action_coverage_bootstrap(const CommonStore&common,const CraftJobsState&state){
 if(state.schema_version!=1)return common_failure("UnsupportedCraftJobsVersion");
 for(const auto&row:state.jobs.rows()){
  const auto&job=row.second;
  if(row.first!=job.id||job.execution_binding.action_id.empty())return common_failure("CraftJobIdentityMismatch",row.first);
  for(const auto&prior:state.jobs.rows()){if(prior.first==row.first)break;if(prior.second.execution_binding.action_id==job.execution_binding.action_id)return common_failure("DuplicateCraftActionOwner",job.execution_binding.action_id);}
  const bool terminal=job.phase==CraftJobPhase::completed||job.phase==CraftJobPhase::cancelled;
  if(terminal){if(common.action(job.execution_binding.action_id))return common_failure("CraftTerminalActionRetained",job.id);}
  else{auto read=read_craft_original_action_owner_bootstrap(common,state,job.execution_binding.action_id);if(!read.ok)return {false,{},read.error};}
 }
 for(const auto&row:common.state().actions.rows())if(row.second.kind=="craft_job"){
  if(row.first!=row.second.id)return common_failure("CraftOriginalActionIdentityMismatch",row.first);
  auto read=read_craft_original_action_owner_bootstrap(common,state,row.first);if(!read.ok)return {false,{},read.error};
 }
 return common_success();
}
Status emit_craft_jobs_projection(ProjectionWriter&w,const StableId&name,const CraftJobsState&state){if(state.schema_version!=1)return w.reject("UnsupportedCraftJobsVersion",name);try{return craft_codec::emit(w,name,state);}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_craft_jobs(const CraftJobsState&state){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=emit_craft_jobs_projection(writer,"",state);if(status.ok)status=writer.finish();if(!status.ok)return {false,{},status.error};return {true,std::move(value),{}};}
// 遍历制作任务检查点；缺少类型化资源或预约引用能力时拒绝。
Status visit_craft_jobs_checkpoint(CodecVisitor &c, CraftJobsState &state) {
  if (!(c.schema_features & common_checkpoint::typed_resource_references) ||
      !c.domain_activity_reservation_refs_enabled)
    return c.reject("CraftTypedResourcesRequired");
  try {
    auto status = craft_codec::checkpoint(c, "craft_jobs", state);
    if (status.ok && state.schema_version != 1)
      return c.reject("UnsupportedCraftJobsVersion");
    return status;
  } catch (const exact_fields::CodecFailure &e) {
    return {false, {}, e.error};
  }
}
Status emit_craft_original_fact_payload_fields(ProjectionWriter&w,const CommonStore&common,const CraftJobsState&state,const FactRecord&head){
 const auto schema=read_craft_original_fact_payload_schema(common,state,head);if(!schema.ok)return {false,{},schema.error};
 auto fields=[&](const auto&row){using T=std::decay_t<decltype(row)>;craft_codec::ProjectFields original{w};craft_fields::fields(original,const_cast<T&>(row));return original.status();};
 try{return craft_original_tables(state,[&](const char*kind,const auto&table)->Status{
  if(head.kind!=kind)return common_success();const auto&row=*table.find(head.id);
  if constexpr(std::is_same_v<std::decay_t<decltype(row)>,CraftJob>)return fields(row.terms);
  else return fields(row);
 });}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}
}
Status emit_craft_original_fact_payload(ProjectionWriter&w,const StableId&name,const CommonStore&common,const CraftJobsState&state,const FactRecord&head){return w.object(name,[&](auto&out){return emit_craft_original_fact_payload_fields(out,common,state,head);});}
}
