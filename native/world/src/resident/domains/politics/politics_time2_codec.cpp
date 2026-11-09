// /2共同原字段序，只有PoliticsFact尾追加必填occurred_at；writer只借读。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "politics_time2.hpp"
#include "politics_codec_fields.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
namespace hanmo::resident {namespace {
using namespace organization_lifecycle_fields;
template<class C,class T>Status value(C&,const StableId&,T&);
template<class C>struct Archive {C& c;template<class T>void operator()(const char* name,T& v){value(c,name,v);}};
template<class C,class T>Status record(C& c,T& v){Archive<C> a{c};dynamic_economy_codec::record(a,v);if constexpr(std::is_same_v<T,PoliticsFact>){if(c.reading())v.occurred_at.emplace();if(!v.occurred_at)return c.reject("PoliticalTime2MomentRequired");c.object("occurred_at",[&](auto& x){return exact_fields::moment(x,*v.occurred_at);});if(c.status().ok&&v.tick!=v.occurred_at->half_day_tick)return c.reject("PoliticalTime2TickMismatch");}return c.status();}
template<class C,class T>Status value(C& c,const StableId& name,T& v){if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,v);else if constexpr(std::is_enum_v<T>){StableId tag;if(!c.reading())for(const auto& x:dynamic_economy_codec::EnumNames<T>::rows())if(x.first==v)tag=x.second;c.field(name,tag);if(!c.status().ok)return c.status();for(const auto& x:dynamic_economy_codec::EnumNames<T>::rows())if(x.second==tag){if(c.reading())v=x.first;return c.status();}return c.reject("InvalidPoliticalTime2Enum");}else if constexpr(dynamic_economy_codec::IsVector<T>::value)return c.sequence(name,v,[](auto& a,auto& x){return value(a,"value",x);});else if constexpr(dynamic_economy_codec::IsTable<T>::value)return table(c,name,v,[](auto& a,auto& x){return record(a,x);});else return c.object(name,[&](auto& a){return record(a,v);});}
}
Status visit_politics2(CodecVisitor& c,PoliticsState& state){return record(c,state);}
Status emit_politics2_projection(ProjectionWriter& writer,std::string_view name,const PoliticsState& state){return emit_projection_record<ActorFoundationProjectionArchive>(writer,name,state,[](auto& c,auto& s){return record(c,s);});}
Outcome<BoundaryValue> encode_politics2(const PoliticsState& state){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_politics2_projection(writer,"",state);if(status.ok)status=writer.finish();return {status.ok,std::move(out),status.error};}
Outcome<PoliticsState> decode_politics2(const BoundaryValue& value){PoliticsState out;ActorFoundationJsonVisitor reader(value);auto status=record(reader,out);if(status.ok)status=reader.finish();return {status.ok,std::move(out),status.error};}
}
