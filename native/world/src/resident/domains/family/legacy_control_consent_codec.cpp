// 条件控制许可的永久来源；本域无钱货字段，坏档不能凭额外scope取得遗产或知情。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "legacy_control_consent.hpp"
#include "succession_personal_assets.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
#include "family_extension_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_proof_codec.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_proof_codec.hpp"
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using Source=LegacyControlDecisionSource;
enum class SourceFormat { original1,biological2,collateral3,declared_age4 };
const std::vector<std::pair<const char*,StableId Source::*>> ids={
    {"request_id",&Source::request_id},{"decision",&Source::decision},{"predecessor_actor_id",&Source::predecessor_actor_id},
    {"successor_actor_id",&Source::successor_actor_id},{"relation_fact_id",&Source::relation_fact_id},{"dialogue_id",&Source::dialogue_id},
    {"node_id",&Source::node_id},{"node_kind",&Source::node_kind},{"session_id",&Source::session_id},{"scenario_id",&Source::scenario_id}};
const std::vector<std::pair<const char*,Integer Source::*>> numbers={
    {"terms_version",&Source::terms_version},{"tick",&Source::tick},{"revision",&Source::revision},
    {"predecessor_birth_tick",&Source::predecessor_birth_tick},{"successor_birth_tick",&Source::successor_birth_tick}};
