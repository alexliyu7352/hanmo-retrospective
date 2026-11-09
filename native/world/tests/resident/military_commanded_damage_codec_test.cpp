// 原JSON重键/伪资格与typed截断门；旧13只做同实现字节往返，不冒独立旧发行或World。
#include "resident/domains/military/military_commanded_damage_codec.hpp"
#include "resident/domains/military/battle_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void good(Status s){if(!s.ok)throw std::runtime_error(s.error.original.code);}
template<class T>T take(Outcome<T> r){if(!r.ok)throw std::runtime_error(r.error.original.code);return std::move(r.value);}
MilitaryCommandedDamageState state(){MilitaryCommandedDamageState s;s.entry_tick=7;MilitaryCommandedDamageUnit r;r.unit_id="unit";r.commander_actor_id="actor";r.troop_type_id="foot";r.skill_id="infantry";r.authored_actor_id="actor";r.command_source={MilitaryUnitCommandSourceKind::authored,"unit"};r.leadership=60;r.experience=300;r.level=2;r.training=20;r.causal_fact_ids={"action:4"};s.units={r};return s;}
}
int main(){try{
 auto s=state();auto json=military_commanded_damage_codec::encode(s);auto decoded=take(military_commanded_damage_codec::decode(json));check(compare_boundary(json,military_commanded_damage_codec::encode(decoded)).values_equal,"exact JSON roundtrip");
 auto bad=json;bad.as<BoundaryValue::Dict>()->push_back(bad.as<BoundaryValue::Dict>()->front());check(!military_commanded_damage_codec::decode(bad).ok,"duplicate root rejected");
 bad=json;auto row=(*bad.find("units")->as<BoundaryValue::Array>())[0].as<BoundaryValue::Dict>();row->push_back(row->front());check(!military_commanded_damage_codec::decode(bad).ok,"duplicate unit key rejected");
 bad=json;auto& unit=(*bad.find("units")->as<BoundaryValue::Array>())[0];auto* source=unit.find("command_source")->as<BoundaryValue::Dict>();source->push_back(source->front());check(!military_commanded_damage_codec::decode(bad).ok,"duplicate nested authority rejected");
 bad=json;*(*bad.find("units")->as<BoundaryValue::Array>())[0].find("experience")=BoundaryValue{Integer(100)};check(!military_commanded_damage_codec::decode(bad).ok,"forged skill level rejected");
 bad=json;auto* ids=(*bad.find("units")->as<BoundaryValue::Array>())[0].find("causal_fact_ids")->as<BoundaryValue::Array>();ids->push_back(ids->front());check(!military_commanded_damage_codec::decode(bad).ok,"duplicate cause rejected");
 bad=json;*(*bad.find("units")->as<BoundaryValue::Array>())[0].find("command_source")->find("kind")=BoundaryValue{StableId("future_source")};check(!military_commanded_damage_codec::decode(bad).ok,"unknown command source rejected");
 constexpr auto old=military_combat_conditions_checkpoint_feature|military_garrison_rest_checkpoint_feature|military_command_shock_checkpoint_feature|military_rout_checkpoint_feature|military_supply_attrition_checkpoint_feature|military_recovery_checkpoint_feature;
 constexpr auto feature=old|military_commanded_damage_checkpoint_feature;
 MilitaryBattleRecord b;b.id="battle:8";b.created_tick=7;CodecVisitor::Buffer old_bytes;CodecVisitor old_writer(old_bytes);old_writer.schema_features=old;good(battle_codec::visit(old_writer,b));good(old_writer.finish());
 const auto old_input=old_bytes;MilitaryBattleRecord old_restored;CodecVisitor old_reader(old_input);old_reader.schema_features=old;good(battle_codec::visit(old_reader,old_restored));good(old_reader.finish());check(!old_restored.commanded_damage,"old13 has no new state");
 CodecVisitor::Buffer repeated;CodecVisitor old_repeat(repeated);old_repeat.schema_features=old;good(battle_codec::visit(old_repeat,old_restored));good(old_repeat.finish());check(old_bytes==repeated,"old13 same implementation byte roundtrip");
 b.commanded_damage=s;CodecVisitor::Buffer saved;CodecVisitor writer(saved);writer.schema_features=feature;good(battle_codec::visit(writer,b));good(writer.finish());const auto input=saved;MilitaryBattleRecord restored;CodecVisitor reader(input);reader.schema_features=feature;good(battle_codec::visit(reader,restored));good(reader.finish());check(restored.commanded_damage&&restored.commanded_damage->units[0].experience==300,"new14 original qualification retained");
 CodecVisitor::Buffer refused;CodecVisitor wrong(refused);wrong.schema_features=old;check(!battle_codec::visit(wrong,b).ok,"new qualification cannot hide in old13");
 auto clipped=saved;clipped.pop_back();const auto truncated=clipped;MilitaryBattleRecord broken;CodecVisitor short_read(truncated);short_read.schema_features=feature;auto status=battle_codec::visit(short_read,broken);check(!status.ok||!short_read.finish().ok,"truncated frame rejected");
 check(!validate_military_commanded_damage_modules({{military_commanded_damage_module,"1"}}).ok,"missing full13 rejected");check(!validate_military_commanded_damage_modules({{military_commanded_damage_module,"2"}}).ok,"future version rejected");check(validate_military_commanded_damage_modules({}).ok,"old disabled profile retained");
 std::cout<<"MILITARY_COMMANDED_DAMAGE_CODEC="<<checks<<" FAILURES=0 SCOPE=CODEC_SHAPE_COMPONENT\n";return 0;
}catch(const std::exception& e){std::cerr<<"MILITARY_COMMANDED_DAMAGE_CODEC_FAIL "<<checks<<" "<<e.what()<<'\n';return 1;}}
