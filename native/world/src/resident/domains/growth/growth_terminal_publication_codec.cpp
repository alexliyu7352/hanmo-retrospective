#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "growth_terminal_publication.hpp"
#include <type_traits>
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
namespace {
template<class C>Status moment(C& c,WorldMoment& value){if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)(void)0;EncodedWorldMoment wire;if(!c.reading()){auto v=encode_world_moment(value);if(!v.ok)return {false,{},v.error};wire=std::move(v.value);}c.field("half_day_tick",wire.half_day_tick);c.object("sub_half_day",[&](auto& x){x.field("numerator",wire.sub_half_day.numerator);return x.field("denominator",wire.sub_half_day.denominator);});if(c.reading()&&c.status().ok){auto v=decode_world_moment(wire);if(!v.ok)return {false,{},v.error};value=std::move(v.value);}return c.status();}
// 原字段布局不变；schema2只允许同一LifeActivity结果地址，不放宽其他source。
bool terminal_source_shape(const GrowthTerminalPublication& value){
 const auto& source=value.terminal_source_ref;
 if(source.domain!="growth"||source.id.empty())return false;
 if(value.terminal_source_schema==1)return source.kind=="growth_fixed_service_boundary";
 return value.terminal_source_schema==2&&source.kind=="life_activity"&&
     source.id==value.action_id&&value.original_result_ref.kind==LifeFixedOutcomeRef::Kind::life_activity;
}
template<class C>Status row(C& c,GrowthTerminalPublication& value){c.field("publication_id",value.publication_id);c.field("action_id",value.action_id);c.object("original_result_ref",[&](auto& x){auto& r=value.original_result_ref;Integer kind=static_cast<Integer>(r.kind);x.field("kind",kind);if(kind<0||kind>3)return x.reject("UnsupportedGrowthTerminalOutcome");if(x.reading())r.kind=static_cast<LifeFixedOutcomeRef::Kind>(kind);x.field("id",r.id);x.field("period_ordinal",r.period_ordinal);return x.field("secondary_ref",r.secondary_ref);});c.object("terminal_source_ref",[&](auto& x){x.field("domain",value.terminal_source_ref.domain);x.field("kind",value.terminal_source_ref.kind);return x.field("id",value.terminal_source_ref.id);});c.field("terminal_source_schema",value.terminal_source_schema);c.object("occurred_at",[&](auto& x){return moment(x,value.occurred_at);});if(c.status().ok&&(value.publication_id.empty()||value.action_id.empty()||value.original_result_ref.id!=value.action_id||value.original_result_ref.period_ordinal||!value.original_result_ref.secondary_ref.empty()||!terminal_source_shape(value)))return c.reject("InvalidGrowthTerminalPublication");return c.status();}
template<class C>Status table(C& c,OrderedTable<GrowthTerminalPublication>& values){return c.table("publications",values,[](auto& x,auto& value){return row(x,value);});}
// CodecVisitor沿原ordered table的key/value顺序；JSON沿相同callback输出字典。
Status table(CodecVisitor& c,OrderedTable<GrowthTerminalPublication>& values){return common_checkpoint::table(c,"publications",values,[](auto& x,auto& value){return row(x,value);});}
}
Status visit_growth_terminal_publications_v1(CodecVisitor& c,OrderedTable<GrowthTerminalPublication>& values){return table(c,values);}
Status emit_growth_terminal_publications_v1_projection(ProjectionWriter& sink,std::string_view name,const OrderedTable<GrowthTerminalPublication>& values){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,values,[](auto& c,auto& value){return table(c,value);});}
Outcome<BoundaryValue> encode_growth_terminal_publications_v1(const OrderedTable<GrowthTerminalPublication>& values){BoundaryValue out;BoundaryProjectionWriter sink(out);auto result=emit_growth_terminal_publications_v1_projection(sink,"",values);if(result.ok)result=sink.finish();return {result.ok,std::move(out),result.error};}
Outcome<OrderedTable<GrowthTerminalPublication>> decode_growth_terminal_publications_v1(const BoundaryValue& input){OrderedTable<GrowthTerminalPublication> out;ActorFoundationJsonVisitor c(input);auto result=table(c,out);if(result.ok)result=c.finish();return {result.ok,std::move(out),result.error};}
}
