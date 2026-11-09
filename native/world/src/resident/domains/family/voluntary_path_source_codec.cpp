#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "family_projection.hpp"
#include "family_source_projection.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_proof_codec.hpp"
// 仅新family_voluntary_paths独立尾保存；原Family/Legacy格式不增加此来源存在位。
#include "voluntary_path_source.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
namespace hanmo::resident {namespace {
using V=BoundaryValue;
bool reference(const StableId& id){return !id.empty()&&id.size()<=256;}
std::set<StableId> keys(bool biological){std::set<StableId> out{"relation_kind","relation_established_tick","disciple_skill_id","statement"};if(biological)out.insert("biological");return out;}
bool origin_shape(const KnownPersonalActorOrigin& o){
    if(!reference(o.actor_id))return false;
    if(o.kind=="installed_actor")return o.birth_fact_id.empty()&&o.creation_fact_id.empty()&&o.prototype_id.empty();
    return o.kind=="recorded_birth"&&reference(o.birth_fact_id)&&o.creation_fact_id==o.actor_id&&reference(o.prototype_id);
}
}
Status validate_voluntary_path_source_shape(const VoluntaryPathSource& s){
    if(!voluntary_relation_kind_name(s.relation_kind)||s.relation_established_tick<0||s.relation_established_tick>2147483647)
        return common_failure("InvalidVoluntaryPathSource");
    if((s.relation_kind==VoluntaryRelationKind::biological_child)!=bool(s.biological))return common_failure("VoluntaryBiologicalTagRequired");
    if(s.relation_kind==VoluntaryRelationKind::accepted_disciple?!reference(s.disciple_skill_id):!s.disciple_skill_id.empty())
        return common_failure("VoluntaryDiscipleSourceMismatch");
    const auto fields=keys(bool(s.biological));
    if(!s.field_order.empty()&&(s.field_order.size()!=fields.size()||std::set<StableId>(s.field_order.begin(),s.field_order.end())!=fields))
        return common_failure("BoundaryFieldOrderMismatch");
    const auto& p=s.statement;const auto& m=p.declaration.meeting;
    for(const auto* id:{&p.opening_fact_id,&p.template_id,&m.actor_id,&m.provider_actor_id,&m.session_id,&m.dialogue_id,&m.node_id,&m.context.place_id})
        if(!reference(*id))return common_failure("InvalidVoluntaryPathStatement");
    if(m.actor_id==m.provider_actor_id||m.node_kind!="say"||p.declaration.speaker_actor_id!=m.provider_actor_id||
       !origin_shape(p.listener_origin)||!origin_shape(p.provider_origin)||p.listener_origin.actor_id!=m.actor_id||p.provider_origin.actor_id!=m.provider_actor_id||
       m.source_identity.execution_epoch<1||m.source_identity.revision<0||m.source_identity.tick<0||m.source_identity.tick>2147483647||
       s.relation_established_tick>m.source_identity.tick||std::all_of(m.source_identity.world_id.begin(),m.source_identity.world_id.end(),[](auto b){return b==0;})||
       m.context.kind!=std::optional<StableId>{"narrative"}||!m.context.role_id.empty()||m.context.provider_actor_id!=m.provider_actor_id||m.context.encounter_id||m.context.input_error)
        return common_failure("InvalidVoluntaryPathStatement");
    if(s.biological){
        const auto& b=*s.biological;
        if(b.parent_actor_id!=m.actor_id||b.child_actor_id!=m.provider_actor_id||b.source_available_tick!=s.relation_established_tick)
            return common_failure("VoluntaryBiologicalPartiesMismatch");
        // 原标签codec是唯一形状门，不把Installed条目填作运行事实。
        auto source=validate_biological_control_origin_shape(b);if(!source.ok)return source;
    }
    return common_success();
}
Outcome<VoluntaryPathSource> decode_voluntary_path_source(const V& input){try{
    auto unique=validate_succession_json_unique(input);if(!unique.ok)return {false,{},unique.error};using namespace boundary;
    fields(input,{"relation_kind","relation_established_tick","disciple_skill_id","statement"},{"biological"},"voluntary_path_source");
    auto kind=decode_voluntary_relation_kind(get_string(input,"relation_kind",""));if(!kind.ok)return {false,{},kind.error};
    VoluntaryPathSource s;s.relation_kind=kind.value;s.relation_established_tick=get_int(input,"relation_established_tick","");s.disciple_skill_id=get_string(input,"disciple_skill_id","");
    auto proof=decode_known_personal_dialogue_proof(field(input,"statement",""));if(!proof.ok)return {false,{},proof.error};s.statement=std::move(proof.value);
    if(const auto* value=input.find("biological")){auto b=decode_biological_control_origin(*value);if(!b.ok)return {false,{},b.error};s.biological=std::move(b.value);}
    s.field_order=order(input);auto valid=validate_voluntary_path_source_shape(s);
    return valid.ok?Outcome<VoluntaryPathSource>{true,std::move(s),{}}:Outcome<VoluntaryPathSource>{false,{},valid.error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_voluntary_path_source_fields(ProjectionWriter& w,const VoluntaryPathSource& s){auto valid=validate_voluntary_path_source_shape(s);if(!valid.ok)return w.reject(valid.error.original.code,"voluntary_path_source");w.text("relation_kind",voluntary_relation_kind_name(s.relation_kind));w.field("relation_established_tick",s.relation_established_tick);w.field("disciple_skill_id",s.disciple_skill_id);w.object("statement",[&](auto& out){return emit_known_personal_dialogue_proof_fields(out,s.statement);});if(s.biological)w.object("biological",[&](auto& out){return emit_biological_control_origin_fields(out,*s.biological);});return w.order_fields(s.field_order);}
BoundaryValue encode_voluntary_path_source(const VoluntaryPathSource& s){return family_projection::encode([&](auto& w){return emit_voluntary_path_source_fields(w,s);});}
Status visit_voluntary_path_source(CodecVisitor& c,VoluntaryPathSource& s){
    if(!c.reading()){auto valid=validate_voluntary_path_source_shape(s);if(!valid.ok)return c.reject(valid.error.original.code);}
    StableId kind=c.reading()?StableId{}:StableId{voluntary_relation_kind_name(s.relation_kind)};c.field("relation_kind",kind);
    if(c.reading()&&c.status().ok){auto read=decode_voluntary_relation_kind(kind);if(!read.ok)return c.reject(read.error.original.code);s.relation_kind=read.value;}
    c.field("relation_established_tick",s.relation_established_tick);c.field("disciple_skill_id",s.disciple_skill_id);
    c.object("statement",[&](CodecVisitor& v){return visit_known_personal_dialogue_proof(v,s.statement);});
    c.optional("biological",s.biological,[](CodecVisitor& v,BiologicalControlOrigin& b){return visit_biological_control_origin_checkpoint(v,b);});
    common_checkpoint::strings(c,"field_order",s.field_order);if(!c.status().ok)return c.status();
    auto valid=validate_voluntary_path_source_shape(s);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
