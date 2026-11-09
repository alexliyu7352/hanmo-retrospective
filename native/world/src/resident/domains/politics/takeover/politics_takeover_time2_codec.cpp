// /2与/1复用全部原record字段；仅两个原source尾加必填Moment。三输出不各建字段表。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "politics_takeover_time2.hpp"
#include "politics_takeover_codec_fields.hpp"
#include "politics_takeover_payload_guard.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_projection.hpp"
namespace hanmo::resident {namespace takeover_time2_codec {
using namespace dynamic_economy_codec;
// 原world_id字段仍为32位小写hex；只在reader分支修改源对象。
template<class A>void identity(A& a,WorldIdentity& v){
 StableId hex;if(!a.reading()){static constexpr char d[]="0123456789abcdef";for(auto b:v.world_id){hex.push_back(d[b>>4]);hex.push_back(d[b&15]);}}
 a("world_id",hex);if(hex.size()!=32)boundary::fail("InvalidTakeoverWorldId",{});
 auto nibble=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
 for(std::size_t i=0;i<16;++i){auto h=nibble(hex[2*i]),l=nibble(hex[2*i+1]);if(h<0||l<0)boundary::fail("InvalidTakeoverWorldId",{});if(a.reading())v.world_id[i]=static_cast<std::uint8_t>(h*16+l);}
 a("execution_epoch",v.execution_epoch);a("revision",v.revision);a("tick",v.tick);
}
template<class A,class T>void record2(A& a,T& value){
 if constexpr(std::is_same_v<T,WorldIdentity>)identity(a,value);else dynamic_economy_codec::record(a,value);
 if constexpr(std::is_same_v<T,PoliticalTakeoverRegistration>)a.moment("registered_at",value.registered_at,value.tick);
 if constexpr(std::is_same_v<T,PoliticalTakeoverApprovalRecord>)a.moment("approved_at",value.approved_at,value.tick);
 if constexpr(std::is_same_v<T,PoliticalTakeoverState>||std::is_same_v<T,PoliticalTakeoverCatalog>)if(value.version!=2)boundary::fail("UnsupportedPoliticalTakeoverVersion",{});
 if constexpr(std::is_same_v<T,PoliticalTakeoverCatalog>){a("task_execution_version",value.task_execution_version);if((value.task_execution_version!=1&&value.task_execution_version!=2)||(!value.enabled&&value.task_execution_version!=1))boundary::fail("UnsupportedPoliticalTakeoverVersion",{});}
}
// JSON恢复直接借原输入；Owner仍用共同原decoder保所属FieldOrder。
template<class T>void decode(const BoundaryValue&,T&,const StableId&);
struct JsonRead {
 const BoundaryValue& input;StableId path;std::vector<std::string_view> used;
 bool reading()const{return true;}
 template<class T>void operator()(const char* name,T& value){used.push_back(name);takeover_time2_codec::decode(boundary::field(input,name,path),value,path+"/"+name);}
 void moment(const char* name,std::optional<WorldMoment>& value,Integer tick){used.push_back(name);WorldMoment actual;ActorFoundationJsonVisitor visitor(boundary::field(input,name,path));auto valid=exact_fields::moment(visitor,actual);if(valid.ok)valid=visitor.finish();if(!valid.ok)throw boundary::DecodeFailure{valid.error};if(actual.half_day_tick!=tick)boundary::fail("PoliticalTakeoverTickProjectionMismatch",path);value=std::move(actual);}
 void finish(){const auto& rows=boundary::object(input,path);if(rows.size()!=used.size())boundary::fail("UnsupportedResidentField",path);for(std::size_t i=0;i<rows.size();++i){if(std::find(used.begin(),used.end(),rows[i].first)==used.end())boundary::fail("UnsupportedResidentField",path+"/"+rows[i].first);for(std::size_t j=0;j<i;++j)if(rows[j].first==rows[i].first)boundary::fail("DuplicateResidentField",path+"/"+rows[i].first);}}
};
template<class T>void decode(const BoundaryValue& input,T& value,const StableId& path){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_same_v<T,Owner>||std::is_enum_v<T>)dynamic_economy_codec::decode(input,value,path);
 else if constexpr(IsVector<T>::value){value.clear();for(const auto& row:boundary::array(input,path)){typename IsVector<T>::Value item{};takeover_time2_codec::decode(row,item,path);value.push_back(std::move(item));}}
 else if constexpr(IsOptional<T>::value){if(input.as<std::monostate>())value.reset();else{typename IsOptional<T>::Value item{};takeover_time2_codec::decode(input,item,path);value=std::move(item);}}
 else if constexpr(IsTable<T>::value){value={};for(const auto& row:boundary::object(input,path)){typename IsTable<T>::Value item{};takeover_time2_codec::decode(row.second,item,path+"/"+row.first);if(!value.insert(row.first,std::move(item)))boundary::fail("DuplicateDynamicRecord",path+"/"+row.first);}}
 else{JsonRead a{input,path,{}};record2(a,value);a.finish();}
}
// 无树投影沿同一record2；Owner与旧字段顺序由共同emit原样处理。
template<class T>Status emit(ProjectionWriter&,const StableId&,const T&);
struct Projection {
 ProjectionWriter& writer;bool reading()const{return false;}
 template<class T>void operator()(const char* name,T& value){emit(writer,name,value);}
 void moment(const char* name,std::optional<WorldMoment>& value,Integer tick){if(!value){writer.reject("PoliticalTakeoverMomentRequired",name);return;}if(value->half_day_tick!=tick){writer.reject("PoliticalTakeoverTickProjectionMismatch",name);return;}emit_projection_record<ActorFoundationProjectionArchive>(writer,name,*value,[](auto& c,auto& actual){return exact_fields::moment(c,actual);});}
};
template<class T>Status emit(ProjectionWriter& writer,const StableId& name,const T& value){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return writer.field(name,value);
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(writer,name,value);
 else if constexpr(std::is_enum_v<T>){for(const auto& row:EnumNames<T>::rows())if(row.first==value)return writer.field(name,row.second);return writer.reject("InvalidDynamicEnum",name);}
 else if constexpr(IsVector<T>::value)return writer.sequence(name,value,[](auto& out,const auto& item){return emit(out,"",item);});
 else if constexpr(IsOptional<T>::value)return value?emit(writer,name,*value):writer.null(name);
 else if constexpr(IsTable<T>::value)return writer.object(name,[&](auto& out){for(const auto& row:value.rows()){emit(out,row.first,row.second);if(!out.status().ok)break;}return out.status();});
 else return writer.object(name,[&](auto& out){Projection a{out};record2(a,const_cast<T&>(value));return out.status();});
}
// 二进制保全部原前缀，尾Moment不带optional位；仅reading可emplace。
template<class T>Status visit(CodecVisitor&,const StableId&,T&);
struct Binary {
 CodecVisitor& codec;bool reading()const{return codec.reading();}
 template<class T>void operator()(const char* name,T& value){visit(codec,name,value);}
 void moment(const char* name,std::optional<WorldMoment>& value,Integer tick){if(codec.reading())value.emplace();if(!value){codec.reject("PoliticalTakeoverMomentRequired",name);return;}codec.object(name,[&](auto& child){return exact_fields::moment(child,*value);});if(codec.status().ok&&value->half_day_tick!=tick)codec.reject("PoliticalTakeoverTickProjectionMismatch",name);}
};
template<class T>Status visit(CodecVisitor& c,const StableId& name,T& value){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,value);
 else if constexpr(std::is_same_v<T,Owner>)return c.object(name,[&](auto& child){return common_checkpoint::owner(child,value);});
 else if constexpr(std::is_enum_v<T>){StableId tag;if(!c.reading())for(const auto& row:EnumNames<T>::rows())if(row.first==value)tag=row.second;c.field(name,tag);if(!c.status().ok)return c.status();for(const auto& row:EnumNames<T>::rows())if(row.second==tag){if(c.reading())value=row.first;return c.status();}return c.reject("InvalidDynamicEnum",name);}
 else if constexpr(IsVector<T>::value)return c.sequence(name,value,[](auto& child,auto& row){return visit(child,"item",row);});
 else if constexpr(IsOptional<T>::value)return c.optional(name,value,[](auto& child,auto& row){return visit(child,"value",row);});
 else if constexpr(IsTable<T>::value)return common_checkpoint::table(c,name,value,[](auto& child,auto& row){return visit(child,"record",row);});
 else return c.object(name,[&](auto& child){Binary a{child};record2(a,value);return child.status();});
}
}
Status visit_political_takeover_checkpoint2(CodecVisitor& c,PoliticalTakeoverState& state){try{return takeover_time2_codec::visit(c,"political_takeover",state);}catch(const boundary::DecodeFailure& failure){return c.reject(failure.error.original.code);}}
Status emit_political_takeover_state2_projection(ProjectionWriter& writer,std::string_view name,const PoliticalTakeoverState& state){try{return takeover_time2_codec::emit(writer,StableId{name},state);}catch(const boundary::DecodeFailure& failure){return writer.reject(failure.error.original.code);}}
Outcome<BoundaryValue> encode_political_takeover_state2(const PoliticalTakeoverState& state){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_political_takeover_state2_projection(writer,"",state);if(status.ok)status=writer.finish();return {status.ok,std::move(out),status.error};}
Outcome<PoliticalTakeoverState> decode_political_takeover_state2(const BoundaryValue& input){auto guard=validate_political_takeover_payload(input);if(!guard.ok)return {false,{},guard.error};try{PoliticalTakeoverState out;takeover_time2_codec::decode(input,out,"/political_takeover");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
Outcome<PoliticalTakeoverCatalog> decode_political_takeover_catalog2(const BoundaryValue& input){auto guard=validate_political_takeover_payload(input);if(!guard.ok)return {false,{},guard.error};try{PoliticalTakeoverCatalog out;takeover_time2_codec::decode(input,out,"/political_takeover");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
Outcome<BoundaryValue> encode_political_takeover_catalog2(const PoliticalTakeoverCatalog& catalog){BoundaryValue out;BoundaryProjectionWriter writer(out);try{auto status=takeover_time2_codec::emit(writer,"",catalog);if(status.ok)status=writer.finish();return {status.ok,std::move(out),status.error};}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
}
