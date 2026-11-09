// 字段序唯一，JSON与二进制共用同一个遍历器，未知枚举/多字段/缺字段拒绝。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "item_service_fields.hpp"
#include "item_service_codec.hpp"
#include "item_condition_codec_fields.hpp"
#include "repair_codec.hpp"
#include "resident/domains/commerce/repair_service/repair_service_codec.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <string_view>
#include "craft_codec_archive.hpp"
#include "resident/storage/common_projection.hpp"
#include "hanmo/resident/world_moment.hpp"
#include <type_traits>
namespace hanmo::resident::item_codec {
template<class T>struct EnumNames;
template<class T>struct IsVector:std::false_type{};
template<class T>struct IsVector<std::vector<T>>:std::true_type{using Value=T;};
template<class T>struct IsOptional:std::false_type{};
template<class T>struct IsOptional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>struct IsTable:std::false_type{};
template<class T>struct IsTable<OrderedTable<T>>:std::true_type{using Value=T;};
template<>struct EnumNames<SubjectKind>{inline static constexpr std::pair<SubjectKind,const char*> rows[]={{SubjectKind::actor,"actor"},{SubjectKind::organization,"organization"},{SubjectKind::cohort,"cohort"}};};
template<>struct EnumNames<ItemInstanceState>{inline static constexpr std::pair<ItemInstanceState,const char*> rows[]={{ItemInstanceState::live,"live"},{ItemInstanceState::destroyed,"destroyed"}};};
template<>struct EnumNames<ItemOutputKind>{inline static constexpr std::pair<ItemOutputKind,const char*> rows[]={{ItemOutputKind::resource_harvest,"resource_harvest"},{ItemOutputKind::production_output,"production_output"},{ItemOutputKind::craft_output,"craft_output"},{ItemOutputKind::pharmacy_output,"pharmacy_output"},{ItemOutputKind::ritual_material_output,"ritual_material_output"},{ItemOutputKind::arcane_reward_output,"arcane_reward_output"},{ItemOutputKind::cultural_carrier,"cultural_carrier"}};};
template<>struct EnumNames<ItemConditionSourceKind>{inline static constexpr std::pair<ItemConditionSourceKind,const char*> rows[]={{ItemConditionSourceKind::use_damage,"use_damage"},{ItemConditionSourceKind::repair_completed,"repair_completed"}};};
template<>struct EnumNames<ItemConditionDisposition>{inline static constexpr std::pair<ItemConditionDisposition,const char*> rows[]={{ItemConditionDisposition::applied,"applied"},{ItemConditionDisposition::no_change,"no_change"}};};
template<class A>void record(A& a,SourceRef& v){a("domain",v.domain);a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,SubjectRef& v){a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,LotPropertiesRef& v){a("properties_id",v.properties_id);}
template<class A>void record(A& a,ItemInstance& v){item_service_fields::fields(a,v);}
template<class A>void record(A& a,LotProperties& v){item_service_fields::fields(a,v);}
template<class A>void record(A& a,ItemOutputKey& v){item_service_fields::fields(a,v);}
template<class A>void record(A& a,ItemOutputAdoption& v){item_service_fields::fields(a,v);}
template<class A>void record(A& a,ItemTerminationResult& v){item_service_fields::fields(a,v);}
template<class A>void record(A&a,ItemUseSource&v){item_condition_fields::fields(a,v);}
template<class A>void record(A&a,ItemConditionResultView&v){item_condition_fields::fields(a,v);auto checked=validate_item_condition_result_shape(v);if(!checked.ok)a.reject(checked.error.original.code,checked.error.original.path);}
template<class A>void record_state(A&a,ItemInstancesState&v,Integer version){
 if(version!=1&&version!=2&&version!=3){a.reject("UnsupportedItemInstancesVersion","item_instances");return;}
 if(!a.reading()&&version==1&&!v.condition_results.empty()){a.reject("UnselectedItemConditionResults","condition_results");return;}
 if(!a.reading()&&version!=3&&(!v.repair_jobs.acceptances.rows().empty()||!v.repair_jobs.unstarted_interruptions.rows().empty()||!v.repair_jobs.jobs.rows().empty()||!v.repair_jobs.controls.rows().empty()||!v.repair_jobs.completions.rows().empty()||!v.repair_jobs.interruptions.rows().empty())){a.reject("UnselectedRepairJobs","repair_jobs");return;}
 if(!a.reading()&&version!=3&&!repair_service_contracts_empty(v.repair_service_contracts)){a.reject("UnselectedRepairServiceContracts","repair_service_contracts");return;}
 item_service_fields::fields(a,v,version);
}
template<class A>void record(A&a,ItemInstancesState&v){record_state(a,v,1);}
template<class A>void record(A& a,LotPropertiesState& v){item_service_fields::fields(a,v);}
template<class A>void record(A& a,ItemPropertyPolicy& v){item_service_fields::fields(a,v);}
template<class A>void record(A& a,ItemServiceCatalog& v){item_service_fields::fields(a,v);}
template<class T> BoundaryValue encode(const T& value);
template<class T> Status project(ProjectionWriter&,std::string_view,const T&);
template<class T> void decode(const BoundaryValue&,T&,std::string_view);
template<class T> Status checkpoint(CodecVisitor&,std::string_view,T&);
struct Projector {ProjectionWriter& writer;Status result=common_success();bool reading()const{return false;}template<class T>Status prepare_default_values(){return common_success();}Status status()const{return result.ok?writer.status():result;}void reject(std::string_view code,std::string_view path){if(result.ok)result=writer.reject(code,path);}template<class T>void operator()(const char* key,const T& v){if(result.ok)result=project(writer,key,v);}};
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view);
struct Decoder {
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;std::size_t count=0;
 bool reading()const{return true;}
 template<class T>Status prepare_default_values(){return archive.template prepare_default_values<T>();}
 void reject(std::string_view code,std::string_view detail){boundary::fail_parts(code,{path,"/",detail});}
 template<class T>void operator()(const char* name,T& value){++count;craft_codec::checked(json_field(archive,input,name,value,path));}
 void finish(){if(boundary::object(input,path).size()!=count)boundary::fail("DynamicFieldCoverageMismatch",path);craft_codec::checked(archive.finish());}
};
struct Checkpointer {CodecVisitor& codec;Status result=common_success();bool reading()const{return codec.reading();}template<class T>Status prepare_default_values(){return codec.template prepare_default_values<T>();}Status status()const{return result.ok?codec.status():result;}void reject(std::string_view code,std::string_view path){if(result.ok)result=codec.reject(code,path);}template<class T>void operator()(const char* key,T& v){if(result.ok)result=checkpoint(codec,key,v);}};
// 缺失字段不补跑初始化，重复表键不覆盖既有记录。
template<class T> Status project(ProjectionWriter& w,std::string_view name,const T& value){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,value);
 else if constexpr(std::is_same_v<T,RepairJobsState>)return emit_repair_jobs(w,name,value);
 else if constexpr(std::is_same_v<T,RepairServiceContractsState>)return emit_repair_service_contracts(w,name,value);
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,name,value);
 else if constexpr(std::is_same_v<T,Location>)return emit_common_location(w,name,value);
 else if constexpr(std::is_same_v<T,CanonicalFraction>){auto encoded=encode_canonical_fraction(value);if(!encoded.ok)return w.reject("InvalidEconomicExactFraction",name);return w.object(name,[&](auto& x){x.field("numerator",encoded.value.numerator);return x.field("denominator",encoded.value.denominator);});}
 else if constexpr(std::is_enum_v<T>){for(const auto& p:EnumNames<T>::rows)if(p.first==value)return w.field(name,StableId{p.second});return w.reject("InvalidDynamicEnum",name);}
 else if constexpr(IsVector<T>::value)return w.sequence(name,value,[](auto& x,const auto& v){return project(x,"",v);});
 else if constexpr(IsOptional<T>::value){if(value)return project(w,name,*value);return w.null(name);}
 else if constexpr(IsTable<T>::value)return w.keyed_dictionary(name,{},[&](auto& x){for(const auto& p:value.rows()){project(x,p.first,p.second);if(!x.status().ok)break;}return x.status();});
 else return w.object(name,[&](auto& x){Projector fields{x};record(fields,const_cast<T&>(value));return fields.status();});
}
template<class T> BoundaryValue encode(const T& value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=project(writer,"",value);if(status.ok)status=writer.finish();if(!status.ok)throw [&]{boundary::DecodeFailure failure{status.error.original.code,status.error.original.path};failure.error=status.error;return failure;}();return out;}
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_same_v<T,Owner>)return craft_codec::json_value(archive,input,value,path);
 else if constexpr(std::is_same_v<T,RepairJobsState>)return read_repair_jobs_json_fields(archive,input,value,path);
 else if constexpr(std::is_same_v<T,RepairServiceContractsState>)return read_repair_service_contracts_json_fields(archive,input,value,path);
 else if constexpr(std::is_enum_v<T>){StableId named;boundary::string_ref(input,{path});craft_codec::checked(archive.field("",named));for(const auto& row:EnumNames<T>::rows)if(named==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",path);}
 else{craft_codec::checked(archive.defer_duplicate_field_check_to_finish());Decoder fields{archive,input,path};record(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,std::uint32_t>){auto number=boundary::integer(node,child_path);if(number<0||static_cast<std::uint64_t>(number)>std::numeric_limits<std::uint32_t>::max())boundary::fail("CraftUnsignedIntegerOutOfRange",child_path);craft_codec::checked(archive.field(name,number));value=static_cast<std::uint32_t>(number);return archive.status();}
 else if constexpr(std::is_enum_v<T>){StableId named;boundary::string_ref(node,{child_path});craft_codec::checked(archive.field(name,named));for(const auto& row:EnumNames<T>::rows)if(named==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",child_path);}
 else if constexpr(IsVector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,craft_codec::index_path(child_path,position));},craft_codec::json_scalar<typename IsVector<T>::Value>);}
 else if constexpr(IsOptional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path);},craft_codec::json_scalar<typename IsOptional<T>::Value>);
 else if constexpr(IsTable<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}));for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateDynamicRecord",{child_path,"/",rows[position].first});return result;};return archive.table(name,value,read);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path);});
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path){ActorFoundationJsonVisitor archive(input,craft_codec::json_scalar<T>);craft_codec::checked(json_value(archive,input,value,path));craft_codec::checked(archive.finish());}
// 字符串枚举固定协议语义，恢复时不接受 C++ ordinal 巧合。
template<class T> Status checkpoint(CodecVisitor& c,std::string_view name,T& value) {
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,value);
    else if constexpr(std::is_same_v<T,RepairJobsState>)return c.object(name,[&](auto&x){return visit_repair_jobs(x,value);});
    else if constexpr(std::is_same_v<T,RepairServiceContractsState>)return c.object(name,[&](auto&x){return visit_repair_service_contracts(x,value);});
    else if constexpr(std::is_same_v<T,Owner>)return c.object(name,[&](CodecVisitor& x){return common_checkpoint::owner(x,value);});
    else if constexpr(std::is_enum_v<T>) {StableId named;for(const auto& p:EnumNames<T>::rows)if(p.first==value)named=p.second;c.field(name,named);if(!c.status().ok)return c.status();for(const auto& p:EnumNames<T>::rows)if(p.second==named){if(c.reading())value=p.first;return c.status();}return c.reject("InvalidDynamicEnum",name);}
    else if constexpr(IsVector<T>::value)return c.sequence(name,value,[&](auto& x,auto& v){return checkpoint(x,"item",v);});
    else if constexpr(IsOptional<T>::value)return c.optional(name,value,[&](auto& x,auto& v){return checkpoint(x,"value",v);});
    else if constexpr(IsTable<T>::value)return common_checkpoint::table(c,name,value,[&](auto& x,auto& v){return checkpoint(x,"record",v);});
    else return c.object(name,[&](CodecVisitor& x){Checkpointer a{x};record(a,value);return a.status();});
}
}
namespace hanmo::resident {
BoundaryValue encode_item_instances(const ItemInstancesState&v,Integer version){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_item_instances(writer,v,version);if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure{status.error.original.code,status.error.original.path};failure.error=status.error;throw failure;}return out;}
Outcome<ItemInstancesState> decode_item_instances(const BoundaryValue&v,Integer version){try{ActorFoundationJsonVisitor archive(v);craft_codec::checked(archive.defer_duplicate_field_check_to_finish());craft_codec::checked(archive.prepare_default_values<ItemInstancesState>());ItemInstancesState out;item_codec::Decoder fields{archive,v,"/item_instances"};item_codec::record_state(fields,out,version);fields.finish();return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Status visit_item_instances(CodecVisitor&c,ItemInstancesState&v,Integer version){item_codec::Checkpointer fields{c};item_codec::record_state(fields,v,version);return fields.status();}
BoundaryValue encode_item_condition_result(const ItemConditionResultView&v){return item_codec::encode(v);}
Outcome<ItemConditionResultView> decode_item_condition_result(const BoundaryValue&v){try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<ItemConditionResultView>();ItemConditionResultView out;item_codec::decode(v,out,"/item_condition_result");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Status visit_item_condition_result(CodecVisitor&c,ItemConditionResultView&v){item_codec::Checkpointer fields{c};item_codec::record(fields,v);return fields.status();}
Status emit_item_condition_result(ProjectionWriter&w,const ItemConditionResultView&v){return item_codec::project(w,"",v);}
BoundaryValue encode_lot_properties(const LotPropertiesState& v){return item_codec::encode(v);}
Outcome<LotPropertiesState> decode_lot_properties(const BoundaryValue& v){try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<LotPropertiesState>();LotPropertiesState out;item_codec::decode(v,out,"/lot_properties");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_lot_properties(CodecVisitor& c,LotPropertiesState& v){item_codec::Checkpointer fields{c};item_codec::record(fields,v);return fields.status();}
BoundaryValue encode_item_service_catalog(const ItemServiceCatalog& v){return item_codec::encode(v);}
Outcome<ItemServiceCatalog> decode_item_service_catalog(const BoundaryValue& v){try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<ItemServiceCatalog>();ItemServiceCatalog out;item_codec::decode(v,out,"/item_service_catalog");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<ItemServiceCatalog> decode_item_service_catalog(const BoundaryValue&v,const CommonCatalog&common){auto result=decode_item_service_catalog(v);if(!result.ok)return result;auto checked=validate_item_service_catalog(result.value,common);if(!checked.ok)return {false,{},checked.error};return result;}
Status visit_item_service_catalog(CodecVisitor& c,ItemServiceCatalog& v){item_codec::Checkpointer fields{c};item_codec::record(fields,v);return fields.status();}
Status emit_item_instances(ProjectionWriter&w,const ItemInstancesState&v,Integer version){return w.object("",[&](auto&child){item_codec::Projector fields{child};item_codec::record_state(fields,const_cast<ItemInstancesState&>(v),version);return fields.status();});}
Status emit_lot_properties(ProjectionWriter& w,const LotPropertiesState& v){return item_codec::project(w,"",v);}
Status emit_item_service_catalog(ProjectionWriter& w,const ItemServiceCatalog& v){return item_codec::project(w,"",v);}
}
