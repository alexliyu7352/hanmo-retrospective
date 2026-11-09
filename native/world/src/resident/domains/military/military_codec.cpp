#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_projection_envelope.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "military_siege_engineering_codec.hpp"
#include "military_siege_lifecycle_codec.hpp"
#include "military_tactic_practice_codec.hpp"
#include "military_tactic_learning_catalog.hpp"
#include "military_tactic_entry.hpp"
// 军政域逐字段冷编解码；固定schema无动态字典缓存，未知字段或未实现事实整域拒绝。
#include "battle_request_codec.hpp"
#include "military_officer_codec.hpp"
#include "military_field_command_codec.hpp"
#include "military_recovery_codec.hpp"
#include "military_supply_attrition_codec.hpp"
#include <set>
#include "military_rout_codec.hpp"
#include "military_rout_profile.hpp"
#include "military_rout_retreat.hpp"
#include "military_command_shock.hpp"
#include "military_garrison_rest_codec.hpp"
#include "military_garrison_rest.hpp"
#include "military_combat_conditions.hpp"
#include "military_combat_conditions_profile.hpp"
#include "military_commanded_damage_profile.hpp"
#include "military_commanded_damage.hpp"
#include "military_command_resources.hpp"
#include "military_command_resources_profile.hpp"
#include "military_sequential_rounds.hpp"
#include "military_field_terrain.hpp"
#include "military_recovery_views.hpp"
#include "military_codec.hpp"
#include "training_exact_model.hpp"
#include "field_battle/field_world.hpp"
#include "battle_codec.hpp"
#include "lifecycle_codec.hpp"
#include "siege_codec.hpp"
#include "siege_command_codec.hpp"
#include "occupation_codec.hpp"
#include "march_codec.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
namespace hanmo::resident {
namespace military_codec {
using namespace domain_codec;
Status emit_fields(ProjectionWriter&,const OfficeRecord&);
Status emit_fields(ProjectionWriter&,const GarrisonRecord&);
Status emit_fields(ProjectionWriter&,const RetainerRecord&);
Status emit_fields(ProjectionWriter&,const MilitaryUnitRecord&);
Status emit_fields(ProjectionWriter&,const ArmyRecord&);
Status emit_fields(ProjectionWriter&,const CampaignObjectiveResult&);
Status emit_fields(ProjectionWriter&,const CampaignRecord&);
Status emit_fields(ProjectionWriter&,const StrongholdRecord&);
Status emit_fields(ProjectionWriter&,const OfficeTerms&);
Status emit_fields(ProjectionWriter&,const ArmyFormationTerms&);
Status emit_fields(ProjectionWriter&,const MilitarySupplyFact&);
Status emit_fields(ProjectionWriter&,const CampaignArrivalFact&);
template<class T>Status emit_value(ProjectionWriter&,const StableId&,const T&);
template<class T,class A>Status emit_value(ProjectionWriter&,const StableId&,const std::vector<T,A>&);
template<class T,class A>Status emit_value(ProjectionWriter&,const StableId&,const std::vector<std::pair<StableId,T>,A>&);
template<class T>Status emit_value(ProjectionWriter& w,const StableId& key,const T& value){if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(key,value);else return w.object(key,[&](auto& c){return emit_fields(c,value);});}
template<class T,class A>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T,A>& rows){return w.sequence(key,rows,[](auto& c,const auto& row){return emit_value(c,"",row);});}
template<class T,class A>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<std::pair<StableId,T>,A>& rows){return w.object(key,[&](auto& c){for(const auto& row:rows)emit_value(c,row.first,row.second);return c.status();});}
Status emit_retainers(ProjectionWriter& w,const StableId& key,const std::vector<RetainerRecord>& rows){return w.object(key,[&](auto& c){for(const auto& row:rows)emit_value(c,row.actor_id,row);return c.status();});}
template<class T>BoundaryValue row_json(const T& value){BoundaryValue out;BoundaryProjectionWriter sink(out);auto check=emit_value(sink,"",value);if(check.ok)check=sink.finish();if(!check.ok)boundary::fail(check.error.original.code,check.error.original.path);return out;}

