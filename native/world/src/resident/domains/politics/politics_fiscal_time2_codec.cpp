// /2唯一字段路径：复用原财政record前缀，只在每政策行尾追加必填真Moment。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "politics_fiscal_time2.hpp"
#include "politics_fiscal_codec_fields.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
namespace hanmo::resident {namespace fiscal_time2_codec {
using namespace organization_lifecycle_fields;
template<class C,class T>Status value(C&,const StableId&,T&);
template<class C>struct Archive {C& c;template<class T>void operator()(const char* name,T& v){value(c,name,v);}};
template<class C,class T>Status record(C& c,T& v){
 if constexpr(std::is_same_v<T,PoliticalFiscalState>||std::is_same_v<T,PoliticalFiscalCatalog>){if(c.reading())v.version=2;else if(v.version!=2)return c.reject("PoliticalFiscalTime2VersionRequired");}
 Archive<C> fields{c};dynamic_economy_codec::record(fields,v);
 if constexpr(std::is_same_v<T,FiscalPolicyRecord>){
  if(c.reading())v.issued_at.emplace();if(!v.issued_at)return c.reject("FiscalPolicyActualMomentRequired",v.id);
  c.object("issued_at",[&](auto& child){return exact_fields::moment(child,*v.issued_at);});
  if(c.status().ok&&v.snapshot.issued_tick!=v.issued_at->half_day_tick)return c.reject("FiscalPolicyTickProjectionMismatch",v.id);
 }
 return c.status();
}
template<class C,class T>Status value(C& c,const StableId& name,T& v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,v);
 else if constexpr(std::is_enum_v<T>){StableId tag;if(!c.reading())for(const auto& x:dynamic_economy_codec::EnumNames<T>::rows())if(x.first==v)tag=x.second;c.field(name,tag);if(!c.status().ok)return c.status();for(const auto& x:dynamic_economy_codec::EnumNames<T>::rows())if(x.second==tag){if(c.reading())v=x.first;return c.status();}return c.reject("InvalidFiscalTime2Enum",name);}
 else if constexpr(dynamic_economy_codec::IsTable<T>::value){if constexpr(std::is_same_v<C,CodecVisitor>)return common_checkpoint::table(c,name,v,[](auto& child,auto& row){return child.object("record",[&](auto& fields){return record(fields,row);});});else return table(c,name,v,[](auto& child,auto& row){return record(child,row);});}
 else return c.object(name,[&](auto& child){return record(child,v);});
}
}
Status visit_political_fiscal_checkpoint2(CodecVisitor& c,PoliticalFiscalState& state){return c.object("political_fiscal",[&](auto& child){return fiscal_time2_codec::record(child,state);});}
Status emit_political_fiscal_state2_projection(ProjectionWriter& writer,std::string_view name,const PoliticalFiscalState& state){return emit_projection_record<ActorFoundationProjectionArchive>(writer,name,state,[](auto& c,auto& s){return fiscal_time2_codec::record(c,s);});}
Outcome<BoundaryValue> encode_political_fiscal_state2(const PoliticalFiscalState& state){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_political_fiscal_state2_projection(writer,"",state);if(status.ok)status=writer.finish();return {status.ok,std::move(out),status.error};}
Outcome<PoliticalFiscalState> decode_political_fiscal_state2(const BoundaryValue& value){PoliticalFiscalState out;ActorFoundationJsonVisitor reader(value);auto status=fiscal_time2_codec::record(reader,out);if(status.ok)status=reader.finish();return {status.ok,std::move(out),status.error};}
Outcome<PoliticalFiscalCatalog> decode_political_fiscal_catalog2(const BoundaryValue& value){PoliticalFiscalCatalog out;ActorFoundationJsonVisitor reader(value);auto status=fiscal_time2_codec::record(reader,out);if(status.ok)status=reader.finish();return {status.ok,std::move(out),status.error};}
// 所选目录沿同一原字段表借读发射；外层shape不构造目录值树。
Status emit_political_fiscal_catalog2_projection(ProjectionWriter& writer,std::string_view name,const PoliticalFiscalCatalog& catalog){return emit_projection_record<ActorFoundationProjectionArchive>(writer,name,catalog,[](auto& c,auto& v){return fiscal_time2_codec::record(c,v);});}
Outcome<BoundaryValue> encode_political_fiscal_catalog2(const PoliticalFiscalCatalog& catalog){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_political_fiscal_catalog2_projection(writer,"",catalog);if(status.ok)status=writer.finish();return {status.ok,std::move(out),status.error};}
}
