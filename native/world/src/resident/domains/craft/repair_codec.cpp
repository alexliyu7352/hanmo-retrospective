#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "repair_codec.hpp"
#include "repair_fields.hpp"
#include "craft_job_codec.hpp"
#include "craft_codec_archive.hpp"
#include "resident/kernel/world_timeline_codec_fields.hpp"
namespace hanmo::resident::repair_codec {
using craft_codec::Vector;using craft_codec::Optional;using craft_codec::Table;
template<class E>struct Names;
template<>struct Names<RepairConditionRule>{inline static constexpr std::pair<RepairConditionRule,const char*> rows[]={{RepairConditionRule::restore_existing_maximum,"restore_existing_maximum"}};};
template<>struct Names<RepairJobPhase>{inline static constexpr std::pair<RepairJobPhase,const char*> rows[]={{RepairJobPhase::reserved,"reserved"},{RepairJobPhase::working,"working"},{RepairJobPhase::suspended,"suspended"},{RepairJobPhase::completed,"completed"},{RepairJobPhase::interrupted,"interrupted"}};};
template<>struct Names<RepairControlKind>{inline static constexpr std::pair<RepairControlKind,const char*> rows[]={{RepairControlKind::suspended,"suspended"},{RepairControlKind::resumed,"resumed"},{RepairControlKind::input_rebound,"input_rebound"}};};
template<class T>inline constexpr bool original=std::is_same_v<T,Owner>||std::is_same_v<T,ReservationRef>||std::is_same_v<T,WorldMoment>||std::is_same_v<T,ActorConditionHead>||std::is_same_v<T,SourceRef>||std::is_same_v<T,MomentOwnerSource>||std::is_same_v<T,CargoSelection>||std::is_same_v<T,FacilitySlotRef>||std::is_same_v<T,LotPropertiesRef>||std::is_same_v<T,CraftFacilityClaimUntil>;
template<class T>Status emit(ProjectionWriter&,std::string_view,const T&);template<class T>void decode(const BoundaryValue&,T&,std::string_view);template<class T>Status visit(CodecVisitor&,std::string_view,T&);
struct Project{ProjectionWriter&w;Status result=common_success();template<class T>void operator()(const char*n,const T&v){if(result.ok)result=emit(w,n,v);}};
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view);
struct Decode{
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;std::size_t count=0;
 template<class T>void operator()(const char* name,T& value){if(count==32)boundary::fail("RepairFieldCountOverflow",path);++count;craft_codec::checked(json_field(archive,input,name,value,path));}
 void finish(){if(boundary::object(input,path).size()!=count)boundary::fail("RepairFieldCoverageMismatch",path);craft_codec::checked(archive.finish());}
};
struct Visit{CodecVisitor&c;Status result=common_success();template<class T>void operator()(const char*n,T&v){if(result.ok)result=visit(c,n,v);}};
template<class T>Status emit(ProjectionWriter&w,std::string_view name,const T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,v);
 else if constexpr(original<T>)return craft_codec::emit(w,name,v);
 else if constexpr(std::is_same_v<T,FixedServiceInputRef>)return emit_projection_record<ActorFoundationProjectionArchive>(w,name,v,[](auto&c,auto&value){return time_fields::input(c,value);});
 else if constexpr(std::is_enum_v<T>){for(const auto&row:Names<T>::rows)if(v==row.first)return w.field(name,StableId{row.second});return w.reject("UnknownRepairEnum",name);}
 else if constexpr(Vector<T>::value)return w.sequence(name,v,[](auto&out,const auto&row){return emit(out,"",row);});
 else if constexpr(Optional<T>::value){if(v)return emit(w,name,*v);return w.null(name);}
 else if constexpr(Table<T>::value)return w.keyed_dictionary(name,{},[&](auto&out){for(const auto&row:v.rows()){auto s=emit(out,row.first,row.second);if(!s.ok)return s;}return out.status();});
 else return w.object(name,[&](auto&out){Project f{out};repair_fields::fields(f,const_cast<T&>(v));return f.result.ok?out.status():f.result;});
}
template<class T>inline constexpr bool exact_table_row=std::is_same_v<T,RepairAcceptedSource>||std::is_same_v<T,RepairJob>||std::is_same_v<T,RepairControlSource>||std::is_same_v<T,RepairCompletedSource>||std::is_same_v<T,RepairInterruptedSource>||std::is_same_v<T,RepairUnstartedInterruptionSource>;
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path){
 if constexpr(original<T>||std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return craft_codec::json_value(archive,input,value,path);
 else if constexpr(std::is_same_v<T,FixedServiceInputRef>){
  auto original=[](const BoundaryValue& node){return exact_fields::decode_json<T>(node,[](auto& codec,auto& row){return time_fields::input(codec,row);});};
  auto emitter=[](ProjectionWriter& out,const T& row){return emit(out,"",row);};
  if(ScopedCheckpointJsonDecode::current())return archive.foreign_record(value,[&](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget){if(&budget!=ScopedCheckpointJsonDecode::current())budget.fail("CheckpointCandidateMemoryJobMismatch","repair_input");return original(node);},emitter);
  return archive.foreign_record(value,original,emitter);
 }
 else if constexpr(std::is_enum_v<T>){StableId named;boundary::string_ref(input,{path});craft_codec::checked(archive.field("",named));for(const auto& row:Names<T>::rows)if(named==row.second){value=row.first;return archive.status();}boundary::fail("UnknownRepairEnum",path);}
 else{craft_codec::checked(archive.defer_duplicate_field_check_to_finish());Decode fields{archive,input,path};repair_fields::fields(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,std::uint32_t>){auto number=boundary::integer(node,child_path);if(number<0||static_cast<std::uint64_t>(number)>std::numeric_limits<std::uint32_t>::max())boundary::fail("CraftUnsignedIntegerOutOfRange",child_path);craft_codec::checked(archive.field(name,number));value=static_cast<std::uint32_t>(number);return archive.status();}
 else if constexpr(std::is_enum_v<T>){StableId named;boundary::string_ref(node,{child_path});craft_codec::checked(archive.field(name,named));for(const auto& row:Names<T>::rows)if(named==row.second){value=row.first;return archive.status();}boundary::fail("UnknownRepairEnum",child_path);}
 else if constexpr(Vector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,craft_codec::index_path(child_path,position));},craft_codec::json_scalar<typename Vector<T>::Value>);}
 else if constexpr(Optional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path);},craft_codec::json_scalar<typename Optional<T>::Value>);
 else if constexpr(Table<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}));for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateRepairRecord",{child_path,"/",rows[position].first});return result;};using Row=typename Table<T>::Value;if constexpr(exact_table_row<Row>)return archive.table(name,value,read,[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();});else return archive.table(name,value,read);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path);});
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path){ActorFoundationJsonVisitor archive(input,craft_codec::json_scalar<T>);craft_codec::checked(json_value(archive,input,value,path));craft_codec::checked(archive.finish());}
template<class T>Status visit(CodecVisitor&c,std::string_view name,T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,v);
 else if constexpr(original<T>)return craft_codec::checkpoint(c,name,v);
 else if constexpr(std::is_same_v<T,FixedServiceInputRef>)return c.object(name,[&](auto&out){return time_fields::input(out,v);});
 else if constexpr(std::is_enum_v<T>){StableId named;if(!c.reading()){bool found=false;for(const auto&row:Names<T>::rows)if(v==row.first){named=row.second;found=true;}if(!found)return c.reject("UnknownRepairEnum",name);}c.field(name,named);if(!c.status().ok)return c.status();for(const auto&row:Names<T>::rows)if(named==row.second){if(c.reading())v=row.first;return c.status();}return c.reject("UnknownRepairEnum",name);}
 else if constexpr(Vector<T>::value)return c.sequence(name,v,[](auto&out,auto&row){return visit(out,"item",row);});
 else if constexpr(Optional<T>::value)return c.optional(name,v,[](auto&out,auto&row){return visit(out,"value",row);});
 else if constexpr(Table<T>::value)return common_checkpoint::table(c,name,v,[](auto&out,auto&row){return visit(out,"record",row);});
 else return c.object(name,[&](auto&out){Visit f{out};repair_fields::fields(f,v);return f.result.ok?out.status():f.result;});
}
template<class T>Outcome<BoundaryValue> encode(const T&v){BoundaryValue value;BoundaryProjectionWriter w(value);auto s=emit(w,"",v);if(s.ok)s=w.finish();return s.ok?Outcome<BoundaryValue>{true,std::move(value),{}}:Outcome<BoundaryValue>{false,{},s.error};}
}
namespace hanmo::resident {
Outcome<RepairJobsState> decode_repair_jobs(const BoundaryValue&v){try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<RepairJobsState>();RepairJobsState out;repair_codec::decode(v,out,"/repair_jobs");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_repair_jobs(const RepairJobsState&v){return repair_codec::encode(v);}
Status emit_repair_jobs(ProjectionWriter&w,std::string_view name,const RepairJobsState&v){return repair_codec::emit(w,name,v);}
Status read_repair_jobs_json_fields(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,RepairJobsState& value,std::string_view path){return repair_codec::json_value(archive,input,value,path);}
Status visit_repair_jobs(CodecVisitor&c,RepairJobsState&v){repair_codec::Visit f{c};repair_fields::fields(f,v);return f.result.ok?c.status():f.result;}
Outcome<RepairPolicyCatalog> decode_repair_policy_catalog(const BoundaryValue&v,const CommonCatalog&common){try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<RepairPolicyCatalog>();RepairPolicyCatalog out;repair_codec::decode(v,out,"/repair_policy_catalog");auto valid=validate_repair_policy_catalog(out,common);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_repair_policy_catalog(const RepairPolicyCatalog&v){return repair_codec::encode(v);}
Status emit_repair_policy_catalog(ProjectionWriter&w,const StableId&name,const RepairPolicyCatalog&v){return repair_codec::emit(w,name,v);}
Status visit_repair_policy_catalog(CodecVisitor&c,RepairPolicyCatalog&v){repair_codec::Visit f{c};repair_fields::fields(f,v);return f.result.ok?c.status():f.result;}
}
namespace hanmo::resident {
bool same_repair_accepted_terms(const RepairAcceptedTerms&a,const RepairAcceptedTerms&b){
 auto source=[](const MomentOwnerSource&x,const MomentOwnerSource&y){return x.source_schema==y.source_schema&&same_source_ref(x.source_ref,y.source_ref);};
 return a.policy_ref.policy_id==b.policy_ref.policy_id&&a.policy_ref.version==b.policy_ref.version&&a.instance_id==b.instance_id&&a.worker_id==b.worker_id&&a.original_item.inventory_id==b.original_item.inventory_id&&a.original_item.lot_id==b.original_item.lot_id&&a.original_item.quantity==b.original_item.quantity&&!a.legal_owner.extra&&!b.legal_owner.extra&&a.legal_owner.kind==b.legal_owner.kind&&a.legal_owner.id==b.legal_owner.id&&a.condition_head.instance_id==b.condition_head.instance_id&&a.condition_head.instance_version==b.condition_head.instance_version&&a.condition_head.last_condition_result_ref==b.condition_head.last_condition_result_ref&&a.workstation.provider_domain==b.workstation.provider_domain&&a.workstation.facility_id==b.workstation.facility_id&&a.workstation.slot_kind==b.workstation.slot_kind&&source(a.actual_contact_ref,b.actual_contact_ref)&&source(a.owner_authority_ref,b.owner_authority_ref)&&source(a.fee_terms_ref,b.fee_terms_ref)&&source(a.return_terms_ref,b.return_terms_ref)&&a.delivered_custody_source_ref.has_value()==b.delivered_custody_source_ref.has_value()&&(!a.delivered_custody_source_ref||source(*a.delivered_custody_source_ref,*b.delivered_custody_source_ref))&&a.terms_revision==b.terms_revision;
}
bool same_repair_terms(const RepairTerms&a,const RepairTerms&b){
 auto source=[](const auto&x,const auto&y){return x.source_schema==y.source_schema&&same_source_ref(x.source_ref,y.source_ref);};auto selection=[](const CargoSelection&x,const CargoSelection&y){return x.inventory_id==y.inventory_id&&x.lot_id==y.lot_id&&x.quantity==y.quantity;};auto owner=[](const Owner&x,const Owner&y){return !x.extra&&!y.extra&&x.kind==y.kind&&x.id==y.id;};const auto moment=compare_world_moments(a.accepted_at,b.accepted_at);
 const auto&x=a.worker_condition_head;const auto&y=b.worker_condition_head;
 if(a.policy_ref.policy_id!=b.policy_ref.policy_id||a.policy_ref.version!=b.policy_ref.version||a.instance_id!=b.instance_id||a.worker_id!=b.worker_id||!source(a.accepted_service_ref,b.accepted_service_ref)||!source(a.custody_source_ref,b.custody_source_ref)||!selection(a.original_item,b.original_item)||!owner(a.legal_owner,b.legal_owner)||a.service_holder!=b.service_holder||a.condition_head.instance_id!=b.condition_head.instance_id||a.condition_head.instance_version!=b.condition_head.instance_version||a.condition_head.last_condition_result_ref!=b.condition_head.last_condition_result_ref||x.actor_id!=y.actor_id||x.condition_revision!=y.condition_revision||x.initial_condition_seed_ref.seed_key!=y.initial_condition_seed_ref.seed_key||x.latest_receipt_ref.has_value()!=y.latest_receipt_ref.has_value()||(x.latest_receipt_ref&&x.latest_receipt_ref->result_id!=y.latest_receipt_ref->result_id)||a.workstation.provider_domain!=b.workstation.provider_domain||a.workstation.facility_id!=b.workstation.facility_id||a.workstation.slot_kind!=b.workstation.slot_kind||!source(a.fee_terms_ref,b.fee_terms_ref)||!source(a.return_terms_ref,b.return_terms_ref)||!moment.ok||moment.value||a.terms_revision!=b.terms_revision||a.materials.size()!=b.materials.size())return false;
 for(std::size_t n=0;n<a.materials.size();++n)if(a.materials[n].material_index!=b.materials[n].material_index||!selection(a.materials[n].selected,b.materials[n].selected)||!owner(a.materials[n].legal_owner,b.materials[n].legal_owner))return false;return true;
}
}

namespace hanmo::resident {
namespace {
template<class F>Status repair_original_table_members(F&&f){
 Status out=common_success();auto table=[&](const char*kind,auto member){if(out.ok)out=f(kind,member);};
 table("repair_accepted",&RepairJobsState::acceptances);table("repair_unstarted_interrupted",&RepairJobsState::unstarted_interruptions);
 table("repair_job",&RepairJobsState::jobs);table("repair_control",&RepairJobsState::controls);
 table("repair_completed",&RepairJobsState::completions);table("repair_interrupted",&RepairJobsState::interruptions);
 return out;
}
template<class F>Status repair_original_tables(const RepairJobsState&s,F&&f){return repair_original_table_members([&](const char*kind,auto member){return f(kind,s.*member);});}
}
bool is_repair_original_fact_kind(std::string_view kind){bool found=false;repair_original_table_members([&](const char*name,auto){if(kind==name)found=true;return common_success();});return found;}
Outcome<Integer> read_repair_original_fact_payload_schema(const CommonStore&common,const ItemInstancesState&items,const FactRecord&head){
 const auto*actual=common.state().facts.find(head.id);
 if(head.id.empty()||!actual||actual->id!=head.id||actual->domain!=head.domain||actual->kind!=head.kind||head.domain!="crafting")return {false,0,common_failure("RepairOriginalHeaderRequired",head.id).error};
 if(!is_repair_original_fact_kind(head.kind))return {false,0,common_failure("UnsupportedRepairOriginalKind",head.id).error};
 std::size_t matched=0;const auto checked=repair_original_tables(items.repair_jobs,[&](const char*kind,const auto&rows){
  for(const auto&row:rows.rows())if(row.first==head.id||row.second.id==head.id){
   if(row.first!=head.id||row.second.id!=head.id||head.kind!=kind)return common_failure("RepairOriginalPayloadIdentityMismatch",head.id);
   ++matched;
  }
  return common_success();
 });
 if(!checked.ok)return {false,0,checked.error};
 if(matched!=1)return {false,0,common_failure("RepairOriginalPayloadRequired",head.id).error};
 return {true,1,{}};
}
Status visit_repair_original_fact_headers(const CommonStore&common,const ItemInstancesState&items,CraftOriginalFactHeaderSink&sink){
 auto checked=repair_original_tables(items.repair_jobs,[&](const char*kind,const auto&rows){
  for(const auto&row:rows.rows()){
   const auto*head=common.state().facts.find(row.first);
   if(!head||head->id!=row.first||head->domain!="crafting"||head->kind!=kind||row.second.id!=row.first)return common_failure("RepairOriginalHeaderRequired",row.first);
   auto schema=read_repair_original_fact_payload_schema(common,items,*head);if(!schema.ok)return Status{false,{},schema.error};
  }
  return common_success();
 });
 if(!checked.ok)return checked;
 for(const auto&row:common.state().facts.rows())if(row.second.domain=="crafting"&&is_repair_original_fact_kind(row.second.kind)){
  if(row.first!=row.second.id)return common_failure("RepairOriginalHeaderIdentityMismatch",row.first);
  auto schema=read_repair_original_fact_payload_schema(common,items,row.second);if(!schema.ok)return {false,{},schema.error};
 }
 for(const auto&row:common.state().facts.rows())if(row.second.domain=="crafting"&&is_repair_original_fact_kind(row.second.kind)){checked=sink.append(row.second,1);if(!checked.ok)return checked;}
 return common_success();
}
Outcome<const RepairJob*> read_repair_original_action_owner_bootstrap(const CommonStore&common,const ItemInstancesState&items,const StableId&action_id){
 auto fail=[&](const char*code){return Outcome<const RepairJob*>{false,nullptr,common_failure(code,action_id).error};};
 const auto*action=common.action(action_id);
 if(action_id.empty()||!action||action->id!=action_id||action->kind!="repair")return fail("RepairOriginalActionRequired");
 const RepairJob*job=nullptr;
 for(const auto&row:items.repair_jobs.jobs.rows())if(row.second.action_id==action_id){
  if(job)return fail("DuplicateRepairOriginalExecution");
  if(row.first!=row.second.id)return fail("RepairJobIdentityMismatch");
  job=&row.second;
 }
 if(!job)return fail("RepairOriginalActionOwnerRequired");
 const auto*head=common.state().facts.find(job->id);
 if(!head||head->kind!="repair_job")return fail("RepairOriginalHeaderRequired");
 const auto schema=read_repair_original_fact_payload_schema(common,items,*head);if(!schema.ok)return {false,nullptr,schema.error};
 if(job->phase!=RepairJobPhase::working&&job->phase!=RepairJobPhase::suspended)return fail("RepairOriginalActionPhaseMismatch");
 if(job->terms.worker_id.empty()||action->actor_id!=job->terms.worker_id||action->participants.size()!=1||action->participants.front()!=job->terms.worker_id||action->start_tick!=job->started_at.half_day_tick)return fail("RepairOriginalActionIdentityMismatch");
 const auto*actor=common.actor(job->terms.worker_id);
 if(!actor||actor->action_id!=action_id)return fail("RepairOriginalActionParticipantBindingMismatch");
 return {true,job,{}};
}
Status validate_repair_original_action_coverage_bootstrap(const CommonStore&common,const ItemInstancesState&items){
 for(const auto&row:items.repair_jobs.jobs.rows()){
  const auto&job=row.second;
  if(row.first!=job.id||job.action_id.empty())return common_failure("RepairJobIdentityMismatch",row.first);
  for(const auto&prior:items.repair_jobs.jobs.rows()){if(prior.first==row.first)break;if(prior.second.action_id==job.action_id)return common_failure("DuplicateRepairOriginalExecution",job.action_id);}
  const bool terminal=job.phase==RepairJobPhase::completed||job.phase==RepairJobPhase::interrupted;
  if(terminal){if(common.action(job.action_id))return common_failure("RepairTerminalActionRetained",job.id);}
  else{auto read=read_repair_original_action_owner_bootstrap(common,items,job.action_id);if(!read.ok)return {false,{},read.error};}
 }
 for(const auto&row:common.state().actions.rows())if(row.second.kind=="repair"){
  if(row.first!=row.second.id)return common_failure("RepairOriginalActionIdentityMismatch",row.first);
  auto read=read_repair_original_action_owner_bootstrap(common,items,row.first);if(!read.ok)return {false,{},read.error};
 }
 return common_success();
}
Status emit_repair_original_fact_payload_fields(ProjectionWriter&w,const CommonStore&common,const ItemInstancesState&items,const FactRecord&header){
 const auto schema=read_repair_original_fact_payload_schema(common,items,header);if(!schema.ok)return {false,{},schema.error};
 try{return repair_original_tables(items.repair_jobs,[&](const char*kind,const auto&rows)->Status{
  if(header.kind!=kind)return common_success();const auto&row=*rows.find(header.id);using T=std::decay_t<decltype(row)>;
  repair_codec::Project original{w};repair_fields::fields(original,const_cast<T&>(row));return original.result.ok?w.status():original.result;
 });}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}
}
Status emit_repair_original_fact_payload(ProjectionWriter&w,const StableId&name,const CommonStore&common,const ItemInstancesState&items,const FactRecord&header){return w.object(name,[&](auto&out){return emit_repair_original_fact_payload_fields(out,common,items,header);});}
}
