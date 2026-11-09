#include "family_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
// 具名接续同意的严格永久来源；不把暂态epoch、页面回调或一般对白保存成授权。
#include "succession_consent_source.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
const std::vector<std::pair<const char*,StableId SuccessionConsentSource::*>> ids={
    {"request_id",&SuccessionConsentSource::request_id},{"relation_fact_id",&SuccessionConsentSource::relation_fact_id},
    {"dialogue_id",&SuccessionConsentSource::dialogue_id},{"node_id",&SuccessionConsentSource::node_id},
    {"node_kind",&SuccessionConsentSource::node_kind},{"session_id",&SuccessionConsentSource::session_id},
    {"scenario_id",&SuccessionConsentSource::scenario_id}};
const std::vector<std::pair<const char*,Integer SuccessionConsentSource::*>> numbers={
    {"terms_version",&SuccessionConsentSource::terms_version},{"accepted_tick",&SuccessionConsentSource::accepted_tick},
    {"accepted_revision",&SuccessionConsentSource::accepted_revision},{"predecessor_birth_tick",&SuccessionConsentSource::predecessor_birth_tick},
    {"successor_birth_tick",&SuccessionConsentSource::successor_birth_tick}};
// 非空字段顺序必须完整而唯一；不能读后再丢失未识别来源键。
bool valid_order(const FieldOrder& order,const std::set<StableId>& fields){return order.empty()||(order.size()==fields.size()&&std::set<StableId>(order.begin(),order.end())==fields);}
// 同一具名字段表同时约束二进制顺序元数据，减少JSON与typed冷边界漂移。
std::set<StableId> expected_keys(){std::set<StableId> result{"world_id","context","personal"};for(const auto& field:ids)result.insert(field.first);for(const auto& field:numbers)result.insert(field.first);return result;}
// world_id保持原16字节小写十六进制；不接受宽松解析或截断。

// 只解析规范世界标识；全零有效性由具名历史来源validator核验。
WorldId decode_world(const StableId& text){using namespace boundary;if(text.size()!=32)fail("InvalidWorldId","succession_consent/world_id");auto digit=[](char c)->Integer{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;boundary::fail("InvalidWorldId","succession_consent/world_id");return 0;};WorldId result{};for(std::size_t i=0;i<16;++i)result[i]=static_cast<std::uint8_t>(digit(text[2*i])*16+digit(text[2*i+1]));return result;}
}
// 新域先核递归唯一键，再由原boundary核必需/未知字段；业务许可仍须完整家事验证。
Outcome<SuccessionConsentSource> decode_succession_consent_source(const BoundaryValue& value){try{
    auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};
    using namespace boundary;fields(value,{"request_id","relation_fact_id","dialogue_id","node_id","node_kind","session_id","scenario_id","world_id","context","personal","terms_version","accepted_tick","accepted_revision","predecessor_birth_tick","successor_birth_tick"},{},"succession_consent_source");
    SuccessionConsentSource result;for(const auto& field:ids)result.*field.second=get_string(value,field.first,"");for(const auto& field:numbers)result.*field.second=get_int(value,field.first,"");result.world_id=decode_world(get_string(value,"world_id",""));result.context=interaction_codec::decode_context(field(value,"context",""),"succession_consent/context");auto personal=decode_succession_personal_terms(field(value,"personal",""));if(!personal.ok)return {false,{},personal.error};result.personal=std::move(personal.value);result.field_order=order(value);return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 每次导出完整原条款，不从当前账户重新推导当时赠与与同意范围。
Status emit_succession_consent_source_fields(ProjectionWriter& out,const SuccessionConsentSource& source){for(const auto& f:ids)out.field(f.first,source.*f.second);family_projection::world_hex(out,"world_id",source.world_id);out.object("context",[&](auto& w){return emit_interaction_context_fields(w,source.context);});out.object("personal",[&](auto& w){return emit_succession_personal_terms_fields(w,source.personal);});for(const auto& f:numbers)out.field(f.first,source.*f.second);return out.order_fields(source.field_order);}
BoundaryValue encode_succession_consent_source(const SuccessionConsentSource& source){return family_projection::encode([&](auto& w){return emit_succession_consent_source_fields(w,source);});}
// 二进制只按显式新段版本调用；旧Family字节不自动增加存在位。
namespace {
Status visit_source(CodecVisitor& c,SuccessionConsentSource& source,bool paths){
    for(const auto& field:ids)c.field(field.first,source.*field.second);std::vector<Integer> bytes;for(const auto byte:source.world_id)bytes.push_back(byte);c.sequence("world_id",bytes,[](CodecVisitor& v,Integer& b){return v.field("byte",b);});
    if(c.reading()&&c.status().ok){if(bytes.size()!=16)return c.reject("InvalidWorldId");for(std::size_t i=0;i<16;++i){if(bytes[i]<0||bytes[i]>255)return c.reject("InvalidWorldId");source.world_id[i]=static_cast<std::uint8_t>(bytes[i]);}}
    c.object("context",[&](CodecVisitor& v){return interaction_codec::visit_context(v,source.context);});c.object("personal",[&](CodecVisitor& v){return visit_succession_personal_terms(v,source.personal);});for(const auto& field:numbers)c.field(field.first,source.*field.second);common_checkpoint::strings(c,"field_order",source.field_order);
    if(!c.status().ok)return c.status();if(paths&&source.context.kind==std::optional<StableId>{"narrative"}&&(!source.context.role_id.empty()||source.context.encounter_id||source.context.input_error))return c.reject("InvalidVoluntaryPathContext");const auto context_fields=paths&&source.context.kind==std::optional<StableId>{"narrative"}?std::set<StableId>{"place_id","role_id","provider_actor_id","kind"}:std::set<StableId>{"place_id","role_id","provider_actor_id"};if(!valid_order(source.context.field_order,context_fields)||!valid_order(source.field_order,expected_keys()))return c.reject("BoundaryFieldOrderMismatch");return c.status();
}
}
// 原公开入口仍执行原字段顺序规则，字节与旧Family1..16一致。
Status visit_succession_consent_source(CodecVisitor& c,SuccessionConsentSource& source){return visit_source(c,source,false);}
Status visit_succession_consent_source_paths(CodecVisitor& c,SuccessionConsentSource& source){return visit_source(c,source,true);}
}