// 元数据非空时须包含全部唯一字段，不修正坏档中的键顺序。
bool ordered(const FieldOrder& order,const std::set<StableId>& fields){return order.empty()||(order.size()==fields.size()&&std::set<StableId>(order.begin(),order.end())==fields);}
// 两种冷边界共用相同字段集合，不能各自默认省略许可范围的关键来源。
std::set<StableId> keys(SourceFormat format){std::set<StableId> out{"world_id","context"};if(format!=SourceFormat::original1){out.insert("biological_origin");out.insert("born_dialogue");}if((format==SourceFormat::collateral3||format==SourceFormat::declared_age4))out.insert("collateral_dialogue");for(const auto& p:ids)out.insert(p.first);for(const auto& p:numbers)if(format!=SourceFormat::declared_age4||p.second==&Source::terms_version||p.second==&Source::tick||p.second==&Source::revision)out.insert(p.first);if(format==SourceFormat::declared_age4)out.insert("age_sources");return out;}
// 结构层只允许生前真实场所节点；业务层继续重验人物、关系、日期和作者语义。
Status shape(const Source& s,SourceFormat format){
    const bool biological=s.biological_origin.has_value();
    if(format==SourceFormat::declared_age4){if(!s.age_sources||s.predecessor_birth_tick!=0||s.successor_birth_tick!=0)return common_failure("LegacyControlActualAgeSourcesRequired");}else if(s.age_sources)return common_failure("LegacyControlDeclaredAgeFormatRequired");
    if((biological||s.born_dialogue)&&format==SourceFormat::original1)return common_failure("LegacyControlBiologicalFormatRequired");
    if(s.collateral_dialogue&&(format!=SourceFormat::collateral3&&format!=SourceFormat::declared_age4))return common_failure("LegacyControlCollateralFormatRequired");
    if(s.collateral_dialogue&&(biological||s.born_dialogue))return common_failure("AmbiguousLegacyControlRelation");
    for(const auto& p:ids){if(p.second==&Source::relation_fact_id&&biological){if(!s.relation_fact_id.empty())return common_failure("AmbiguousLegacyControlRelation");continue;}if((s.*p.second).empty()||(s.*p.second).size()>256)return common_failure("InvalidLegacyControlReference",p.first);}
    if((s.decision!="accept"&&s.decision!="revoke")||s.terms_version!=1||s.tick<0||s.revision<0||s.predecessor_actor_id==s.successor_actor_id||!face_to_face_contact_context_valid(s.context)||(!biological&&!s.collateral_dialogue&&s.context.kind)||s.context.encounter_id||s.context.input_error)return common_failure("InvalidLegacyControlDecision");
    if(biological&&(s.node_kind!="say"||s.biological_origin->parent_actor_id!=s.predecessor_actor_id||s.biological_origin->child_actor_id!=s.successor_actor_id))return common_failure("LegacyControlBiologicalPartiesMismatch");
    if(s.born_dialogue){
        const auto& p=*s.born_dialogue;const auto& m=p.declaration.meeting;const auto& b=p.born;
        if(!biological||s.biological_origin->kind!=BiologicalChildOriginKind::recorded_birth||p.opening_fact_id.empty()||p.template_id.empty()||p.opening_fact_id.size()>256||p.template_id.size()>256||p.declaration.speaker_actor_id!=s.successor_actor_id||m.actor_id!=s.predecessor_actor_id||m.provider_actor_id!=s.successor_actor_id||m.session_id!=s.session_id||m.dialogue_id!=s.dialogue_id||m.node_id!=s.node_id||m.node_kind!=s.node_kind||m.source_identity.world_id!=s.world_id||m.source_identity.tick!=s.tick||m.source_identity.revision!=s.revision||m.source_identity.execution_epoch<1||m.context.place_id!=s.context.place_id||m.context.role_id!=s.context.role_id||m.context.provider_actor_id!=s.context.provider_actor_id||m.context.kind!=s.context.kind||m.context.encounter_id!=s.context.encounter_id||m.context.input_error!=s.context.input_error||!s.context.kind||*s.context.kind!="narrative"||b.actor_id!=s.successor_actor_id||b.birth_fact_id!=s.biological_origin->birth_fact_id||b.creation_fact_id!=s.biological_origin->creation_fact_id||(format==SourceFormat::declared_age4?(!std::holds_alternative<BirthFactRef>(s.age_sources->successor)||std::get<BirthFactRef>(s.age_sources->successor).birth_fact_id!=b.birth_fact_id):b.birth_tick!=s.successor_birth_tick)||b.observed_tick!=s.tick||!b.before_fact_id.empty()||!b.adult||!b.alive||b.source_identity.world_id!=m.source_identity.world_id||b.source_identity.tick!=m.source_identity.tick||b.source_identity.revision!=m.source_identity.revision||b.source_identity.execution_epoch!=m.source_identity.execution_epoch||b.biological_parent_ids.size()!=2||b.biological_parent_ids[0]==b.biological_parent_ids[1]||std::find(b.biological_parent_ids.begin(),b.biological_parent_ids.end(),s.predecessor_actor_id)==b.biological_parent_ids.end())return common_failure("LegacyControlBornDialogueMismatch");
    }else if(biological&&s.biological_origin->kind==BiologicalChildOriginKind::recorded_birth)return common_failure("LegacyControlBornDialogueRequired");
    if(s.collateral_dialogue){const auto& p=*s.collateral_dialogue;const auto& m=p.declaration.meeting;
        if(s.node_kind!="say"||p.opening_fact_id.empty()||p.opening_fact_id.size()>256||p.template_id.empty()||p.template_id.size()>256||p.declaration.speaker_actor_id!=s.successor_actor_id||m.actor_id!=s.predecessor_actor_id||m.provider_actor_id!=s.successor_actor_id||m.session_id!=s.session_id||m.dialogue_id!=s.dialogue_id||m.node_id!=s.node_id||m.node_kind!=s.node_kind||m.source_identity.world_id!=s.world_id||m.source_identity.tick!=s.tick||m.source_identity.revision!=s.revision||m.source_identity.execution_epoch<1||m.context.place_id!=s.context.place_id||m.context.provider_actor_id!=s.context.provider_actor_id||m.context.role_id!=s.context.role_id||m.context.kind!=s.context.kind||m.context.encounter_id!=s.context.encounter_id||m.context.input_error!=s.context.input_error||s.context.kind!=std::optional<StableId>{"narrative"}||p.listener_origin.actor_id!=s.predecessor_actor_id||p.provider_origin.actor_id!=s.successor_actor_id)return common_failure("LegacyControlCollateralDialogueMismatch");
        for(const auto* o:{&p.listener_origin,&p.provider_origin})if(o->kind=="installed_actor"){if(!o->birth_fact_id.empty()||!o->creation_fact_id.empty()||!o->prototype_id.empty())return common_failure("LegacyControlCollateralActorMismatch");}else if(o->kind!="recorded_birth"||o->birth_fact_id.empty()||o->creation_fact_id!=o->actor_id||o->prototype_id.empty())return common_failure("LegacyControlCollateralActorMismatch");
    }
    std::set<StableId> context_keys{"place_id","role_id","provider_actor_id"};if(s.context.kind)context_keys.insert("kind");
    return ordered(s.field_order,keys(format))&&ordered(s.context.field_order,context_keys)?common_success():common_failure("BoundaryFieldOrderMismatch");
}
// 永久world_id固定小写十六进制，不截断任何一字节。
StableId hex(const WorldId& id){StableId out;const char* digits="0123456789abcdef";for(auto b:id){out+=digits[b>>4];out+=digits[b&15];}return out;}
// 只解码规范表示，全零或不同世界的权源由业务校验拒绝。
WorldId world(const StableId& value){if(value.size()!=32)boundary::fail("InvalidWorldId","legacy_control/world_id");auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};WorldId out{};for(std::size_t i=0;i<16;++i){auto a=digit(value[2*i]),b=digit(value[2*i+1]);if(a<0||b<0)boundary::fail("InvalidWorldId","legacy_control/world_id");out[i]=static_cast<std::uint8_t>(a*16+b);}return out;}
}
namespace {
// 解码前递归拒绝根/嵌套重复键，不让find首匹配隐藏另一份接受或撤回。
Outcome<LegacyControlDecisionSource> decode_source(const BoundaryValue& v,SourceFormat format){try{
    auto unique=validate_succession_json_unique(v);if(!unique.ok)return {false,{},unique.error};using namespace boundary;
    if(format==SourceFormat::declared_age4)fields(v,{"request_id","decision","predecessor_actor_id","successor_actor_id","relation_fact_id","dialogue_id","node_id","node_kind","session_id","scenario_id","world_id","context","terms_version","tick","revision","age_sources"},{"biological_origin","born_dialogue","collateral_dialogue"},"legacy_control_decision");else
    fields(v,{"request_id","decision","predecessor_actor_id","successor_actor_id","relation_fact_id","dialogue_id","node_id","node_kind","session_id","scenario_id","world_id","context","terms_version","tick","revision","predecessor_birth_tick","successor_birth_tick"},{"biological_origin","born_dialogue","collateral_dialogue"},"legacy_control_decision");
    Source out;const auto* biological=v.find("biological_origin");if(format==SourceFormat::original1){if(biological)fail("UnsupportedResidentField","legacy_control_decision/biological_origin");}
    else{if(!biological)fail("MissingResidentField","legacy_control_decision/biological_origin");if(!biological->as<std::monostate>()){auto origin=decode_biological_control_origin(*biological);if(!origin.ok)return {false,{},origin.error};out.biological_origin=std::move(origin.value);}}
    const auto* meeting=v.find("born_dialogue");if(format==SourceFormat::original1){if(meeting)fail("UnsupportedResidentField","legacy_control_decision/born_dialogue");}else{if(!meeting)fail("MissingResidentField","legacy_control_decision/born_dialogue");if(!meeting->as<std::monostate>()){auto proof=decode_born_personal_dialogue_proof(*meeting);if(!proof.ok)return {false,{},proof.error};out.born_dialogue=std::move(proof.value);}}
    const auto* collateral=v.find("collateral_dialogue");if((format!=SourceFormat::collateral3&&format!=SourceFormat::declared_age4)){if(collateral)fail("UnsupportedResidentField","legacy_control_decision/collateral_dialogue");}else{if(!collateral)fail("MissingResidentField","legacy_control_decision/collateral_dialogue");if(!collateral->as<std::monostate>()){auto proof=decode_known_personal_dialogue_proof(*collateral);if(!proof.ok)return {false,{},proof.error};out.collateral_dialogue=std::move(proof.value);}}
    for(const auto& p:ids)out.*p.second=get_string(v,p.first,"");for(const auto& p:numbers)if(format!=SourceFormat::declared_age4||p.second==&Source::terms_version||p.second==&Source::tick||p.second==&Source::revision)out.*p.second=get_int(v,p.first,"");if(format==SourceFormat::declared_age4){auto ages=decode_family_decision_age_sources4(field(v,"age_sources",""));if(!ages.ok)return {false,{},ages.error};out.age_sources=std::move(ages.value);}out.world_id=world(get_string(v,"world_id",""));out.context=interaction_codec::decode_context(field(v,"context",""),"legacy_control/context");out.field_order=order(v);auto valid=shape(out,format);return valid.ok?Outcome<Source>{true,std::move(out),{}}:Outcome<Source>{false,{},valid.error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原接受/撤回快照完整导出，不用当前页面或目录默认值回填已保存来源。
BoundaryValue encode_source(const LegacyControlDecisionSource& s,SourceFormat format){return family_projection::encode([&](auto& w){return emit_legacy_control_decision_source_fields(w,s,static_cast<Integer>(format)+1);});}
// 新段逐字段遍历；旧Family与既有退隐同意visitor从不进入此路径。
Status visit_source(CodecVisitor &c, LegacyControlDecisionSource &s,
                    SourceFormat format) {
    for (const auto &p : ids)
        c.field(p.first, s.*p.second);
    std::vector<Integer> bytes;
    for (auto b : s.world_id)
        bytes.push_back(b);
    c.sequence("world_id", bytes, [](CodecVisitor &v, Integer &b) {
        return v.field("byte", b);
    });
    if (c.reading() && c.status().ok) {
        if (bytes.size() != 16)
            return c.reject("InvalidWorldId");
        for (std::size_t i = 0; i < 16; ++i) {
            if (bytes[i] < 0 || bytes[i] > 255)
                return c.reject("InvalidWorldId");
            s.world_id[i] = static_cast<std::uint8_t>(bytes[i]);
        }
    }
    c.object("context", [&](CodecVisitor &v) {
        return interaction_codec::visit_context(v, s.context);
    });
    for (const auto &p : numbers)
        if (format != SourceFormat::declared_age4 ||
            p.second == &Source::terms_version || p.second == &Source::tick ||
            p.second == &Source::revision)
            c.field(p.first, s.*p.second);
    if (format == SourceFormat::declared_age4) {
        if (c.reading())
            s.age_sources.emplace();
        if (!s.age_sources)
            return c.reject("LegacyControlActualAgeSourcesRequired");
        c.object("age_sources", [&](CodecVisitor &v) {
            return visit_family_decision_age_sources4(v, *s.age_sources);
        });
    }
    if (format != SourceFormat::original1) {
        c.optional("biological_origin", s.biological_origin,
                   visit_biological_control_origin_checkpoint);
        c.optional("born_dialogue", s.born_dialogue,
                   [](CodecVisitor &visitor, BornPersonalDialogueProof &proof) {
                       return visit_born_personal_dialogue_proof(visitor,
                                                                 proof);
                   });
    }
    if ((format == SourceFormat::collateral3 ||
         format == SourceFormat::declared_age4))
        c.optional(
            "collateral_dialogue", s.collateral_dialogue,
            [](CodecVisitor &visitor, KnownPersonalDialogueProof &proof) {
                return visit_known_personal_dialogue_proof(visitor, proof);
            });
    common_checkpoint::strings(c, "field_order", s.field_order);
    if (!c.status().ok)
        return c.status();
    auto valid = shape(s, format);
    return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
}
Outcome<LegacyControlDecisionSource> decode_legacy_control_decision_source(const BoundaryValue& v){return decode_source(v,SourceFormat::original1);}
Status emit_legacy_control_decision_source_fields(ProjectionWriter& w,const LegacyControlDecisionSource& s,Integer version){if(version<1||version>4)return w.reject("UnsupportedLegacyControlSourceVersion");const auto format=static_cast<SourceFormat>(version-1);auto valid=shape(s,format);if(!valid.ok)return w.reject(valid.error.original.code,"legacy_control_decision");for(const auto& p:ids)w.field(p.first,s.*p.second);family_projection::world_hex(w,"world_id",s.world_id);w.object("context",[&](auto& out){return emit_interaction_context_fields(out,s.context);});for(const auto& p:numbers)if(format!=SourceFormat::declared_age4||p.second==&Source::terms_version||p.second==&Source::tick||p.second==&Source::revision)w.field(p.first,s.*p.second);if(format==SourceFormat::declared_age4)emit_family_decision_age_sources4(w,"age_sources",*s.age_sources);if(format!=SourceFormat::original1){if(s.biological_origin)w.object("biological_origin",[&](auto& out){return emit_biological_control_origin_fields(out,*s.biological_origin);});else w.null("biological_origin");if(s.born_dialogue)w.object("born_dialogue",[&](auto& out){return emit_born_personal_dialogue_proof_fields(out,*s.born_dialogue);});else w.null("born_dialogue");}if((format==SourceFormat::collateral3||format==SourceFormat::declared_age4)){if(s.collateral_dialogue)w.object("collateral_dialogue",[&](auto& out){return emit_known_personal_dialogue_proof_fields(out,*s.collateral_dialogue);});else w.null("collateral_dialogue");}return w.order_fields(s.field_order);}
BoundaryValue encode_legacy_control_decision_source(const LegacyControlDecisionSource& s){return encode_source(s,SourceFormat::original1);}
Status visit_legacy_control_decision_source(CodecVisitor& c,LegacyControlDecisionSource& s){return visit_source(c,s,SourceFormat::original1);}
Outcome<LegacyControlDecisionSource> decode_legacy_control_decision_source_v2(const BoundaryValue& v){return decode_source(v,SourceFormat::biological2);}
BoundaryValue encode_legacy_control_decision_source_v2(const LegacyControlDecisionSource& s){return encode_source(s,SourceFormat::biological2);}
Status visit_legacy_control_decision_source_v2(CodecVisitor& c,LegacyControlDecisionSource& s){return visit_source(c,s,SourceFormat::biological2);}
Outcome<LegacyControlDecisionSource> decode_legacy_control_decision_source_v3(const BoundaryValue& v){return decode_source(v,SourceFormat::collateral3);}
BoundaryValue encode_legacy_control_decision_source_v3(const LegacyControlDecisionSource& s){return encode_source(s,SourceFormat::collateral3);}
Status visit_legacy_control_decision_source_v3(CodecVisitor& c,LegacyControlDecisionSource& s){return visit_source(c,s,SourceFormat::collateral3);}
Outcome<LegacyControlDecisionSource> decode_legacy_control_decision_source_v4(const BoundaryValue& v){return decode_source(v,SourceFormat::declared_age4);}
BoundaryValue encode_legacy_control_decision_source_v4(const LegacyControlDecisionSource& s){return encode_source(s,SourceFormat::declared_age4);}
Status visit_legacy_control_decision_source_v4(CodecVisitor& c,LegacyControlDecisionSource& s){return visit_source(c,s,SourceFormat::declared_age4);}

}
