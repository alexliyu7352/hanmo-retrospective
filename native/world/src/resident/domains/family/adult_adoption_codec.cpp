#include "family_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "adult_adoption_source.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using namespace boundary;
using V=BoundaryValue;
const std::vector<std::pair<const char*,StableId AdultAdoptionAgreement::*>> text_fields={
    {"request_id",&AdultAdoptionAgreement::request_id},{"adopter_actor_id",&AdultAdoptionAgreement::adopter_actor_id},
    {"adoptee_actor_id",&AdultAdoptionAgreement::adoptee_actor_id},{"dialogue_id",&AdultAdoptionAgreement::dialogue_id},
    {"node_id",&AdultAdoptionAgreement::node_id},{"node_kind",&AdultAdoptionAgreement::node_kind},
    {"session_id",&AdultAdoptionAgreement::session_id},{"scenario_id",&AdultAdoptionAgreement::scenario_id}};
const std::vector<std::pair<const char*,Integer AdultAdoptionAgreement::*>> integer_fields={
    {"terms_version",&AdultAdoptionAgreement::terms_version},{"accepted_tick",&AdultAdoptionAgreement::accepted_tick},
    {"accepted_revision",&AdultAdoptionAgreement::accepted_revision},{"adopter_birth_tick",&AdultAdoptionAgreement::adopter_birth_tick},
    {"adoptee_birth_tick",&AdultAdoptionAgreement::adoptee_birth_tick}};
bool valid_order(const FieldOrder& order,const std::set<StableId>& expected){
    return order.empty()||(order.size()==expected.size()&&std::set<StableId>(order.begin(),order.end())==expected);
}
std::set<StableId> agreement_keys(){
    std::set<StableId> result{"world_id","context"};
    for(const auto& f:text_fields)result.insert(f.first);for(const auto& f:integer_fields)result.insert(f.first);
    return result;
}

WorldId world_id(const StableId& value){
    if(value.size()!=32)fail("InvalidWorldId","adult_adoption/world_id");
    auto digit=[](char c)->Integer{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;fail("InvalidWorldId","adult_adoption/world_id");};
    WorldId result{};for(std::size_t n=0;n<16;++n)result[n]=static_cast<std::uint8_t>(16*digit(value[n*2])+digit(value[n*2+1]));return result;
}
AdultAdoptionAgreement agreement(const V& value){
    fields(value,{"request_id","adopter_actor_id","adoptee_actor_id","dialogue_id","node_id","node_kind","session_id","scenario_id","world_id","context","terms_version","accepted_tick","accepted_revision","adopter_birth_tick","adoptee_birth_tick"},{},"adult_adoption/agreement");
    AdultAdoptionAgreement result;
    for(const auto& f:text_fields)result.*f.second=get_string(value,f.first,"");
    for(const auto& f:integer_fields)result.*f.second=get_int(value,f.first,"");
    result.world_id=world_id(get_string(value,"world_id",""));result.context=interaction_codec::decode_context(field(value,"context",""),"adult_adoption/context");result.field_order=order(value);return result;
}

Status visit_agreement(CodecVisitor& c,AdultAdoptionAgreement& value){
    for(const auto& f:text_fields)c.field(f.first,value.*f.second);
    std::vector<Integer> bytes;for(auto byte:value.world_id)bytes.push_back(byte);
    c.sequence("world_id",bytes,[](CodecVisitor& v,Integer& byte){return v.field("byte",byte);});
    if(c.reading()&&c.status().ok){
        if(bytes.size()!=16)return c.reject("InvalidWorldId");
        for(std::size_t n=0;n<16;++n){if(bytes[n]<0||bytes[n]>255)return c.reject("InvalidWorldId");value.world_id[n]=static_cast<std::uint8_t>(bytes[n]);}
    }
    c.object("context",[&](CodecVisitor& v){return interaction_codec::visit_context(v,value.context);});
    for(const auto& f:integer_fields)c.field(f.first,value.*f.second);
    common_checkpoint::strings(c,"field_order",value.field_order);
    if(!c.status().ok)return c.status();
    if(!valid_order(value.context.field_order,{"place_id","role_id","provider_actor_id"}))return c.reject("BoundaryFieldOrderMismatch");
    return valid_order(value.field_order,agreement_keys())?c.status():c.reject("BoundaryFieldOrderMismatch");
}
}
Outcome<AdultAdoptionRequestCatalog> decode_adult_adoption_request_catalog(const BoundaryValue& config,const BoundaryValue& definitions){try{
    using namespace boundary;fields(config,{"profile","request_ids"},{},"adult_adoption");
    if(get_string(config,"profile","")!="authored_adult_adoption_requests_v1")unsupported("UnsupportedAdultAdoptionProfile","adult_adoption/profile");
    const auto selected=strings(field(config,"request_ids",""),"adult_adoption/request_ids");
    if(selected.empty()||selected.size()>4096)fail("InvalidAdultAdoptionCatalogSize","adult_adoption/request_ids");
    AdultAdoptionRequestCatalog result;result.enabled=true;
    for(const auto& id:selected){
        const auto& value=field(definitions,id,"adult_adoption_requests");
        fields(value,{"id","adopter_actor_id","adoptee_actor_id","dialogue_id","node_id","terms_version","consent_kind","scope"},{},"adult_adoption_requests");
        if(get_string(value,"consent_kind","")!="adoptee_requests_adoption_v1"||get_string(value,"scope","")!="adult_adoption_only")fail("InvalidAdultAdoptionConsentScope",id);
        AdultAdoptionRequestDefinition d;d.id=get_string(value,"id","");d.adopter_actor_id=get_string(value,"adopter_actor_id","");d.adoptee_actor_id=get_string(value,"adoptee_actor_id","");d.dialogue_id=get_string(value,"dialogue_id","");d.node_id=get_string(value,"node_id","");d.terms_version=get_int(value,"terms_version","");
        if(d.id!=id||!result.definitions.insert(id,std::move(d)))fail("DuplicateAdultAdoptionDefinition",id);
    }
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
AdultAdoptionFact decode_adult_adoption_fact(const BoundaryValue& value){
    using namespace boundary;fields(value,{"kind","agreement"},{},"adult_adoption_fact");
    AdultAdoptionFact result;result.kind=get_string(value,"kind","");result.agreement=agreement(field(value,"agreement",""));result.field_order=order(value);return result;
}
Status emit_adult_adoption_fact_fields(ProjectionWriter& out,const AdultAdoptionFact& value){out.field("kind",value.kind);out.object("agreement",value.agreement.field_order,[&](auto& w){const auto& a=value.agreement;for(const auto& f:text_fields)w.field(f.first,a.*f.second);family_projection::world_hex(w,"world_id",a.world_id);w.object("context",[&](auto& v){return emit_interaction_context_fields(v,a.context);});for(const auto& f:integer_fields)w.field(f.first,a.*f.second);return w.status();});return out.order_fields(value.field_order);}
BoundaryValue encode_adult_adoption_fact(const AdultAdoptionFact& value){return family_projection::encode([&](auto& w){return emit_adult_adoption_fact_fields(w,value);});}
Status visit_adult_adoption_fact(CodecVisitor& c,AdultAdoptionFact& value){
    c.field("kind",value.kind);c.object("agreement",[&](CodecVisitor& v){return visit_agreement(v,value.agreement);});
    common_checkpoint::strings(c,"field_order",value.field_order);if(!c.status().ok)return c.status();
    return valid_order(value.field_order,{"kind","agreement"})?c.status():c.reject("BoundaryFieldOrderMismatch");
}
}
