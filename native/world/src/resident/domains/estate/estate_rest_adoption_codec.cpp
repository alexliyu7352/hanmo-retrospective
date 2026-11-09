// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "estate_rest_adoption.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <type_traits>
namespace hanmo::resident {namespace {
// One field list drives JSON, the non-tree shape sink, and paired checkpoint IO.
template<class A,class T>void fields(A&a,T&v){
 using V=std::remove_const_t<T>;
 if constexpr(std::is_same_v<V,EstateRestAdoptionBasis>)estate_rest_adoption_fields(a,v);
 else if constexpr(std::is_same_v<V,ScenarioOriginRef>){a("catalog_digest",v.catalog_digest);a("scenario_id",v.scenario_id);a("row_key",v.row_key);a("initial_tick",v.initial_tick);}
 else if constexpr(std::is_same_v<V,SourceRef>){a("domain",v.domain);a("kind",v.kind);a("id",v.id);}
 else if constexpr(std::is_same_v<V,EncodedWorldMoment>){a("half_day_tick",v.half_day_tick);a("sub_half_day",v.sub_half_day);}
 else if constexpr(std::is_same_v<V,EncodedFraction>){a("numerator",v.numerator);a("denominator",v.denominator);}
}
template<class T>Status emit(ProjectionWriter&,std::string_view,const T&);
template<class T>Status visit(CodecVisitor&,std::string_view,T&);
struct ProjectFields {ProjectionWriter&w;template<class T>void operator()(const char*k,const T&v){emit(w,k,v);}};
inline Status checked(Status status){if(!status.ok)throw boundary::DecodeFailure{status.error};return status;}
template<class T>Status read_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view);
template<class T>Status read_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view);
struct ReadFields {
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;
 template<class T>void operator()(const char* name,T& value){checked(read_field(archive,input,name,value,path));}
 void finish(){for(const auto& row:boundary::object(input,path))if(!archive.has_visited_field(row.first))boundary::fail_parts("UnsupportedEstateRestAdoptionField",{path,"/",row.first});checked(archive.finish());}
};
struct VisitFields {CodecVisitor&c;template<class T>void operator()(const char*k,T&v){visit(c,k,v);}};
template<class T>Status emit(ProjectionWriter&w,std::string_view name,const T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>)return w.field(name,v);
 else if constexpr(std::is_same_v<T,std::optional<SourceRef>>){if(v)return emit(w,name,*v);return w.null(name);}
 else if constexpr(std::is_same_v<T,WorldMoment>){auto encoded=encode_world_moment(v);if(!encoded.ok)return w.reject("InvalidEstateRestAdoptionMoment",name);return emit(w,name,encoded.value);}
 else return w.object(name,[&](auto&out){ProjectFields f{out};fields(f,v);return out.status();});
}
template<class T>Status read_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path){
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(input,{path});return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,WorldMoment>){checked(archive.prepare_default_values<EncodedWorldMoment>());EncodedWorldMoment encoded;checked(read_value(archive,input,encoded,path));auto decoded=decode_world_moment(encoded);if(!decoded.ok)throw boundary::DecodeFailure{decoded.error};value=std::move(decoded.value);return archive.status();}
 else{checked(archive.defer_duplicate_field_check_to_finish());ReadFields original{archive,input,path};fields(original,value);original.finish();return archive.status();}
}
template<class T>Status read_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path){
 const auto& node=boundary::field(input,name,path);const auto at=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{at});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,at);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,std::optional<SourceRef>>)return archive.optional(name,value,[&](auto& child,auto& source){return read_value(child,node,source,at);},false);
 else return archive.object(name,[&](auto& child){return read_value(child,node,value,at);});
}
template<class T>Status visit(CodecVisitor&c,std::string_view name,T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>)return c.field(name,v);
 else if constexpr(std::is_same_v<T,std::optional<SourceRef>>)return c.optional(name,v,[](auto&x,auto&ref){VisitFields f{x};fields(f,ref);return x.status();});
 else if constexpr(std::is_same_v<T,WorldMoment>){EncodedWorldMoment encoded;if(!c.reading()){auto result=encode_world_moment(v);if(!result.ok)return c.reject("InvalidEstateRestAdoptionMoment",name);encoded=std::move(result.value);}auto result=visit(c,name,encoded);if(result.ok&&c.reading()){auto decoded=decode_world_moment(encoded);if(!decoded.ok)return c.reject("InvalidEstateRestAdoptionMoment",name);v=decoded.value;}return c.status();}
 else return c.object(name,[&](auto&x){VisitFields f{x};fields(f,v);return x.status();});
}
Status shape(const EstateRestAdoptionBasis&v){if(v.source_schema!=1)return common_failure("UnsupportedEstateRestAdoptionSchema");if(v.estate_id.empty()||v.initial_origin.catalog_digest.empty()||v.initial_origin.scenario_id.empty()||v.initial_origin.row_key.empty()||v.initial_origin.initial_tick<0||v.common_fact_count<0||(v.common_fact_count==0)==v.last_common_fact.has_value())return common_failure("InvalidEstateRestAdoptionBasis",v.estate_id);if(v.last_common_fact&&(v.last_common_fact->domain.empty()||v.last_common_fact->kind.empty()||v.last_common_fact->id.empty()))return common_failure("InvalidEstateRestAdoptionPrefix",v.estate_id);auto at=encode_world_moment(v.adopted_at);return at.ok?common_success():Status{false,{},at.error};}
}
Status validate_estate_rest_adoption_shape(const EstateRestAdoptionBasis&v){return shape(v);}
Status emit_estate_rest_adoption(ProjectionWriter&w,const StableId& name,const EstateRestAdoptionBasis&v){auto valid=shape(v);if(!valid.ok)return w.reject(valid.error.original.code,name);return emit(w,name,v);}
BoundaryValue encode_estate_rest_adoption(const EstateRestAdoptionBasis&v){BoundaryValue out;BoundaryProjectionWriter writer(out);auto result=emit_estate_rest_adoption(writer,"",v);if(result.ok)result=writer.finish();if(!result.ok)throw boundary::DecodeFailure{result.error};return out;}
Outcome<EstateRestAdoptionBasis> decode_estate_rest_adoption(const BoundaryValue&input){try{ActorFoundationJsonVisitor archive(input);checked(archive.prepare_default_values<EstateRestAdoptionBasis>());EstateRestAdoptionBasis value;checked(read_value(archive,input,value,"/estate_rest_adoption"));checked(archive.finish());auto valid=shape(value);if(!valid.ok)return {false,{},valid.error};return {true,std::move(value),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Status visit_estate_rest_adoption(CodecVisitor&c,EstateRestAdoptionBasis&v){if(!c.reading()){auto valid=shape(v);if(!valid.ok)return c.reject(valid.error.original.code);}VisitFields f{c};fields(f,v);if(c.status().ok){auto valid=shape(v);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();}
}
