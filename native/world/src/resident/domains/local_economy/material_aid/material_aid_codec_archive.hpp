// P27: one field table for JSON, binary, projection and true source payloads.
#pragma once
#include "material_aid_fields.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <array>
#include <type_traits>
namespace hanmo::resident::material_aid_codec {
template<class T>struct Vector:std::false_type{};
template<class T,class A>struct Vector<std::vector<T,A>>:std::true_type{using Value=T;};
template<class T>struct Optional:std::false_type{};
template<class T>struct Optional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>struct Table:std::false_type{};
template<class T>struct Table<OrderedTable<T>>:std::true_type{using Value=T;};
template<class E>struct Names;
template<>struct Names<SubjectKind>{inline static constexpr std::pair<SubjectKind,const char*> rows[]={{SubjectKind::actor,"actor"},{SubjectKind::organization,"organization"},{SubjectKind::cohort,"cohort"}};};
template<>struct Names<MaterialAidObligationState>{inline static constexpr std::pair<MaterialAidObligationState,const char*>rows[]={{MaterialAidObligationState::owed,"owed"},{MaterialAidObligationState::partly_received,"partly_received"},{MaterialAidObligationState::received,"received"},{MaterialAidObligationState::late,"late"},{MaterialAidObligationState::settling,"settling"},{MaterialAidObligationState::closed,"closed"}};};
template<class T>inline constexpr bool shared_record=std::is_same_v<T,WorldMoment>;
template<class C,class T>Status shared_fields(C&c,T&v){return exact_fields::moment(c,v);}
template<class T>Status emit(ProjectionWriter&,const StableId&,const T&);
template<class T>void decode(const BoundaryValue&,T&,const StableId&);
template<class T>Status checkpoint(CodecVisitor&,const StableId&,T&);
struct ProjectFields{ProjectionWriter&w;Status result=common_success();bool reading()const{return false;}Status status()const{return result.ok?w.status():result;}void reject(const StableId&c,const StableId&p){if(result.ok)result=w.reject(c,p);}template<class T>void operator()(const char*n,const T&v){if(result.ok)result=emit(w,n,v);}};
struct DecodeFields{const BoundaryValue&input;StableId path;std::array<const char*,32>keys{};std::size_t count=0;bool reading()const{return true;}void reject(const StableId&c,const StableId&p){boundary::fail(c,path+"/"+p);}template<class T>void operator()(const char*n,T&v){if(count==keys.size())boundary::fail("MaterialAidFieldCountOverflow",path);keys[count++]=n;decode(boundary::field(input,n,path),v,path+"/"+n);}void finish(){const auto&rows=boundary::object(input,path);if(rows.size()!=count)boundary::fail("MaterialAidFieldCoverageMismatch",path);for(std::size_t n=0;n<rows.size();++n){const auto&row=rows[n];bool known=false;for(std::size_t i=0;i<count;++i)if(row.first==keys[i])known=true;if(!known)boundary::fail("UnsupportedResidentField",path+"/"+row.first);for(std::size_t i=0;i<n;++i)if(rows[i].first==row.first)boundary::fail("DuplicateResidentField",path+"/"+row.first);}}};
struct CheckpointFields{CodecVisitor&c;Status result=common_success();bool reading()const{return c.reading();}Status status()const{return result.ok?c.status():result;}void reject(const StableId&code,const StableId&p){if(result.ok)result=c.reject(code,p);}template<class T>void operator()(const char*n,T&v){if(result.ok)result=checkpoint(c,n,v);}};
template<class T>Status emit(ProjectionWriter&w,const StableId&name,const T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,v);
 else if constexpr(shared_record<T>)return emit_projection_record<ActorFoundationProjectionArchive>(w,name,v,[](auto&c,auto&value){return shared_fields(c,value);});
 else if constexpr(std::is_enum_v<T>){for(const auto&row:Names<T>::rows)if(row.first==v)return w.field(name,StableId{row.second});return w.reject("UnknownMaterialAidEnum",name);}
 else if constexpr(Vector<T>::value)return w.sequence(name,v,[](auto&out,const auto&row){return emit(out,"",row);});
 else if constexpr(Optional<T>::value){if(v)return emit(w,name,*v);return w.null(name);}
 else if constexpr(Table<T>::value)return w.keyed_dictionary(name,{},[&](auto&out){for(const auto&row:v.rows()){auto status=emit(out,row.first,row.second);if(!status.ok)return status;}return out.status();});
 else return w.object(name,[&](auto&out){ProjectFields f{out};material_aid_fields::fields(f,const_cast<T&>(v));return f.status();});
}
template<class T>void decode(const BoundaryValue&input,T&v,const StableId&path){
 if constexpr(std::is_same_v<T,StableId>)v=boundary::string(input,path);
 else if constexpr(std::is_same_v<T,Integer>)v=boundary::integer(input,path);
 else if constexpr(std::is_same_v<T,bool>)v=boundary::boolean(input,path);
 else if constexpr(shared_record<T>){auto read=exact_fields::decode_json<T>(input,[](auto&c,auto&value){return shared_fields(c,value);});if(!read.ok)throw boundary::DecodeFailure{read.error};v=std::move(read.value);}
 else if constexpr(std::is_enum_v<T>){const auto&name=boundary::string(input,path);for(const auto&row:Names<T>::rows)if(name==row.second){v=row.first;return;}boundary::fail("UnknownMaterialAidEnum",path);}
 else if constexpr(Vector<T>::value){v.clear();const auto&rows=boundary::array(input,path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",path);for(const auto&row:rows){typename Vector<T>::Value value{};decode(row,value,path+"/"+std::to_string(v.size()));v.push_back(std::move(value));}}
 else if constexpr(Optional<T>::value){if(std::holds_alternative<std::monostate>(input.data))v.reset();else{typename Optional<T>::Value value{};decode(input,value,path);v=std::move(value);}}
 else if constexpr(Table<T>::value){v={};const auto&rows=boundary::object(input,path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",path);for(const auto&row:rows){typename Table<T>::Value value{};decode(row.second,value,path+"/"+row.first);if(!v.insert(row.first,std::move(value)))boundary::fail("DuplicateMaterialAidRecord",path+"/"+row.first);}}
 else{DecodeFields f{input,path};material_aid_fields::fields(f,v);f.finish();}
}
template<class T>Status checkpoint(CodecVisitor&c,const StableId&name,T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,v);
 else if constexpr(shared_record<T>)return c.object(name,[&](auto&out){return shared_fields(out,v);});
 else if constexpr(std::is_enum_v<T>){StableId value;if(!c.reading()){bool found=false;for(const auto&row:Names<T>::rows)if(row.first==v){value=row.second;found=true;}if(!found)return c.reject("UnknownMaterialAidEnum",name);}c.field(name,value);if(!c.status().ok)return c.status();for(const auto&row:Names<T>::rows)if(value==row.second){if(c.reading())v=row.first;return c.status();}return c.reject("UnknownMaterialAidEnum",name);}
 else if constexpr(Vector<T>::value)return c.sequence(name,v,[](auto&out,auto&row){return checkpoint(out,"item",row);});
 else if constexpr(Optional<T>::value)return c.optional(name,v,[](auto&out,auto&row){return checkpoint(out,"value",row);});
 else if constexpr(Table<T>::value)return common_checkpoint::table(c,name,v,[](auto&out,auto&row){return checkpoint(out,"record",row);});
 else return c.object(name,[&](auto&out){CheckpointFields f{out};material_aid_fields::fields(f,v);return f.status();});
}
}
