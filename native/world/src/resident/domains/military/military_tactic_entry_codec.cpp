// 独立机械形状不能授已学资格；原完整entry/习得/教学来源门仍须全部通过。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_tactic_entry_codec.hpp"
#include "military_officer_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::military_tactic_entry_codec {
namespace {
using namespace domain_codec;
Status emit_row(ProjectionWriter& w,const MilitaryTacticEntryUnit& u){w.field("unit_id",u.unit_id);w.field("commander_actor_id",u.commander_actor_id);w.object("tactic_acquisition_ids",[&](auto& c){for(const auto& p:u.tactic_acquisition_ids)c.field(p.first,p.second);return c.status();});w.order_fields(u.field_order);return w.status();}
MilitaryTacticEntryUnit read(const V& v){const StableId path="learned_tactics/units";boundary::fields(v,{"unit_id","commander_actor_id","tactic_acquisition_ids"},{},path);MilitaryTacticEntryUnit u;u.field_order=boundary::order(v);u.unit_id=boundary::get_string(v,"unit_id",path);u.commander_actor_id=boundary::get_string(v,"commander_actor_id",path);for(const auto& p:boundary::object(boundary::field(v,"tactic_acquisition_ids",path),path))u.tactic_acquisition_ids.emplace_back(p.first,boundary::string(p.second,path));return u;}
Status walk(CodecVisitor& c,MilitaryTacticEntryUnit& u){c.field("unit_id",u.unit_id);c.field("commander_actor_id",u.commander_actor_id);c.sequence("tactic_acquisition_ids",u.tactic_acquisition_ids,[](auto& child,auto& pair){child.field("tactic_id",pair.first);child.field("acquisition_fact_id",pair.second);return child.status();});common_checkpoint::strings(c,"field_order",u.field_order);return c.status();}
}
Status emit_fields(ProjectionWriter& w,const MilitaryTacticEntryState& s){auto checked=validate_military_tactic_entry_shape(s);if(!checked.ok)return checked;w.field("entry_tick",s.entry_tick);w.sequence("units",s.units,[](auto& c,const auto& r){return c.object("",[&](auto& row){return emit_row(row,r);});});w.order_fields(s.field_order);return w.status();}
BoundaryValue encode(const MilitaryTacticEntryState& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& w){return emit_fields(w,s);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
Outcome<MilitaryTacticEntryState> decode(const BoundaryValue& v){try{auto checked=military_officer_codec::validate_json_unique_keys(v);if(!checked.ok)return {false,{},checked.error};boundary::fields(v,{"entry_tick","units"},{},"learned_tactics");MilitaryTacticEntryState s;s.field_order=boundary::order(v);s.entry_tick=boundary::get_int(v,"entry_tick","learned_tactics");for(const auto& u:boundary::array(boundary::field(v,"units","learned_tactics"),"units"))s.units.push_back(read(u));checked=validate_military_tactic_entry_shape(s);return checked.ok?Outcome<MilitaryTacticEntryState>{true,std::move(s),{}}:Outcome<MilitaryTacticEntryState>{false,{},checked.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit(CodecVisitor& c,MilitaryTacticEntryState& s,std::uint32_t version){if(version!=16)return c.reject("MilitaryTacticEntryCodecVersionMismatch");c.field("entry_tick",s.entry_tick);c.sequence("units",s.units,[](auto& child,auto& u){return walk(child,u);});common_checkpoint::strings(c,"field_order",s.field_order);if(!c.status().ok)return c.status();auto checked=validate_military_tactic_entry_shape(s);return checked.ok?c.status():c.reject(checked.error.original.code,checked.error.original.path);}
}
