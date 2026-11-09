// S9唯一字段表的三种访问器。目录不写当前库存，恢复不补缺失字段。
#pragma once
#include "craft_job_fields.hpp"
#include "resident/kernel/work_period_codec_fields.hpp"
#include "resident/domains/actor_foundation/condition_head_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <array>
#include <type_traits>
#include <limits>
namespace hanmo::resident::craft_codec {
template<class T>struct Vector:std::false_type{};
template<class T,class A>struct Vector<std::vector<T,A>>:std::true_type{using Value=T;};
template<class T>struct Optional:std::false_type{};
template<class T>struct Optional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>struct Table:std::false_type{};
template<class T>struct Table<OrderedTable<T>>:std::true_type{using Value=T;};
template<class E>struct Names;
template<>struct Names<SubjectKind>{inline static constexpr std::pair<SubjectKind,const char*> rows[]={{SubjectKind::actor,"actor"},{SubjectKind::organization,"organization"},{SubjectKind::cohort,"cohort"}};};
template<>struct Names<CraftPurpose>{inline static constexpr std::pair<CraftPurpose,const char*> rows[]={{CraftPurpose::civilian_tool,"civilian_tool"},{CraftPurpose::weapon,"weapon"},{CraftPurpose::gift,"gift"}};};
template<>struct Names<CraftMode>{inline static constexpr std::pair<CraftMode,const char*> rows[]={{CraftMode::automatic,"auto"},{CraftMode::local,"local"}};};
template<>struct Names<CraftStageKind>{inline static constexpr std::pair<CraftStageKind,const char*> rows[]={{CraftStageKind::material_process,"material_process"},{CraftStageKind::final_inspection,"final_inspection"}};};
template<>struct Names<CraftStageTrigger>{inline static constexpr std::pair<CraftStageTrigger,const char*> rows[]={{CraftStageTrigger::after_first_work_tick,"after_first_work_tick"},{CraftStageTrigger::at_work_progress,"at_work_progress"},{CraftStageTrigger::at_completion,"at_completion"}};};
template<>struct Names<CraftPace>{inline static constexpr std::pair<CraftPace,const char*> rows[]={{CraftPace::normal,"normal"},{CraftPace::rush,"rush"}};};
template<>struct Names<CraftJobPhase>{inline static constexpr std::pair<CraftJobPhase,const char*> rows[]={{CraftJobPhase::reserved,"reserved"},{CraftJobPhase::working,"working"},{CraftJobPhase::paused,"paused"},{CraftJobPhase::awaiting_output_space,"awaiting_output_space"},{CraftJobPhase::completed,"completed"},{CraftJobPhase::cancelling,"cancelling"},{CraftJobPhase::settling,"settling"},{CraftJobPhase::cancelled,"cancelled"},{CraftJobPhase::failed,"failed"}};};
template<>struct Names<CraftControlKind>{inline static constexpr std::pair<CraftControlKind,const char*> rows[]={{CraftControlKind::pause,"pause"},{CraftControlKind::resume,"resume"}};};
template<>struct Names<CraftFacilityOccupancyKind>{inline static constexpr std::pair<CraftFacilityOccupancyKind,const char*> rows[]={{CraftFacilityOccupancyKind::occupied,"Occupied"},{CraftFacilityOccupancyKind::continues,"Continues"},{CraftFacilityOccupancyKind::cleared,"Cleared"}};};
template<class T>inline constexpr bool shared_record=std::is_same_v<T,WorldMoment>||std::is_same_v<T,WorkExecutionBinding>||std::is_same_v<T,CraftWorkGateResult>||std::is_same_v<T,ActorConditionHead>;
template<class C,class T>Status shared_fields(C&c,T&v){if constexpr(std::is_same_v<T,WorldMoment>)return exact_fields::moment(c,v);else if constexpr(std::is_same_v<T,WorkExecutionBinding>)return work_fields::binding(c,v);else if constexpr(std::is_same_v<T,ActorConditionHead>)return actor_condition_codec::head(c,v);else return work_fields::craft_result(c,v);}
template<class T>Status emit(ProjectionWriter&,std::string_view,const T&);
template<class T>void decode(const BoundaryValue&,T&,std::string_view);
template<class T>Status checkpoint(CodecVisitor&,std::string_view,T&);
struct ProjectFields{ProjectionWriter&w;Status result=common_success();bool reading()const{return false;}template<class T>Status prepare_default_values(std::uint64_t=1){return status();}Status status()const{return result.ok?w.status():result;}void reject(std::string_view c,std::string_view p){if(result.ok)result=w.reject(c,p);}template<class T>void operator()(const char*n,const T&v){if(result.ok)result=emit(w,n,v);}};
// The original schema adapter borrows the sole JSON archive. It has no owning
// visited-key set or allocation loop; paths use the same Common decode charge.
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view);
inline Status checked(Status value){if(!value.ok)throw boundary::DecodeFailure{value.error};return value;}
inline StableId index_path(std::string_view path,std::size_t index){char digits[3*sizeof(index)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+index%10);index/=10;}while(index);return checkpoint_decode_boundary_join({path,"/",std::string_view(start,std::size_t(end-start))});}
struct DecodeFields{
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;std::size_t count=0;
 bool reading()const{return true;}Status status()const{return archive.status();}
 void reject(std::string_view code,std::string_view detail){boundary::fail_parts(code,{path,"/",detail});}
 template<class T>Status prepare_default_values(std::uint64_t count=1){return archive.template prepare_default_values<T>(count);}
 template<class T>void operator()(const char* name,T& value){if(count==32)boundary::fail("CraftFieldCountOverflow",path);++count;checked(json_field(archive,input,name,value,path));}
 void finish(){if(boundary::object(input,path).size()!=count)boundary::fail("CraftFieldCoverageMismatch",path);checked(archive.finish());}
};
struct CheckpointFields{CodecVisitor&c;Status result=common_success();bool reading()const{return c.reading();}template<class T>Status prepare_default_values(std::uint64_t count=1){return c.template prepare_default_values<T>(count);}Status status()const{return result.ok?c.status():result;}void reject(std::string_view code,std::string_view p){if(result.ok)result=c.reject(code,p);}template<class T>void operator()(const char*n,T&v){if(result.ok)result=checkpoint(c,n,v);}};
template<class T>Status emit(ProjectionWriter&w,std::string_view name,const T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,v);
 else if constexpr(std::is_same_v<T,std::uint32_t>)return w.field(name,static_cast<Integer>(v));
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,name,v);
 else if constexpr(std::is_same_v<T,ReservationRef>)return emit_common_reservation(w,name,v,true,true);
 else if constexpr(shared_record<T>)return emit_projection_record<ActorFoundationProjectionArchive>(w,name,v,[](auto&c,auto&value){return shared_fields(c,value);});
 else if constexpr(std::is_enum_v<T>){for(const auto&row:Names<T>::rows)if(row.first==v)return w.field(name,StableId{row.second});return w.reject("UnknownCraftEnum",name);}
 else if constexpr(Vector<T>::value)return w.sequence(name,v,[](auto&out,const auto&row){return emit(out,"",row);});
 else if constexpr(Optional<T>::value){if(v)return emit(w,name,*v);return w.null(name);}
 else if constexpr(Table<T>::value)return w.keyed_dictionary(name,{},[&](auto&out){for(const auto&row:v.rows()){auto status=emit(out,row.first,row.second);if(!status.ok)return status;}return out.status();});
 else return w.object(name,[&](auto&out){ProjectFields f{out};craft_fields::fields(f,const_cast<T&>(v));return f.status();});
}
// These named original rows have only moving ordinary fields and exact
// fractions that copy through their genuine allocator. No unknown-T transfer.
template<class T>inline constexpr bool exact_table_row=std::is_same_v<T,CraftJob>||std::is_same_v<T,CraftServiceSpan>||std::is_same_v<T,CraftStageResult>||std::is_same_v<T,CraftGateApplied>||std::is_same_v<T,CraftQualityResult>||std::is_same_v<T,CraftCompletionResult>||std::is_same_v<T,CraftPaceChange>||std::is_same_v<T,CraftControlSource>||std::is_same_v<T,CraftCancellationSource>||std::is_same_v<T,CraftCancellationResult>||std::is_same_v<T,CraftFacilityBoundarySource>||std::is_same_v<T,CraftFacilityOccupancyEvent>;
template<class T>inline constexpr bool json_scalar=std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_same_v<T,std::uint32_t>||std::is_enum_v<T>;
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path){
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(input,{path});return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,std::uint32_t>){auto number=boundary::integer(input,path);if(number<0||static_cast<std::uint64_t>(number)>std::numeric_limits<std::uint32_t>::max())boundary::fail("CraftUnsignedIntegerOutOfRange",path);checked(archive.field("",number));value=static_cast<std::uint32_t>(number);return archive.status();}
 else if constexpr(std::is_same_v<T,Owner>||std::is_same_v<T,ReservationRef>||shared_record<T>){
  auto original=[&](const BoundaryValue& node)->Outcome<T>{try{if constexpr(std::is_same_v<T,Owner>)return {true,boundary::decode_owner(node,checkpoint_decode_boundary_text(path)),{}};else if constexpr(std::is_same_v<T,ReservationRef>)return {true,boundary::decode_reservation_ref(node,checkpoint_decode_boundary_text(path),true),{}};else return exact_fields::decode_json<T>(node,[](auto& codec,auto& row){return shared_fields(codec,row);});}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}};
  auto emit_original=[&](ProjectionWriter& out,const T& row){return emit(out,"",row);};
  if(ScopedCheckpointJsonDecode::current())return archive.foreign_record(value,[&](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget)->Outcome<T>{if(&budget!=ScopedCheckpointJsonDecode::current())budget.fail("CheckpointCandidateMemoryJobMismatch","craft_original");if constexpr(!shared_record<T>)budget.template default_values<T>();return original(node);},emit_original);
  return archive.foreign_record(value,original,emit_original);
 }
 else if constexpr(std::is_enum_v<T>){StableId name;boundary::string_ref(input,{path});checked(archive.field("",name));for(const auto& row:Names<T>::rows)if(name==row.second){value=row.first;return archive.status();}boundary::fail("UnknownCraftEnum",path);}
 else{checked(archive.defer_duplicate_field_check_to_finish());DecodeFields fields{archive,input,path};craft_fields::fields(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,std::uint32_t>){auto number=boundary::integer(node,child_path);if(number<0||static_cast<std::uint64_t>(number)>std::numeric_limits<std::uint32_t>::max())boundary::fail("CraftUnsignedIntegerOutOfRange",child_path);checked(archive.field(name,number));value=static_cast<std::uint32_t>(number);return archive.status();}
 else if constexpr(std::is_enum_v<T>){StableId named;boundary::string_ref(node,{child_path});checked(archive.field(name,named));for(const auto& row:Names<T>::rows)if(named==row.second){value=row.first;return archive.status();}boundary::fail("UnknownCraftEnum",child_path);}
 else if constexpr(Vector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(child_path,position));},json_scalar<typename Vector<T>::Value>);}
 else if constexpr(Optional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path);},json_scalar<typename Optional<T>::Value>);
 else if constexpr(Table<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}));for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateCraftRecord",{child_path,"/",rows[position].first});return result;};using Row=typename Table<T>::Value;if constexpr(exact_table_row<Row>)return archive.table(name,value,read,[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();});else return archive.table(name,value,read);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path);});
}
template<class T>void decode(const BoundaryValue&input,T&v,std::string_view path){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(json_value(archive,input,v,path));checked(archive.finish());}
template<class T>Status checkpoint(CodecVisitor&c,std::string_view name,T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,v);
 else if constexpr(std::is_same_v<T,std::uint32_t>){Integer n=v;auto status=c.field(name,n);if(!status.ok)return status;if(n<0||static_cast<std::uint64_t>(n)>std::numeric_limits<std::uint32_t>::max())return c.reject("CraftUnsignedIntegerOutOfRange",name);if(c.reading())v=static_cast<std::uint32_t>(n);return c.status();}
 else if constexpr(std::is_same_v<T,Owner>)return c.object(name,[&](auto&out){return common_checkpoint::owner(out,v);});
 else if constexpr(std::is_same_v<T,ReservationRef>)return c.object(name,[&](auto&out){return common_checkpoint::reservation(out,v,"reserved_by");});
 else if constexpr(shared_record<T>)return c.object(name,[&](auto&out){return shared_fields(out,v);});
 else if constexpr(std::is_enum_v<T>){StableId value;if(!c.reading()){bool found=false;for(const auto&row:Names<T>::rows)if(row.first==v){value=row.second;found=true;}if(!found)return c.reject("UnknownCraftEnum",name);}c.field(name,value);if(!c.status().ok)return c.status();for(const auto&row:Names<T>::rows)if(value==row.second){if(c.reading())v=row.first;return c.status();}return c.reject("UnknownCraftEnum",name);}
 else if constexpr(Vector<T>::value)return c.sequence(name,v,[](auto&out,auto&row){return checkpoint(out,"item",row);});
 else if constexpr(Optional<T>::value)return c.optional(name,v,[](auto&out,auto&row){return checkpoint(out,"value",row);});
 else if constexpr(Table<T>::value)return common_checkpoint::table(c,name,v,[](auto&out,auto&row){return checkpoint(out,"record",row);});
 else return c.object(name,[&](auto&out){CheckpointFields f{out};craft_fields::fields(f,v);return f.status();});
}
}
