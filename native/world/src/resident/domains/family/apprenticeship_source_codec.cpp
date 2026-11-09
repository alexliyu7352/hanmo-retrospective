// 师承本人来源严格冷热编解码；递归重名、未知许可字段或不完整世界号均拒绝。
#include "apprenticeship_source.hpp"
#include "succession_personal_assets.hpp"
#include "family_projection.hpp"
#include "family_source_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_proof_codec.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
namespace hanmo::resident {namespace {
using V=BoundaryValue;using S=ApprenticeshipStatementSource;
const std::vector<std::pair<const char*,StableId S::*>> texts={
    {"statement_id",&S::statement_id},{"speaker_actor_id",&S::speaker_actor_id},
    {"listener_actor_id",&S::listener_actor_id},{"teacher_actor_id",&S::teacher_actor_id},
    {"student_actor_id",&S::student_actor_id},{"skill_id",&S::skill_id},
    {"dialogue_id",&S::dialogue_id},{"node_id",&S::node_id},{"node_kind",&S::node_kind},
    {"session_id",&S::session_id},{"scenario_id",&S::scenario_id}};
const std::vector<std::pair<const char*,Integer S::*>> numbers={
    {"terms_version",&S::terms_version},{"tick",&S::tick},{"revision",&S::revision}};
// 原 WorldId 固定完整十六字节，不把 epoch 混入永久许可或截断跨世界校验。
WorldId world(const StableId& value){
    if(value.size()!=32)boundary::fail("InvalidWorldId","apprenticeship/world_id");
    auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    WorldId out{};for(std::size_t i=0;i<16;++i){const auto a=digit(value[2*i]),b=digit(value[2*i+1]);if(a<0||b<0)boundary::fail("InvalidWorldId","apprenticeship/world_id");out[i]=static_cast<std::uint8_t>(16*a+b);}return out;
}
}
Outcome<ApprenticeshipCatalog> decode_apprenticeship_catalog(const V& input){try{
    auto unique=validate_succession_json_unique(input);if(!unique.ok)return {false,{},unique.error};using namespace boundary;
    fields(input,{"version","profile","statements"},{},"family_apprenticeship");
    if(get_int(input,"version","")!=1||get_string(input,"profile","")!="adult_personal_apprenticeship_v1")unsupported("UnsupportedApprenticeshipProfile","");
    const auto& rows=object(field(input,"statements",""),"apprenticeship/statements");if(rows.empty()||rows.size()>4096)fail("InvalidApprenticeshipCatalogSize","");
    ApprenticeshipCatalog out;out.enabled=true;
    for(const auto& row:rows){const auto& v=row.second;fields(v,{"id","speaker_actor_id","speaker_side","skill_id","dialogue_id","node_id","terms_version"},{"listener_actor_id"},"apprenticeship/statement");ApprenticeshipStatementDefinition s;s.id=get_string(v,"id","");s.speaker_actor_id=get_string(v,"speaker_actor_id","");s.speaker_side=get_string(v,"speaker_side","");s.skill_id=get_string(v,"skill_id","");s.dialogue_id=get_string(v,"dialogue_id","");s.node_id=get_string(v,"node_id","");s.terms_version=get_int(v,"terms_version","");if(v.find("listener_actor_id"))s.listener_actor_id=get_string(v,"listener_actor_id","");if(row.first!=s.id||row.first.empty()||row.first.size()>256||s.terms_version!=1||(s.speaker_side!="teacher"&&s.speaker_side!="student")||!out.statements.insert(row.first,std::move(s)))fail("InvalidApprenticeshipStatement",row.first);}
    return {true,std::move(out),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<ApprenticeshipStatementSource> decode_apprenticeship_statement_source(const V& input){try{
    auto unique=validate_succession_json_unique(input);if(!unique.ok)return {false,{},unique.error};using namespace boundary;
    fields(input,{"statement_id","speaker_actor_id","listener_actor_id","teacher_actor_id","student_actor_id","skill_id","dialogue_id","node_id","node_kind","session_id","scenario_id","world_id","context","terms_version","tick","revision"},{},"apprenticeship/source");
    S out;for(const auto& member:texts)out.*member.second=get_string(input,member.first,"");for(const auto& member:numbers)out.*member.second=get_int(input,member.first,"");out.world_id=world(get_string(input,"world_id",""));out.context=interaction_codec::decode_context(field(input,"context",""),"apprenticeship/context");out.field_order=order(input);
    auto valid=validate_apprenticeship_source_shape(out);return valid.ok?Outcome<S>{true,std::move(out),{}}:Outcome<S>{false,{},valid.error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_apprenticeship_statement_source_fields(ProjectionWriter& w,const S& source){auto valid=validate_apprenticeship_source_shape(source);if(!valid.ok)return w.reject(valid.error.original.code,"apprenticeship/source");for(const auto& member:texts)w.field(member.first,source.*member.second);family_projection::world_hex(w,"world_id",source.world_id);w.object("context",[&](auto& out){return emit_interaction_context_fields(out,source.context);});for(const auto& member:numbers)w.field(member.first,source.*member.second);return w.order_fields(source.field_order);}
BoundaryValue encode_apprenticeship_statement_source(const S& source){return family_projection::encode([&](auto& w){return emit_apprenticeship_statement_source_fields(w,source);});}
Status visit_apprenticeship_statement_source(CodecVisitor& visitor,S& source){
    for(const auto& member:texts)visitor.field(member.first,source.*member.second);
    std::vector<Integer> bytes;for(auto b:source.world_id)bytes.push_back(b);visitor.sequence("world_id",bytes,[](CodecVisitor& at,Integer& b){return at.field("byte",b);});
    if(visitor.reading()&&visitor.status().ok){if(bytes.size()!=16)return visitor.reject("InvalidWorldId");for(std::size_t i=0;i<16;++i){if(bytes[i]<0||bytes[i]>255)return visitor.reject("InvalidWorldId");source.world_id[i]=static_cast<std::uint8_t>(bytes[i]);}}
    visitor.object("context",[&](CodecVisitor& at){return interaction_codec::visit_context(at,source.context);});for(const auto& member:numbers)visitor.field(member.first,source.*member.second);common_checkpoint::strings(visitor,"field_order",source.field_order);
    if(!visitor.status().ok)return visitor.status();auto valid=validate_apprenticeship_source_shape(source);return valid.ok?visitor.status():visitor.reject(valid.error.original.code);
}
}
