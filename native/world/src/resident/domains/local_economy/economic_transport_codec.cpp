// 原运输付款关联的唯一字段表，JSON与shape共用emit；authority复用S1唯一codec。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "economic_transport_codec_fields.hpp"
#include "resident/domains/travel/transport_authority_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <type_traits>
namespace hanmo::resident::economic_transport_codec {
template<class T>struct EnumNames;
template<class T>struct IsVector:std::false_type{};
template<class T,class Allocator>struct IsVector<std::vector<T,Allocator>>:std::true_type{using Value=T;};
template<class T>struct IsOptional:std::false_type{};
template<class T>struct IsOptional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>struct IsTable:std::false_type{};
template<class T>struct IsTable<OrderedTable<T>>:std::true_type{using Value=T;};
template<>struct EnumNames<TransportExpensePurpose>{inline static constexpr std::pair<TransportExpensePurpose,const char*> names[]={{TransportExpensePurpose::base_fare,"base_fare"},{TransportExpensePurpose::handling,"handling"},{TransportExpensePurpose::toll,"toll"},{TransportExpensePurpose::safe_return,"safe_return"}};static constexpr const auto& rows(){return names;}};
template<class T> BoundaryValue encode(const T& value);
template<class T> Status project(ProjectionWriter&,std::string_view,const T&);
template<class T> void decode(const BoundaryValue&,T&,std::string_view);
template<class T> Status checkpoint(CodecVisitor&,std::string_view,T&);
struct Projector {ProjectionWriter& writer;template<class T>void operator()(const char* key,const T& v){project(writer,key,v);}};
template<class T>inline constexpr bool json_scalar=std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_enum_v<T>;
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view);
inline Status checked(Status value){if(!value.ok)throw boundary::DecodeFailure{value.error};return value;}
inline StableId index_path(std::string_view path,std::size_t index){char digits[3*sizeof(index)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+index%10);index/=10;}while(index);return checkpoint_decode_boundary_join({path,"/",std::string_view(start,std::size_t(end-start))});}
struct Decoder {
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;
 template<class T>void operator()(const char* key,T& value){checked(json_field(archive,input,key,value,path));}
 void finish(){for(const auto& row:boundary::object(input,path))if(!archive.has_visited_field(row.first))boundary::fail_parts("UnsupportedResidentField",{path,"/",row.first});checked(archive.finish());}
};
struct Checkpointer {
    CodecVisitor& codec;
    template<class T> void operator()(const char* key,T& v) { checkpoint(codec,key,v); }
};
// 缺失字段不补跑初始化，重复表键不覆盖既有记录。
template<class T> Status project(ProjectionWriter& w,std::string_view name,const T& value){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,value);
 else if constexpr(std::is_same_v<T,TransportExpenseAuthority>)return emit_transport_expense_authority_projection(w,name,value);
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,name,value);
 else if constexpr(std::is_same_v<T,Location>)return emit_common_location(w,name,value);
 else if constexpr(std::is_same_v<T,CanonicalFraction>){auto encoded=encode_canonical_fraction(value);if(!encoded.ok)return w.reject("InvalidTransportExactFraction",name);return w.object(name,[&](auto& x){x.field("numerator",encoded.value.numerator);return x.field("denominator",encoded.value.denominator);});}
 else if constexpr(std::is_enum_v<T>){for(const auto& p:EnumNames<T>::rows())if(p.first==value)return w.field(name,StableId{p.second});return w.reject("InvalidDynamicEnum",name);}
 else if constexpr(IsVector<T>::value)return w.sequence(name,value,[](auto& x,const auto& v){return project(x,"",v);});
 else if constexpr(IsOptional<T>::value){if(value)return project(w,name,*value);return w.null(name);}
 else if constexpr(IsTable<T>::value)return w.keyed_dictionary(name,{},[&](auto& x){for(const auto& p:value.rows()){project(x,p.first,p.second);if(!x.status().ok)break;}return x.status();});
 else return w.object(name,[&](auto& x){Projector fields{x};record(fields,const_cast<T&>(value));return x.status();});
}
template<class T> BoundaryValue encode(const T& value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=project(writer,"",value);if(status.ok)status=writer.finish();if(!status.ok)throw [&]{boundary::DecodeFailure failure{status.error.original.code,status.error.original.path};failure.error=status.error;return failure;}();return out;}
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path){
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(input,{path});return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,TransportExpenseAuthority>){
  auto emit_original=[](ProjectionWriter& writer,const T& row){return emit_transport_expense_authority_projection(writer,"",row);};
  if(ScopedCheckpointJsonDecode::current())return archive.foreign_record(value,[](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget){return decode_transport_expense_authority(node,budget);},emit_original);
  return archive.foreign_record(value,[](const BoundaryValue& node){return decode_transport_expense_authority(node);},emit_original);
 }
 else if constexpr(std::is_same_v<T,Owner>||std::is_same_v<T,Location>){
  auto original=[&](const BoundaryValue& node)->Outcome<T>{try{if constexpr(std::is_same_v<T,Owner>)return {true,boundary::decode_owner(node,checkpoint_decode_boundary_text(path)),{}};else return {true,boundary::decode_location(node,checkpoint_decode_boundary_text(path)),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}};
  auto emit_original=[](ProjectionWriter& writer,const T& row){return project(writer,"",row);};
  if(ScopedCheckpointJsonDecode::current())return archive.foreign_record(value,[&](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget)->Outcome<T>{if(&budget!=ScopedCheckpointJsonDecode::current())budget.fail("CheckpointCandidateMemoryJobMismatch","economic_transport_original");budget.template default_values<T>();return original(node);},emit_original);
  return archive.foreign_record(value,original,emit_original);
 }
 else if constexpr(std::is_same_v<T,CanonicalFraction>){for(const auto& field:boundary::object(input,path))if(field.first!="numerator"&&field.first!="denominator")boundary::fail_parts("UnsupportedResidentField",{path,"/",field.first});checked(archive.defer_duplicate_field_check_to_finish());EncodedFraction fields;checked(json_field(archive,input,"numerator",fields.numerator,path));checked(json_field(archive,input,"denominator",fields.denominator,path));auto decoded=decode_canonical_fraction(fields);if(!decoded.ok)throw boundary::DecodeFailure{decoded.error};value=std::move(decoded.value);return archive.finish();}
 else if constexpr(std::is_enum_v<T>){StableId name;boundary::string_ref(input,{path});checked(archive.field("",name));for(const auto& row:EnumNames<T>::rows())if(name==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",path);}
 else{checked(archive.defer_duplicate_field_check_to_finish());Decoder fields{archive,input,path};record(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_enum_v<T>){StableId text;boundary::string_ref(node,{child_path});checked(archive.field(name,text));for(const auto& row:EnumNames<T>::rows())if(text==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",child_path);}
 else if constexpr(IsVector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(child_path,position));},json_scalar<typename IsVector<T>::Value>);}
 else if constexpr(IsOptional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path);},json_scalar<typename IsOptional<T>::Value>);
 else if constexpr(IsTable<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}));for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateDynamicRecord",{child_path,"/",rows[position].first});return result;};using Row=typename IsTable<T>::Value;if constexpr(std::is_same_v<Row,TransportPaymentReceipt>)return archive.table(name,value,read,[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();},json_scalar<Row>);else return archive.table(name,value,read,json_scalar<Row>);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path);});
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(json_value(archive,input,value,path));checked(archive.finish());}
template<class T>T decode_root(const BoundaryValue& input,std::string_view path){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(archive.template prepare_default_values<T>());T value;checked(json_value(archive,input,value,path));checked(archive.finish());return value;}
// 字符串枚举固定协议语义，恢复时不接受 C++ ordinal 巧合。
template<class T> Status checkpoint(CodecVisitor& c,std::string_view name,T& value) {
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,value);
    else if constexpr(std::is_same_v<T,TransportExpenseAuthority>)return visit_transport_expense_authority(c,name,value);
    else if constexpr(std::is_same_v<T,Owner>)return c.object(name,[&](CodecVisitor& x){return common_checkpoint::owner(x,value);});
    else if constexpr(std::is_same_v<T,Location>)return c.object(name,[&](CodecVisitor& x){return common_checkpoint::location(x,value);});
    else if constexpr(std::is_same_v<T,CanonicalFraction>)return c.object(name,[&](CodecVisitor& x){EncodedFraction fields;if(!x.reading()){auto encoded=encode_canonical_fraction(value);if(!encoded.ok)return x.reject("InvalidTransportExactFraction");fields=std::move(encoded.value);}x.field("numerator",fields.numerator);x.field("denominator",fields.denominator);if(x.reading()&&x.status().ok){auto decoded=decode_canonical_fraction(fields);if(!decoded.ok)return x.reject("InvalidTransportExactFraction");value=decoded.value;}return x.status();});
    else if constexpr(std::is_enum_v<T>) {StableId named;for(const auto& p:EnumNames<T>::rows())if(p.first==value)named=p.second;c.field(name,named);if(!c.status().ok)return c.status();for(const auto& p:EnumNames<T>::rows())if(p.second==named){if(c.reading())value=p.first;return c.status();}return c.reject("InvalidDynamicEnum",name);}
    else if constexpr(IsVector<T>::value)return c.sequence(name,value,[&](auto& x,auto& v){return checkpoint(x,"item",v);});
    else if constexpr(IsOptional<T>::value)return c.optional(name,value,[&](auto& x,auto& v){return checkpoint(x,"value",v);});
    else if constexpr(IsTable<T>::value)return common_checkpoint::table(c,name,value,[&](auto& x,auto& v){return checkpoint(x,"record",v);});
    else return c.object(name,[&](CodecVisitor& x){Checkpointer a{x};record(a,value);return x.status();});
}
}
namespace hanmo::resident {
Outcome<BoundaryValue> encode_transport_payment_receipt(const TransportPaymentReceipt& v){try{return {true,economic_transport_codec::encode(v),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<TransportPaymentReceipt> decode_transport_payment_receipt(const BoundaryValue& v){try{auto out=economic_transport_codec::decode_root<TransportPaymentReceipt>(v,"/transport_payment_receipt");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_transport_payment_receipt(CodecVisitor& c,TransportPaymentReceipt& v){economic_transport_codec::Checkpointer fields{c};economic_transport_codec::record(fields,v);return c.status();}
Status visit_transport_payment_receipt(CodecVisitor& c,const char* name,TransportPaymentReceipt& v){return economic_transport_codec::checkpoint(c,name,v);}
Status emit_transport_payment_receipt_fields(ProjectionWriter& w,const TransportPaymentReceipt& v){economic_transport_codec::Projector fields{w};economic_transport_codec::record(fields,const_cast<TransportPaymentReceipt&>(v));return w.status();}
Status emit_transport_payment_receipt(ProjectionWriter& w,const StableId& name,const TransportPaymentReceipt& v){return w.object(name,[&](auto&out){return emit_transport_payment_receipt_fields(out,v);});}
Outcome<BoundaryValue> encode_economic_transport_expenses(const EconomicTransportExpenseState& v){try{return {true,economic_transport_codec::encode(v),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<EconomicTransportExpenseState> decode_economic_transport_expenses(const BoundaryValue& v){try{auto out=economic_transport_codec::decode_root<EconomicTransportExpenseState>(v,"/economic_transport_expenses");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_economic_transport_expenses(CodecVisitor& c,EconomicTransportExpenseState& v){economic_transport_codec::Checkpointer fields{c};economic_transport_codec::record(fields,v);return c.status();}
Status emit_economic_transport_expenses(ProjectionWriter& w,const EconomicTransportExpenseState& v){return economic_transport_codec::project(w,"",v);}
}
