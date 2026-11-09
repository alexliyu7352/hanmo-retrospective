// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "self_transport_funding_fields.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <type_traits>
namespace hanmo::resident::self_funding_codec {
template<class T>struct Vector:std::false_type{};
template<class T,class A>struct Vector<std::vector<T,A>>:std::true_type{using value_type=T;};
template<class T>struct Table:std::false_type{};
template<class T>struct Table<OrderedTable<T>>:std::true_type{using value_type=T;};
template<class T>Status emit(ProjectionWriter&,std::string_view,const T&);
template<class T>Status checkpoint(CodecVisitor&,std::string_view,T&);
struct ProjectionFields {
 ProjectionWriter&w;void reject(std::string_view code,std::string_view path){w.reject(code,path);}
 template<class T>void operator()(const char*name,const T&value){emit(w,name,value);}
};
template<class T>inline constexpr bool json_scalar=std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_enum_v<T>;
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view);
inline Status checked(Status status){if(!status.ok)throw boundary::DecodeFailure{status.error};return status;}
inline StableId index_path(std::string_view path,std::size_t index){char digits[3*sizeof(index)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+index%10);index/=10;}while(index);return checkpoint_decode_boundary_join({path,"/",std::string_view(start,std::size_t(end-start))});}
struct DecodeFields {
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;
 void reject(std::string_view code,std::string_view where){boundary::fail_parts(code,{path,"/",where});}
 template<class T>void operator()(const char* name,T& value){checked(json_field(archive,input,name,value,path));}
 void finish(){for(const auto& entry:boundary::object(input,path))if(!archive.has_visited_field(entry.first))boundary::fail_parts("UnsupportedResidentField",{path,"/",entry.first});checked(archive.finish());}
};
struct CheckpointFields {
 CodecVisitor&c;void reject(std::string_view code,std::string_view path){c.reject(code,path);}
 template<class T>void operator()(const char*name,T&value){checkpoint(c,name,value);}
};
template<class T>Status emit(ProjectionWriter&w,std::string_view name,const T&value){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>)return w.field(name,value);
 else if constexpr(std::is_same_v<T,SelfTransportFundingContinuity>){if(value!=SelfTransportFundingContinuity::honor_committed_safe_return)return w.reject("InvalidSelfFundingContinuity",name);return w.field(name,StableId{"honor_committed_safe_return"});}
 else if constexpr(std::is_same_v<T,CanonicalFraction>){auto e=encode_canonical_fraction(value);if(!e.ok)return w.reject(e.error.original.code,e.error.original.path);return w.object(name,[&](auto&out){out.field("numerator",e.value.numerator);return out.field("denominator",e.value.denominator);});}
 else if constexpr(Vector<T>::value)return w.sequence(name,value,[](auto&out,const auto&v){return emit(out,"",v);});
 else if constexpr(Table<T>::value)return w.keyed_dictionary(name,{},[&](auto&out){for(const auto&row:value.rows()){emit(out,row.first,row.second);if(!out.status().ok)break;}return out.status();});
 else return w.object(name,[&](auto&out){ProjectionFields fields{out};self_transport_funding_fields::fields(fields,const_cast<T&>(value));return out.status();});
}
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path){
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(input,{path});return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,SelfTransportFundingContinuity>){StableId name;boundary::string_ref(input,{path});checked(archive.field("",name));if(name!="honor_committed_safe_return")boundary::fail("InvalidSelfFundingContinuity",path);value=SelfTransportFundingContinuity::honor_committed_safe_return;return archive.status();}
 else if constexpr(std::is_same_v<T,CanonicalFraction>){for(const auto& row:boundary::object(input,path))if(row.first!="numerator"&&row.first!="denominator")boundary::fail_parts("UnsupportedResidentField",{path,"/",row.first});checked(archive.defer_duplicate_field_check_to_finish());EncodedFraction fields;checked(json_field(archive,input,"numerator",fields.numerator,path));checked(json_field(archive,input,"denominator",fields.denominator,path));auto decoded=decode_canonical_fraction(fields);if(!decoded.ok)throw boundary::DecodeFailure{decoded.error};value=std::move(decoded.value);return archive.finish();}
 else{checked(archive.defer_duplicate_field_check_to_finish());DecodeFields fields{archive,input,path};self_transport_funding_fields::fields(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,SelfTransportFundingContinuity>){StableId text;boundary::string_ref(node,{child_path});checked(archive.field(name,text));if(text!="honor_committed_safe_return")boundary::fail("InvalidSelfFundingContinuity",child_path);value=SelfTransportFundingContinuity::honor_committed_safe_return;return archive.status();}
 else if constexpr(Vector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(child_path,position));},json_scalar<typename Vector<T>::value_type>);}
 else if constexpr(Table<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto status=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}));if(rows[position].first!=row.id)boundary::fail_parts("InvalidSelfFundingRecordKey",{child_path,"/",rows[position].first});for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("InvalidSelfFundingRecordKey",{child_path,"/",rows[position].first});return status;};using Row=typename Table<T>::value_type;static_assert(std::is_same_v<Row,SelfTransportFundingTerms>,"Only the original exact funding row needs this transfer");return archive.table(name,value,read,[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();});}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path);});
}
template<class T>T decode_root(const BoundaryValue& input,std::string_view path){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(archive.template prepare_default_values<T>());T value;checked(json_value(archive,input,value,path));checked(archive.finish());return value;}
template<class T>Status checkpoint(CodecVisitor&c,std::string_view name,T&value){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>)return c.field(name,value);
 else if constexpr(std::is_same_v<T,SelfTransportFundingContinuity>){StableId named;if(!c.reading()){if(value!=SelfTransportFundingContinuity::honor_committed_safe_return)return c.reject("InvalidSelfFundingContinuity",name);named="honor_committed_safe_return";}c.field(name,named);if(!c.status().ok)return c.status();if(named!="honor_committed_safe_return")return c.reject("InvalidSelfFundingContinuity",name);if(c.reading())value=SelfTransportFundingContinuity::honor_committed_safe_return;return c.status();}
 else if constexpr(std::is_same_v<T,CanonicalFraction>)return c.object(name,[&](auto&out){EncodedFraction fields;if(!out.reading()){auto v=encode_canonical_fraction(value);if(!v.ok)return out.reject(v.error.original.code,v.error.original.path);fields=std::move(v.value);}out.field("numerator",fields.numerator);out.field("denominator",fields.denominator);if(out.reading()&&out.status().ok){auto v=decode_canonical_fraction(fields);if(!v.ok)return out.reject(v.error.original.code,v.error.original.path);value=std::move(v.value);}return out.status();});
 else if constexpr(Vector<T>::value)return c.sequence(name,value,[](auto&out,auto&v){return checkpoint(out,"item",v);});
 else if constexpr(Table<T>::value)return common_checkpoint::table(c,name,value,[](auto&out,auto&v){return checkpoint(out,"record",v);});
 else return c.object(name,[&](auto&out){CheckpointFields fields{out};self_transport_funding_fields::fields(fields,value);return out.status();});
}
}
namespace hanmo::resident {
Outcome<PrivateTransportFundingState> decode_private_transport_funding(const BoundaryValue&input){try{auto out=self_funding_codec::decode_root<PrivateTransportFundingState>(input,"/private_transport_funding");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_private_transport_funding(const PrivateTransportFundingState&value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_private_transport_funding_projection(writer,"",value);if(status.ok)status=writer.finish();if(!status.ok)return {false,{},status.error};return {true,std::move(out),{}};}
Status visit_private_transport_funding_checkpoint(CodecVisitor&c,PrivateTransportFundingState&value){self_funding_codec::CheckpointFields fields{c};self_transport_funding_fields::fields(fields,value);return c.status();}
Status emit_private_transport_funding_projection(ProjectionWriter&w,const StableId& name,const PrivateTransportFundingState&value){return self_funding_codec::emit(w,name,value);}
}
