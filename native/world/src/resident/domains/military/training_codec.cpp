#include "resident/domains/military/military_model.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include <array>
#include "resident/checkpoint/projection_writer.hpp"
// 题面/课时存在位与原字段顺序只在此保存，热规则不能访问JSON来源或重抽seed。
#include "training_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/checkpoint/canonical_digest.hpp"
namespace hanmo::resident::training_codec {

namespace {
template<class T> Status emit_value(ProjectionWriter&,const StableId&,const T&);
template<class T,class Alloc> Status emit_value(ProjectionWriter&,const StableId&,const std::vector<T,Alloc>&);
template<class T,class Alloc> Status emit_value(ProjectionWriter&,const StableId&,const std::vector<std::pair<StableId,T>,Alloc>&);
template<class T> Status emit_value(ProjectionWriter& w,const StableId& key,const T& value){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(key,value);
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,key,value);
 else if constexpr(std::is_same_v<T,ReservationRef>)return emit_common_reservation(w,key,value,false,false);
 else if constexpr(std::is_same_v<T,Location>)return emit_common_location(w,key,value);
 else if constexpr(std::is_same_v<T,InteractionContext>)return w.object(key,[&](auto& child){return emit_interaction_context_fields(child,value);});
 else if constexpr(std::is_same_v<T,HexPoint>){const std::array<Integer,2> xy{value.q,value.r};return w.sequence(key,xy,[](auto& child,Integer n){return child.field("",n);});}
 else return w.object(key,[&](auto& child){return emit_fields(child,value);});
}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T,Alloc>& rows){return w.sequence(key,rows,[](auto& child,const auto& row){return emit_value(child,"",row);});}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<std::pair<StableId,T>,Alloc>& rows){return w.object(key,[&](auto& child){for(const auto& row:rows){auto result=emit_value(child,row.first,row.second);if(!result.ok)return result;}return child.status();});}
template<class Fn>void append_projected_fields(BoundaryValue::Dict& target,Fn emit){BoundaryValue result;BoundaryProjectionWriter sink(result);auto checked=sink.object("",emit);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);auto* rows=result.as<BoundaryValue::Dict>();for(auto& row:*rows)target.push_back(std::move(row));}
template<class T>BoundaryValue encode_projected(const T& value){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=emit_value(sink,"",value);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
}
using V=BoundaryValue;using D=V::Dict;using A=V::Array;using domain_codec::order;using domain_codec::ordered;using domain_codec::visit_order;
V encode(const StableId& x){return V{x};}V encode(Integer x){return V{x};}V encode(bool x){return V{x};}
void decode(const V& v,StableId& x,const StableId& p){x=boundary::string(v,p);}void decode(const V& v,Integer& x,const StableId& p){x=boundary::integer(v,p);}void decode(const V& v,bool& x,const StableId& p){x=boundary::boolean(v,p);}
Status visit(CodecVisitor& c,StableId& x){return c.field("value",x);}Status visit(CodecVisitor& c,Integer& x){return c.field("value",x);}Status visit(CodecVisitor& c,bool& x){return c.field("value",x);}
V encode(const HexPoint& p){return V{A{V{p.q},V{p.r}}};}void decode(const V& v,HexPoint& x,const StableId& p){auto& a=boundary::array(v,p);if(a.size()!=2)boundary::fail("InvalidDrillTarget",p);x={boundary::integer(a[0],p),boundary::integer(a[1],p)};}Status visit(CodecVisitor& c,HexPoint& p){c.field("q",p.q);return c.field("r",p.r);}
template<class T>V encode(const std::vector<T>& rows){A a;for(auto& r:rows)a.push_back(encode(r));return V{std::move(a)};}
template<class T>void decode(const V& v,std::vector<T>& out,const StableId& p){domain_codec::dec_list(v,out,p,[](const V& input,T& row,const StableId& path){decode(input,row,path);});}
template<class T>Status visit(CodecVisitor& c,std::vector<T>& rows){return c.sequence("rows",rows,[](auto& c,auto& x){return visit(c,x);});}
// DrillPending固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,DrillPending& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"group_id","target"},{},p);
decode(boundary::field(v,"group_id",p),r.group_id,checkpoint_decode_boundary_join({p,"/group_id"}));
decode(boundary::field(v,"target",p),r.target,checkpoint_decode_boundary_join({p,"/target"}));
}
Status emit_fields(ProjectionWriter& w,const DrillPending& r){
emit_value(w,"group_id",r.group_id);
emit_value(w,"target",r.target);
w.order_fields(r.field_order);return w.status();}
V encode(const DrillPending& r){return encode_projected(r);}
Status visit(CodecVisitor& c,DrillPending& r){
c.object("group_id",[&](auto& c){return visit(c,r.group_id);});
c.object("target",[&](auto& c){return visit(c,r.target);});
visit_order(c,r.field_order);return c.status();}
// DrillMove固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,DrillMove& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"group_id","from","target"},{},p);
decode(boundary::field(v,"group_id",p),r.group_id,checkpoint_decode_boundary_join({p,"/group_id"}));
decode(boundary::field(v,"from",p),r.from,checkpoint_decode_boundary_join({p,"/from"}));
decode(boundary::field(v,"target",p),r.target,checkpoint_decode_boundary_join({p,"/target"}));
}
Status emit_fields(ProjectionWriter& w,const DrillMove& r){
emit_value(w,"group_id",r.group_id);
emit_value(w,"from",r.from);
emit_value(w,"target",r.target);
w.order_fields(r.field_order);return w.status();}
V encode(const DrillMove& r){return encode_projected(r);}
Status visit(CodecVisitor& c,DrillMove& r){
c.object("group_id",[&](auto& c){return visit(c,r.group_id);});
c.object("from",[&](auto& c){return visit(c,r.from);});
c.object("target",[&](auto& c){return visit(c,r.target);});
visit_order(c,r.field_order);return c.status();}
// DrillSnapshot固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,DrillSnapshot& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"skill_level","auxiliary_attribute"},{},p);
decode(boundary::field(v,"skill_level",p),r.skill_level,checkpoint_decode_boundary_join({p,"/skill_level"}));
decode(boundary::field(v,"auxiliary_attribute",p),r.auxiliary_attribute,checkpoint_decode_boundary_join({p,"/auxiliary_attribute"}));
}
Status emit_fields(ProjectionWriter& w,const DrillSnapshot& r){
emit_value(w,"skill_level",r.skill_level);
emit_value(w,"auxiliary_attribute",r.auxiliary_attribute);
w.order_fields(r.field_order);return w.status();}
V encode(const DrillSnapshot& r){return encode_projected(r);}
Status visit(CodecVisitor& c,DrillSnapshot& r){
c.object("skill_level",[&](auto& c){return visit(c,r.skill_level);});
c.object("auxiliary_attribute",[&](auto& c){return visit(c,r.auxiliary_attribute);});
visit_order(c,r.field_order);return c.status();}
// DrillObjectives固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,DrillObjectives& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"supply","passage","screen","cavalry"},{},p);
decode(boundary::field(v,"supply",p),r.supply,checkpoint_decode_boundary_join({p,"/supply"}));
decode(boundary::field(v,"passage",p),r.passage,checkpoint_decode_boundary_join({p,"/passage"}));
decode(boundary::field(v,"screen",p),r.screen,checkpoint_decode_boundary_join({p,"/screen"}));
decode(boundary::field(v,"cavalry",p),r.cavalry,checkpoint_decode_boundary_join({p,"/cavalry"}));
}
Status emit_fields(ProjectionWriter& w,const DrillObjectives& r){
emit_value(w,"supply",r.supply);
emit_value(w,"passage",r.passage);
emit_value(w,"screen",r.screen);
emit_value(w,"cavalry",r.cavalry);
w.order_fields(r.field_order);return w.status();}
V encode(const DrillObjectives& r){return encode_projected(r);}
Status visit(CodecVisitor& c,DrillObjectives& r){
c.object("supply",[&](auto& c){return visit(c,r.supply);});
c.object("passage",[&](auto& c){return visit(c,r.passage);});
c.object("screen",[&](auto& c){return visit(c,r.screen);});
c.object("cavalry",[&](auto& c){return visit(c,r.cavalry);});
visit_order(c,r.field_order);return c.status();}
// DrillResult固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,DrillResult& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"completion","score","raw_score","auto_score","performance_bp","objectives","choices"},{},p);
decode(boundary::field(v,"completion",p),r.completion,checkpoint_decode_boundary_join({p,"/completion"}));
decode(boundary::field(v,"score",p),r.score,checkpoint_decode_boundary_join({p,"/score"}));
if(!boundary::field(v,"raw_score",p).as<std::monostate>()){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.raw_score)>::value_type>();r.raw_score.emplace();decode(boundary::field(v,"raw_score",p),*r.raw_score,checkpoint_decode_boundary_join({p,"/raw_score"}));}
decode(boundary::field(v,"auto_score",p),r.auto_score,checkpoint_decode_boundary_join({p,"/auto_score"}));
decode(boundary::field(v,"performance_bp",p),r.performance_bp,checkpoint_decode_boundary_join({p,"/performance_bp"}));
if(!boundary::field(v,"objectives",p).as<std::monostate>()){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.objectives)>::value_type>();r.objectives.emplace();decode(boundary::field(v,"objectives",p),*r.objectives,checkpoint_decode_boundary_join({p,"/objectives"}));}
decode(boundary::field(v,"choices",p),r.choices,checkpoint_decode_boundary_join({p,"/choices"}));
}
Status emit_fields(ProjectionWriter& w,const DrillResult& r){
emit_value(w,"completion",r.completion);
emit_value(w,"score",r.score);
if(r.raw_score)emit_value(w,"raw_score",*r.raw_score);else w.null("raw_score");
emit_value(w,"auto_score",r.auto_score);
emit_value(w,"performance_bp",r.performance_bp);
if(r.objectives)emit_value(w,"objectives",*r.objectives);else w.null("objectives");
emit_value(w,"choices",r.choices);
w.order_fields(r.field_order);return w.status();}
V encode(const DrillResult& r){return encode_projected(r);}
Status visit(CodecVisitor& c,DrillResult& r){
c.object("completion",[&](auto& c){return visit(c,r.completion);});
c.object("score",[&](auto& c){return visit(c,r.score);});
c.optional("raw_score",r.raw_score,[](auto& c,auto& x){return visit(c,x);});
c.object("auto_score",[&](auto& c){return visit(c,r.auto_score);});
c.object("performance_bp",[&](auto& c){return visit(c,r.performance_bp);});
c.optional("objectives",r.objectives,[](auto& c,auto& x){return visit(c,x);});
c.object("choices",[&](auto& c){return visit(c,r.choices);});
visit_order(c,r.field_order);return c.status();}
// TrainingGroupSource固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,TrainingGroupSource& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","unit_id","troop_type_id","roster_sha256","group_ids"},{},p);
decode(boundary::field(v,"kind",p),r.kind,checkpoint_decode_boundary_join({p,"/kind"}));
decode(boundary::field(v,"unit_id",p),r.unit_id,checkpoint_decode_boundary_join({p,"/unit_id"}));
decode(boundary::field(v,"troop_type_id",p),r.troop_type_id,checkpoint_decode_boundary_join({p,"/troop_type_id"}));
decode(boundary::field(v,"roster_sha256",p),r.roster_sha256,checkpoint_decode_boundary_join({p,"/roster_sha256"}));
decode(boundary::field(v,"group_ids",p),r.group_ids,checkpoint_decode_boundary_join({p,"/group_ids"}));
}
Status emit_fields(ProjectionWriter& w,const TrainingGroupSource& r){
emit_value(w,"kind",r.kind);
emit_value(w,"unit_id",r.unit_id);
emit_value(w,"troop_type_id",r.troop_type_id);
emit_value(w,"roster_sha256",r.roster_sha256);
emit_value(w,"group_ids",r.group_ids);
w.order_fields(r.field_order);return w.status();}
V encode(const TrainingGroupSource& r){return encode_projected(r);}
Status visit(CodecVisitor& c,TrainingGroupSource& r){
c.object("kind",[&](auto& c){return visit(c,r.kind);});
c.object("unit_id",[&](auto& c){return visit(c,r.unit_id);});
c.object("troop_type_id",[&](auto& c){return visit(c,r.troop_type_id);});
c.object("roster_sha256",[&](auto& c){return visit(c,r.roster_sha256);});
c.object("group_ids",[&](auto& c){return visit(c,r.group_ids);});
visit_order(c,r.field_order);return c.status();}
// TrainingDrillTerms固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,TrainingDrillTerms& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","challenge_id","mode","tactics_xp","actor_snapshot","source_sha256","group_source"},{},p);
decode(boundary::field(v,"kind",p),r.kind,checkpoint_decode_boundary_join({p,"/kind"}));
decode(boundary::field(v,"challenge_id",p),r.challenge_id,checkpoint_decode_boundary_join({p,"/challenge_id"}));
decode(boundary::field(v,"mode",p),r.mode,checkpoint_decode_boundary_join({p,"/mode"}));
decode(boundary::field(v,"tactics_xp",p),r.tactics_xp,checkpoint_decode_boundary_join({p,"/tactics_xp"}));
decode(boundary::field(v,"actor_snapshot",p),r.actor_snapshot,checkpoint_decode_boundary_join({p,"/actor_snapshot"}));
decode(boundary::field(v,"source_sha256",p),r.source_sha256,checkpoint_decode_boundary_join({p,"/source_sha256"}));
decode(boundary::field(v,"group_source",p),r.group_source,checkpoint_decode_boundary_join({p,"/group_source"}));
}
Status emit_fields(ProjectionWriter& w,const TrainingDrillTerms& r){
emit_value(w,"kind",r.kind);
emit_value(w,"challenge_id",r.challenge_id);
emit_value(w,"mode",r.mode);
emit_value(w,"tactics_xp",r.tactics_xp);
emit_value(w,"actor_snapshot",r.actor_snapshot);
emit_value(w,"source_sha256",r.source_sha256);
emit_value(w,"group_source",r.group_source);
w.order_fields(r.field_order);return w.status();}
V encode(const TrainingDrillTerms& r){return encode_projected(r);}
Status visit(CodecVisitor& c,TrainingDrillTerms& r){
c.object("kind",[&](auto& c){return visit(c,r.kind);});
c.object("challenge_id",[&](auto& c){return visit(c,r.challenge_id);});
c.object("mode",[&](auto& c){return visit(c,r.mode);});
c.object("tactics_xp",[&](auto& c){return visit(c,r.tactics_xp);});
c.object("actor_snapshot",[&](auto& c){return visit(c,r.actor_snapshot);});
c.object("source_sha256",[&](auto& c){return visit(c,r.source_sha256);});
c.object("group_source",[&](auto& c){return visit(c,r.group_source);});
visit_order(c,r.field_order);return c.status();}
// MilitaryTrainingTerms固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,MilitaryTrainingTerms& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"actor_id","office_id","authority_source","unit_id","garrison_id","organization_id","ownership_id","place_id","commander_actor_id","troop_type_id","fit_ids","participants","skill_id","skill_xp","skill_level","gain_total","training_before","fatigue_before","fatigue_per_tick","payer_wallet_id","recipient_wallet_id","duration","fee"},{"drill"},p);
decode(boundary::field(v,"actor_id",p),r.actor_id,checkpoint_decode_boundary_join({p,"/actor_id"}));
decode(boundary::field(v,"office_id",p),r.office_id,checkpoint_decode_boundary_join({p,"/office_id"}));
decode(boundary::field(v,"authority_source",p),r.authority_source,checkpoint_decode_boundary_join({p,"/authority_source"}));
decode(boundary::field(v,"unit_id",p),r.unit_id,checkpoint_decode_boundary_join({p,"/unit_id"}));
decode(boundary::field(v,"garrison_id",p),r.garrison_id,checkpoint_decode_boundary_join({p,"/garrison_id"}));
decode(boundary::field(v,"organization_id",p),r.organization_id,checkpoint_decode_boundary_join({p,"/organization_id"}));
decode(boundary::field(v,"ownership_id",p),r.ownership_id,checkpoint_decode_boundary_join({p,"/ownership_id"}));
decode(boundary::field(v,"place_id",p),r.place_id,checkpoint_decode_boundary_join({p,"/place_id"}));
decode(boundary::field(v,"commander_actor_id",p),r.commander_actor_id,checkpoint_decode_boundary_join({p,"/commander_actor_id"}));
decode(boundary::field(v,"troop_type_id",p),r.troop_type_id,checkpoint_decode_boundary_join({p,"/troop_type_id"}));
decode(boundary::field(v,"fit_ids",p),r.fit_ids,checkpoint_decode_boundary_join({p,"/fit_ids"}));
decode(boundary::field(v,"participants",p),r.participants,checkpoint_decode_boundary_join({p,"/participants"}));
decode(boundary::field(v,"skill_id",p),r.skill_id,checkpoint_decode_boundary_join({p,"/skill_id"}));
decode(boundary::field(v,"skill_xp",p),r.skill_xp,checkpoint_decode_boundary_join({p,"/skill_xp"}));
decode(boundary::field(v,"skill_level",p),r.skill_level,checkpoint_decode_boundary_join({p,"/skill_level"}));
decode(boundary::field(v,"gain_total",p),r.gain_total,checkpoint_decode_boundary_join({p,"/gain_total"}));
decode(boundary::field(v,"training_before",p),r.training_before,checkpoint_decode_boundary_join({p,"/training_before"}));
decode(boundary::field(v,"fatigue_before",p),r.fatigue_before,checkpoint_decode_boundary_join({p,"/fatigue_before"}));
decode(boundary::field(v,"fatigue_per_tick",p),r.fatigue_per_tick,checkpoint_decode_boundary_join({p,"/fatigue_per_tick"}));
decode(boundary::field(v,"payer_wallet_id",p),r.payer_wallet_id,checkpoint_decode_boundary_join({p,"/payer_wallet_id"}));
decode(boundary::field(v,"recipient_wallet_id",p),r.recipient_wallet_id,checkpoint_decode_boundary_join({p,"/recipient_wallet_id"}));
decode(boundary::field(v,"duration",p),r.duration,checkpoint_decode_boundary_join({p,"/duration"}));
decode(boundary::field(v,"fee",p),r.fee,checkpoint_decode_boundary_join({p,"/fee"}));
if(auto x=v.find("drill")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.drill)>::value_type>();r.drill.emplace();decode(*x,*r.drill,checkpoint_decode_boundary_join({p,"/drill"}));}
}
Status emit_fields(ProjectionWriter& w,const MilitaryTrainingTerms& r){
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"office_id",r.office_id);
emit_value(w,"authority_source",r.authority_source);
emit_value(w,"unit_id",r.unit_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"organization_id",r.organization_id);
emit_value(w,"ownership_id",r.ownership_id);
emit_value(w,"place_id",r.place_id);
emit_value(w,"commander_actor_id",r.commander_actor_id);
emit_value(w,"troop_type_id",r.troop_type_id);
emit_value(w,"fit_ids",r.fit_ids);
emit_value(w,"participants",r.participants);
emit_value(w,"skill_id",r.skill_id);
emit_value(w,"skill_xp",r.skill_xp);
emit_value(w,"skill_level",r.skill_level);
emit_value(w,"gain_total",r.gain_total);
emit_value(w,"training_before",r.training_before);
emit_value(w,"fatigue_before",r.fatigue_before);
emit_value(w,"fatigue_per_tick",r.fatigue_per_tick);
emit_value(w,"payer_wallet_id",r.payer_wallet_id);
emit_value(w,"recipient_wallet_id",r.recipient_wallet_id);
emit_value(w,"duration",r.duration);
emit_value(w,"fee",r.fee);
if(r.drill)emit_value(w,"drill",*r.drill);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryTrainingTerms& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryTrainingTerms& r){
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("office_id",[&](auto& c){return visit(c,r.office_id);});
c.object("authority_source",[&](auto& c){return visit(c,r.authority_source);});
c.object("unit_id",[&](auto& c){return visit(c,r.unit_id);});
c.object("garrison_id",[&](auto& c){return visit(c,r.garrison_id);});
c.object("organization_id",[&](auto& c){return visit(c,r.organization_id);});
c.object("ownership_id",[&](auto& c){return visit(c,r.ownership_id);});
c.object("place_id",[&](auto& c){return visit(c,r.place_id);});
c.object("commander_actor_id",[&](auto& c){return visit(c,r.commander_actor_id);});
c.object("troop_type_id",[&](auto& c){return visit(c,r.troop_type_id);});
c.object("fit_ids",[&](auto& c){return visit(c,r.fit_ids);});
c.object("participants",[&](auto& c){return visit(c,r.participants);});
c.object("skill_id",[&](auto& c){return visit(c,r.skill_id);});
c.object("skill_xp",[&](auto& c){return visit(c,r.skill_xp);});
c.object("skill_level",[&](auto& c){return visit(c,r.skill_level);});
c.object("gain_total",[&](auto& c){return visit(c,r.gain_total);});
c.object("training_before",[&](auto& c){return visit(c,r.training_before);});
c.object("fatigue_before",[&](auto& c){return visit(c,r.fatigue_before);});
c.object("fatigue_per_tick",[&](auto& c){return visit(c,r.fatigue_per_tick);});
c.object("payer_wallet_id",[&](auto& c){return visit(c,r.payer_wallet_id);});
c.object("recipient_wallet_id",[&](auto& c){return visit(c,r.recipient_wallet_id);});
c.object("duration",[&](auto& c){return visit(c,r.duration);});
c.object("fee",[&](auto& c){return visit(c,r.fee);});
c.optional("drill",r.drill,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);return c.status();}
// MilitaryTrainingLesson固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,MilitaryTrainingLesson& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"tick","supply_fact_id","fee_paid","training_before","training_after","fatigue_before","fatigue_after"},{},p);
decode(boundary::field(v,"tick",p),r.tick,checkpoint_decode_boundary_join({p,"/tick"}));
decode(boundary::field(v,"supply_fact_id",p),r.supply_fact_id,checkpoint_decode_boundary_join({p,"/supply_fact_id"}));
decode(boundary::field(v,"fee_paid",p),r.fee_paid,checkpoint_decode_boundary_join({p,"/fee_paid"}));
decode(boundary::field(v,"training_before",p),r.training_before,checkpoint_decode_boundary_join({p,"/training_before"}));
decode(boundary::field(v,"training_after",p),r.training_after,checkpoint_decode_boundary_join({p,"/training_after"}));
decode(boundary::field(v,"fatigue_before",p),r.fatigue_before,checkpoint_decode_boundary_join({p,"/fatigue_before"}));
decode(boundary::field(v,"fatigue_after",p),r.fatigue_after,checkpoint_decode_boundary_join({p,"/fatigue_after"}));
}
Status emit_fields(ProjectionWriter& w,const MilitaryTrainingLesson& r){
emit_value(w,"tick",r.tick);
emit_value(w,"supply_fact_id",r.supply_fact_id);
emit_value(w,"fee_paid",r.fee_paid);
emit_value(w,"training_before",r.training_before);
emit_value(w,"training_after",r.training_after);
emit_value(w,"fatigue_before",r.fatigue_before);
emit_value(w,"fatigue_after",r.fatigue_after);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryTrainingLesson& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryTrainingLesson& r){
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.object("supply_fact_id",[&](auto& c){return visit(c,r.supply_fact_id);});
c.object("fee_paid",[&](auto& c){return visit(c,r.fee_paid);});
c.object("training_before",[&](auto& c){return visit(c,r.training_before);});
c.object("training_after",[&](auto& c){return visit(c,r.training_after);});
c.object("fatigue_before",[&](auto& c){return visit(c,r.fatigue_before);});
c.object("fatigue_after",[&](auto& c){return visit(c,r.fatigue_after);});
visit_order(c,r.field_order);return c.status();}
// MilitaryTrainingFact固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,MilitaryTrainingFact& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","phase","actor_id","start_tick","completed_tick","success","terms","lessons","code"},{"drill"},p);
if(boundary::get_string(v,"kind",p)!="military_training")boundary::fail("InvalidMilitaryTrainingFact",p);
decode(boundary::field(v,"phase",p),r.phase,checkpoint_decode_boundary_join({p,"/phase"}));
decode(boundary::field(v,"actor_id",p),r.actor_id,checkpoint_decode_boundary_join({p,"/actor_id"}));
decode(boundary::field(v,"start_tick",p),r.start_tick,checkpoint_decode_boundary_join({p,"/start_tick"}));
decode(boundary::field(v,"completed_tick",p),r.completed_tick,checkpoint_decode_boundary_join({p,"/completed_tick"}));
decode(boundary::field(v,"success",p),r.success,checkpoint_decode_boundary_join({p,"/success"}));
decode(boundary::field(v,"terms",p),r.terms,checkpoint_decode_boundary_join({p,"/terms"}));
decode(boundary::field(v,"lessons",p),r.lessons,checkpoint_decode_boundary_join({p,"/lessons"}));
decode(boundary::field(v,"code",p),r.code,checkpoint_decode_boundary_join({p,"/code"}));
if(auto x=v.find("drill")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.drill)>::value_type>();r.drill.emplace();decode(*x,*r.drill,checkpoint_decode_boundary_join({p,"/drill"}));}
}
Status emit_fields(ProjectionWriter& w,const MilitaryTrainingFact& r){
w.field("kind",StableId("military_training"));
emit_value(w,"phase",r.phase);
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"start_tick",r.start_tick);
emit_value(w,"completed_tick",r.completed_tick);
emit_value(w,"success",r.success);
emit_value(w,"terms",r.terms);
emit_value(w,"lessons",r.lessons);
emit_value(w,"code",r.code);
if(r.drill)emit_value(w,"drill",*r.drill);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryTrainingFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryTrainingFact& r){
c.field("id",r.id);
c.object("phase",[&](auto& c){return visit(c,r.phase);});
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("start_tick",[&](auto& c){return visit(c,r.start_tick);});
c.object("completed_tick",[&](auto& c){return visit(c,r.completed_tick);});
c.object("success",[&](auto& c){return visit(c,r.success);});
c.object("terms",[&](auto& c){return visit(c,r.terms);});
c.object("lessons",[&](auto& c){return visit(c,r.lessons);});
c.object("code",[&](auto& c){return visit(c,r.code);});
c.optional("drill",r.drill,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);return c.status();}
// MilitaryTrainingReceipt固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,MilitaryTrainingReceipt& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"unit_id","provided_half_days","fee_paid","training_gain","released_unprovided_fee"},{},p);
decode(boundary::field(v,"unit_id",p),r.unit_id,checkpoint_decode_boundary_join({p,"/unit_id"}));
decode(boundary::field(v,"provided_half_days",p),r.provided_half_days,checkpoint_decode_boundary_join({p,"/provided_half_days"}));
decode(boundary::field(v,"fee_paid",p),r.fee_paid,checkpoint_decode_boundary_join({p,"/fee_paid"}));
decode(boundary::field(v,"training_gain",p),r.training_gain,checkpoint_decode_boundary_join({p,"/training_gain"}));
decode(boundary::field(v,"released_unprovided_fee",p),r.released_unprovided_fee,checkpoint_decode_boundary_join({p,"/released_unprovided_fee"}));
}
Status emit_fields(ProjectionWriter& w,const MilitaryTrainingReceipt& r){w.field("unit_id",r.unit_id);w.field("provided_half_days",r.provided_half_days);w.field("fee_paid",r.fee_paid);w.field("training_gain",r.training_gain);w.field("released_unprovided_fee",r.released_unprovided_fee);w.order_fields(r.field_order);return w.status();}
BoundaryValue encode(const MilitaryTrainingReceipt& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto written=sink.object("",[&](auto& w){return emit_fields(w,r);});if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);auto finished=sink.finish();if(!finished.ok)boundary::fail(finished.error.original.code,finished.error.original.path);return out;}
Status visit(CodecVisitor& c,MilitaryTrainingReceipt& r){
c.object("unit_id",[&](auto& c){return visit(c,r.unit_id);});
c.object("provided_half_days",[&](auto& c){return visit(c,r.provided_half_days);});
c.object("fee_paid",[&](auto& c){return visit(c,r.fee_paid);});
c.object("training_gain",[&](auto& c){return visit(c,r.training_gain);});
c.object("released_unprovided_fee",[&](auto& c){return visit(c,r.released_unprovided_fee);});
visit_order(c,r.field_order);return c.status();}
// MilitaryMarchUnit固定schema与原空值类型，解码失败不能发布半份记录。
void decode(const V& v,MilitaryMarchUnit& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"training","fatigue_before","fatigue_after"},{},p);
decode(boundary::field(v,"training",p),r.training,checkpoint_decode_boundary_join({p,"/training"}));
decode(boundary::field(v,"fatigue_before",p),r.fatigue_before,checkpoint_decode_boundary_join({p,"/fatigue_before"}));
decode(boundary::field(v,"fatigue_after",p),r.fatigue_after,checkpoint_decode_boundary_join({p,"/fatigue_after"}));
}
Status emit_fields(ProjectionWriter& w,const MilitaryMarchUnit& r){
emit_value(w,"training",r.training);
emit_value(w,"fatigue_before",r.fatigue_before);
emit_value(w,"fatigue_after",r.fatigue_after);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryMarchUnit& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryMarchUnit& r){
c.field("unit_id",r.unit_id);
c.object("training",[&](auto& c){return visit(c,r.training);});
c.object("fatigue_before",[&](auto& c){return visit(c,r.fatigue_before);});
c.object("fatigue_after",[&](auto& c){return visit(c,r.fatigue_after);});
visit_order(c,r.field_order);return c.status();}
// 实例binding与随机来源按各自字典顺序保全，pending/null和result/空字典不能互换。
void decode(const V& v,DrillState& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"format","definition_id","source_sha256","binding","actor_snapshot","seed","mode","phase","moves","pending","result"},{},p);
#define GET(n) decode(boundary::field(v,#n,p),r.n,checkpoint_decode_boundary_join({p,"/",#n}))
GET(format);GET(definition_id);GET(source_sha256);GET(actor_snapshot);GET(seed);GET(mode);GET(phase);GET(moves);
#undef GET
auto& b=boundary::field(v,"binding",p);boundary::fields(b,{"actor_id","source_id"},{},p);r.binding_field_order=order(b);r.actor_id=boundary::get_string(b,"actor_id",p);r.source_id=boundary::get_string(b,"source_id",p);auto& pending=boundary::field(v,"pending",p);if(!pending.as<std::monostate>()){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.pending)>::value_type>();r.pending.emplace();decode(pending,*r.pending,p);}auto& result=boundary::field(v,"result",p);if(!boundary::object(result,p).empty()){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.result)>::value_type>();r.result.emplace();decode(result,*r.result,p);}}
Status emit_fields(ProjectionWriter& w,const DrillState& r){w.field("format",r.format);w.field("definition_id",r.definition_id);w.field("source_sha256",r.source_sha256);w.object("binding",r.binding_field_order,[&](auto& child){child.field("actor_id",r.actor_id);return child.field("source_id",r.source_id);});emit_value(w,"actor_snapshot",r.actor_snapshot);w.field("seed",r.seed);w.field("mode",r.mode);w.field("phase",r.phase);emit_value(w,"moves",r.moves);if(r.pending)emit_value(w,"pending",*r.pending);else w.null("pending");if(r.result)emit_value(w,"result",*r.result);else w.object("result",[](auto& child){return child.status();});w.order_fields(r.field_order);return w.status();}
V encode(const DrillState& r){return encode_projected(r);}
Status visit(CodecVisitor& c,DrillState& r){
#define VISIT(n) c.object(#n,[&](auto& c){return visit(c,r.n);})
VISIT(format);VISIT(definition_id);VISIT(source_sha256);VISIT(actor_id);VISIT(source_id);VISIT(binding_field_order);VISIT(actor_snapshot);VISIT(seed);VISIT(mode);VISIT(phase);VISIT(moves);
#undef VISIT
c.optional("pending",r.pending,[](auto& c,auto& x){return visit(c,x);});c.optional("result",r.result,[](auto& c,auto& x){return visit(c,x);});visit_order(c,r.field_order);return c.status();}
void decode(const V& v,TrainingDrillInstance& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"seed_origin","challenge","phase","frozen_tick","performance_bp"},{},p);auto& o=boundary::field(v,"seed_origin",p);boundary::fields(o,{"seed","state"},{},p);r.origin_field_order=order(o);r.origin_seed=boundary::get_string(o,"seed",p);r.origin_state=boundary::get_string(o,"state",p);decode(boundary::field(v,"challenge",p),r.challenge,p);r.phase=boundary::get_string(v,"phase",p);r.frozen_tick=boundary::get_int(v,"frozen_tick",p);auto& bp=boundary::field(v,"performance_bp",p);if(!bp.as<std::monostate>())r.performance_bp=boundary::integer(bp,p);}
Status emit_fields(ProjectionWriter& w,const TrainingDrillInstance& r){w.object("seed_origin",r.origin_field_order,[&](auto& child){child.field("seed",r.origin_seed);return child.field("state",r.origin_state);});emit_value(w,"challenge",r.challenge);w.field("phase",r.phase);w.field("frozen_tick",r.frozen_tick);if(r.performance_bp)w.field("performance_bp",*r.performance_bp);else w.null("performance_bp");w.order_fields(r.field_order);return w.status();}
V encode(const TrainingDrillInstance& r){return encode_projected(r);}
Status visit(CodecVisitor& c,TrainingDrillInstance& r){c.field("origin_seed",r.origin_seed);c.field("origin_state",r.origin_state);c.object("origin_field_order",[&](auto& c){return visit(c,r.origin_field_order);});c.object("challenge",[&](auto& c){return visit(c,r.challenge);});c.field("phase",r.phase);c.field("frozen_tick",r.frozen_tick);c.optional("performance_bp",r.performance_bp,[](auto& c,auto& x){return visit(c,x);});visit_order(c,r.field_order);return c.status();}
StableId roster_digest(const std::vector<StableId>& ids){return boundary_canonical_digest(encode(ids));}
Outcome<std::vector<DrillDefinition>> decode_drill_catalog(const V& root){try{std::vector<DrillDefinition> out;auto data=root.find("drill_challenges");if(!data)return {true,out,{}};auto& texts=boundary::field(root,"texts","data");for(auto& row:boundary::object(*data,"drill_challenges")){auto& v=row.second;auto p="drill_challenges/"+row.first;boundary::fields(v,{"id","difficulty","prompt_key","groups","objectives","solution"},{},p);DrillDefinition d;d.id=boundary::get_string(v,"id",p);d.difficulty=boundary::get_int(v,"difficulty",p);d.prompt_key=boundary::get_string(v,"prompt_key",p);D selected;auto text=[&](const StableId& key){auto value=boundary::string(boundary::field(texts,key,p),p);auto found=std::find_if(selected.begin(),selected.end(),[&](auto& x){return x.first==key;});if(found==selected.end())selected.emplace_back(key,V{value});return value;};d.prompt=text(d.prompt_key);for(auto& value:boundary::array(boundary::field(v,"groups",p),p)){boundary::fields(value,{"id","kind","text_key","origin","targets"},{},p);DrillGroup g;g.id=boundary::get_string(value,"id",p);g.kind=boundary::get_string(value,"kind",p);g.text_key=boundary::get_string(value,"text_key",p);g.text=text(g.text_key);decode(boundary::field(value,"origin",p),g.origin,p);decode(boundary::field(value,"targets",p),g.targets,p);d.groups.push_back(g);}auto& o=boundary::field(v,"objectives",p);boundary::fields(o,{"supply","passage","screen","cavalry"},{},p);d.objectives_field_order=order(o);auto& supply=boundary::field(o,"supply",p);d.supply.field_order=order(supply);boundary::fields(supply,{"group_id","point","guard_cells","text_key"},{},p);d.supply.group_id=boundary::get_string(supply,"group_id",p);d.supply.text_key=boundary::get_string(supply,"text_key",p);d.supply.text=text(d.supply.text_key);decode(boundary::field(supply,"point",p),d.supply.point,p);decode(boundary::field(supply,"guard_cells",p),d.supply.guard_cells,p);auto& passage=boundary::field(o,"passage",p);d.passage.field_order=order(passage);boundary::fields(passage,{"group_id","cells","text_key"},{},p);d.passage.group_id=boundary::get_string(passage,"group_id",p);d.passage.text_key=boundary::get_string(passage,"text_key",p);d.passage.text=text(d.passage.text_key);decode(boundary::field(passage,"cells",p),d.passage.cells,p);auto& screen=boundary::field(o,"screen",p);d.screen.field_order=order(screen);boundary::fields(screen,{"front_group_id","ranged_group_id","pairs","text_key"},{},p);d.screen.front_group_id=boundary::get_string(screen,"front_group_id",p);d.screen.ranged_group_id=boundary::get_string(screen,"ranged_group_id",p);d.screen.text_key=boundary::get_string(screen,"text_key",p);d.screen.text=text(d.screen.text_key);for(auto& pair:boundary::array(boundary::field(screen,"pairs",p),p)){boundary::fields(pair,{"front","rear"},{},p);DrillScreenPair x;x.field_order=order(pair);decode(boundary::field(pair,"front",p),x.front,p);decode(boundary::field(pair,"rear",p),x.rear,p);d.screen.pairs.push_back(x);}auto& cavalry=boundary::field(o,"cavalry",p);d.cavalry.field_order=order(cavalry);boundary::fields(cavalry,{"group_id","bad_cells","text_key"},{},p);d.cavalry.group_id=boundary::get_string(cavalry,"group_id",p);d.cavalry.text_key=boundary::get_string(cavalry,"text_key",p);d.cavalry.text=text(d.cavalry.text_key);decode(boundary::field(cavalry,"bad_cells",p),d.cavalry.bad_cells,p);decode(boundary::field(v,"solution",p),d.solution,p);auto valid=validate_drill_definition(d);if(!valid.ok)return {false,{},valid.error};d.source_sha256=boundary_canonical_digest(V{D{{"definition",v},{"texts",V{selected}}}});out.push_back(std::move(d));}return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}

// 命令这里只记录结构/类型是否有效，领域热叶按原首错次序拒绝，不把坏值转成默认请求。
bool command_supported(const StableId& kind){return kind=="military_train"||kind=="military_train_cancel"||kind=="military_drill_draft"||kind=="military_drill_confirm"||kind=="military_drill_cancel_pending"||kind=="military_drill_exit";}
Outcome<MilitaryCommand> decode_command(const StableId& kind,const V& params){
    auto object=params.as<D>();auto text=[&](const char* key)->std::optional<StableId>{auto v=params.find(key);if(v&&v->as<StableId>())return *v->as<StableId>();return std::nullopt;};auto number=[&](const char* key)->std::optional<Integer>{auto v=params.find(key);if(v&&v->as<Integer>())return *v->as<Integer>();return std::nullopt;};
    if(kind=="military_train"){MilitaryTrain request;request.office_id=text("office_id").value_or("");request.unit_id=text("unit_id").value_or("");request.input_valid=object&&text("office_id")&&text("unit_id")&&object->size()==2+Integer(params.find("challenge_mode")!=nullptr);if(params.find("challenge_mode")){request.challenge_mode=text("challenge_mode");request.input_valid=request.input_valid&&request.challenge_mode.has_value();}if(object)request.field_order=order(params);return {true,request,{}};}
    if(kind=="military_train_cancel"){MilitaryTrainCancel request;request.action_id=text("action_id").value_or("");request.input_valid=object&&object->size()==1&&text("action_id").has_value();if(object)request.field_order=order(params);return {true,request,{}};}
    if(!command_supported(kind))return {false,{},common_failure("UnknownMilitaryCommand").error};
    MilitaryDrillCommand request;request.kind=kind;request.action_id=text("action_id").value_or("");if(object)request.field_order=order(params);request.input_valid=object&&text("action_id").has_value();
    // TYPE_ARRAY目标的格形状由纯叶核，保留StaleDrillStep先于InvalidDrillTarget的次序。
    auto point=[](const V& v){HexPoint p{-1,-1};auto list=v.as<A>();if(list&&list->size()==2&&(*list)[0].as<Integer>()&&(*list)[1].as<Integer>())p={*(*list)[0].as<Integer>(),*(*list)[1].as<Integer>()};return p;};
    if(kind=="military_drill_draft"){auto target=params.find("target");request.expected_step=number("expected_step");request.group_id=text("group_id").value_or("");request.input_valid=request.input_valid&&object->size()==4&&request.expected_step&&text("group_id")&&target&&target->as<A>();if(target&&target->as<A>())request.target=point(*target);}
    else if(kind=="military_drill_confirm"||kind=="military_drill_cancel_pending"){auto pending=params.find("expected_pending");request.expected_step=number("expected_step");request.input_valid=request.input_valid&&object->size()==3&&request.expected_step&&pending&&pending->as<D>();if(pending&&pending->as<D>()){DrillPending p;p.target={-1,-1};auto group=pending->find("group_id"),target=pending->find("target");if(pending->as<D>()->size()==2&&group&&group->as<StableId>()&&target&&target->as<A>()){p.group_id=*group->as<StableId>();p.target=point(*target);p.field_order=order(*pending);}request.expected_pending=p;}}
    else request.input_valid=request.input_valid&&object->size()==1;
    return {true,request,{}};
}
// 原报价的每个可见键按GD字面顺序输出，训练不会混入普通军事params或无关空details。
Outcome<BoundaryValue> encode_quote(const MilitaryQuote& quote,const MilitaryCommand& command){
    D out{{"ok",V{true}}};if(std::holds_alternative<MilitaryTrain>(command)){if(!quote.training_terms||!quote.training_cost||!quote.expected_training_gain)return {false,{},common_failure("InvalidMilitaryTrainingQuote").error};out.emplace_back("duration",V{quote.duration});out.emplace_back("cost",V{*quote.training_cost});out.emplace_back("participants",encode(quote.participants));out.emplace_back("terms",encode(*quote.training_terms));out.emplace_back("expected_training_gain",V{*quote.expected_training_gain});out.emplace_back("payment_policy",V{StableId("provided_half_days_only")});}
    else if(std::holds_alternative<MilitaryTrainCancel>(command)){if(!quote.provided_half_days)return {false,{},common_failure("InvalidMilitaryTrainingQuote").error};out.emplace_back("action_id",V{quote.action_id});out.emplace_back("provided_half_days",V{*quote.provided_half_days});}
    else if(std::holds_alternative<MilitaryDrillCommand>(command)){if(!quote.drill_before||!quote.drill_after)return {false,{},common_failure("InvalidMilitaryTrainingQuote").error};out.emplace_back("action_id",V{quote.action_id});out.emplace_back("before",encode(*quote.drill_before));out.emplace_back("after",encode(*quote.drill_after));}
    else return {false,{},common_failure("UnknownMilitaryCommand").error};
    return {true,V{std::move(out)},{}};
}
// 冷读只安装本域真实事实，行军收据由独立march codec拥有。
bool decode_fact(const StableId& kind,const V& value,const StableId& id,MilitaryState& state){if(kind!="military_training")return false;MilitaryTrainingFact fact;decode(value,fact,id);fact.id=id;state.training_facts.push_back(std::move(fact));return true;}
// 活动参数已在begin收敛成唯一training条款，不能恢复旧启动请求或另造名册。
bool decode_action(const StableId& kind,const V& value,const StableId& id,MilitaryState& state){if(kind!="military_train")return false;auto path="actions/"+id;auto& params=boundary::field(value,"params",path);boundary::fields(params,{"training"},{},path);MilitaryTrainingAction action;action.action_id=id;action.field_order=order(params);decode(boundary::field(params,"training",path),action.training,path+"/training");state.training_actions.push_back(std::move(action));return true;}
// 世界事实总序和actions头序由中央维护，本函数只补所属typed payload。
Status emit_action_payload_fields(ProjectionWriter& w,const MilitaryState& state){for(const auto& action:state.training_actions){auto checked=w.object(action.action_id,action.field_order,[&](auto& child){return emit_value(child,"training",action.training);});if(!checked.ok)return checked;}return w.status();}
Status emit_fact_fields(ProjectionWriter& w,const MilitaryState& state){for(const auto& fact:state.training_facts){auto checked=emit_value(w,fact.id,fact);if(!checked.ok)return checked;}return w.status();}
void append_state(D& actions,D& facts,const MilitaryState& state){append_projected_fields(actions,[&](auto& w){return emit_action_payload_fields(w,state);});append_projected_fields(facts,[&](auto& w){return emit_fact_fields(w,state);});}
// typed checkpoint同时保存业务字段及冷边界键序，不经动态JSON重建运行世界。
Status visit_state(CodecVisitor& codec,MilitaryState& state){codec.sequence("training_facts",state.training_facts,[](auto& c,auto& fact){return visit(c,fact);});codec.sequence("training_actions",state.training_actions,[](auto& c,MilitaryTrainingAction& action){c.field("action_id",action.action_id);c.object("training",[&](auto& c){return visit(c,action.training);});visit_order(c,action.field_order);return c.status();});return codec.status();}

namespace {
template<class T>Outcome<T> decode_checkpoint_original(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget,std::string_view path){
 static_assert(std::is_same_v<T,MilitaryTrainingTerms>||std::is_same_v<T,DrillState>);
 static_assert(std::is_nothrow_move_constructible_v<T>,"ordinary original payload must transfer without a hidden copy");
 if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch",path);
 try{budget.default_values<T>();T row;decode(raw,row,checkpoint_decode_boundary_text(path));return {true,std::move(row),{}};}
 catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
}
Outcome<MilitaryTrainingTerms> decode_training_terms(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return decode_checkpoint_original<MilitaryTrainingTerms>(raw,budget,"exact_military_training/original");}
Outcome<DrillState> decode_drill_state(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return decode_checkpoint_original<DrillState>(raw,budget,"exact_military_training/original");}

}
