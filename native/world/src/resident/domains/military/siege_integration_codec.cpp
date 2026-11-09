// 围城决定冷边界与typed语义摘要；持久格式显式版本化，不接受额外字段或布尔同意替身。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "siege_integration_decisions.hpp"
#include "siege_integration_projection.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "picosha2.h"
#include <set>
#include <type_traits>
namespace hanmo::resident {
namespace integration_codec {
// 有界字段访客共用于JSON和检查点，不在热状态藏入任意动态字段。
template<class T> struct Vector:std::false_type{};
template<class T> struct Vector<std::vector<T>>:std::true_type{using Value=T;};
template<class T,class=void> struct Ordered:std::false_type{};
template<class T> struct Ordered<T,std::void_t<decltype(std::declval<T>().field_order)>>:std::true_type{};
// SiegeIntegrationTerms只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationTerms>,int> =0> void record(A& a,V& v){a("kind",v.kind);a("preserve_unit_command",v.preserve_unit_command);a("preserve_private_property",v.preserve_private_property);a("transfer_current_public_assets",v.transfer_current_public_assets);a("transfer_prisoner_custody",v.transfer_prisoner_custody);a("recipient_future_service_responsibility",v.recipient_future_service_responsibility);a("retain_original_arrears",v.retain_original_arrears);a("population_policy_ids",v.population_policy_ids);}
// SiegeIntegrationAppointment只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationAppointment>,int> =0> void record(A& a,V& v){a("id",v.id);a("holder_actor_id",v.holder_actor_id);a("appointment_id",v.appointment_id);a("enabled",v.enabled);}
// SiegeIntegrationUnit只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationUnit>,int> =0> void record(A& a,V& v){a("id",v.id);a("source_garrison_id",v.source_garrison_id);a("organization_id",v.organization_id);a("army_id",v.army_id);a("commander_actor_id",v.commander_actor_id);a("troop_type_id",v.troop_type_id);a("member_ids",v.member_ids);a("fit",v.fit);a("wounded",v.wounded);a("dead",v.dead);a("captured",v.captured);a("deserted",v.deserted);a("captors",v.captors);a("retainer_sources",v.retainer_sources);}
// SiegeIntegrationOfficer只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationOfficer>,int> =0> void record(A& a,V& v){a("actor_id",v.actor_id);a("army_id",v.army_id);a("formation_source_id",v.formation_source_id);a("unit_ids",v.unit_ids);a("appointment_sources",v.appointment_sources);}
// SiegeIntegrationRetainer只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationRetainer>,int> =0> void record(A& a,V& v){a("actor_id",v.actor_id);a("estate_id",v.estate_id);a("source_id",v.source_id);a("unit_id",v.unit_id);}
// SiegeIntegrationArmy只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationArmy>,int> =0> void record(A& a,V& v){a("id",v.id);a("organization_id",v.organization_id);a("campaign_id",v.campaign_id);a("formation_source_id",v.formation_source_id);a("commander_actor_id",v.commander_actor_id);a("office_id",v.office_id);a("garrison_id",v.garrison_id);a("inventory_id",v.inventory_id);a("wallet_id",v.wallet_id);a("location",v.location);a("unit_ids",v.unit_ids);a("member_actor_ids",v.member_actor_ids);a("prisoner_ids",v.prisoner_ids);a("retainers",v.retainers);a("units",v.units);a("officers",v.officers);}
// SiegeIntegrationProposal只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationProposal>,int> =0> void record(A& a,V& v){a("id",v.id);a("proposal_key",v.proposal_key);a("siege_id",v.siege_id);a("campaign_id",v.campaign_id);a("stronghold_id",v.stronghold_id);a("proposer_actor_id",v.proposer_actor_id);a("actor_action_source_id",v.actor_action_source_id);a("created_tick",v.created_tick);a("recipient",v.recipient);a("defender_armies",v.defender_armies);a("required_officers",v.required_officers);a("terms",v.terms);}
// SiegeIntegrationActorAction只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationActorAction>,int> =0> void record(A& a,V& v){a("id",v.id);a("command",v.command);a("actor_id",v.actor_id);a("proposal_id",v.proposal_id);a("proposal_key",v.proposal_key);a("decision",v.decision);a("decision_source_id",v.decision_source_id);a("policy_reason",v.policy_reason);a("tick",v.tick);}
// SiegeOfficerDecision只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeOfficerDecision>,int> =0> void record(A& a,V& v){a("id",v.id);a("proposal_id",v.proposal_id);a("proposal_key",v.proposal_key);a("actor_id",v.actor_id);a("decision",v.decision);a("actor_action_source_id",v.actor_action_source_id);a("legal_basis",v.legal_basis);a("policy_reason",v.policy_reason);a("tick",v.tick);a("officer",v.officer);}
// SiegeIntegrationAttempt只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationAttempt>,int> =0> void record(A& a,V& v){a("proposal_id",v.proposal_id);a("status",v.status);a("reason",v.reason);a("completion_source_id",v.completion_source_id);a("decision_source_ids",v.decision_source_ids);}
// SiegeIntegrationOfficerPolicy只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationOfficerPolicy>,int> =0> void record(A& a,V& v){a("actor_id",v.actor_id);a("allowed_recipient_organization_ids",v.allowed_recipient_organization_ids);a("allowed_terms",v.allowed_terms);a("minimum_willingness",v.minimum_willingness);a("require_unit_command",v.require_unit_command);a("require_private_property",v.require_private_property);}
// SiegeIntegrationState只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationState>,int> =0> void record(A& a,V& v){a("proposals",v.proposals);a("actor_actions",v.actor_actions);a("decisions",v.decisions);a("attempts",v.attempts);}
// SiegeIntegrationCatalog只访问正式声明字段，拒绝静默遗漏或新增来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,SiegeIntegrationCatalog>,int> =0> void record(A& a,V& v){a("enabled",v.enabled);a("officer_policies",v.officer_policies);a("default_terms",v.default_terms);}

// MilitaryIntegrationWallet与提议复用同一快照编解码，禁止缩减真实钱货或人口来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,MilitaryIntegrationWallet>,int> =0> void record(A& a,V& v){a("source_wallet_id",v.source_wallet_id);a("destination_wallet_id",v.destination_wallet_id);a("source_organization_id",v.source_organization_id);a("recipient_organization_id",v.recipient_organization_id);a("amount",v.amount);a("source_before",v.source_before);a("source_after",v.source_after);a("destination_before",v.destination_before);a("destination_after",v.destination_after);}
// MilitaryIntegrationLot与提议复用同一快照编解码，禁止缩减真实钱货或人口来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,MilitaryIntegrationLot>,int> =0> void record(A& a,V& v){a("source_inventory_id",v.source_inventory_id);a("destination_inventory_id",v.destination_inventory_id);a("lot_id",v.lot_id);a("item_id",v.item_id);a("source_organization_id",v.source_organization_id);a("recipient_organization_id",v.recipient_organization_id);a("quantity",v.quantity);}
// MilitaryIntegrationCustody与提议复用同一快照编解码，禁止缩减真实钱货或人口来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,MilitaryIntegrationCustody>,int> =0> void record(A& a,V& v){a("member_id",v.member_id);a("from_army_id",v.from_army_id);a("to_army_id",v.to_army_id);}
// MilitaryIntegrationFact与提议复用同一快照编解码，禁止缩减真实钱货或人口来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,MilitaryIntegrationFact>,int> =0> void record(A& a,V& v){a("id",v.id);a("proposal_id",v.proposal_id);a("proposal_key",v.proposal_key);a("siege_id",v.siege_id);a("stronghold_id",v.stronghold_id);a("recipient_army_id",v.recipient_army_id);a("recipient_organization_id",v.recipient_organization_id);a("tick",v.tick);a("decision_source_ids",v.decision_source_ids);a("source_armies",v.source_armies);a("recipient_units_before",v.recipient_units_before);a("recipient_units_after",v.recipient_units_after);a("recipient_members_before",v.recipient_members_before);a("recipient_members_after",v.recipient_members_after);a("wallets",v.wallets);a("lots",v.lots);a("custody",v.custody);a("ended_appointments",v.ended_appointments);a("closed_campaign_ids",v.closed_campaign_ids);a("population_policy_ids",v.population_policy_ids);a("population_receipt_ids",v.population_receipt_ids);}
template<class T> BoundaryValue encode(const T&);
template<class T> Status emit(ProjectionWriter&,const StableId&,const T&);
template<class T> void decode(const BoundaryValue&,T&,const StableId&);
template<class T> Status visit(CodecVisitor&,const StableId&,T&,bool=false);
// 字段序是边界元数据，不属于业务摘要。
struct Encoder {ProjectionWriter& writer;template<class T> void operator()(const char* key,const T& value){integration_codec::emit(writer,key,value);}};
struct Decoder {const BoundaryValue& value;StableId path;std::set<StableId> fields;template<class T> void operator()(const char* key,T& output){fields.insert(key);decode(boundary::field(value,key,path),output,path+"/"+key);}void finish(){std::set<StableId> actual;for(const auto& row:boundary::object(value,path)){if(!actual.insert(row.first).second)boundary::fail("DuplicateSiegeIntegrationField",path+"/"+row.first);if(!fields.count(row.first))boundary::fail("UnsupportedResidentField",path+"/"+row.first);}}};
struct Checkpointer {CodecVisitor& codec;bool semantic;template<class T> void operator()(const char* key,T& value){integration_codec::visit(codec,key,value,semantic);}};
// 关联映射沿原GD字典格式输出，所有叶值仍使用共同类型。
template<class T> Status emit(ProjectionWriter& w,const StableId& key,const T& value){
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(key,value);
    else if constexpr(std::is_same_v<T,Location>)return emit_common_location(w,key,value);
    else if constexpr(std::is_same_v<T,std::vector<std::pair<StableId,StableId>>>)return w.object(key,[&](auto& child){for(const auto& row:value)child.field(row.first,row.second);return child.status();});
    else if constexpr(Vector<T>::value)return w.sequence(key,value,[](auto& child,const auto& row){return integration_codec::emit(child,"",row);});
    else return w.object(key,[&](auto& child){Encoder a{child};record(a,value);if constexpr(Ordered<T>::value)child.order_fields(value.field_order);return child.status();});
}
template<class T> BoundaryValue encode(const T& value){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=integration_codec::emit(sink,"",value);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
// 类型、重复键、未知字段均硬拒绝，不补造事实来源。
template<class T> void decode(const BoundaryValue& value,T& out,const StableId& path){
    if constexpr(std::is_same_v<T,StableId>)out=boundary::string(value,path);
    else if constexpr(std::is_same_v<T,Integer>)out=boundary::integer(value,path);
    else if constexpr(std::is_same_v<T,bool>)out=boundary::boolean(value,path);
    else if constexpr(std::is_same_v<T,Location>)out=boundary::decode_location(value,path);
    else if constexpr(std::is_same_v<T,std::vector<std::pair<StableId,StableId>>>){out.clear();std::set<StableId> keys;for(auto& row:boundary::object(value,path)){if(!keys.insert(row.first).second)boundary::fail("DuplicateSiegeIntegrationField",path+"/"+row.first);out.emplace_back(row.first,boundary::string(row.second,path+"/"+row.first));}}
    else if constexpr(Vector<T>::value){out.clear();for(auto& row:boundary::array(value,path)){typename Vector<T>::Value item{};decode(row,item,path+"/"+std::to_string(out.size()));out.push_back(std::move(item));}}
    else{Decoder a{value,path,{}};record(a,out);a.finish();if constexpr(Ordered<T>::value)out.field_order=boundary::order(value);}
}
// 检查点逐类型保全，摘要模式仅略去可重排字典元数据。
template<class T> Status visit(CodecVisitor& c,const StableId& key,T& value,bool semantic){
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(key,value);
    else if constexpr(std::is_same_v<T,Location>)return c.object(key,[&](auto& child){return common_checkpoint::location(child,value);});
    else if constexpr(std::is_same_v<T,std::vector<std::pair<StableId,StableId>>>)return c.sequence(key,value,[](auto& child,auto& row){child.field("key",row.first);return child.field("value",row.second);});
    else if constexpr(Vector<T>::value)return c.sequence(key,value,[&](auto& child,auto& row){return integration_codec::visit(child,"item",row,semantic);});
    else return c.object(key,[&](auto& child){Checkpointer a{child,semantic};record(a,value);if constexpr(Ordered<T>::value)if(!semantic)common_checkpoint::strings(child,"field_order",value.field_order);return child.status();});
}
}
// 同一个typed合同固定摘要，不使用全局tick、revision、提议ID或公库余额。
StableId siege_integration_proposal_key(const SiegeIntegrationProposal& proposal){auto value=proposal;value.id.clear();value.proposal_key.clear();value.actor_action_source_id.clear();value.created_tick=0;CodecVisitor::Buffer bytes;CodecVisitor codec(bytes);integration_codec::visit(codec,"siege_integration_semantics_v1",value,true);auto status=codec.finish();if(!status.ok)return {};return picosha2::hash256_hex_string(bytes);}
// JSON状态必须显式声明版本；旧reader不会静默把本域记录当普通军令。
Outcome<SiegeIntegrationState> decode_siege_integration_state(const BoundaryValue& value){try{boundary::fields(value,{"format","state"},{},"siege_integration");if(boundary::get_string(value,"format","siege_integration")!="HM-SIEGE-INTEGRATION/1")boundary::fail("UnsupportedSiegeIntegrationFormat","siege_integration/format");SiegeIntegrationState out;integration_codec::decode(boundary::field(value,"state","siege_integration"),out,"siege_integration/state");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 导出只序列化军事owner已拥有的事实，不替读入值修补来源。
Status emit_siege_integration_state_fields(ProjectionWriter& w,const SiegeIntegrationState& state){w.field("format",StableId("HM-SIEGE-INTEGRATION/1"));integration_codec::emit(w,"state",state);return w.status();}
BoundaryValue encode_siege_integration_state(const SiegeIntegrationState& state){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& w){return emit_siege_integration_state_fields(w,state);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
// 新版域由中央显式调用；错误版本使整个候选失败。
Status visit_siege_integration_checkpoint(CodecVisitor& codec,SiegeIntegrationState& state){Integer version=1;codec.field("version",version);if(!codec.status().ok)return codec.status();if(version!=1)return codec.reject("UnsupportedSiegeIntegrationFormat");return integration_codec::visit(codec,"state",state);}
// 作者政策也是有限严格字段，缺失政策列表的NPC由真实命令拒绝。
Outcome<SiegeIntegrationCatalog> decode_siege_integration_catalog(const BoundaryValue& value){try{SiegeIntegrationCatalog out;integration_codec::decode(value,out,"siege_integration");auto valid=validate_siege_integration_catalog(out);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 有限query只导出既有提议和决定，读取不能创建任何接受。
BoundaryValue encode_siege_integration_proposal(const SiegeIntegrationProposal& value){return integration_codec::encode(value);}
BoundaryValue encode_siege_integration_decision(const SiegeOfficerDecision& value){return integration_codec::encode(value);}
BoundaryValue encode_siege_integration_actor_action(const SiegeIntegrationActorAction& value){return integration_codec::encode(value);}
BoundaryValue encode_siege_integration_terms(const SiegeIntegrationTerms& value){return integration_codec::encode(value);}
// 外部命令条款与持久条款严格同型，未知键不能成为隐式接受或私人财产授权。
Outcome<SiegeIntegrationTerms> decode_siege_integration_terms(const BoundaryValue& value){try{SiegeIntegrationTerms out;integration_codec::decode(value,out,"siege_integration_terms");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 追加交接事实按唯一owner数组顺序保存；不把来源快照折叠成当前军籍。
Status emit_military_integration_facts(ProjectionWriter& w,const StableId& name,const std::vector<MilitaryIntegrationFact>& value){return integration_codec::emit(w,name,value);}
BoundaryValue encode_military_integration_facts(const std::vector<MilitaryIntegrationFact>& value){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=emit_military_integration_facts(sink,"",value);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
Outcome<std::vector<MilitaryIntegrationFact>> decode_military_integration_facts(const BoundaryValue& value){try{std::vector<MilitaryIntegrationFact> out;integration_codec::decode(value,out,"military_integration_facts");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status visit_military_integration_facts(CodecVisitor& codec,std::vector<MilitaryIntegrationFact>& value){return integration_codec::visit(codec,"military_integration_facts",value);}

}
