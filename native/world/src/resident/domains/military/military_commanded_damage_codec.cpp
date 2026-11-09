// 递归重键、有限枚举及精确字段先验；真实军权与XP另由原入战早/晚来源门证明。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_commanded_damage_codec.hpp"
#include "military_commanded_damage.hpp"
#include "military_officer_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::military_commanded_damage_codec {
namespace {
using namespace domain_codec;
const std::vector<StableId> names{"unit_authored","army_formation","officer_assignment","field_assignment"};
Status wire(const MilitaryCommandedDamageState& s){
 MilitaryBattleRecord b;b.id="commanded_wire";b.created_tick=s.entry_tick;b.rout_retreat_enabled=true;MilitaryCombatConditionState condition;condition.entry_tick=s.entry_tick;MilitaryBattleCatalog cat;
 for(const auto& r:s.units){BattleUnit u;u.unit_id=r.unit_id;u.commander_actor_id=r.commander_actor_id;u.troop_type_id=r.troop_type_id;b.units.push_back(u);condition.units.push_back({r.unit_id,"wire",{},r.training,Integer(0),10000,{}});TroopDefinition t;t.id=r.troop_type_id;t.category=r.skill_id=="archery"?"archer":r.skill_id;cat.troop_types.push_back(t);}
 b.combat_conditions=std::move(condition);return validate_military_commanded_damage_shape(b,cat,s);
}
Status emit_row(ProjectionWriter& w,const MilitaryCommandedDamageUnit& r){w.field("unit_id",r.unit_id);w.field("commander_actor_id",r.commander_actor_id);w.field("troop_type_id",r.troop_type_id);w.field("skill_id",r.skill_id);w.field("authored_actor_id",r.authored_actor_id);const auto kind=static_cast<int>(r.command_source.kind);if(kind<0||kind>=int(names.size()))return w.reject("MilitaryCommandedSourceMismatch",r.unit_id);w.object("command_source",r.source_field_order,[&](auto& c){c.field("kind",names[kind]);return c.field("id",r.command_source.id);});w.field("leadership",r.leadership);w.field("experience",r.experience);w.field("level",r.level);w.field("training",r.training);w.sequence("causal_fact_ids",r.causal_fact_ids,[](auto& c,const auto& id){return c.field("",id);});w.order_fields(r.field_order);return w.status();}
void decode_row(const V& v,MilitaryCommandedDamageUnit& r){
 const StableId p="commanded_damage/units";boundary::fields(v,{"unit_id","commander_actor_id","troop_type_id","skill_id","authored_actor_id","command_source","leadership","experience","level","training","causal_fact_ids"},{},p);r.field_order=boundary::order(v);
 r.unit_id=boundary::get_string(v,"unit_id",p);r.commander_actor_id=boundary::get_string(v,"commander_actor_id",p);r.troop_type_id=boundary::get_string(v,"troop_type_id",p);r.skill_id=boundary::get_string(v,"skill_id",p);r.authored_actor_id=boundary::get_string(v,"authored_actor_id",p);
 const auto& source=boundary::field(v,"command_source",p);boundary::fields(source,{"kind","id"},{},p+"/command_source");r.source_field_order=boundary::order(source);auto kind=boundary::get_string(source,"kind",p);auto at=std::find(names.begin(),names.end(),kind);if(at==names.end())boundary::fail("MilitaryCommandedSourceMismatch",p);r.command_source.kind=static_cast<MilitaryUnitCommandSourceKind>(at-names.begin());r.command_source.id=boundary::get_string(source,"id",p);
 r.leadership=boundary::get_int(v,"leadership",p);r.experience=boundary::get_int(v,"experience",p);r.level=boundary::get_int(v,"level",p);r.training=boundary::get_int(v,"training",p);
 for(const auto& id:boundary::array(boundary::field(v,"causal_fact_ids",p),p+"/causal_fact_ids"))r.causal_fact_ids.push_back(boundary::string(id,p));
}
Status walk(CodecVisitor& c,MilitaryCommandedDamageUnit& r){
 c.field("unit_id",r.unit_id);c.field("commander_actor_id",r.commander_actor_id);c.field("troop_type_id",r.troop_type_id);c.field("skill_id",r.skill_id);c.field("authored_actor_id",r.authored_actor_id);
 Integer kind=static_cast<int>(r.command_source.kind);c.field("source_kind",kind);if(kind<0||kind>3)return c.reject("MilitaryCommandedSourceMismatch");r.command_source.kind=static_cast<MilitaryUnitCommandSourceKind>(kind);c.field("source_id",r.command_source.id);
 c.field("leadership",r.leadership);c.field("experience",r.experience);c.field("level",r.level);c.field("training",r.training);common_checkpoint::strings(c,"causal_fact_ids",r.causal_fact_ids);common_checkpoint::strings(c,"field_order",r.field_order);common_checkpoint::strings(c,"source_field_order",r.source_field_order);return c.status();
}
}
Status emit_fields(ProjectionWriter& w,const MilitaryCommandedDamageState& s){auto checked=wire(s);if(!checked.ok)return checked;w.field("entry_tick",s.entry_tick);w.sequence("units",s.units,[](auto& c,const auto& r){return c.object("",[&](auto& row){return emit_row(row,r);});});w.order_fields(s.field_order);return w.status();}
BoundaryValue encode(const MilitaryCommandedDamageState& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& w){return emit_fields(w,s);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
Outcome<MilitaryCommandedDamageState> decode(const BoundaryValue& v){try{
 auto unique=military_officer_codec::validate_json_unique_keys(v);if(!unique.ok)return {false,{},unique.error};boundary::fields(v,{"entry_tick","units"},{},"commanded_damage");MilitaryCommandedDamageState s;s.field_order=boundary::order(v);s.entry_tick=boundary::get_int(v,"entry_tick","commanded_damage");for(const auto& row:boundary::array(boundary::field(v,"units","commanded_damage"),"units")){MilitaryCommandedDamageUnit r;decode_row(row,r);s.units.push_back(std::move(r));}
 auto valid=wire(s);return valid.ok?Outcome<MilitaryCommandedDamageState>{true,std::move(s),{}}:Outcome<MilitaryCommandedDamageState>{false,{},valid.error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit(CodecVisitor& c,MilitaryCommandedDamageState& s){
 if(!(c.schema_features&military_commanded_damage_checkpoint_feature)||!(c.schema_features&military_combat_conditions_checkpoint_feature))return c.reject("MilitaryCommandedDamagePrerequisiteRequired");
 c.field("entry_tick",s.entry_tick);c.sequence("units",s.units,[](auto& child,auto& row){return walk(child,row);});common_checkpoint::strings(c,"field_order",s.field_order);if(!c.status().ok)return c.status();auto valid=wire(s);return valid.ok?c.status():c.reject(valid.error.original.code,valid.error.original.path);
}
}