V enc(const OfficeRecord&);
void dec(const V&, OfficeRecord&,const StableId&);
Status visit(CodecVisitor&, OfficeRecord&);
V enc(const GarrisonRecord&);
void dec(const V&, GarrisonRecord&,const StableId&);
Status visit(CodecVisitor&, GarrisonRecord&);
V enc(const RetainerRecord&);
void dec(const V&, RetainerRecord&,const StableId&);
Status visit(CodecVisitor&, RetainerRecord&);
V enc(const MilitaryUnitRecord&);
void dec(const V&, MilitaryUnitRecord&,const StableId&);
Status visit(CodecVisitor&, MilitaryUnitRecord&);
V enc(const ArmyRecord&);
void dec(const V&, ArmyRecord&,const StableId&);
Status visit(CodecVisitor&, ArmyRecord&);
V enc(const CampaignObjectiveResult&);
void dec(const V&, CampaignObjectiveResult&,const StableId&);
Status visit(CodecVisitor&, CampaignObjectiveResult&);
V enc(const CampaignRecord&);
void dec(const V&, CampaignRecord&,const StableId&);
Status visit(CodecVisitor&, CampaignRecord&);
V enc(const StrongholdRecord&);
void dec(const V&, StrongholdRecord&,const StableId&);
Status visit(CodecVisitor&, StrongholdRecord&);
V enc(const OfficeTerms&);
void dec(const V&, OfficeTerms&,const StableId&);
Status visit(CodecVisitor&, OfficeTerms&);
V enc(const ArmyFormationTerms&);
void dec(const V&, ArmyFormationTerms&,const StableId&);
Status visit(CodecVisitor&, ArmyFormationTerms&);
V enc(const MilitarySupplyFact&);
void dec(const V&, MilitarySupplyFact&,const StableId&);
Status visit(CodecVisitor&, MilitarySupplyFact&);
using domain_codec::enc;using domain_codec::dec;using domain_codec::visit;
template<class T> V list(const std::vector<T>& rows){A out;for(auto& v:rows)out.push_back(enc(v));return V{std::move(out)};}
template<class T> void list(const V& v,std::vector<T>& out,const StableId& path){domain_codec::dec_list(v,out,path,[](const V& input,T& row,const StableId& source_path){dec(input,row,source_path);});}
V retainers(const std::vector<RetainerRecord>& rows){D out;for(auto& r:rows)out.emplace_back(r.actor_id,enc(r));return V{std::move(out)};}
void retainers(const V& v,std::vector<RetainerRecord>& out,const StableId& path){
 const auto& input=boundary::object(v,path);domain_codec::prepare_decoded_collection<RetainerRecord>(input.size(),path);
 std::vector<RetainerRecord> decoded(input.size());
 for(std::size_t i=0;i<input.size();++i){const auto& source=input[i];auto& row=decoded[i];
  dec(source.second,row,checkpoint_decode_boundary_join({path,"/",source.first}));
  row.actor_id=checkpoint_decode_boundary_string(source.first);
 }
 out.swap(decoded);
}
// OfficeRecord完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,OfficeRecord& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"id","holder_actor_id","enabled","appointment_id"},{},path);
dec(boundary::field(v,"id",path),r.id,checkpoint_decode_boundary_join({path,"/id"}));
dec(boundary::field(v,"holder_actor_id",path),r.holder_actor_id,checkpoint_decode_boundary_join({path,"/holder_actor_id"}));
dec(boundary::field(v,"enabled",path),r.enabled,checkpoint_decode_boundary_join({path,"/enabled"}));
dec(boundary::field(v,"appointment_id",path),r.appointment_id,checkpoint_decode_boundary_join({path,"/appointment_id"}));
}
Status emit_fields(ProjectionWriter& w,const OfficeRecord& r){
emit_value(w,"id",r.id);
emit_value(w,"holder_actor_id",r.holder_actor_id);
emit_value(w,"enabled",r.enabled);
emit_value(w,"appointment_id",r.appointment_id);w.order_fields(r.field_order);return w.status();}
V enc(const OfficeRecord& r){return row_json(r);}
Status visit(CodecVisitor& c,OfficeRecord& r){
c.field("id",r.id);
c.field("holder_actor_id",r.holder_actor_id);
c.field("enabled",r.enabled);
c.field("appointment_id",r.appointment_id);
visit_order(c,r.field_order);return c.status();}
// GarrisonRecord完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,GarrisonRecord& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"id","unit_ids","prisoner_ids","last_supply_tick"},{},path);
dec(boundary::field(v,"id",path),r.id,checkpoint_decode_boundary_join({path,"/id"}));
list(boundary::field(v,"unit_ids",path),r.unit_ids,checkpoint_decode_boundary_join({path,"/unit_ids"}));
list(boundary::field(v,"prisoner_ids",path),r.prisoner_ids,checkpoint_decode_boundary_join({path,"/prisoner_ids"}));
dec(boundary::field(v,"last_supply_tick",path),r.last_supply_tick,checkpoint_decode_boundary_join({path,"/last_supply_tick"}));
}
Status emit_fields(ProjectionWriter& w,const GarrisonRecord& r){
emit_value(w,"id",r.id);
emit_value(w,"unit_ids",r.unit_ids);
emit_value(w,"prisoner_ids",r.prisoner_ids);
emit_value(w,"last_supply_tick",r.last_supply_tick);w.order_fields(r.field_order);return w.status();}
V enc(const GarrisonRecord& r){return row_json(r);}
Status visit(CodecVisitor& c,GarrisonRecord& r){
c.field("id",r.id);
c.sequence("unit_ids",r.unit_ids,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("prisoner_ids",r.prisoner_ids,[](auto& codec,auto& value){return visit(codec,value);});
c.field("last_supply_tick",r.last_supply_tick);
visit_order(c,r.field_order);return c.status();}
// RetainerRecord完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,RetainerRecord& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"estate_id","source_id","unit_id"},{},path);
dec(boundary::field(v,"estate_id",path),r.estate_id,checkpoint_decode_boundary_join({path,"/estate_id"}));
dec(boundary::field(v,"source_id",path),r.source_id,checkpoint_decode_boundary_join({path,"/source_id"}));
dec(boundary::field(v,"unit_id",path),r.unit_id,checkpoint_decode_boundary_join({path,"/unit_id"}));
}
Status emit_fields(ProjectionWriter& w,const RetainerRecord& r){
emit_value(w,"estate_id",r.estate_id);
emit_value(w,"source_id",r.source_id);
emit_value(w,"unit_id",r.unit_id);w.order_fields(r.field_order);return w.status();}
V enc(const RetainerRecord& r){return row_json(r);}
Status visit(CodecVisitor& c,RetainerRecord& r){
c.field("actor_id",r.actor_id);
c.field("estate_id",r.estate_id);
c.field("source_id",r.source_id);
c.field("unit_id",r.unit_id);
visit_order(c,r.field_order);return c.status();}
// MilitaryUnitRecord完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,MilitaryUnitRecord& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"id","source_garrison_id","organization_id","garrison_id","army_id","mobilizing_action_id","commander_actor_id","troop_type_id","member_ids","fit","wounded","dead","captured","deserted","captors","retainer_sources","morale"},{"training","fatigue","training_action_id"},path);
dec(boundary::field(v,"id",path),r.id,checkpoint_decode_boundary_join({path,"/id"}));
dec(boundary::field(v,"source_garrison_id",path),r.source_garrison_id,checkpoint_decode_boundary_join({path,"/source_garrison_id"}));
dec(boundary::field(v,"organization_id",path),r.organization_id,checkpoint_decode_boundary_join({path,"/organization_id"}));
dec(boundary::field(v,"garrison_id",path),r.garrison_id,checkpoint_decode_boundary_join({path,"/garrison_id"}));
dec(boundary::field(v,"army_id",path),r.army_id,checkpoint_decode_boundary_join({path,"/army_id"}));
dec(boundary::field(v,"mobilizing_action_id",path),r.mobilizing_action_id,checkpoint_decode_boundary_join({path,"/mobilizing_action_id"}));
dec(boundary::field(v,"commander_actor_id",path),r.commander_actor_id,checkpoint_decode_boundary_join({path,"/commander_actor_id"}));
dec(boundary::field(v,"troop_type_id",path),r.troop_type_id,checkpoint_decode_boundary_join({path,"/troop_type_id"}));
list(boundary::field(v,"member_ids",path),r.member_ids,checkpoint_decode_boundary_join({path,"/member_ids"}));
list(boundary::field(v,"fit",path),r.fit,checkpoint_decode_boundary_join({path,"/fit"}));
list(boundary::field(v,"wounded",path),r.wounded,checkpoint_decode_boundary_join({path,"/wounded"}));
list(boundary::field(v,"dead",path),r.dead,checkpoint_decode_boundary_join({path,"/dead"}));
list(boundary::field(v,"captured",path),r.captured,checkpoint_decode_boundary_join({path,"/captured"}));
list(boundary::field(v,"deserted",path),r.deserted,checkpoint_decode_boundary_join({path,"/deserted"}));
dec_pairs(boundary::field(v,"captors",path),r.captors,checkpoint_decode_boundary_join({path,"/captors"}));
dec_pairs(boundary::field(v,"retainer_sources",path),r.retainer_sources,checkpoint_decode_boundary_join({path,"/retainer_sources"}));
dec(boundary::field(v,"morale",path),r.morale,checkpoint_decode_boundary_join({path,"/morale"}));
if(auto value=v.find("training")){r.training.emplace();dec(*value,*r.training,checkpoint_decode_boundary_join({path,"/training"}));}
if(auto value=v.find("fatigue")){r.fatigue.emplace();dec(*value,*r.fatigue,checkpoint_decode_boundary_join({path,"/fatigue"}));}
if(auto value=v.find("training_action_id")){r.training_action_id.emplace();dec(*value,*r.training_action_id,checkpoint_decode_boundary_join({path,"/training_action_id"}));}
}
Status emit_fields(ProjectionWriter& w,const MilitaryUnitRecord& r){
emit_value(w,"id",r.id);
emit_value(w,"source_garrison_id",r.source_garrison_id);
emit_value(w,"organization_id",r.organization_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"army_id",r.army_id);
emit_value(w,"mobilizing_action_id",r.mobilizing_action_id);
emit_value(w,"commander_actor_id",r.commander_actor_id);
emit_value(w,"troop_type_id",r.troop_type_id);
emit_value(w,"member_ids",r.member_ids);
emit_value(w,"fit",r.fit);
emit_value(w,"wounded",r.wounded);
emit_value(w,"dead",r.dead);
emit_value(w,"captured",r.captured);
emit_value(w,"deserted",r.deserted);
emit_value(w,"captors",r.captors);
emit_value(w,"retainer_sources",r.retainer_sources);
emit_value(w,"morale",r.morale);
if(r.training)emit_value(w,"training",*r.training);
if(r.fatigue)emit_value(w,"fatigue",*r.fatigue);
if(r.training_action_id)emit_value(w,"training_action_id",*r.training_action_id);w.order_fields(r.field_order);return w.status();}
V enc(const MilitaryUnitRecord& r){return row_json(r);}
Status visit(CodecVisitor& c,MilitaryUnitRecord& r){
c.field("id",r.id);
c.field("source_garrison_id",r.source_garrison_id);
c.field("organization_id",r.organization_id);
c.field("garrison_id",r.garrison_id);
c.field("army_id",r.army_id);
c.field("mobilizing_action_id",r.mobilizing_action_id);
c.field("commander_actor_id",r.commander_actor_id);
c.field("troop_type_id",r.troop_type_id);
c.sequence("member_ids",r.member_ids,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("fit",r.fit,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("wounded",r.wounded,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("dead",r.dead,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("captured",r.captured,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("deserted",r.deserted,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("captors",r.captors,[](auto& codec,auto& pair){codec.field("key",pair.first);codec.field("value",pair.second);return codec.status();});
c.sequence("retainer_sources",r.retainer_sources,[](auto& codec,auto& pair){codec.field("key",pair.first);codec.field("value",pair.second);return codec.status();});
c.field("morale",r.morale);
c.optional("training",r.training,[](auto& codec,auto& value){return visit(codec,value);});
c.optional("fatigue",r.fatigue,[](auto& codec,auto& value){return visit(codec,value);});
c.optional("training_action_id",r.training_action_id,[](auto& codec,auto& value){return visit(codec,value);});
visit_order(c,r.field_order);return c.status();}
// ArmyRecord完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,ArmyRecord& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"id","organization_id","commander_actor_id","member_actor_ids","unit_ids","inventory_id","wallet_id","order_id","garrison_id","status","last_supply_tick","retainers","prisoner_ids","formation_source"},{"return_garrison_id"},path);
dec(boundary::field(v,"id",path),r.id,checkpoint_decode_boundary_join({path,"/id"}));
dec(boundary::field(v,"organization_id",path),r.organization_id,checkpoint_decode_boundary_join({path,"/organization_id"}));
dec(boundary::field(v,"commander_actor_id",path),r.commander_actor_id,checkpoint_decode_boundary_join({path,"/commander_actor_id"}));
list(boundary::field(v,"member_actor_ids",path),r.member_actor_ids,checkpoint_decode_boundary_join({path,"/member_actor_ids"}));
list(boundary::field(v,"unit_ids",path),r.unit_ids,checkpoint_decode_boundary_join({path,"/unit_ids"}));
dec(boundary::field(v,"inventory_id",path),r.inventory_id,checkpoint_decode_boundary_join({path,"/inventory_id"}));
dec(boundary::field(v,"wallet_id",path),r.wallet_id,checkpoint_decode_boundary_join({path,"/wallet_id"}));
dec(boundary::field(v,"order_id",path),r.order_id,checkpoint_decode_boundary_join({path,"/order_id"}));
dec(boundary::field(v,"garrison_id",path),r.garrison_id,checkpoint_decode_boundary_join({path,"/garrison_id"}));
dec(boundary::field(v,"status",path),r.status,checkpoint_decode_boundary_join({path,"/status"}));
dec(boundary::field(v,"last_supply_tick",path),r.last_supply_tick,checkpoint_decode_boundary_join({path,"/last_supply_tick"}));
retainers(boundary::field(v,"retainers",path),r.retainers,checkpoint_decode_boundary_join({path,"/retainers"}));
list(boundary::field(v,"prisoner_ids",path),r.prisoner_ids,checkpoint_decode_boundary_join({path,"/prisoner_ids"}));
dec(boundary::field(v,"formation_source",path),r.formation_source,checkpoint_decode_boundary_join({path,"/formation_source"}));
if(auto value=v.find("return_garrison_id")){r.return_garrison_id.emplace();dec(*value,*r.return_garrison_id,checkpoint_decode_boundary_join({path,"/return_garrison_id"}));}
}
Status emit_fields(ProjectionWriter& w,const ArmyRecord& r){
emit_value(w,"id",r.id);
emit_value(w,"organization_id",r.organization_id);
emit_value(w,"commander_actor_id",r.commander_actor_id);
emit_value(w,"member_actor_ids",r.member_actor_ids);
emit_value(w,"unit_ids",r.unit_ids);
emit_value(w,"inventory_id",r.inventory_id);
emit_value(w,"wallet_id",r.wallet_id);
emit_value(w,"order_id",r.order_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"status",r.status);
emit_value(w,"last_supply_tick",r.last_supply_tick);
emit_retainers(w,"retainers",r.retainers);
emit_value(w,"prisoner_ids",r.prisoner_ids);
emit_value(w,"formation_source",r.formation_source);
if(r.return_garrison_id)emit_value(w,"return_garrison_id",*r.return_garrison_id);w.order_fields(r.field_order);return w.status();}
V enc(const ArmyRecord& r){return row_json(r);}
Status visit(CodecVisitor& c,ArmyRecord& r){
c.field("id",r.id);
c.field("organization_id",r.organization_id);
c.field("commander_actor_id",r.commander_actor_id);
c.sequence("member_actor_ids",r.member_actor_ids,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("unit_ids",r.unit_ids,[](auto& codec,auto& value){return visit(codec,value);});
c.field("inventory_id",r.inventory_id);
c.field("wallet_id",r.wallet_id);
c.field("order_id",r.order_id);
c.field("garrison_id",r.garrison_id);
c.field("status",r.status);
c.field("last_supply_tick",r.last_supply_tick);
c.sequence("retainers",r.retainers,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("prisoner_ids",r.prisoner_ids,[](auto& codec,auto& value){return visit(codec,value);});
c.field("formation_source",r.formation_source);
c.optional("return_garrison_id",r.return_garrison_id,[](auto& codec,auto& value){return visit(codec,value);});
visit_order(c,r.field_order);return c.status();}
// CampaignObjectiveResult完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,CampaignObjectiveResult& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"kind","tick"},{"source_id"},path);
dec(boundary::field(v,"kind",path),r.kind,checkpoint_decode_boundary_join({path,"/kind"}));
dec(boundary::field(v,"tick",path),r.tick,checkpoint_decode_boundary_join({path,"/tick"}));
if(auto value=v.find("source_id")){r.source_id.emplace();dec(*value,*r.source_id,checkpoint_decode_boundary_join({path,"/source_id"}));}
}
Status emit_fields(ProjectionWriter& w,const CampaignObjectiveResult& r){
emit_value(w,"kind",r.kind);
emit_value(w,"tick",r.tick);
if(r.source_id)emit_value(w,"source_id",*r.source_id);w.order_fields(r.field_order);return w.status();}
V enc(const CampaignObjectiveResult& r){return row_json(r);}
Status visit(CodecVisitor& c,CampaignObjectiveResult& r){
c.field("kind",r.kind);
c.field("tick",r.tick);
c.optional("source_id",r.source_id,[](auto& codec,auto& value){return visit(codec,value);});
visit_order(c,r.field_order);return c.status();}
// CampaignRecord完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,CampaignRecord& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"id","template_id","actor_id","office_id","garrison_id","issuer_role_id","issuer_organization_id","status","army_id","accepted_tick","deadline_tick","last_tick","objective_result","settlement_id","arrival_fact_id","return_receipt_id","recipient_army_id"},{"return_garrison_id","return_wait_reason"},path);
dec(boundary::field(v,"id",path),r.id,checkpoint_decode_boundary_join({path,"/id"}));
dec(boundary::field(v,"template_id",path),r.template_id,checkpoint_decode_boundary_join({path,"/template_id"}));
dec(boundary::field(v,"actor_id",path),r.actor_id,checkpoint_decode_boundary_join({path,"/actor_id"}));
dec(boundary::field(v,"office_id",path),r.office_id,checkpoint_decode_boundary_join({path,"/office_id"}));
dec(boundary::field(v,"garrison_id",path),r.garrison_id,checkpoint_decode_boundary_join({path,"/garrison_id"}));
dec(boundary::field(v,"issuer_role_id",path),r.issuer_role_id,checkpoint_decode_boundary_join({path,"/issuer_role_id"}));
dec(boundary::field(v,"issuer_organization_id",path),r.issuer_organization_id,checkpoint_decode_boundary_join({path,"/issuer_organization_id"}));
dec(boundary::field(v,"status",path),r.status,checkpoint_decode_boundary_join({path,"/status"}));
dec(boundary::field(v,"army_id",path),r.army_id,checkpoint_decode_boundary_join({path,"/army_id"}));
dec(boundary::field(v,"accepted_tick",path),r.accepted_tick,checkpoint_decode_boundary_join({path,"/accepted_tick"}));
dec(boundary::field(v,"deadline_tick",path),r.deadline_tick,checkpoint_decode_boundary_join({path,"/deadline_tick"}));
dec(boundary::field(v,"last_tick",path),r.last_tick,checkpoint_decode_boundary_join({path,"/last_tick"}));
if(!boundary::object(boundary::field(v,"objective_result",path),checkpoint_decode_boundary_join({path,"/objective_result"})).empty()){r.objective_result.emplace();dec(boundary::field(v,"objective_result",path),*r.objective_result,checkpoint_decode_boundary_join({path,"/objective_result"}));}
dec(boundary::field(v,"settlement_id",path),r.settlement_id,checkpoint_decode_boundary_join({path,"/settlement_id"}));
dec(boundary::field(v,"arrival_fact_id",path),r.arrival_fact_id,checkpoint_decode_boundary_join({path,"/arrival_fact_id"}));
dec(boundary::field(v,"return_receipt_id",path),r.return_receipt_id,checkpoint_decode_boundary_join({path,"/return_receipt_id"}));
dec(boundary::field(v,"recipient_army_id",path),r.recipient_army_id,checkpoint_decode_boundary_join({path,"/recipient_army_id"}));
if(auto value=v.find("return_garrison_id")){r.return_garrison_id.emplace();dec(*value,*r.return_garrison_id,checkpoint_decode_boundary_join({path,"/return_garrison_id"}));}
if(auto value=v.find("return_wait_reason")){r.return_wait_reason.emplace();dec(*value,*r.return_wait_reason,checkpoint_decode_boundary_join({path,"/return_wait_reason"}));}
}
Status emit_fields(ProjectionWriter& w,const CampaignRecord& r){
emit_value(w,"id",r.id);
emit_value(w,"template_id",r.template_id);
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"office_id",r.office_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"issuer_role_id",r.issuer_role_id);
emit_value(w,"issuer_organization_id",r.issuer_organization_id);
emit_value(w,"status",r.status);
emit_value(w,"army_id",r.army_id);
emit_value(w,"accepted_tick",r.accepted_tick);
emit_value(w,"deadline_tick",r.deadline_tick);
emit_value(w,"last_tick",r.last_tick);
if(r.objective_result)emit_value(w,"objective_result",*r.objective_result);else w.object("objective_result",[](auto& c){return c.status();});
emit_value(w,"settlement_id",r.settlement_id);
emit_value(w,"arrival_fact_id",r.arrival_fact_id);
emit_value(w,"return_receipt_id",r.return_receipt_id);
emit_value(w,"recipient_army_id",r.recipient_army_id);
if(r.return_garrison_id)emit_value(w,"return_garrison_id",*r.return_garrison_id);
if(r.return_wait_reason)emit_value(w,"return_wait_reason",*r.return_wait_reason);w.order_fields(r.field_order);return w.status();}
V enc(const CampaignRecord& r){return row_json(r);}
Status visit(CodecVisitor& c,CampaignRecord& r){
c.field("id",r.id);
c.field("template_id",r.template_id);
c.field("actor_id",r.actor_id);
c.field("office_id",r.office_id);
c.field("garrison_id",r.garrison_id);
c.field("issuer_role_id",r.issuer_role_id);
c.field("issuer_organization_id",r.issuer_organization_id);
c.field("status",r.status);
c.field("army_id",r.army_id);
c.field("accepted_tick",r.accepted_tick);
c.field("deadline_tick",r.deadline_tick);
c.field("last_tick",r.last_tick);
c.optional("objective_result",r.objective_result,[](auto& codec,auto& value){return visit(codec,value);});
c.field("settlement_id",r.settlement_id);
c.field("arrival_fact_id",r.arrival_fact_id);
c.field("return_receipt_id",r.return_receipt_id);
c.field("recipient_army_id",r.recipient_army_id);
c.optional("return_garrison_id",r.return_garrison_id,[](auto& codec,auto& value){return visit(codec,value);});
c.optional("return_wait_reason",r.return_wait_reason,[](auto& codec,auto& value){return visit(codec,value);});
visit_order(c,r.field_order);return c.status();}
// StrongholdRecord完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,StrongholdRecord& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"id","owner_organization_id","occupation_id","siege_id"},{},path);
dec(boundary::field(v,"id",path),r.id,checkpoint_decode_boundary_join({path,"/id"}));
dec(boundary::field(v,"owner_organization_id",path),r.owner_organization_id,checkpoint_decode_boundary_join({path,"/owner_organization_id"}));
dec(boundary::field(v,"occupation_id",path),r.occupation_id,checkpoint_decode_boundary_join({path,"/occupation_id"}));
dec(boundary::field(v,"siege_id",path),r.siege_id,checkpoint_decode_boundary_join({path,"/siege_id"}));
}
Status emit_fields(ProjectionWriter& w,const StrongholdRecord& r){
emit_value(w,"id",r.id);
emit_value(w,"owner_organization_id",r.owner_organization_id);
emit_value(w,"occupation_id",r.occupation_id);
emit_value(w,"siege_id",r.siege_id);w.order_fields(r.field_order);return w.status();}
V enc(const StrongholdRecord& r){return row_json(r);}
Status visit(CodecVisitor& c,StrongholdRecord& r){
c.field("id",r.id);
c.field("owner_organization_id",r.owner_organization_id);
c.field("occupation_id",r.occupation_id);
c.field("siege_id",r.siege_id);
visit_order(c,r.field_order);return c.status();}
// OfficeTerms完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,OfficeTerms& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"office_id","actor_id","organization_id","place_id","provider_actor_id"},{},path);
dec(boundary::field(v,"office_id",path),r.office_id,checkpoint_decode_boundary_join({path,"/office_id"}));
dec(boundary::field(v,"actor_id",path),r.actor_id,checkpoint_decode_boundary_join({path,"/actor_id"}));
dec(boundary::field(v,"organization_id",path),r.organization_id,checkpoint_decode_boundary_join({path,"/organization_id"}));
dec(boundary::field(v,"place_id",path),r.place_id,checkpoint_decode_boundary_join({path,"/place_id"}));
dec(boundary::field(v,"provider_actor_id",path),r.provider_actor_id,checkpoint_decode_boundary_join({path,"/provider_actor_id"}));
}
Status emit_fields(ProjectionWriter& w,const OfficeTerms& r){
emit_value(w,"office_id",r.office_id);
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"organization_id",r.organization_id);
emit_value(w,"place_id",r.place_id);
emit_value(w,"provider_actor_id",r.provider_actor_id);w.order_fields(r.field_order);return w.status();}
V enc(const OfficeTerms& r){return row_json(r);}
Status visit(CodecVisitor& c,OfficeTerms& r){
c.field("office_id",r.office_id);
c.field("actor_id",r.actor_id);
c.field("organization_id",r.organization_id);
c.field("place_id",r.place_id);
c.field("provider_actor_id",r.provider_actor_id);
visit_order(c,r.field_order);return c.status();}
// ArmyFormationTerms完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,ArmyFormationTerms& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"order_id","office_id","actor_id","garrison_id","ownership_id","place_id","organization_id","unit_ids","commanders","member_actor_ids","retainers","goods","money","source_inventory_id","source_wallet_id"},{},path);
dec(boundary::field(v,"order_id",path),r.order_id,checkpoint_decode_boundary_join({path,"/order_id"}));
dec(boundary::field(v,"office_id",path),r.office_id,checkpoint_decode_boundary_join({path,"/office_id"}));
dec(boundary::field(v,"actor_id",path),r.actor_id,checkpoint_decode_boundary_join({path,"/actor_id"}));
dec(boundary::field(v,"garrison_id",path),r.garrison_id,checkpoint_decode_boundary_join({path,"/garrison_id"}));
dec(boundary::field(v,"ownership_id",path),r.ownership_id,checkpoint_decode_boundary_join({path,"/ownership_id"}));
dec(boundary::field(v,"place_id",path),r.place_id,checkpoint_decode_boundary_join({path,"/place_id"}));
dec(boundary::field(v,"organization_id",path),r.organization_id,checkpoint_decode_boundary_join({path,"/organization_id"}));
list(boundary::field(v,"unit_ids",path),r.unit_ids,checkpoint_decode_boundary_join({path,"/unit_ids"}));
dec_pairs(boundary::field(v,"commanders",path),r.commanders,checkpoint_decode_boundary_join({path,"/commanders"}));
list(boundary::field(v,"member_actor_ids",path),r.member_actor_ids,checkpoint_decode_boundary_join({path,"/member_actor_ids"}));
retainers(boundary::field(v,"retainers",path),r.retainers,checkpoint_decode_boundary_join({path,"/retainers"}));
dec_pairs(boundary::field(v,"goods",path),r.goods,checkpoint_decode_boundary_join({path,"/goods"}));
dec(boundary::field(v,"money",path),r.money,checkpoint_decode_boundary_join({path,"/money"}));
dec(boundary::field(v,"source_inventory_id",path),r.source_inventory_id,checkpoint_decode_boundary_join({path,"/source_inventory_id"}));
dec(boundary::field(v,"source_wallet_id",path),r.source_wallet_id,checkpoint_decode_boundary_join({path,"/source_wallet_id"}));
}
Status emit_fields(ProjectionWriter& w,const ArmyFormationTerms& r){
emit_value(w,"order_id",r.order_id);
emit_value(w,"office_id",r.office_id);
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"ownership_id",r.ownership_id);
emit_value(w,"place_id",r.place_id);
emit_value(w,"organization_id",r.organization_id);
emit_value(w,"unit_ids",r.unit_ids);
emit_value(w,"commanders",r.commanders);
emit_value(w,"member_actor_ids",r.member_actor_ids);
emit_retainers(w,"retainers",r.retainers);
emit_value(w,"goods",r.goods);
emit_value(w,"money",r.money);
emit_value(w,"source_inventory_id",r.source_inventory_id);
emit_value(w,"source_wallet_id",r.source_wallet_id);w.order_fields(r.field_order);return w.status();}
V enc(const ArmyFormationTerms& r){return row_json(r);}
Status visit(CodecVisitor& c,ArmyFormationTerms& r){
c.field("order_id",r.order_id);
c.field("office_id",r.office_id);
c.field("actor_id",r.actor_id);
c.field("garrison_id",r.garrison_id);
c.field("ownership_id",r.ownership_id);
c.field("place_id",r.place_id);
c.field("organization_id",r.organization_id);
c.sequence("unit_ids",r.unit_ids,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("commanders",r.commanders,[](auto& codec,auto& pair){codec.field("key",pair.first);codec.field("value",pair.second);return codec.status();});
c.sequence("member_actor_ids",r.member_actor_ids,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("retainers",r.retainers,[](auto& codec,auto& value){return visit(codec,value);});
c.sequence("goods",r.goods,[](auto& codec,auto& pair){codec.field("key",pair.first);codec.field("value",pair.second);return codec.status();});
c.field("money",r.money);
c.field("source_inventory_id",r.source_inventory_id);
c.field("source_wallet_id",r.source_wallet_id);
visit_order(c,r.field_order);return c.status();}
// MilitarySupplyFact完整字段及缺省存在位只在该域定义一次。
void dec(const V& v,MilitarySupplyFact& r,const StableId& path){
r.field_order=order(v);
boundary::fields(v,{"organization_id","ownership_id","army_id","garrison_id","tick","item_id","mouth_ids","required","consumed","shortage","cause_fact_ids"},{},path);
dec(boundary::field(v,"organization_id",path),r.organization_id,checkpoint_decode_boundary_join({path,"/organization_id"}));
dec(boundary::field(v,"ownership_id",path),r.ownership_id,checkpoint_decode_boundary_join({path,"/ownership_id"}));
dec(boundary::field(v,"army_id",path),r.army_id,checkpoint_decode_boundary_join({path,"/army_id"}));
dec(boundary::field(v,"garrison_id",path),r.garrison_id,checkpoint_decode_boundary_join({path,"/garrison_id"}));
dec(boundary::field(v,"tick",path),r.tick,checkpoint_decode_boundary_join({path,"/tick"}));
dec(boundary::field(v,"item_id",path),r.item_id,checkpoint_decode_boundary_join({path,"/item_id"}));
list(boundary::field(v,"mouth_ids",path),r.mouth_ids,checkpoint_decode_boundary_join({path,"/mouth_ids"}));
dec(boundary::field(v,"required",path),r.required,checkpoint_decode_boundary_join({path,"/required"}));
dec(boundary::field(v,"consumed",path),r.consumed,checkpoint_decode_boundary_join({path,"/consumed"}));
dec(boundary::field(v,"shortage",path),r.shortage,checkpoint_decode_boundary_join({path,"/shortage"}));
list(boundary::field(v,"cause_fact_ids",path),r.cause_fact_ids,checkpoint_decode_boundary_join({path,"/cause_fact_ids"}));
}
Status emit_fields(ProjectionWriter& w,const MilitarySupplyFact& r){
emit_value(w,"organization_id",r.organization_id);
emit_value(w,"ownership_id",r.ownership_id);
emit_value(w,"army_id",r.army_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"tick",r.tick);
emit_value(w,"item_id",r.item_id);
emit_value(w,"mouth_ids",r.mouth_ids);
emit_value(w,"required",r.required);
emit_value(w,"consumed",r.consumed);
emit_value(w,"shortage",r.shortage);emit_value(w,"cause_fact_ids",r.cause_fact_ids);return w.status();}
V enc(const MilitarySupplyFact& r){return row_json(r);}
Status visit(CodecVisitor& c,MilitarySupplyFact& r){
c.field("id",r.id);
c.field("organization_id",r.organization_id);
c.field("ownership_id",r.ownership_id);
c.field("army_id",r.army_id);
c.field("garrison_id",r.garrison_id);
c.field("tick",r.tick);
c.field("item_id",r.item_id);
c.sequence("mouth_ids",r.mouth_ids,[](auto& codec,auto& value){return visit(codec,value);});
c.field("required",r.required);
c.field("consumed",r.consumed);
c.field("shortage",r.shortage);
c.sequence("cause_fact_ids",r.cause_fact_ids,[](auto& codec,auto& value){return visit(codec,value);});
visit_order(c,r.field_order);return c.status();}
// 抵达事实使用原fact身份，完整字段序在该事实自身保存。
Status emit_fields(ProjectionWriter& w,const CampaignArrivalFact& f){w.field("kind",StableId("campaign_arrival"));w.field("campaign_id",f.campaign_id);w.field("army_id",f.army_id);w.field("actor_id",f.actor_id);w.field("tick",f.tick);w.field("stronghold_id",f.stronghold_id);w.order_fields(f.field_order);return w.status();}
V enc(const CampaignArrivalFact& f){return row_json(f);}
void dec(const V& v,CampaignArrivalFact& f,const StableId& path){boundary::fields(v,{"kind","campaign_id","army_id","actor_id","tick","stronghold_id"},{},path);if(boundary::get_string(v,"kind",path)!="campaign_arrival")boundary::fail("CampaignArrivalMissing",path);f.campaign_id=boundary::get_string(v,"campaign_id",path);f.army_id=boundary::get_string(v,"army_id",path);f.actor_id=boundary::get_string(v,"actor_id",path);f.tick=boundary::get_int(v,"tick",path);f.stronghold_id=boundary::get_string(v,"stronghold_id",path);f.field_order=order(v);}
Status visit(CodecVisitor& c,CampaignArrivalFact& f){c.field("id",f.id);c.field("campaign_id",f.campaign_id);c.field("army_id",f.army_id);c.field("actor_id",f.actor_id);c.field("tick",f.tick);c.field("stronghold_id",f.stronghold_id);visit_order(c,f.field_order);return c.status();}
} // 基本记录字段定义结束。
namespace {
using namespace military_codec;
// 条款的变体由真实office_id/order_id形状决定；不把未知军务条款压成字典。
MilitaryTerms decode_terms(const V& v,const StableId& path){
 auto* budget=ScopedCheckpointJsonDecode::current();
 if(v.find("unit_ids")){if(budget)budget->default_values<ArmyFormationTerms>();ArmyFormationTerms t;dec(v,t,path);return t;}
 if(budget)budget->default_values<OfficeTerms>();OfficeTerms t;dec(v,t,path);return t;
}
Status emit_terms(ProjectionWriter& w,const MilitaryTerms& terms){return std::visit([&](const auto& t){return military_codec::emit_fields(w,t);},terms);}
V encode_terms(const MilitaryTerms& terms){return std::visit([](const auto& t){return enc(t);},terms);}
Status visit_terms(CodecVisitor& c,MilitaryTerms& terms){Integer index=terms.index();c.field("kind",index);if(index<0||index>1)return c.reject("InvalidMilitaryTerms");if(c.reading()){if(index==0)terms=OfficeTerms{};else terms=ArmyFormationTerms{};}return std::visit([&](auto& t){return visit(c,t);},terms);}
Status emit_request(ProjectionWriter& w,const std::variant<OfficeAccept,ArmyForm>& request){if(auto o=std::get_if<OfficeAccept>(&request)){w.field("office_id",o->office_id);w.object("context",[&](auto& c){return emit_interaction_context_fields(c,o->context);});}else{const auto& f=std::get<ArmyForm>(request);w.field("order_id",f.order_id);emit_value(w,"unit_ids",f.unit_ids);if(!f.retainer_ids.empty()||had(f.field_order,"retainer_ids"))emit_value(w,"retainer_ids",f.retainer_ids);if(f.food_quantity)w.field("food_quantity",*f.food_quantity);if(f.money)w.field("money",*f.money);}std::visit([&](const auto& r){w.order_fields(r.field_order);},request);return w.status();}
V encode_request(const std::variant<OfficeAccept,ArmyForm>& request){V out;BoundaryProjectionWriter sink(out);auto check=sink.object("",[&](auto& c){return emit_request(c,request);});if(check.ok)check=sink.finish();if(!check.ok)boundary::fail(check.error.original.code,check.error.original.path);return out;}
MilitaryActionResult decode_result(const V& v,const StableId& p){boundary::fields(v,{}, {"ok","office_id","army_id","order_id"},p);if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<MilitaryActionResult>();MilitaryActionResult r;r.field_order=order(v);if(auto x=v.find("ok"))r.ok=boundary::boolean(*x,checkpoint_decode_boundary_join({p,"/ok"}));if(auto x=v.find("office_id"))r.office_id=boundary::string(*x,checkpoint_decode_boundary_join({p,"/office_id"}));if(auto x=v.find("army_id"))r.army_id=boundary::string(*x,checkpoint_decode_boundary_join({p,"/army_id"}));if(auto x=v.find("order_id"))r.order_id=boundary::string(*x,checkpoint_decode_boundary_join({p,"/order_id"}));return r;}
Status emit_result(ProjectionWriter& w,const MilitaryActionResult& r){if(r.ok)w.field("ok",*r.ok);if(!r.office_id.empty()||had(r.field_order,"office_id"))w.field("office_id",r.office_id);if(!r.army_id.empty()||had(r.field_order,"army_id"))w.field("army_id",r.army_id);if(!r.order_id.empty()||had(r.field_order,"order_id"))w.field("order_id",r.order_id);w.order_fields(r.field_order);return w.status();}
V encode_result(const MilitaryActionResult& r){V out;BoundaryProjectionWriter sink(out);auto check=sink.object("",[&](auto& c){return emit_result(c,r);});if(check.ok)check=sink.finish();if(!check.ok)boundary::fail(check.error.original.code,check.error.original.path);return out;}
Status visit_result(CodecVisitor& c,MilitaryActionResult& r){c.optional("ok",r.ok,[](auto& v,auto& x){return v.field("value",x);});c.field("office_id",r.office_id);c.field("army_id",r.army_id);c.field("order_id",r.order_id);visit_order(c,r.field_order);return c.status();}
// 跨域fields提取只在冷边界发生，错误不会留下半个已安装军政域。
template<class T> void decode_table(const V& root,const StableId& name,std::vector<T>& out){for(auto& pair:boundary::object(boundary::field(root,name,"state"),"state/"+name)){T r;dec(pair.second,r,"state/"+name+"/"+pair.first);if(r.id!=pair.first)boundary::fail("InvalidMilitaryState","state/"+name+"/"+pair.first);out.push_back(std::move(r));}}
template<class T> V encode_table(const std::vector<T>& rows){D out;for(auto& r:rows)out.emplace_back(r.id,enc(r));return V{std::move(out)};}
}
// 命令参数类型错误先放入typed无效标记；正式preview按原ready/权限/条款顺序消费，不能提前变成codec首错。
namespace military_original_action_codec {
Outcome<OfficeTerms> decode_office_terms(const BoundaryValue& input,ScopedCheckpointJsonDecode& budget){
 static_assert(std::is_nothrow_move_constructible_v<OfficeTerms>,"original ordinary OfficeTerms transfer must not hide a copy");
 if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch","military_authority_time/original_terms");
 try{budget.default_values<OfficeTerms>();OfficeTerms out;military_codec::dec(input,out,checkpoint_decode_boundary_text("military_authority_time/original_terms"));return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
Status visit_office_terms(CodecVisitor& c,OfficeTerms& value){return military_codec::visit(c,value);}
Status emit_office_terms_fields(ProjectionWriter& w,const OfficeTerms& value){return military_codec::emit_fields(w,value);}

Outcome<MilitaryTerms> decode_terms(const BoundaryValue& value){try{return {true,hanmo::resident::decode_terms(value,checkpoint_decode_boundary_text("exact_military_action/original_terms")),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<MilitaryActionResult> decode_result(const BoundaryValue& value){try{return {true,hanmo::resident::decode_result(value,checkpoint_decode_boundary_text("exact_military_action/result")),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<MilitaryTerms> decode_terms(const BoundaryValue& input,ScopedCheckpointJsonDecode& budget){
 static_assert(std::is_nothrow_move_constructible_v<MilitaryTerms>,"ordinary original payload must transfer without a hidden copy");
 if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch","exact_military_action/original_terms");
 return decode_terms(input);
}
Status visit_terms(CodecVisitor& c,MilitaryTerms& terms){return hanmo::resident::visit_terms(c,terms);}
Outcome<MilitaryActionResult> decode_result(const BoundaryValue& input,ScopedCheckpointJsonDecode& budget){
 static_assert(std::is_nothrow_move_constructible_v<MilitaryActionResult>,"ordinary original payload must transfer without a hidden copy");
 if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch","exact_military_action/result");
 return decode_result(input);
}
Status visit_result(CodecVisitor& c,MilitaryActionResult& result){return hanmo::resident::visit_result(c,result);}
Status emit_terms_fields(ProjectionWriter& w,const MilitaryTerms& terms){return hanmo::resident::emit_terms(w,terms);}
Status emit_result_fields(ProjectionWriter& w,const MilitaryActionResult& result){return hanmo::resident::emit_result(w,result);}
}
Outcome<MilitaryCommand> decode_military_command(const StableId& kind,const BoundaryValue& p){try{
    if(kind=="military_tactic_practice"){auto command=military_tactic_practice_codec::decode_command(p);if(!command.ok)return {false,{},command.error};return {true,MilitaryCommand{std::move(command.value)},{}};}
    if(kind=="military_field_command_assign"){auto command=military_field_command_codec::decode_command(p);if(!command.ok)return {false,{},command.error};return {true,MilitaryCommand{std::move(command.value)},{}};}
    if(kind=="military_officer_assign"){auto command=military_officer_codec::decode_command(p);if(!command.ok)return {false,{},command.error};return {true,MilitaryCommand{std::move(command.value)},{}};}
    if(training_codec::command_supported(kind))return training_codec::decode_command(kind,p);
    if(kind=="siege_start"||kind=="siege_join"||kind=="siege_plan"||kind=="siege_strategy"||kind=="siege_close")return decode_siege_command(kind,p);
    if(lifecycle_codec::command_supported(kind))return lifecycle_codec::decode_command(kind,p);
    if(!p.as<BoundaryValue::Dict>())return {false,{},common_failure("InvalidCommand").error};
    std::vector<StableId> allowed;if(kind=="office_accept")allowed={"office_id","context"};else if(kind=="campaign_accept")allowed={"template_id","context","recipient_army_id"};else if(kind=="army_form")allowed={"order_id","unit_ids","retainer_ids","food_quantity","money"};else if(kind=="campaign_depart")allowed={"campaign_id"};else if(kind=="military_plan")allowed={"battle_id","orders"};else if(kind=="military_close")allowed={"battle_id"};else if(kind=="military_request")allowed={"battle_id","request_key","request_kind","unit_id","target_unit_id"};
    if(!allowed.empty())for(auto& field:*p.as<BoundaryValue::Dict>())if(std::find(allowed.begin(),allowed.end(),field.first)==allowed.end()){auto error=domain_codec::unsupported("military",kind,"params/"+field.first);error.phase="command";return {false,{},error};}
    auto text=[&](const char* key){auto x=p.find(key);return x&&x->as<StableId>()?*x->as<StableId>():StableId{};};
    auto context=[&](){InteractionContext c;auto value=p.find("context");if(!value||!value->as<BoundaryValue::Dict>()){c.input_error="MissingInteractionContext";return c;}try{return interaction_codec::decode_context(*value,"context");}catch(const boundary::DecodeFailure&){c.input_error="InvalidInteractionContext";return c;}};
    if(kind=="office_accept"){OfficeAccept r;r.office_id=text("office_id");r.context=context();r.field_order=order(p);return {true,r,{}};}
    if(kind=="campaign_accept"){CampaignAccept r;r.template_id=text("template_id");r.context=context();if(auto x=p.find("recipient_army_id");x&&x->as<StableId>())r.recipient_army_id=*x->as<StableId>();r.field_order=order(p);return {true,r,{}};}
    if(kind=="army_form"){ArmyForm r;r.order_id=text("order_id");r.field_order=order(p);auto strings=[&](const char* name,std::vector<StableId>& out,bool& valid,bool optional){auto x=p.find(name);if(!x&&optional)return;if(!x||!x->as<A>()){valid=false;return;}for(auto& v:*x->as<A>()){if(!v.as<StableId>()){valid=false;return;}out.push_back(*v.as<StableId>());}};strings("unit_ids",r.unit_ids,r.unit_ids_valid,false);strings("retainer_ids",r.retainer_ids,r.retainer_ids_valid,true);if(auto x=p.find("food_quantity")){if(x->as<Integer>())r.food_quantity=*x->as<Integer>();else r.food_quantity_valid=false;}if(auto x=p.find("money")){if(x->as<Integer>())r.money=*x->as<Integer>();else r.money_valid=false;}return {true,r,{}};}
    if(kind=="campaign_depart"){CampaignDepart r;r.campaign_id=text("campaign_id");r.field_order=order(p);return {true,r,{}};}
    if(kind=="military_request"){MilitaryBattleRequest r;r.battle_id=text("battle_id");r.request_key=text("request_key");r.request_kind=text("request_kind");r.unit_id=text("unit_id");r.target_unit_id=text("target_unit_id");for(const auto* key:{"battle_id","request_key","request_kind","unit_id"})if(!p.find(key)||!p.find(key)->as<StableId>())r.fields_valid=false;if(auto target=p.find("target_unit_id");target&&!target->as<StableId>())r.fields_valid=false;r.field_order=order(p);if(!p.find("target_unit_id"))r.field_order.push_back("target_unit_id");return {true,std::move(r),{}};}
    if(kind=="military_close"){MilitaryBattleClose r;r.battle_id=text("battle_id");r.field_order=order(p);return {true,r,{}};}
    if(kind=="military_plan"){MilitaryBattlePlan r;r.battle_id=text("battle_id");r.field_order=order(p);auto x=p.find("orders");if(!x||!x->as<D>())r.orders_valid=false;else for(auto& row:*x->as<D>()){MilitaryOrder o;battle_codec::decode(row.second,o,"orders/"+row.first);r.orders.emplace_back(row.first,std::move(o));}return {true,std::move(r),{}};}
    auto error=domain_codec::unsupported("military",kind,"");error.phase="command";return {false,{},error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 报价JSON只在显式边界生成，字段与计算结果归军事owner；D只附加公共revision。
Outcome<BoundaryValue> encode_military_quote(const MilitaryQuote& q,const MilitaryCommand& command){try{
    if(std::holds_alternative<MilitaryTacticPracticeClaim>(command)){if(!q.tactic_practice)return {false,{},common_failure("MilitaryTacticPracticeQuoteMissing").error};return {true,military_tactic_practice_codec::encode_quote(*q.tactic_practice),{}};}
    if(std::holds_alternative<MilitaryFieldCommandAssign>(command)){if(!q.field_command)return {false,{},common_failure("MilitaryFieldCommandQuoteMissing").error};return {true,military_field_command_codec::encode_quote(*q.field_command),{}};}
    if(std::holds_alternative<MilitaryOfficerAssign>(command)){if(!q.officer_assignment)return {false,{},common_failure("MilitaryOfficerQuoteMissing").error};return {true,military_officer_codec::encode_quote(*q.officer_assignment),{}};}
    if(std::holds_alternative<MilitaryTrain>(command)||std::holds_alternative<MilitaryTrainCancel>(command)||std::holds_alternative<MilitaryDrillCommand>(command))return training_codec::encode_quote(q,command);
    if(std::holds_alternative<SiegeStart>(command)||std::holds_alternative<SiegeJoin>(command)||std::holds_alternative<SiegePlan>(command)||std::holds_alternative<SiegeStrategy>(command)||std::holds_alternative<SiegeClose>(command))return encode_siege_quote(q,command);
    if(std::holds_alternative<CampaignManage>(command)||std::holds_alternative<ArmyLogistics>(command)||std::holds_alternative<OfficeResign>(command)||std::holds_alternative<ArmyPledge>(command))return lifecycle_codec::encode_quote(q,command);
    if(std::holds_alternative<MilitaryBattleRequest>(command)){if(!q.request_receipt)return {false,{},common_failure("MissingMilitaryRequestQuote").error};const auto& r=*q.request_receipt;return {true,boundary::obj({{"ok",V{true}},{"battle_id",V{q.battle_id}},{"request_kind",V{r.request.request_kind}},{"unit_id",V{r.request.unit_id}},{"commander_actor_id",V{r.commander_actor_id}},{"already_completed",V{q.request_replayed}},{"half_days",V{Integer(0)}}}),{}};}
    if(std::holds_alternative<MilitaryBattlePlan>(command)||std::holds_alternative<MilitaryBattleClose>(command)){D d{{"ok",V{true}},{"battle_id",V{q.battle_id}}};if(std::holds_alternative<MilitaryBattlePlan>(command)){D orders;for(auto& row:q.battle_orders)orders.emplace_back(row.first,battle_codec::encode(row.second));d.emplace_back("orders",V{std::move(orders)});}return {true,V{std::move(d)},{}};}
    if(std::holds_alternative<CampaignDepart>(command))return {true,V{D{{"ok",V{true}},{"campaign_id",V{q.campaign_id}}}},{}};
    if(std::holds_alternative<CampaignAccept>(command)){D d{{"ok",V{true}},{"template_id",V{q.template_id}}};if(q.context)d.emplace_back("context",interaction_codec::encode_context(*q.context));d.emplace_back("recipient_army_id",V{q.recipient_army_id});return {true,V{d},{}};}
    std::variant<OfficeAccept,ArmyForm> request;if(auto o=std::get_if<OfficeAccept>(&command))request=*o;else if(auto f=std::get_if<ArmyForm>(&command))request=*f;else return {false,{},domain_codec::unsupported("military","campaign_depart","")};
    D d{{"ok",V{true}},{"duration",V{q.duration}},{"params",encode_request(request)}};if(q.context)d.emplace_back("context",interaction_codec::encode_context(*q.context));d.emplace_back("participants",enc_list(q.participants));if(q.terms)d.emplace_back("terms",encode_terms(*q.terms));if(q.provisions){auto& p=*q.provisions;d.emplace_back("provisions",V{D{{"ok",V{true}},{"food_item_id",V{p.food_item_id}},{"mouth_count",V{p.mouth_count}},{"required",V{p.required}},{"available",V{p.available}},{"full_ticks",V{p.full_ticks}},{"shortage",V{p.shortage}}}});}return {true,V{d},{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 冷边界只确认所借原正文的唯一头位置，不替代 office/form 的时间与资源来源门。
Outcome<bool> military_action_time_owns_fact_header(
    const MilitaryActionTimeState& state, const FactRecord& header) {
  if (header.kind != "military_action" &&
      header.kind != "military_office_completion_applied" &&
      header.kind != "military_action_completion_applied" &&
      header.kind != "military_formation_completion_checked")
    return {true, false, {}};
  Integer owners = 0;
  for (const auto& row : state.dynamic_action_facts.rows()) {
    const auto& source = row.second;
    if (source.id != row.first ||
        (source.command != "office_accept" && source.command != "army_form"))
      return {false, false, common_failure("ExactMilitaryActionKeyMismatch", row.first).error};
    if (header.kind == "military_action" && header.id == row.first)
      ++owners;
    if (source.terminal_source_ref &&
        source.terminal_source_ref->id == header.id &&
        source.terminal_source_ref->kind == header.kind &&
        source.terminal_source_ref->domain == header.domain)
      ++owners;
  }
  if (owners > 1 || (owners && header.domain != "military"))
    return {false, false, common_failure("MilitaryOriginalSourceCoverageMismatch", header.id).error};
  return {true, owners == 1, {}};
}
// 活行动空参数只是一份已在 ActionTime 持有完整条款的投影，不能创建原执行源。
Outcome<bool> military_action_time_owns_active_action(
    const MilitaryActionTimeState& state, const ActionCoreView& action) {
  const auto* original = state.dynamic_action_facts.find(action.id);
  if (!original)
    return {true, false, {}};
  if (original->id != action.id || original->phase != "started" ||
      original->command != action.kind || original->actor_id != action.actor_id ||
      (action.kind != "office_accept" && action.kind != "army_form"))
    return {false, false, common_failure("MilitaryOriginalActiveActionMismatch", action.id).error};
  return {true, true, {}};
}
Outcome<MilitaryState> decode_military_seed(const BoundaryValue& root,Integer occupation_source_schema){return decode_military_seed(root,occupation_source_schema,MilitaryHeaderBootstrapSelection{});}
Outcome<MilitaryState> decode_military_seed(const BoundaryValue& root,Integer occupation_source_schema,const MilitaryHeaderBootstrapSelection& selected){try{if(occupation_source_schema!=1&&occupation_source_schema!=2)boundary::fail("UnsupportedMilitaryOccupationSchema","military");
    if((selected.authority_time_schema!=0&&selected.authority_time_schema!=1)||(selected.action_time_schema!=0&&selected.action_time_schema!=1)||(selected.training_completion_time_schema!=0&&selected.training_completion_time_schema!=1)||(selected.training_completion_time_schema==1&&selected.action_time_schema!=1)||(selected.battle_lifecycle_schema!=0&&selected.battle_lifecycle_schema!=1))boundary::fail("UnsupportedMilitaryHeaderBootstrapSelection","military");
    if(selected.action_time && selected.action_time_schema != 1)
      boundary::fail("MilitaryActionTimeSelectedSchemaRequired", "military");
    MilitaryState s;decode_table(root,"offices",s.offices);decode_table(root,"garrisons",s.garrisons);decode_table(root,"military_units",s.military_units);decode_table(root,"armies",s.armies);decode_table(root,"campaigns",s.campaigns);decode_table(root,"strongholds",s.strongholds);
    for(auto& row:boundary::object(boundary::field(root,"sieges","state"),"sieges")){SiegeRecord siege;siege_codec::decode(row.second,siege,"sieges/"+row.first);if(siege.id!=row.first)boundary::fail("InvalidSiegeState",row.first);s.sieges.push_back(std::move(siege));}
    for(auto& row:boundary::object(boundary::field(root,"battles","state"),"battles")){auto kind=boundary::get_string(row.second,"kind","battles");if(kind=="personal")continue;if(kind!="military")return {false,{},domain_codec::unsupported("military",kind,row.first)};MilitaryBattleRecord battle;battle_codec::decode(row.second,battle,"battles/"+row.first);if(battle.id!=row.first)boundary::fail("InvalidMilitarySession","battles/"+row.first);s.battles.push_back(std::move(battle));}
    for(auto& pair:boundary::object(boundary::field(root,"actions","state"),"actions")){auto kind=boundary::get_string(pair.second,"kind","actions");if(training_codec::decode_action(kind,pair.second,pair.first,s)||lifecycle_codec::decode_action(kind,pair.second,pair.first,s))continue;if(kind!="office_accept"&&kind!="army_form")continue;auto& p=boundary::field(pair.second,"params","actions");
        if(selected.action_time) {
          ActionCoreView action;
          action.id=pair.first; action.kind=kind;
          action.actor_id=boundary::get_string(pair.second,"actor_id","actions");
          auto owned=military_action_time_owns_active_action(*selected.action_time,action);
          if(!owned.ok)return {false,{},std::move(owned.error)};
          if(owned.value) {
            if(!boundary::object(p,"actions/params").empty())
              boundary::fail("MilitaryExactActionParamsMustBeEmpty",pair.first);
            continue;
          }
        }
        boundary::fields(p,{"terms","request"},{"context"},"actions/"+pair.first);MilitaryActionPayload payload;payload.field_order=order(p);payload.action_id=pair.first;payload.terms=decode_terms(boundary::field(p,"terms","action"),"action/terms");auto request=decode_military_command(kind,boundary::field(p,"request","action"));if(!request.ok){if(request.error.kind==ErrorKind::unsupported_capability)request.error.phase="decode";return {false,{},request.error};}if(auto o=std::get_if<OfficeAccept>(&request.value)){if(o->office_id.empty()||o->context.input_error)boundary::fail("InvalidMilitaryAction","action/request");payload.request=*o;}else{auto f=std::get<ArmyForm>(request.value);if(f.order_id.empty()||!f.unit_ids_valid||!f.retainer_ids_valid||!f.food_quantity_valid||!f.money_valid)boundary::fail("InvalidMilitaryAction","action/request");payload.request=f;}if(auto context=p.find("context"))payload.context=interaction_codec::decode_context(*context,"actions/context");s.actions.push_back(payload);}
    for(auto& pair:boundary::object(boundary::field(root,"facts","state"),"facts")){auto kind=boundary::get_string(pair.second,"kind","facts");
        bool typed_header=false;
        if(selected.action_time) {
          FactRecord header; header.id=pair.first; header.kind=kind;
          if(auto* domain=pair.second.find("domain"))
            header.domain=boundary::string(*domain,"facts/domain");
          auto owned=military_action_time_owns_fact_header(*selected.action_time,header);
          if(!owned.ok)return {false,{},std::move(owned.error)};
          typed_header=owned.value;
        }
        if(kind=="military_council" && selected.field_battle) {
          auto found=selected.field_battle->council_facts.find(pair.first);
          typed_header=found!=selected.field_battle->council_facts.end();
        }
        const bool selected_header=typed_header||(kind=="campaign_accepted"&&selected.authority_time_schema==1)||((kind=="military_office_completion_applied"||kind=="military_formation_completion_checked")&&selected.action_time_schema==1)||(kind=="military_training_completion_applied"&&selected.action_time_schema==1&&selected.training_completion_time_schema==1)||((kind=="military_battle_started"||kind=="military_battle_closed")&&selected.battle_lifecycle_schema==1);
        if(selected_header){auto unique=military_officer_codec::validate_json_unique_keys(pair.second);if(!unique.ok)return {false,{},unique.error};boundary::fields(pair.second,{"kind","domain"},{},"facts");if(pair.first.empty()||boundary::get_string(pair.second,"domain","facts")!="military")boundary::fail("InvalidMilitaryHeaderBootstrap","facts");continue;}
        if(kind=="military_integration")continue;if(kind=="military_siege_engineering_prepared"){auto unique=military_officer_codec::validate_json_unique_keys(pair.second);if(!unique.ok)return {false,{},unique.error};auto copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& f){return f.first=="kind";}),fields.end());auto p=military_siege_engineering_codec::decode_fact(copy);if(!p.ok)return {false,{},p.error};if(p.value.id!=pair.first)boundary::fail("MilitarySiegeEngineeringIdentityMismatch",pair.first);s.siege_engineering.preparations.push_back(std::move(p.value));continue;}bool lifecycle_kind=false;for(int n=0;n<=8;++n)if(kind==military_siege_lifecycle_kind_name(static_cast<MilitarySiegeLifecycleKind>(n)))lifecycle_kind=true;if(lifecycle_kind){auto f=military_siege_lifecycle_codec::decode_fact(pair.second);if(!f.ok)return {false,{},f.error};if(f.value.id!=pair.first)boundary::fail("MilitarySiegeLifecycleIdentityMismatch",pair.first);s.siege_lifecycle.facts.push_back(std::move(f.value));continue;}if(kind=="military_garrison_rest"){auto guard=military_officer_codec::validate_json_unique_keys(pair.second);if(!guard.ok)return {false,{},guard.error};V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());auto f=military_garrison_rest_codec::decode_fact(copy);if(!f.ok)return {false,{},f.error};if(f.value.id!=pair.first)boundary::fail("MilitaryGarrisonRestIdentityMismatch",pair.first);s.garrison_rest.facts.push_back(std::move(f.value));continue;}if(kind=="military_rout_entry"){auto guard=military_officer_codec::validate_json_unique_keys(pair.second);if(!guard.ok)return {false,{},guard.error};V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());auto fact=military_rout_codec::decode_entry(copy);if(!fact.ok)return {false,{},fact.error};if(fact.value.id!=pair.first)boundary::fail("MilitaryRoutIdentityMismatch",pair.first);s.rout.entries.push_back(std::move(fact.value));continue;}if(kind=="military_rout_round"){auto guard=military_officer_codec::validate_json_unique_keys(pair.second);if(!guard.ok)return {false,{},guard.error};V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());auto fact=military_rout_codec::decode_round(copy);if(!fact.ok)return {false,{},fact.error};if(fact.value.id!=pair.first)boundary::fail("MilitaryRoutIdentityMismatch",pair.first);s.rout.rounds.push_back(std::move(fact.value));continue;}if(kind=="military_supply_attrition"){auto guard=military_officer_codec::validate_json_unique_keys(pair.second);if(!guard.ok)return {false,{},guard.error};V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());auto f=military_supply_attrition_codec::decode_fact(copy);if(!f.ok)return {false,{},f.error};if(f.value.id!=pair.first)boundary::fail("MilitarySupplyAttritionIdentityMismatch",pair.first);s.supply_attrition.facts.push_back(std::move(f.value));continue;}if(kind=="military_siege_released"){auto guard=military_officer_codec::validate_json_unique_keys(pair.second);if(!guard.ok)return {false,{},guard.error};V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());auto f=military_recovery_codec::decode_siege_release(copy);if(!f.ok)return {false,{},f.error};if(f.value.id!=pair.first)boundary::fail("MilitarySiegeReleaseIdentityMismatch",pair.first);s.recovery.siege_releases.push_back(std::move(f.value));continue;}if(kind=="military_battle_released"){auto guard=military_officer_codec::validate_json_unique_keys(pair.second);if(!guard.ok)return {false,{},guard.error};V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());auto f=military_recovery_codec::decode_release(copy);if(!f.ok)return {false,{},f.error};if(f.value.id!=pair.first)boundary::fail("MilitaryBattleReleaseIdentityMismatch",pair.first);s.recovery.battle_releases.push_back(std::move(f.value));continue;}if(kind=="military_wounded_recovery"){auto guard=military_officer_codec::validate_json_unique_keys(pair.second);if(!guard.ok)return {false,{},guard.error};V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());auto f=military_recovery_codec::decode_fact(copy);if(!f.ok)return {false,{},f.error};if(f.value.id!=pair.first)boundary::fail("MilitaryRecoveryIdentityMismatch",pair.first);s.recovery.facts.push_back(std::move(f.value));continue;}if(kind=="military_field_command_assigned"){auto guard=military_officer_codec::validate_json_unique_keys(pair.second);if(!guard.ok)return {false,{},guard.error};V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());auto f=military_field_command_codec::decode(copy);if(!f.ok)return {false,{},f.error};if(f.value.id!=pair.first)boundary::fail("MilitaryFieldCommandIdentityMismatch",pair.first);s.field_commands.push_back(std::move(f.value));continue;}if(kind=="military_officer_assignment"){auto guard=military_officer_codec::validate_json_unique_keys(pair.second);if(!guard.ok)return {false,{},guard.error};V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());auto f=military_officer_codec::decode(copy);if(!f.ok)return {false,{},f.error};if(f.value.id!=pair.first)boundary::fail("MilitaryOfficerIdentityMismatch",pair.first);s.officer_assignments.push_back(std::move(f.value));continue;}if(kind=="military_request"){V copy=pair.second;auto& fields=*copy.as<D>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& field){return field.first=="kind";}),fields.end());MilitaryRequestReceipt receipt;military_request_codec::decode(copy,receipt,"facts/"+pair.first);if(receipt.id!=pair.first)boundary::fail("MilitaryRequestIdentityMismatch",pair.first);s.request_facts.push_back(std::move(receipt));continue;}if(training_codec::decode_fact(kind,pair.second,pair.first,s)||march_codec::decode_fact(kind,pair.second,pair.first,s)||lifecycle_codec::decode_fact(kind,pair.second,pair.first,s)||occupation_codec::decode_fact(kind,pair.second,pair.first,s,occupation_source_schema))continue;if(kind=="military_supply"){V copy=pair.second;auto& dict=*copy.as<D>();dict.erase(std::remove_if(dict.begin(),dict.end(),[](auto& p){return p.first=="kind";}),dict.end());MilitarySupplyFact f;dec(copy,f,"facts/"+pair.first);f.id=pair.first;f.field_order=order(pair.second);s.supply_facts.push_back(f);}else if(kind=="military_action"){
        auto& v=pair.second;boundary::fields(v,{"kind","command","actor_id","start_tick","completed_tick","phase","success","terms","context","result"},{},"facts/"+pair.first);MilitaryActionFact f;f.field_order=order(v);f.id=pair.first;f.command=boundary::get_string(v,"command","fact");if(f.command!="office_accept"&&f.command!="army_form")return {false,{},domain_codec::unsupported("military",f.command,pair.first)};f.actor_id=boundary::get_string(v,"actor_id","fact");f.start_tick=boundary::get_int(v,"start_tick","fact");f.completed_tick=boundary::get_int(v,"completed_tick","fact");f.phase=boundary::get_string(v,"phase","fact");f.success=boundary::get_bool(v,"success","fact");f.terms=decode_terms(boundary::field(v,"terms","fact"),"fact/terms");auto& context=boundary::field(v,"context","fact");if(!boundary::object(context,"fact/context").empty())f.context=interaction_codec::decode_context(context,"fact/context");auto& result=boundary::field(v,"result","fact");if(!boundary::object(result,"fact/result").empty()||f.phase=="completed")f.result=decode_result(result,"fact/result");s.action_facts.push_back(f);
    }else if(kind=="military_contact"){MilitaryContactFact f;battle_codec::decode(pair.second,f,"facts/"+pair.first);f.id=pair.first;s.contact_facts.push_back(std::move(f));}else if(kind=="military_battle"){MilitaryBattleFact f;battle_codec::decode(pair.second,f,"facts/"+pair.first);f.id=pair.first;s.battle_facts.push_back(std::move(f));}else if(kind=="military_casualties"){MilitaryCasualtyFact f;battle_codec::decode(pair.second,f,"facts/"+pair.first);f.id=pair.first;s.casualty_facts.push_back(std::move(f));}else if(kind=="campaign_arrival"){CampaignArrivalFact arrival;dec(pair.second,arrival,"facts/"+pair.first);arrival.id=pair.first;s.arrival_facts.push_back(std::move(arrival));}else if(kind.rfind("military_",0)==0||kind.rfind("campaign_",0)==0||kind=="office_resignation")return {false,{},domain_codec::unsupported("military",kind,pair.first)};}
    return {true,std::move(s),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
namespace {
Status emit_original_action_payload(ProjectionWriter& c,const MilitaryActionPayload& p){c.object("terms",[&](auto& t){return emit_terms(t,p.terms);});c.object("request",[&](auto& r){return emit_request(r,p.request);});if(p.context)c.object("context",[&](auto& context){return emit_interaction_context_fields(context,*p.context);});return c.status();}
Status emit_original_action_fact(ProjectionWriter& c,const MilitaryActionFact& f){c.field("kind",StableId("military_action"));c.field("command",f.command);c.field("actor_id",f.actor_id);c.field("start_tick",f.start_tick);c.field("completed_tick",f.completed_tick);c.field("phase",f.phase);c.field("success",f.success);c.object("terms",[&](auto& t){return emit_terms(t,f.terms);});c.object("context",[&](auto& context){return f.context?emit_interaction_context_fields(context,*f.context):context.status();});c.object("result",[&](auto& result){return f.result?emit_result(result,*f.result):result.status();});return c.status();}
Status emit_original_action_fields(ProjectionWriter& w,const MilitaryState& s){for(const auto& p:s.actions)w.object(p.action_id,p.field_order,[&](auto& c){return emit_original_action_payload(c,p);});return w.status();}
Status emit_early_military_facts(ProjectionWriter& w,const MilitaryState& s){
 for(const auto& p:s.siege_engineering.preparations)w.object(p.id,[&](auto& c){return emit_tagged_military_fields(c,StableId(military_siege_engineering_fact_kind),p.field_order,[&](auto& row,bool order){return military_siege_engineering_codec::emit_fields(row,p,order);});});
 for(const auto& f:s.siege_lifecycle.facts)w.object(f.id,[&](auto& c){return military_siege_lifecycle_codec::emit_fields(c,f);});return w.status();}
Status emit_remaining_military_facts(ProjectionWriter& w,const MilitaryState& s){
 for(const auto& f:s.action_facts)w.object(f.id,f.field_order,[&](auto& c){return emit_original_action_fact(c,f);});
 for(const auto& f:s.supply_facts)w.object(f.id,f.field_order,[&](auto& c){c.field("kind",StableId("military_supply"));return military_codec::emit_fields(c,f);});
for(const auto& f:s.recovery.siege_releases)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_siege_released",f.field_order,[&](auto& row,bool order){return military_recovery_codec::emit_fields(row,f,order);});});
for(const auto& f:s.recovery.battle_releases)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_battle_released",f.field_order,[&](auto& row,bool order){return military_recovery_codec::emit_fields(row,f,order);});});
for(const auto& f:s.garrison_rest.facts)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_garrison_rest",f.field_order,[&](auto& row,bool order){return military_garrison_rest_codec::emit_fields(row,f,order);});});
for(const auto& f:s.rout.entries)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_rout_entry",f.field_order,[&](auto& row,bool order){return military_rout_codec::emit_fields(row,f,order);});});
for(const auto& f:s.rout.rounds)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_rout_round",f.field_order,[&](auto& row,bool order){return military_rout_codec::emit_fields(row,f,order);});});
for(const auto& f:s.supply_attrition.facts)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_supply_attrition",f.field_order,[&](auto& row,bool order){return military_supply_attrition_codec::emit_fields(row,f,order);});});
for(const auto& f:s.recovery.facts)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_wounded_recovery",f.field_order,[&](auto& row,bool order){return military_recovery_codec::emit_fields(row,f,order);});});
for(const auto& f:s.field_commands)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_field_command_assigned",f.field_order,[&](auto& row,bool order){return military_field_command_codec::emit_fields(row,f,order);});});
for(const auto& f:s.officer_assignments)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_officer_assignment",f.field_order,[&](auto& row,bool order){return military_officer_codec::emit_fields(row,f,order);});});
for(const auto& f:s.request_facts)w.object(f.id,[&](auto& c){return emit_tagged_military_fields(c,"military_request",f.field_order,[&](auto& row,bool order){return military_request_codec::emit_fields(row,f,order);});});
for(const auto& f:s.arrival_facts)emit_value(w,f.id,f);
 for(const auto& f:s.contact_facts)w.object(f.id,[&](auto& c){return battle_codec::emit_fields(c,f);});
 for(const auto& f:s.battle_facts)w.object(f.id,[&](auto& c){return battle_codec::emit_fields(c,f);});
 for(const auto& f:s.casualty_facts)w.object(f.id,[&](auto& c){return battle_codec::emit_fields(c,f);});return w.status();}
template<class T>Status emit_military_table(ProjectionWriter& w,const StableId& name,const std::vector<T>& rows){return w.object(name,[&](auto& c){for(const auto& row:rows)emit_value(c,row.id,row);return c.status();});}
Status emit_siege_fields(ProjectionWriter& w,const MilitaryState& s){for(const auto& row:s.sieges)w.object(row.id,[&](auto& c){return siege_codec::emit_fields(c,row);});return w.status();}
}
Status emit_military_action_payload_fields(ProjectionWriter& w,const MilitaryState& s){auto checked=emit_original_action_fields(w,s);if(!checked.ok)return checked;checked=training_codec::emit_action_payload_fields(w,s);if(!checked.ok)return checked;return lifecycle_codec::emit_action_payload_fields(w,s);}
Status emit_military_fact_fields(ProjectionWriter& w,const MilitaryState& s,Integer occupation_source_schema){auto checked=emit_early_military_facts(w,s);if(!checked.ok)return checked;checked=emit_remaining_military_facts(w,s);if(!checked.ok)return checked;checked=training_codec::emit_fact_fields(w,s);if(!checked.ok)return checked;checked=lifecycle_codec::emit_fact_fields(w,s);if(!checked.ok)return checked;checked=occupation_codec::emit_fact_fields(w,s,occupation_source_schema);if(!checked.ok)return checked;return march_codec::emit_fact_fields(w,s);}
Status emit_military_battle_fields(ProjectionWriter& w,const MilitaryState& s){for(const auto& row:s.battles)w.object(row.id,[&](auto& c){return battle_codec::emit_fields(c,row);});return w.status();}
// 在独立根中校验全部军事载荷，失败即停止，再输出原有状态字段。
Status emit_military_state_fields(ProjectionWriter &w, const MilitaryState &s,
                                  Integer occupation_source_schema) {
  // 保持原载荷先于六个根记录表的校验顺序；独立校验器已建立根对象。
  auto checked = w.validate_detached([&](auto &c) {
    c.object("early_facts",
             [&](auto &v) { return emit_early_military_facts(v, s); });
    c.object("actions",
             [&](auto &v) { return emit_original_action_fields(v, s); });
    c.object("remaining_facts",
             [&](auto &v) { return emit_remaining_military_facts(v, s); });
    c.object("training_actions", [&](auto &v) {
      return training_codec::emit_action_payload_fields(v, s);
    });
    c.object("training_facts",
             [&](auto &v) { return training_codec::emit_fact_fields(v, s); });
    c.object("lifecycle_actions", [&](auto &v) {
      return lifecycle_codec::emit_action_payload_fields(v, s);
    });
    c.object("lifecycle_facts",
             [&](auto &v) { return lifecycle_codec::emit_fact_fields(v, s); });
    c.object("occupation", [&](auto &v) {
      return occupation_codec::emit_fact_fields(v, s, occupation_source_schema);
    });
    c.object("march",
             [&](auto &v) { return march_codec::emit_fact_fields(v, s); });
    c.object("sieges", [&](auto &v) { return emit_siege_fields(v, s); });
    c.object("battles",
             [&](auto &v) { return emit_military_battle_fields(v, s); });
    return c.status();
  });
  if (!checked.ok)
    return checked;
  emit_military_table(w, "offices", s.offices);
  emit_military_table(w, "garrisons", s.garrisons);
  emit_military_table(w, "military_units", s.military_units);
  emit_military_table(w, "armies", s.armies);
  emit_military_table(w, "campaigns", s.campaigns);
  emit_military_table(w, "strongholds", s.strongholds);
  w.object("sieges", [&](auto &c) { return emit_siege_fields(c, s); });
  w.object("battles",
           [&](auto &c) { return emit_military_battle_fields(c, s); });
  w.object("action_params",
           [&](auto &c) { return emit_military_action_payload_fields(c, s); });
  w.object("facts", [&](auto &c) {
    return emit_military_fact_fields(c, s, occupation_source_schema);
  });
  return w.status();
}
namespace {
template<class T>const T* unique_military_record(const std::vector<T>& rows,const StableId& id,Integer& found){const T* result=nullptr;for(const auto& row:rows)if(row.id==id){++found;result=&row;}return result;}
template<class T>const T* unique_military_action(const std::vector<T>& rows,const StableId& id,Integer& found){const T* result=nullptr;for(const auto& row:rows)if(row.action_id==id){++found;result=&row;}return result;}
Outcome<bool> emitted_record(Status checked){return checked.ok?Outcome<bool>{true,true,{}}:Outcome<bool>{false,{},checked.error};}
}
Outcome<bool> emit_military_action_params_record_projection(ProjectionWriter& w,std::string_view name,const MilitaryState& s,const StableId& id){Integer count=0;const auto* original=unique_military_action(s.actions,id,count);const auto* training=unique_military_action(s.training_actions,id,count);const auto* logistics=unique_military_action(s.logistics_actions,id,count);if(count>1)return {false,{},common_failure("DuplicateMilitaryActionPayload",id).error};if(!count)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c){if(original){auto check=emit_original_action_payload(c,*original);if(check.ok)c.order_fields(original->field_order);return check.ok?c.status():check;}if(training){c.object("training",[&](auto& row){return training_codec::emit_fields(row,training->training);});c.order_fields(training->field_order);return c.status();}auto check=lifecycle_codec::emit_fields(c,*logistics);if(check.ok)c.order_fields(logistics->field_order);return check.ok?c.status():check;}));}
Outcome<bool> emit_military_fact_record_projection(ProjectionWriter& w,std::string_view name,const MilitaryState& s,const StableId& id,Integer occupation_source_schema){if(occupation_source_schema!=1&&occupation_source_schema!=2)return {false,{},common_failure("UnsupportedMilitaryOccupationSchema").error};Integer count=0;
const auto* engineering=unique_military_record(s.siege_engineering.preparations,id,count);
const auto* siege_lifecycle=unique_military_record(s.siege_lifecycle.facts,id,count);
const auto* action=unique_military_record(s.action_facts,id,count);
const auto* supply=unique_military_record(s.supply_facts,id,count);
const auto* siege_release=unique_military_record(s.recovery.siege_releases,id,count);
const auto* battle_release=unique_military_record(s.recovery.battle_releases,id,count);
const auto* rest=unique_military_record(s.garrison_rest.facts,id,count);
const auto* rout_entry=unique_military_record(s.rout.entries,id,count);
const auto* rout_round=unique_military_record(s.rout.rounds,id,count);
const auto* attrition=unique_military_record(s.supply_attrition.facts,id,count);
const auto* recovery=unique_military_record(s.recovery.facts,id,count);
const auto* field_command=unique_military_record(s.field_commands,id,count);
const auto* officer=unique_military_record(s.officer_assignments,id,count);
const auto* request=unique_military_record(s.request_facts,id,count);
const auto* arrival=unique_military_record(s.arrival_facts,id,count);
const auto* contact=unique_military_record(s.contact_facts,id,count);
const auto* battle=unique_military_record(s.battle_facts,id,count);
const auto* casualty=unique_military_record(s.casualty_facts,id,count);
const auto* training=unique_military_record(s.training_facts,id,count);
const auto* logistics=unique_military_record(s.logistics_facts,id,count);
const auto* returned=unique_military_record(s.return_facts,id,count);
const auto* muster=unique_military_record(s.muster_facts,id,count);
const auto* resigned=unique_military_record(s.resignation_facts,id,count);
const auto* delivered=unique_military_record(s.delivery_facts,id,count);
const auto* settled=unique_military_record(s.settlement_facts,id,count);
const auto* occupied=unique_military_record(s.occupation_facts,id,count);
const auto* relief=unique_military_record(s.relief_facts,id,count);
const auto* material=unique_military_record(s.siege_material_facts,id,count);
const auto* march=unique_military_record(s.march_facts,id,count);
if(count>1)return {false,{},common_failure("DuplicateMilitaryFactPayload",id).error};if(!count)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c)->Status{
if(engineering){return emit_tagged_military_fields(c,"military_siege_engineering_prepared",engineering->field_order,[&](auto& row,bool order){return military_siege_engineering_codec::emit_fields(row,*engineering,order);});}
if(siege_lifecycle){return military_siege_lifecycle_codec::emit_fields(c,*siege_lifecycle);}
if(action){auto check=emit_original_action_fact(c,*action);if(check.ok)c.order_fields(action->field_order);return check.ok?c.status():check;}
if(supply){c.field("kind",StableId("military_supply"));auto check=military_codec::emit_fields(c,*supply);if(check.ok)c.order_fields(supply->field_order);return check.ok?c.status():check;}
if(siege_release){return emit_tagged_military_fields(c,"military_siege_released",siege_release->field_order,[&](auto& row,bool order){return military_recovery_codec::emit_fields(row,*siege_release,order);});}
if(battle_release){return emit_tagged_military_fields(c,"military_battle_released",battle_release->field_order,[&](auto& row,bool order){return military_recovery_codec::emit_fields(row,*battle_release,order);});}
if(rest){return emit_tagged_military_fields(c,"military_garrison_rest",rest->field_order,[&](auto& row,bool order){return military_garrison_rest_codec::emit_fields(row,*rest,order);});}
if(rout_entry){return emit_tagged_military_fields(c,"military_rout_entry",rout_entry->field_order,[&](auto& row,bool order){return military_rout_codec::emit_fields(row,*rout_entry,order);});}
if(rout_round){return emit_tagged_military_fields(c,"military_rout_round",rout_round->field_order,[&](auto& row,bool order){return military_rout_codec::emit_fields(row,*rout_round,order);});}
if(attrition){return emit_tagged_military_fields(c,"military_supply_attrition",attrition->field_order,[&](auto& row,bool order){return military_supply_attrition_codec::emit_fields(row,*attrition,order);});}
if(recovery){return emit_tagged_military_fields(c,"military_wounded_recovery",recovery->field_order,[&](auto& row,bool order){return military_recovery_codec::emit_fields(row,*recovery,order);});}
if(field_command){return emit_tagged_military_fields(c,"military_field_command_assigned",field_command->field_order,[&](auto& row,bool order){return military_field_command_codec::emit_fields(row,*field_command,order);});}
if(officer){return emit_tagged_military_fields(c,"military_officer_assignment",officer->field_order,[&](auto& row,bool order){return military_officer_codec::emit_fields(row,*officer,order);});}
if(request){return emit_tagged_military_fields(c,"military_request",request->field_order,[&](auto& row,bool order){return military_request_codec::emit_fields(row,*request,order);});}
if(arrival){return military_codec::emit_fields(c,*arrival);}
if(contact){return battle_codec::emit_fields(c,*contact);}
if(battle){return battle_codec::emit_fields(c,*battle);}
if(casualty){return battle_codec::emit_fields(c,*casualty);}
if(training){return training_codec::emit_fields(c,*training);}
if(logistics){auto check=lifecycle_codec::emit_fields(c,*logistics);if(check.ok)c.order_fields(logistics->field_order);return check.ok?c.status():check;}
if(returned){return lifecycle_codec::emit_fields(c,*returned);}
if(muster){return lifecycle_codec::emit_fields(c,*muster);}
if(resigned){return lifecycle_codec::emit_fields(c,*resigned);}
if(delivered){return lifecycle_codec::emit_fields(c,*delivered);}
if(settled){return lifecycle_codec::emit_fields(c,*settled);}
if(occupied){return occupation_codec::emit_fields(c,*occupied,occupation_source_schema);}
if(relief){return occupation_codec::emit_fields(c,*relief);}
if(material){return occupation_codec::emit_fields(c,*material);}
if(march){return training_codec::emit_fields(c,*march);}
return common_failure("MilitaryFactPayloadRequired",id);}));}
Outcome<bool> emit_military_battle_record_projection(ProjectionWriter& w,std::string_view name,const MilitaryState& s,const StableId& id){Integer count=0;const auto* row=unique_military_record(s.battles,id,count);if(count>1)return {false,{},common_failure("DuplicateMilitaryBattlePayload",id).error};if(!row)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c){return battle_codec::emit_fields(c,*row);}));}
Outcome<bool> emit_military_root_record_projection(ProjectionWriter& w,std::string_view name,const MilitaryState& s,MilitaryProjectionRootTable table,const StableId& id){Integer count=0;
if(table==MilitaryProjectionRootTable::offices){const auto* row=unique_military_record(s.offices,id,count);if(count>1)return {false,{},common_failure("DuplicateMilitaryRootPayload",id).error};if(!row)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c){return military_codec::emit_fields(c,*row);}));}
if(table==MilitaryProjectionRootTable::garrisons){const auto* row=unique_military_record(s.garrisons,id,count);if(count>1)return {false,{},common_failure("DuplicateMilitaryRootPayload",id).error};if(!row)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c){return military_codec::emit_fields(c,*row);}));}
if(table==MilitaryProjectionRootTable::military_units){const auto* row=unique_military_record(s.military_units,id,count);if(count>1)return {false,{},common_failure("DuplicateMilitaryRootPayload",id).error};if(!row)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c){return military_codec::emit_fields(c,*row);}));}
if(table==MilitaryProjectionRootTable::armies){const auto* row=unique_military_record(s.armies,id,count);if(count>1)return {false,{},common_failure("DuplicateMilitaryRootPayload",id).error};if(!row)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c){return military_codec::emit_fields(c,*row);}));}
if(table==MilitaryProjectionRootTable::campaigns){const auto* row=unique_military_record(s.campaigns,id,count);if(count>1)return {false,{},common_failure("DuplicateMilitaryRootPayload",id).error};if(!row)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c){return military_codec::emit_fields(c,*row);}));}
if(table==MilitaryProjectionRootTable::strongholds){const auto* row=unique_military_record(s.strongholds,id,count);if(count>1)return {false,{},common_failure("DuplicateMilitaryRootPayload",id).error};if(!row)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c){return military_codec::emit_fields(c,*row);}));}
if(table==MilitaryProjectionRootTable::sieges){const auto* row=unique_military_record(s.sieges,id,count);if(count>1)return {false,{},common_failure("DuplicateMilitaryRootPayload",id).error};if(!row)return {true,false,{}};return emitted_record(w.object(name,[&](auto& c){return siege_codec::emit_fields(c,*row);}));}
return {false,{},common_failure("UnknownMilitaryProjectionRoot",id).error};}
Outcome<BoundaryValue> export_military_state(const MilitaryState& s,Integer occupation_source_schema){try{BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& c){return emit_military_state_fields(c,s,occupation_source_schema);});if(checked.ok)checked=sink.finish();if(!checked.ok)return {false,{},checked.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 只读同一作者源；未启用时返回空值，静态或无military能力不能冒用动态codec5。
Outcome<std::optional<MilitaryRequestPolicy>> decode_military_request_policy(const BoundaryValue& root,const StableId& scenario_id){try{const auto& scenario=boundary::field(boundary::field(root,"scenarios","data"),scenario_id,"scenarios");const auto* policy=scenario.find("military_request_policy");if(!policy)return {true,std::nullopt,{}};boundary::fields(*policy,{"profile","per_actor_round_limit"},{},"military_request_policy");const auto capabilities=boundary::strings(boundary::field(root,"required_capabilities","data"),"required_capabilities");const auto* dynamic=scenario.find("dynamic_world");if(std::find(capabilities.begin(),capabilities.end(),"military")==capabilities.end()||!dynamic||boundary::get_string(*dynamic,"profile","dynamic_world")!="local_society_v1"||boundary::get_string(*policy,"profile","military_request_policy")!="request_policy/1")boundary::fail("MilitaryRequestProfileRequired","military_request_policy");MilitaryRequestPolicy config;config.per_actor_round_limit=boundary::get_int(*policy,"per_actor_round_limit","military_request_policy");if(config.per_actor_round_limit<1)boundary::fail("InvalidMilitaryRequestPolicy","military_request_policy");return {true,config,{}};}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
namespace {
// 作者目录保留被动定义闭包；只有显式活动集进入运行时攻城能力。
// 调用前必须完整解码并校验全部拓扑，不能用未选择掩盖坏定义。
void select_siege_topologies(const BoundaryValue& scenario,
                             const StableId& scenario_id,
                             std::vector<SiegeTopology>& topologies) {
    const auto* enabled = scenario.find("enabled");
    if (!enabled) return; // 无快照选型的旧目录保留原来的全量语义。
    const auto path = "scenarios/" + scenario_id + "/enabled";
    if (!enabled->as<BoundaryValue::Dict>())
        boundary::fail("InvalidScenarioEnabled", path);
    const auto* selected = enabled->find("siege_topologies");
    if (!selected) { topologies.clear(); return; }
    const auto table_path = path + "/siege_topologies";
    const auto* ids = selected->as<BoundaryValue::Array>();
    if (!ids) boundary::fail("InvalidScenarioEnabled", table_path);
    for (auto it = ids->begin(); it != ids->end(); ++it) {
        const auto* id = it->as<StableId>();
        if (!id || id->empty() || std::any_of(ids->begin(), it,
                [&](const auto& prior) { return *prior.template as<StableId>() == *id; }))
            boundary::fail("InvalidScenarioEnabled", table_path);
        if (std::none_of(topologies.begin(), topologies.end(),
                         [&](const auto& row) { return row.id == *id; }))
            boundary::fail("UnknownScenarioEnabled", table_path + "/" + *id);
    }
    // 原地稳定投影，不按作者数组重排，不建立第二份持久目录或借空表判型。
    topologies.erase(std::remove_if(topologies.begin(), topologies.end(), [&](const auto& row) {
        return std::none_of(ids->begin(), ids->end(), [&](const auto& id) {
            return *id.template as<StableId>() == row.id;
        });
    }), topologies.end());
}
}
Outcome<MilitaryCatalog> decode_military_catalog(const BoundaryValue& root,const StableId& scenario_id){try{
    auto display=[&](const V& v){if(auto n=v.find("display_name"))return boundary::string(*n,"display_name");auto key=v.find("name_key");if(!key)key=v.find("title_key");if(key){auto texts=root.find("texts");auto text=texts?texts->find(boundary::string(*key,"name_key")):nullptr;if(text)return boundary::string(*text,"texts");}auto id=v.find("id");return id?boundary::string(*id,"id"):StableId{};};
    MilitaryCatalog c;c.scenario_id=scenario_id;auto& scenario=boundary::field(boundary::field(root,"scenarios","data"),scenario_id,"scenarios");if(auto model=scenario.find("military_condition_model")){if(!model->as<StableId>()||*model->as<StableId>()!="terrain_march_v1")boundary::fail("MILITARY_CONDITION_MODEL","scenarios/"+scenario_id+"/military_condition_model");c.march_condition_model=true;}auto capabilities=boundary::strings(boundary::field(root,"required_capabilities","data"),"capabilities");c.military=std::find(capabilities.begin(),capabilities.end(),"military")!=capabilities.end();
    if(const auto* policy=scenario.find("military_officer_assignments")){auto guard=military_officer_codec::validate_json_unique_keys(*policy);if(!guard.ok)return {false,{},guard.error};boundary::fields(*policy,{"profile"},{},"military_officer_assignments");const auto* dynamic=scenario.find("dynamic_world");if(!c.military||!dynamic||boundary::get_string(*dynamic,"profile","dynamic_world")!="local_society_v1"||std::find(capabilities.begin(),capabilities.end(),"military_officer_assignments_v1")==capabilities.end()||boundary::get_string(*policy,"profile","military_officer_assignments")!="political_appointment_v1")boundary::fail("MilitaryOfficerAssignmentProfileRequired","military_officer_assignments");c.officer_assignment_policy=MilitaryOfficerAssignmentPolicy{};}
    auto request_policy=decode_military_request_policy(root,scenario_id);if(!request_policy.ok)return {false,{},request_policy.error};c.request_policy=std::move(request_policy.value);
    for(auto& row:boundary::object(boundary::field(root,"offices","data"),"offices")){auto& v=row.second;OfficeDefinition d;d.id=boundary::get_string(v,"id",row.first);d.organization_id=boundary::get_string(v,"organization_id",row.first);d.garrison_id=boundary::get_string(v,"garrison_id",row.first);d.appointing_role_id=boundary::get_string(v,"appointing_role_id",row.first);d.command_scope=boundary::get_string(v,"command_scope",row.first);c.offices.push_back(d);}
    for(auto& row:boundary::object(boundary::field(root,"garrisons","data"),"garrisons")){auto& v=row.second;GarrisonDefinition d;d.id=boundary::get_string(v,"id",row.first);d.organization_id=boundary::get_string(v,"organization_id",row.first);d.place_id=boundary::get_string(v,"place_id",row.first);d.inventory_id=boundary::get_string(v,"inventory_id",row.first);d.wallet_id=boundary::get_string(v,"wallet_id",row.first);d.food_item_id=boundary::get_string(v,"food_item_id",row.first);d.army_capacity=boundary::get_int(v,"army_capacity",row.first);d.ration_per_person=boundary::get_int(v,"ration_per_person",row.first);d.display_name=display(v);if(auto x=v.find("training_wallet_id"))d.training_wallet_id=boundary::string(*x,row.first);if(auto x=v.find("training_drill_id"))d.training_drill_id=boundary::string(*x,row.first);if(auto x=v.find("training_fatigue_per_tick"))d.training_fatigue_per_tick=boundary::integer(*x,row.first);for(auto& unit:boundary::array(boundary::field(v,"units",row.first),row.first)){MilitaryUnitDefinition u;u.id=boundary::get_string(unit,"id",row.first);u.commander_actor_id=boundary::get_string(unit,"commander_actor_id",row.first);u.troop_type_id=boundary::get_string(unit,"troop_type_id",row.first);if(auto source=unit.find("population_source_id")){if(unit.find("initial_soldiers")||std::find(capabilities.begin(),capabilities.end(),"population_military_v1")==capabilities.end())boundary::fail("MilitaryPopulationSourceBranch",row.first);u.population_source_id=boundary::string(*source,row.first);if(u.population_source_id->empty())boundary::fail("MilitaryPopulationSourceBranch",row.first);}else u.initial_soldiers=boundary::get_int(unit,"initial_soldiers",row.first);if(auto x=unit.find("initial_training"))u.initial_training=boundary::integer(*x,row.first);if(auto x=unit.find("initial_fatigue"))u.initial_fatigue=boundary::integer(*x,row.first);if(unit.find("name_key"))u.display_name=display(unit);for(auto& eq:boundary::array(boundary::field(unit,"equipment",row.first),row.first))u.equipment.push_back({boundary::get_string(eq,"item_id",row.first),boundary::get_int(eq,"quantity_per_person",row.first)});d.units.push_back(u);}c.garrisons.push_back(d);}
    for(auto& row:boundary::object(boundary::field(root,"military_orders","data"),"orders")){auto& v=row.second;MilitaryOrderDefinition d;d.id=boundary::get_string(v,"id",row.first);d.office_id=boundary::get_string(v,"office_id",row.first);d.garrison_id=boundary::get_string(v,"garrison_id",row.first);d.issuer_role_id=boundary::get_string(v,"issuer_role_id",row.first);d.report_role_id=boundary::get_string(v,"report_role_id",row.first);d.target_stronghold_id=boundary::get_string(v,"target_stronghold_id",row.first);d.deadline_ticks=boundary::get_int(v,"deadline_ticks",row.first);d.display_name=display(v);auto& objective=boundary::field(v,"objective",row.first);d.objective_kind=boundary::get_string(objective,"kind",row.first);if(auto x=objective.find("recipient"))d.recipient=boundary::string(*x,row.first);if(auto x=objective.find("recipient_organization_id"))d.recipient_organization_id=boundary::string(*x,row.first);if(auto x=objective.find("item_id"))d.item_id=boundary::string(*x,row.first);if(auto x=objective.find("quantity"))d.quantity=boundary::integer(*x,row.first);c.orders.push_back(d);}
    for(auto& row:boundary::object(boundary::field(root,"strongholds","data"),"strongholds")){auto& v=row.second;c.strongholds.push_back({boundary::get_string(v,"id",row.first),boundary::get_string(v,"garrison_id",row.first),boundary::get_string(v,"settlement_id",row.first),boundary::get_string(v,"topology_id",row.first),boundary::strings(boundary::field(v,"public_inventory_ids",row.first),row.first),boundary::strings(boundary::field(v,"public_wallet_ids",row.first),row.first)});}
    if(const auto* policy=scenario.find("military_field_commands")){auto guard=military_officer_codec::validate_json_unique_keys(*policy);if(!guard.ok)return {false,{},guard.error};boundary::fields(*policy,{"profile"},{},"military_field_commands");if(!c.officer_assignment_policy||boundary::get_string(*policy,"profile","military_field_commands")!="casualty_appointment_v1"||std::find(capabilities.begin(),capabilities.end(),"military_field_commands_v1")==capabilities.end())boundary::fail("MilitaryFieldCommandProfileRequired","military_field_commands");c.field_command_policy=MilitaryFieldCommandPolicy{};}
    if(const auto* policy=scenario.find("military_wounded_recovery")){const auto* personal=scenario.find("personal_injury_recovery");if(!personal||!personal->as<D>()||std::find(capabilities.begin(),capabilities.end(),"military_wounded_recovery_v1")==capabilities.end())boundary::fail("MilitaryRecoveryPrerequisiteRequired","military_wounded_recovery");auto decoded=decode_military_recovery_policy(*policy,c);if(!decoded.ok)return {false,{},decoded.error};const auto enabled=boundary::strings(boundary::field(boundary::field(scenario,"enabled","scenario"),"garrisons","scenario/enabled"),"scenario/enabled/garrisons");for(const auto& id:decoded.value.garrison_ids)if(std::find(enabled.begin(),enabled.end(),id)==enabled.end())boundary::fail("MilitaryRecoveryGarrisonNotEnabled",id);c.recovery_policy=std::move(decoded.value);}
    if(const auto* policy=scenario.find("military_supply_attrition")){boundary::fields(*policy,{"profile","shortage_threshold_half_days","desertion_basis_points","morale_loss"},{},"military_supply_attrition");MilitarySupplyAttritionPolicy selected;selected.profile=boundary::get_string(*policy,"profile","military_supply_attrition");selected.shortage_threshold_half_days=boundary::get_int(*policy,"shortage_threshold_half_days","military_supply_attrition");selected.desertion_basis_points=boundary::get_int(*policy,"desertion_basis_points","military_supply_attrition");selected.morale_loss=boundary::get_int(*policy,"morale_loss","military_supply_attrition");if(!c.recovery_policy||selected.profile!="anonymous_shortage_v1"||selected.shortage_threshold_half_days!=2||selected.desertion_basis_points!=200||selected.morale_loss!=10||std::find(capabilities.begin(),capabilities.end(),"military_supply_attrition_v1")==capabilities.end())boundary::fail("MilitarySupplyAttritionPrerequisiteRequired","military_supply_attrition");c.supply_attrition_policy=std::move(selected);}
    if(const auto* policy=scenario.find("military_rout_retreat")){auto unique=military_officer_codec::validate_json_unique_keys(*policy);if(!unique.ok)return {false,{},unique.error};boundary::fields(*policy,{"profile","blocked_rounds_before_capture","battlefield_ids"},{},"military_rout_retreat");MilitaryRoutRetreatRule rule{boundary::get_string(*policy,"profile","military_rout_retreat"),boundary::get_int(*policy,"blocked_rounds_before_capture","military_rout_retreat")};if(!c.supply_attrition_policy||rule.profile!="two_blocked_rounds_v1"||rule.blocked_rounds_before_capture!=2||std::find(capabilities.begin(),capabilities.end(),"military_rout_retreat_v1")==capabilities.end())boundary::fail("MilitaryRoutPrerequisiteRequired","military_rout_retreat");auto ids=boundary::strings(boundary::field(*policy,"battlefield_ids","military_rout_retreat"),"military_rout_retreat/battlefield_ids");std::set<StableId> unique_ids(ids.begin(),ids.end());if(ids.empty()||ids.size()>32||unique_ids.size()!=ids.size()||unique_ids.count(""))boundary::fail("MilitaryRoutBattlefieldSelectionMismatch","military_rout_retreat");c.rout_retreat_policy=MilitaryRoutRetreatPolicy{std::move(rule),std::move(ids)};}
    if(const auto* p=scenario.find("military_command_shock")){auto unique=military_officer_codec::validate_json_unique_keys(*p);if(!unique.ok)return {false,{},unique.error};boundary::fields(*p,{"profile","morale_loss"},{},"military_command_shock");MilitaryCommandShockRule r{boundary::get_string(*p,"profile","military_command_shock"),boundary::get_int(*p,"morale_loss","military_command_shock")};auto checked=validate_military_command_shock_rule(r);if(!checked.ok)return {false,{},checked.error};if(!c.rout_retreat_policy||std::find(capabilities.begin(),capabilities.end(),"military_command_shock_v1")==capabilities.end())boundary::fail("MilitaryCommandShockPrerequisiteRequired","military_command_shock");c.command_shock_policy=std::move(r);}
    for(auto& row:boundary::object(boundary::field(root,"troop_types","data"),"troop_types"))c.troop_type_ids.push_back(row.first);
    if(const auto* p=scenario.find("military_garrison_rest")){auto unique=military_officer_codec::validate_json_unique_keys(*p);if(!unique.ok)return {false,{},unique.error};boundary::fields(*p,{"profile","garrison_ids","fatigue_reduction"},{},"military_garrison_rest");MilitaryGarrisonRestPolicy policy;policy.profile=boundary::get_string(*p,"profile","military_garrison_rest");policy.garrison_ids=boundary::strings(boundary::field(*p,"garrison_ids","military_garrison_rest"),"military_garrison_rest/garrison_ids");policy.fatigue_reduction=boundary::get_int(*p,"fatigue_reduction","military_garrison_rest");if(std::find(capabilities.begin(),capabilities.end(),"military_garrison_rest_v1")==capabilities.end())boundary::fail("MilitaryGarrisonRestPrerequisiteRequired","military_garrison_rest");auto valid=validate_military_garrison_rest_policy(policy,c);if(!valid.ok)return {false,{},valid.error};c.garrison_rest_policy=std::move(policy);}
    if(const auto* p=scenario.find("military_combat_conditions")){auto unique=military_officer_codec::validate_json_unique_keys(*p);if(!unique.ok)return {false,{},unique.error};boundary::fields(*p,{"profile"},{},"military_combat_conditions");MilitaryCombatConditionRule r{boundary::get_string(*p,"profile","military_combat_conditions")};auto valid=validate_military_combat_condition_rule(r);if(!valid.ok)return {false,{},valid.error};if(!c.garrison_rest_policy||std::find(capabilities.begin(),capabilities.end(),"military_combat_conditions_v1")==capabilities.end())boundary::fail("MilitaryCombatConditionPrerequisiteRequired","military_combat_conditions");c.combat_condition_policy=std::move(r);}
    if(const auto* p=scenario.find("military_commanded_damage")){auto unique=military_officer_codec::validate_json_unique_keys(*p);if(!unique.ok)return {false,{},unique.error};boundary::fields(*p,{"profile"},{},"military_commanded_damage");MilitaryCommandedDamageRule r{boundary::get_string(*p,"profile","military_commanded_damage")};auto valid=validate_military_commanded_damage_rule(r);if(!valid.ok)return {false,{},valid.error};if(!c.combat_condition_policy||std::find(capabilities.begin(),capabilities.end(),"military_commanded_damage_v1")==capabilities.end())boundary::fail("MilitaryCommandedDamagePrerequisiteRequired","military_commanded_damage");c.commanded_damage_policy=std::move(r);}
    if(const auto* p=scenario.find("military_sequential_rounds")){auto unique=military_officer_codec::validate_json_unique_keys(*p);if(!unique.ok)return {false,{},unique.error};boundary::fields(*p,{"profile"},{},"military_sequential_rounds");MilitarySequentialRoundRule r{boundary::get_string(*p,"profile","military_sequential_rounds")};auto valid=validate_military_sequential_rule(r);if(!valid.ok)return {false,{},valid.error};if(!c.commanded_damage_policy||std::find(capabilities.begin(),capabilities.end(),"military_sequential_rounds_v1")==capabilities.end())boundary::fail("MilitarySequentialPrerequisiteRequired","military_sequential_rounds");c.sequential_round_policy=std::move(r);}
    if(const auto* p=scenario.find("military_command_resources")){auto unique=military_officer_codec::validate_json_unique_keys(*p);if(!unique.ok)return {false,{},unique.error};boundary::fields(*p,{"profile"},{},"military_command_resources");MilitaryCommandResourcesRule r{boundary::get_string(*p,"profile","military_command_resources")};auto valid=validate_military_command_resources_rule(r);if(!valid.ok)return {false,{},valid.error};if(!c.sequential_round_policy||std::find(capabilities.begin(),capabilities.end(),"military_command_resources_v1")==capabilities.end())boundary::fail("MilitaryCommandResourcesPrerequisiteRequired","military_command_resources");c.command_resources_policy=std::move(r);}
    if(const auto* p=scenario.find("military_field_terrain")){auto unique=military_officer_codec::validate_json_unique_keys(*p);if(!unique.ok)return {false,{},unique.error};boundary::fields(*p,{"profile","battlefields"},{},"military_field_terrain");MilitaryFieldTerrainPolicy policy;policy.rule.profile=boundary::get_string(*p,"profile","military_field_terrain");if(!c.sequential_round_policy||!c.rout_retreat_policy||std::find(capabilities.begin(),capabilities.end(),"military_field_terrain_v1")==capabilities.end())boundary::fail("MilitaryFieldTerrainPrerequisiteRequired","military_field_terrain");for(const auto& row:boundary::array(boundary::field(*p,"battlefields","military_field_terrain"),"military_field_terrain/battlefields")){boundary::fields(row,{"approach_battlefield_id","field_battlefield_id"},{},"military_field_terrain/battlefields");policy.battlefields.push_back({boundary::get_string(row,"approach_battlefield_id","military_field_terrain"),boundary::get_string(row,"field_battlefield_id","military_field_terrain")});}auto valid=select_military_field_contact_battlefield(policy,policy.battlefields.empty()?StableId{}:policy.battlefields.front().approach_battlefield_id);if(!valid.ok)return {false,{},valid.error};for(const auto& row:policy.battlefields)if(std::find(c.rout_retreat_policy->battlefield_ids.begin(),c.rout_retreat_policy->battlefield_ids.end(),row.field_battlefield_id)==c.rout_retreat_policy->battlefield_ids.end())boundary::fail("MilitaryFieldTerrainLandBoardRequired",row.field_battlefield_id);c.field_terrain_policy=std::move(policy);}
    auto combat=battle_codec::decode_catalog(root);if(!combat.ok)return {false,{},combat.error};c.battle_catalog=std::move(combat.value);c.battle_catalog.supply_attrition=c.supply_attrition_policy.has_value();c.battle_catalog.rout_retreat_enabled=c.rout_retreat_policy.has_value();c.battle_catalog.command_shock_rule=c.command_shock_policy;c.battle_catalog.combat_condition_rule=c.combat_condition_policy;c.battle_catalog.commanded_damage_rule=c.commanded_damage_policy;c.battle_catalog.sequential_round_rule=c.sequential_round_policy;c.battle_catalog.command_resources_rule=c.command_resources_policy;c.battle_catalog.field_terrain_enabled=c.field_terrain_policy.has_value();if(c.field_terrain_policy)for(const auto& binding:c.field_terrain_policy->battlefields){BattlefieldDefinition* selected=nullptr;const BattlefieldDefinition* approach=nullptr;for(auto& board:c.battle_catalog.battlefields){if(board.id==binding.field_battlefield_id)selected=&board;if(board.id==binding.approach_battlefield_id)approach=&board;}if(!selected||!approach)boundary::fail("MilitaryFieldTerrainBoardMissing",binding.field_battlefield_id);if(selected->objective_id!=approach->objective_id)boundary::fail("MilitaryFieldTerrainObjectiveMismatch",selected->id);selected->field_terrain=c.field_terrain_policy->rule;}for(auto& board:c.battle_catalog.battlefields)board.rout_retreat.reset();if(c.rout_retreat_policy){const auto selected=boundary::strings(boundary::field(boundary::field(scenario,"enabled","scenario"),"battlefields","scenario/enabled"),"scenario/enabled/battlefields");for(const auto& id:c.rout_retreat_policy->battlefield_ids){bool found=false;for(auto& board:c.battle_catalog.battlefields)if(board.id==id){if(std::find(selected.begin(),selected.end(),id)==selected.end())boundary::fail("MilitaryRoutBattlefieldNotEnabled",id);if(c.combat_condition_policy&&board.damage_model.kind!="legacy_base_factors_v1")boundary::fail("MilitaryCombatConditionDamageModelRequired",board.id);board.rout_retreat=c.rout_retreat_policy->rule;auto valid=validate_military_rout_rule(*board.rout_retreat,board);if(!valid.ok)return {false,{},valid.error};found=true;}if(!found)boundary::fail("MilitaryRoutBattlefieldMissing",id);}}if(c.command_resources_policy){const auto enabled=boundary::strings(boundary::field(boundary::field(scenario,"enabled","scenario"),"garrisons","scenario/enabled"),"scenario/enabled/garrisons");for(const auto& id:enabled){const GarrisonDefinition* g=nullptr;for(const auto& candidate:c.garrisons)if(candidate.id==id)g=&candidate;if(!g)boundary::fail("UnknownGarrison",id);for(const auto& u:g->units){const TroopDefinition* t=nullptr;for(const auto& candidate:c.battle_catalog.troop_types)if(candidate.id==u.troop_type_id)t=&candidate;if(!t||t->tactic_capacity!=100)boundary::fail("MilitaryCommandResourcesCapacityRequired",u.id);}}}if(const auto* p=scenario.find("military_tactic_learning")){if(!c.command_resources_policy||(!scenario.find("dynamic_world")||!scenario.find("dynamic_world")->find("growth_tactic_lessons"))||std::find(capabilities.begin(),capabilities.end(),"military_tactic_learning_v1")==capabilities.end())boundary::fail("MilitaryTacticLearningPrerequisiteRequired","military_tactic_learning");auto learned=decode_military_tactic_learning_catalog(*p,root);if(!learned.ok)return {false,{},learned.error};c.battle_catalog.tactic_learning=std::move(learned.value);const auto enabled=boundary::strings(boundary::field(boundary::field(scenario,"enabled","scenario"),"garrisons","scenario/enabled"),"scenario/enabled/garrisons");for(const auto& g:c.garrisons)if(std::find(enabled.begin(),enabled.end(),g.id)!=enabled.end())for(const auto& u:g.units){const TroopDefinition* troop=nullptr;for(const auto& t:c.battle_catalog.troop_types)if(t.id==u.troop_type_id)troop=&t;if(!troop)boundary::fail("UnknownTroopType",u.id);for(const auto& t:troop->tactics){auto checked=validate_military_learned_tactic_binding(t,*c.battle_catalog.tactic_learning);if(!checked.ok)return {false,{},checked.error};}}}auto siege=siege_codec::decode_catalog(root,c.battle_catalog);if(!siege.ok)return {false,{},siege.error};c.siege_topologies=std::move(siege.value);if(c.field_terrain_policy){for(const auto& topology:c.siege_topologies)for(const auto& node:topology.nodes)if(std::any_of(c.field_terrain_policy->battlefields.begin(),c.field_terrain_policy->battlefields.end(),[&](const auto& binding){return binding.field_battlefield_id==node.battlefield_id;}))boundary::fail("MilitaryFieldTerrainSiegeBoardForbidden",node.battlefield_id);auto valid=validate_military_field_terrain_policy(c.battle_catalog);if(!valid.ok)return {false,{},valid.error};}auto drills=training_codec::decode_drill_catalog(root);if(!drills.ok)return {false,{},drills.error};c.drill_challenges=std::move(drills.value);
    select_siege_topologies(scenario, scenario_id, c.siege_topologies);
    if(const auto* p=scenario.find(military_siege_engineering_module)){auto unique=military_officer_codec::validate_json_unique_keys(*p);if(!unique.ok)return {false,{},unique.error};boundary::fields(*p,{"version","profile","topology_ids"},{},military_siege_engineering_module);if(boundary::get_int(*p,"version",military_siege_engineering_module)!=1||!c.battle_catalog.tactic_learning||std::find(capabilities.begin(),capabilities.end(),"military_siege_engineering_v1")==capabilities.end())boundary::fail("MilitarySiegeEngineeringPrerequisiteRequired",military_siege_engineering_module);MilitarySiegeEngineeringPolicy policy;policy.rule.profile=boundary::get_string(*p,"profile",military_siege_engineering_module);policy.topology_ids=boundary::strings(boundary::field(*p,"topology_ids",military_siege_engineering_module),military_siege_engineering_module);auto checked=validate_military_siege_engineering_rule(policy.rule);if(!checked.ok)return {false,{},checked.error};std::set<StableId> ids;if(policy.topology_ids.empty()||policy.topology_ids.size()>64)boundary::fail("MilitarySiegeEngineeringTopologyRequired",military_siege_engineering_module);for(const auto& id:policy.topology_ids)if(!ids.insert(id).second||std::none_of(c.siege_topologies.begin(),c.siege_topologies.end(),[&](const auto& t){return t.id==id;}))boundary::fail("MilitarySiegeEngineeringTopologyRequired",id);c.siege_engineering_policy=std::move(policy);}
    for(auto& row:boundary::object(boundary::field(root,"items","data"),"items"))c.item_ids.push_back(row.first);
    if(auto estates=root.find("estates"))for(auto& row:boundary::object(*estates,"estates"))c.estate_display_names.emplace_back(row.first,display(row.second));
    for(auto& row:boundary::object(boundary::field(root,"actors","data"),"actors"))if(auto plan=row.second.find("military_plan")){MilitaryPlan p;p.actor_id=row.first;p.office_id=boundary::get_string(*plan,"office_id",row.first);p.order_id=boundary::get_string(*plan,"order_id",row.first);p.food_quantity=boundary::get_int(*plan,"food_quantity",row.first);p.money=boundary::get_int(*plan,"money",row.first);p.unit_ids=boundary::strings(boundary::field(*plan,"unit_ids",row.first),row.first);c.plans.push_back(p);}
    return {true,std::move(c),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_military_checkpoint(CodecVisitor& c,MilitaryState& s,Integer occupation_source_schema){
    if(occupation_source_schema!=1&&occupation_source_schema!=2)return c.reject("UnsupportedMilitaryOccupationSchema");
    // 17显式选择才附加两个永久段；旧1..16连空段也不写，未知版本在此防御拒绝。
    if(c.military_codec_version>17)return c.reject("MilitarySiegeEngineeringCodecVersionMismatch");
    if(c.military_codec_version==17){if(!(c.schema_features&military_command_resources_checkpoint_feature))return c.reject("MilitarySiegeEngineeringPrerequisiteRequired");auto p=c.object("siege_engineering",[&](auto& child){return military_siege_engineering_codec::visit(child,s.siege_engineering);});if(!p.ok)return p;auto h=c.object("siege_lifecycle",[&](auto& child){return military_siege_lifecycle_codec::visit(child,s.siege_lifecycle);});if(!h.ok)return h;}
    else if(!s.siege_engineering.preparations.empty()||!s.siege_engineering.field_order.empty()||!s.siege_lifecycle.facts.empty()||!s.siege_lifecycle.field_order.empty())return c.reject("UnexpectedMilitarySiegeEngineering");

    if((c.schema_features&military_command_resources_checkpoint_feature)&&!(c.schema_features&military_commanded_damage_checkpoint_feature))return c.reject("MilitaryCommandResourcesPrerequisiteRequired");
    if((c.schema_features&military_combat_conditions_checkpoint_feature)&&!(c.schema_features&military_garrison_rest_checkpoint_feature))return c.reject("MilitaryCombatConditionPrerequisiteRequired");if((c.schema_features&military_commanded_damage_checkpoint_feature)&&!(c.schema_features&military_combat_conditions_checkpoint_feature))return c.reject("MilitaryCommandedDamagePrerequisiteRequired");
    if(c.schema_features&military_garrison_rest_checkpoint_feature){auto restored=c.object("garrison_rest",[&](auto& child){return military_garrison_rest_codec::visit(child,s.garrison_rest);});if(!restored.ok)return restored;}else if(!s.garrison_rest.facts.empty()||!s.garrison_rest.field_order.empty())return c.reject("UnexpectedMilitaryGarrisonRest");
    if((c.schema_features&military_command_shock_checkpoint_feature)&&!(c.schema_features&military_rout_checkpoint_feature))return c.reject("MilitaryCommandShockPrerequisiteRequired");
    if(c.schema_features&military_rout_checkpoint_feature){if(!(c.schema_features&military_supply_attrition_checkpoint_feature))return c.reject("MilitaryRoutPrerequisiteRequired");auto restored=c.object("rout",[&](auto& child){return military_rout_codec::visit(child,s.rout);});if(!restored.ok)return restored;}else if(!s.rout.entries.empty()||!s.rout.rounds.empty()||!s.rout.field_order.empty())return c.reject("UnexpectedMilitaryRoutHistory");

    if(c.schema_features&military_supply_attrition_checkpoint_feature){if(!(c.schema_features&military_recovery_checkpoint_feature))return c.reject("MilitarySupplyAttritionPrerequisiteRequired");auto restored=c.object("supply_attrition",[&](auto& child){return military_supply_attrition_codec::visit(child,s.supply_attrition);});if(!restored.ok)return restored;}else if(!s.supply_attrition.facts.empty()||!s.supply_attrition.field_order.empty())return c.reject("UnexpectedMilitarySupplyAttrition");
    // 新段只由显式Military8位启用；旧1..7同字节，不写空占位。
    if(c.schema_features&military_recovery_checkpoint_feature){if(!(c.schema_features&military_field_command_checkpoint_feature)||!(c.schema_features&military_officer_checkpoint_feature))return c.reject("MilitaryRecoveryPrerequisiteRequired");auto restored=c.object("recovery",[&](auto& child){return military_recovery_codec::visit(child,s.recovery);});if(!restored.ok)return restored;}
    else if(!s.recovery.facts.empty()||!s.recovery.battle_releases.empty()||!s.recovery.siege_releases.empty()||!s.recovery.field_order.empty())return c.reject("UnexpectedMilitaryRecovery");
    if(c.schema_features&military_field_command_checkpoint_feature){if(!(c.schema_features&military_officer_checkpoint_feature))return c.reject("MilitaryFieldCommandPrerequisiteRequired");c.sequence("field_commands",s.field_commands,[](auto& codec,auto& fact){return military_field_command_codec::visit(codec,fact);});}
    else if(!s.field_commands.empty())return c.reject("UnexpectedMilitaryFieldCommands");
    if(c.schema_features&military_officer_codec::checkpoint_feature)c.sequence("officer_assignments",s.officer_assignments,[](auto& codec,auto& fact){return military_officer_codec::visit(codec,fact);});
    else if(!s.officer_assignments.empty())return c.reject("UnexpectedMilitaryOfficerAssignments");
    if(c.schema_features&military_request_codec::checkpoint_feature)c.sequence("request_facts",s.request_facts,[](auto& codec,auto& receipt){return military_request_codec::visit(codec,receipt);});
    else if(!s.request_facts.empty())return c.reject("UnexpectedMilitaryRequests");
    auto trained=training_codec::visit_state(c,s);if(!trained.ok)return trained;
    auto marched=march_codec::visit_state(c,s);if(!marched.ok)return marched;
    auto lifecycle=lifecycle_codec::visit_state(c,s);if(!lifecycle.ok)return lifecycle;
    auto occupation=occupation_codec::visit_state(c,s,occupation_source_schema);if(!occupation.ok)return occupation;
    c.sequence("sieges",s.sieges,[](auto& c,auto& x){return siege_codec::visit(c,x);});
    c.sequence("battles",s.battles,[](auto& c,auto& x){return battle_codec::visit(c,x);});
    c.sequence("contact_facts",s.contact_facts,[](auto& c,auto& x){return battle_codec::visit(c,x);});
    c.sequence("battle_facts",s.battle_facts,[](auto& c,auto& x){return battle_codec::visit(c,x);});
    c.sequence("casualty_facts",s.casualty_facts,[](auto& c,auto& x){return battle_codec::visit(c,x);});
#define SEQ(name) c.sequence(#name,s.name,[](auto& codec,auto& value){return military_codec::visit(codec,value);})
    SEQ(offices);SEQ(garrisons);SEQ(military_units);SEQ(armies);SEQ(campaigns);SEQ(strongholds);SEQ(supply_facts);SEQ(arrival_facts);
#undef SEQ
    c.sequence("actions",s.actions,[](CodecVisitor& codec,MilitaryActionPayload& p){codec.field("action_id",p.action_id);codec.object("terms",[&](auto& v){return visit_terms(v,p.terms);});Integer kind=p.request.index();codec.field("request_kind",kind);if(kind<0||kind>1)return codec.reject("InvalidMilitaryAction");if(codec.reading()){if(kind==0)p.request=OfficeAccept{};else p.request=ArmyForm{};}if(auto o=std::get_if<OfficeAccept>(&p.request)){codec.field("office_id",o->office_id);interaction_codec::visit_context(codec,o->context);visit_order(codec,o->field_order);}else{auto& f=std::get<ArmyForm>(p.request);codec.field("order_id",f.order_id);codec.sequence("unit_ids",f.unit_ids,[](auto& v,auto& x){return v.field("value",x);});codec.sequence("retainer_ids",f.retainer_ids,[](auto& v,auto& x){return v.field("value",x);});codec.optional("food_quantity",f.food_quantity,[](auto& v,auto& x){return v.field("value",x);});codec.optional("money",f.money,[](auto& v,auto& x){return v.field("value",x);});visit_order(codec,f.field_order);}codec.optional("context",p.context,interaction_codec::visit_context);visit_order(codec,p.field_order);return codec.status();});
    c.sequence("action_facts",s.action_facts,[](CodecVisitor& codec,MilitaryActionFact& f){codec.field("id",f.id);codec.field("command",f.command);codec.field("actor_id",f.actor_id);codec.field("start_tick",f.start_tick);codec.field("completed_tick",f.completed_tick);codec.field("phase",f.phase);codec.field("success",f.success);codec.object("terms",[&](auto& v){return visit_terms(v,f.terms);});codec.optional("context",f.context,interaction_codec::visit_context);codec.optional("result",f.result,visit_result);visit_order(codec,f.field_order);return codec.status();});return c.status();
}
}
