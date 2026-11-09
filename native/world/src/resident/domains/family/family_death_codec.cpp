// 生命账以独立新段保存原因引用和先后，不升级既有Family段或把死者财物重编码给别人。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "family_death.hpp"
#include "succession_personal_assets.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
#include "family_extension_projection.hpp"
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
const std::set<StableId> record_keys{"kind","actor_id","scenario_id","cause_kind","cause_fact_id","battle_id","world_id","death_tick","recorded_tick"};
// 键序可为空或完整唯一；缺失、重复和未知键均不可在冷读后被静默归一化。
bool valid_order(const FieldOrder& order,const std::set<StableId>& expected){return order.empty()||(order.size()==expected.size()&&std::set<StableId>(order.begin(),order.end())==expected);}
// 世界身份固定小写32位；与存档封套分开保存是为了核本事件原世界而非迁移数据。
StableId hex(const WorldId& id){StableId out;const char* digits="0123456789abcdef";for(auto byte:id){out+=digits[byte>>4];out+=digits[byte&15];}return out;}
// 字符串只按规范十六进制接受，不允许截断或把全大写当另一种保存拼写。
WorldId world_id(const StableId& text){if(text.size()!=32)boundary::fail("InvalidFamilyDeathWorldId","world_id");auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};WorldId out{};for(std::size_t i=0;i<16;++i){auto a=digit(text[2*i]),b=digit(text[2*i+1]);if(a<0||b<0)boundary::fail("InvalidFamilyDeathWorldId","world_id");out[i]=static_cast<std::uint8_t>(a*16+b);}return out;}
// 结构层拒绝未知原因、空引用及未来于记账的死亡日期；具体原因仍查原军事账。
Status shape(const FamilyDeathRecord& v){if(v.precise)return common_failure("FamilyLifeProfileMismatch");if(v.kind!="family_death"||v.cause_kind!="military_casualty"||v.death_tick<0||v.recorded_tick<v.death_tick)return common_failure("InvalidFamilyDeathRecord");for(const auto* id:{&v.actor_id,&v.scenario_id,&v.cause_fact_id,&v.battle_id})if(id->empty()||id->size()>256)return common_failure("InvalidFamilyDeathReference");return valid_order(v.field_order,record_keys)?common_success():common_failure("BoundaryFieldOrderMismatch");}
// 单记录显式形状解码，外层已递归核唯一键。
FamilyDeathRecord decode_record(const V& value){using namespace boundary;fields(value,{"kind","actor_id","scenario_id","cause_kind","cause_fact_id","battle_id","world_id","death_tick","recorded_tick"},{},"family_death");FamilyDeathRecord out;out.kind=get_string(value,"kind","");out.actor_id=get_string(value,"actor_id","");out.scenario_id=get_string(value,"scenario_id","");out.cause_kind=get_string(value,"cause_kind","");out.cause_fact_id=get_string(value,"cause_fact_id","");out.battle_id=get_string(value,"battle_id","");out.world_id=world_id(get_string(value,"world_id",""));out.death_tick=get_int(value,"death_tick","");out.recorded_tick=get_int(value,"recorded_tick","");out.field_order=order(value);auto valid=shape(out);if(!valid.ok)fail(valid.error.original.code,"family_death");return out;}
// 编码只保原日期和来源，不以当前死者库存或人物名改写历史。
V encode_record(const FamilyDeathRecord& v){return family_projection::encode([&](auto& w){return emit_family_death_fields(w,v);});}
// typed段完整遍历世界标识、记录来源和字段序，字节范围不靠截断转型。
Status visit_record(CodecVisitor& c,FamilyDeathRecord& v){c.field("kind",v.kind);c.field("actor_id",v.actor_id);c.field("scenario_id",v.scenario_id);c.field("cause_kind",v.cause_kind);c.field("cause_fact_id",v.cause_fact_id);c.field("battle_id",v.battle_id);std::vector<Integer> bytes;for(auto byte:v.world_id)bytes.push_back(byte);c.sequence("world_id",bytes,[](CodecVisitor& q,Integer& b){return q.field("byte",b);});if(c.reading()&&c.status().ok){if(bytes.size()!=16)return c.reject("InvalidFamilyDeathWorldId");for(std::size_t i=0;i<16;++i){if(bytes[i]<0||bytes[i]>255)return c.reject("InvalidFamilyDeathWorldId");v.world_id[i]=static_cast<std::uint8_t>(bytes[i]);}}c.field("death_tick",v.death_tick);c.field("recorded_tick",v.recorded_tick);common_checkpoint::strings(c,"field_order",v.field_order);if(!c.status().ok)return c.status();auto valid=shape(v);return valid.ok?c.status():c.reject(valid.error.original.code);}
}
// profile逐字段确认风险与停止边界，未知死因或自动继任不能借一个bool启用。
Outcome<FamilyDeathPolicy> decode_family_death_policy(const BoundaryValue& value){try{auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};using namespace boundary;fields(value,{"profile","player_death"},{},"family_life");if(get_string(value,"profile","")!="verified_military_deaths_v1")unsupported("UnsupportedFamilyDeathProfile","family_life/profile");if(get_string(value,"player_death","")!="await_explicit_continuation")fail("InvalidPlayerDeathPolicy","family_life/player_death");return {true,{true},{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// Same original explicit continuation requirement; this selects the complete
// original military/natural-age/sea cause union, not automatic inheritance.
Outcome<FamilyDeathPolicy> decode_family_death_policy4(const BoundaryValue& value){try{
 auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};
 using namespace boundary;fields(value,{"profile","player_death"},{},"family_life");
 if(get_string(value,"profile","")!="complete_life_causes_v4")unsupported("UnsupportedFamilyDeathProfile","family_life/profile");
 if(get_string(value,"player_death","")!="await_explicit_continuation")fail("InvalidPlayerDeathPolicy","family_life/player_death");
 return {true,{true,4},{}};
 }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_family_death_policy_selection(const FamilyDeathPolicy& policy,const CatalogIdentity& identity){
 if(policy.selected_version!=1&&policy.selected_version!=4)return common_failure("UnsupportedFamilyLifeVersion",{},ErrorKind::unsupported_capability);
 const StableId* selected=nullptr;for(const auto& module:identity.module_versions)if(module.first=="family_life"){if(selected)return common_failure("DuplicateFamilyLifeModule",{},ErrorKind::internal);selected=&module.second;}
 if(!policy.enabled)return selected?common_failure("FamilyLifeProfileMismatch",{},ErrorKind::internal):common_success();
 if(!selected||*selected!=(policy.selected_version==4?"4":"1"))return common_failure("FamilyLifeSelectedVersionMismatch",{},ErrorKind::unsupported_capability);
 return common_success();
}
// 整个新域先核唯一键，再逐项进入唯一的typed死亡来源模型。
Outcome<FamilyDeathState> decode_family_deaths(const BoundaryValue& value){try{auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};using namespace boundary;fields(value,{"deaths"},{},"family_life");FamilyDeathState out;out.field_order=order(value);for(const auto& row:object(field(value,"deaths",""),"deaths"))if(!out.deaths.insert(row.first,decode_record(row.second)))fail("DuplicateFamilyDeathFact",row.first);return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 保留原Common头对应的记录ID和原插入序；不另生成actor索引或新主键。
Status emit_family_death_fields(ProjectionWriter& w,const FamilyDeathRecord& v){auto valid=shape(v);if(!valid.ok)return w.reject(valid.error.original.code,"family_death");w.field("kind",v.kind);w.field("actor_id",v.actor_id);w.field("scenario_id",v.scenario_id);w.field("cause_kind",v.cause_kind);w.field("cause_fact_id",v.cause_fact_id);w.field("battle_id",v.battle_id);family_projection::world_hex(w,"world_id",v.world_id);w.field("death_tick",v.death_tick);w.field("recorded_tick",v.recorded_tick);return w.order_fields(v.field_order);}
Status emit_family_deaths_fields(ProjectionWriter& w,const FamilyDeathState& s){w.object("deaths",[&](auto& rows){for(const auto& row:s.deaths.rows()){rows.object(row.first,[&](auto& out){return emit_family_death_fields(out,row.second);});if(!rows.status().ok)return rows.status();}return rows.status();});return w.order_fields(s.field_order);}
BoundaryValue encode_family_deaths(const FamilyDeathState& state){return family_projection::encode([&](auto& w){return emit_family_deaths_fields(w,state);});}
// 新family_life/1才调用本visitor；既有Family与Common字节格式均不变化。
Status visit_family_deaths_checkpoint(CodecVisitor& c,FamilyDeathState& state){common_checkpoint::table(c,"deaths",state.deaths,visit_record);common_checkpoint::strings(c,"field_order",state.field_order);if(!c.status().ok)return c.status();return valid_order(state.field_order,{"deaths"})?c.status():c.reject("BoundaryFieldOrderMismatch");}
}
