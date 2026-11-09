// 公开来源精确冻结原条款与真实会面；不在这里授予受领同意或资源执行权。
#include "bequest_disclosure_source.hpp"
#include "family_life_exact_codec_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "succession_personal_assets.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
#include "family_extension_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using Source=BequestDisclosureSource;
const std::vector<std::pair<const char*,StableId Source::*>> ids={{"declaration_id",&Source::declaration_id},{"donor_actor_id",&Source::donor_actor_id},{"beneficiary_actor_id",&Source::beneficiary_actor_id},{"dialogue_id",&Source::dialogue_id},{"node_id",&Source::node_id},{"node_kind",&Source::node_kind},{"session_id",&Source::session_id},{"scenario_id",&Source::scenario_id}};
const std::vector<std::pair<const char*,Integer Source::*>> numbers={{"tick",&Source::tick},{"revision",&Source::revision}};
// 元数据非空时须包含全部唯一字段，不修正坏档中的键顺序。
bool ordered(const FieldOrder& order,const std::set<StableId>& fields){return order.empty()||(order.size()==fields.size()&&std::set<StableId>(order.begin(),order.end())==fields);}
// 两种冷边界共用相同字段集合，不能各自默认省略许可范围的关键来源。
std::set<StableId> keys(Integer version){std::set<StableId> out{"world_id","context"};for(const auto& p:ids)out.insert(p.first);for(const auto& p:numbers)out.insert(p.first);if(version==2)out.insert("occurred_at");return out;}
// 结构层只允许真实场所say/choice；业务层重验原声明、本人、日期和实际源。
Status shape(const Source& s,Integer version){
    if(version!=1&&version!=2)return common_failure("FamilyInheritanceVersionRequired",{},ErrorKind::unsupported_capability);if((version==2)!=s.occurred_at.has_value())return common_failure("InheritanceProfileMismatch");if(s.occurred_at){auto valid=encode_world_moment(*s.occurred_at);if(!valid.ok)return {false,{},valid.error};if(s.tick!=s.occurred_at->half_day_tick)return common_failure("BequestDisclosureTimeMismatch");}
    for(const auto& p:ids)if((s.*p.second).empty()||(s.*p.second).size()>256)return common_failure("InvalidBequestDisclosureReference",p.first);
    if((s.node_kind!="say"&&s.node_kind!="choice")||s.tick<0||s.revision<0||s.donor_actor_id==s.beneficiary_actor_id||!face_to_face_contact_context_valid(s.context)||s.context.provider_actor_id!=s.beneficiary_actor_id)return common_failure("InvalidBequestDisclosureSource");
    const std::set<StableId> context=s.context.kind?std::set<StableId>{"place_id","kind","provider_actor_id","role_id"}:std::set<StableId>{"place_id","role_id","provider_actor_id"};
    return ordered(s.field_order,keys(version))&&ordered(s.context.field_order,context)?common_success():common_failure("BoundaryFieldOrderMismatch");
}
// 永久world_id固定小写十六进制，不截断任何一字节。
StableId hex(const WorldId& id){StableId out;const char* digits="0123456789abcdef";for(auto b:id){out+=digits[b>>4];out+=digits[b&15];}return out;}
// 只解码规范表示，全零或不同世界的权源由业务校验拒绝。
WorldId world(const StableId& value){if(value.size()!=32)boundary::fail("InvalidWorldId","bequest_disclosure/world_id");auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};WorldId out{};for(std::size_t i=0;i<16;++i){auto a=digit(value[2*i]),b=digit(value[2*i+1]);if(a<0||b<0)boundary::fail("InvalidWorldId","bequest_disclosure/world_id");out[i]=static_cast<std::uint8_t>(a*16+b);}return out;}
}
// 解码前递归拒绝根/嵌套重复键，不让find首匹配隐藏另一份接受或撤回。
Outcome<BequestDisclosureSource> decode_bequest_disclosure_source(const BoundaryValue& v,Integer version){try{
    auto unique=validate_succession_json_unique(v);if(!unique.ok)return {false,{},unique.error};using namespace boundary;
    if(version==2)fields(v,{"declaration_id","donor_actor_id","beneficiary_actor_id","dialogue_id","node_id","node_kind","session_id","scenario_id","world_id","context","tick","revision","occurred_at"},{},"bequest_disclosure_source");else fields(v,{"declaration_id","donor_actor_id","beneficiary_actor_id","dialogue_id","node_id","node_kind","session_id","scenario_id","world_id","context","tick","revision"},{},"bequest_disclosure_source");Source out;for(const auto& p:ids)out.*p.second=get_string(v,p.first,"");for(const auto& p:numbers)out.*p.second=get_int(v,p.first,"");out.world_id=world(get_string(v,"world_id",""));out.context=interaction_codec::decode_context(field(v,"context",""),"bequest_disclosure/context");if(version==2){out.occurred_at=WorldMoment{};ActorFoundationJsonVisitor reader(field(v,"occurred_at",""));auto valid=family_life4_codec::moment(reader,*out.occurred_at);if(valid.ok)valid=reader.finish();if(!valid.ok)return {false,{},valid.error};}out.field_order=order(v);auto valid=shape(out,version);return valid.ok?Outcome<Source>{true,std::move(out),{}}:Outcome<Source>{false,{},valid.error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原公开快照完整导出，当前页面和目录默认值不能回填已保存来源。
Status emit_bequest_disclosure_source_fields(ProjectionWriter& w,const BequestDisclosureSource& s,Integer version){auto valid=shape(s,version);if(!valid.ok)return w.reject(valid.error.original.code,"bequest_disclosure_source");for(const auto& f:ids)w.field(f.first,s.*f.second);family_projection::world_hex(w,"world_id",s.world_id);w.object("context",[&](auto& out){return emit_interaction_context_fields(out,s.context);});for(const auto& f:numbers)w.field(f.first,s.*f.second);if(version==2)emit_projection_record<ActorFoundationProjectionArchive>(w,"occurred_at",*s.occurred_at,[](auto& a,auto& v){return family_life4_codec::moment(a,v);});return w.order_fields(s.field_order);}
BoundaryValue encode_bequest_disclosure_source(const BequestDisclosureSource& s,Integer version){return family_projection::encode([&](auto& w){return emit_bequest_disclosure_source_fields(w,s,version);});}
// 新遗产段逐字段遍历，旧Family/声明/控制visitor不进入此路径。
Status visit_bequest_disclosure_source(CodecVisitor& c,BequestDisclosureSource& s,Integer version){
    if(version!=1&&version!=2)return c.reject("FamilyInheritanceVersionRequired");if(!c.reading()&&((version==2)!=s.occurred_at.has_value()))return c.reject("InheritanceProfileMismatch");if(c.reading()){s.occurred_at.reset();if(version==2)s.occurred_at=WorldMoment{};}
    for(const auto& p:ids)c.field(p.first,s.*p.second);std::vector<Integer> bytes;if(c.reading())c.sequence("world_id",bytes,[](CodecVisitor& v,Integer& b){return v.field("byte",b);});else c.write_sequence("world_id",s.world_id,[](CodecVisitor& v,const auto& raw){Integer b=raw;return v.field("byte",b);});if(c.reading()&&c.status().ok){if(bytes.size()!=16)return c.reject("InvalidWorldId");for(std::size_t i=0;i<16;++i){if(bytes[i]<0||bytes[i]>255)return c.reject("InvalidWorldId");s.world_id[i]=static_cast<std::uint8_t>(bytes[i]);}}
    c.object("context",[&](CodecVisitor& v){return interaction_codec::visit_context(v,s.context);});for(const auto& p:numbers)c.field(p.first,s.*p.second);if(version==2)c.object("occurred_at",[&](auto& a){return family_life4_codec::moment(a,*s.occurred_at);});common_checkpoint::strings(c,"field_order",s.field_order);if(!c.status().ok)return c.status();auto valid=shape(s,version);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
