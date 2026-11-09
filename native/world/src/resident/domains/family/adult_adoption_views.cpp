#include "adult_adoption.hpp"
#include "resident/views/common_presentation.hpp"
#include <algorithm>
namespace hanmo::resident {
Outcome<BoundaryValue> query_adult_adoption_page(const CommonStore& common,const FamilyState& family,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const AdultAdoptionRequestCatalog& requests,Integer page){
    using namespace boundary;using V=BoundaryValue;
    if(page<0||page>255)return {false,{},common_failure("InvalidAdultAdoptionPage").error};
    if(!requests.enabled)return {false,{},common_failure("AdultAdoptionUnavailable").error};
    if(!interaction.common)return {false,{},common_failure("MissingAdultAdoptionCommonCatalog").error};
    auto valid=validate_adult_adoption_request_catalog(requests,*interaction.common,interaction);if(!valid.ok)return {false,{},valid.error};
    valid=validate_adult_adoptions(common,family,*interaction.common,interaction,&requests);if(!valid.ok)return {false,{},valid.error};
    const auto& actor=common.state().metadata.player_actor_id;
    if(!common.actor(actor))return {false,{},common_failure("UnknownActor",actor).error};
    CommonPresentation presentation(common.state(),*interaction.common);
    V::Array pending,history;
    auto definitions=requests.definitions.order();std::sort(definitions.begin(),definitions.end());
    for(const auto& id:definitions){
        const auto& request=*requests.definitions.find(id);if(request.adopter_actor_id!=actor)continue;
        const auto source=preview_adult_adoption_request(common,state,interaction,access,requests,actor,request.adoptee_actor_id,id);
        if(!source.ok)continue;
        auto name=presentation.display_name(request.adoptee_actor_id);if(!name.ok)return {false,{},name.error};
        const auto quote=preview_adult_adoption(common,family,state,interaction,access,requests,actor,request.adoptee_actor_id,id);
        pending.push_back(obj({{"request_id",V{id}},{"adoptee_actor_id",V{request.adoptee_actor_id}},{"adoptee_name",V{name.value}},{"eligible",V{quote.ok}},
            {"code",V{quote.ok?StableId{}:quote.error.original.code}},
            {"params",obj({{"request_id",V{id}},{"adoptee_actor_id",V{request.adoptee_actor_id}}})}}));
    }
    for(const auto& id:family.facts.order())if(const auto* f=std::get_if<AdultAdoptionFact>(family.facts.find(id))){
        const auto& a=f->agreement;if(a.adopter_actor_id!=actor&&a.adoptee_actor_id!=actor)continue;
        auto adopter=presentation.display_name(a.adopter_actor_id),adoptee=presentation.display_name(a.adoptee_actor_id);
        if(!adopter.ok)return {false,{},adopter.error};if(!adoptee.ok)return {false,{},adoptee.error};
        history.push_back(obj({{"fact_id",V{id}},{"adopter_actor_id",V{a.adopter_actor_id}},{"adoptee_actor_id",V{a.adoptee_actor_id}},
            {"adopter_name",V{adopter.value}},{"adoptee_name",V{adoptee.value}},{"accepted_tick",V{a.accepted_tick}}}));
    }
    const auto request_count=Integer(pending.size()),history_count=Integer(history.size());
    auto slice=[page](V::Array& values){V::Array result;for(auto n=page*16;n<std::min(Integer(values.size()),page*16+16);++n)result.push_back(std::move(values[std::size_t(n)]));values=std::move(result);};slice(pending);slice(history);
    StableId world;const char* hex="0123456789abcdef";for(auto byte:common.state().identity.world_id){world+=hex[byte>>4];world+=hex[byte&15];}
    const auto& identity=common.state().identity;
    return {true,obj({{"actor_id",V{actor}},{"source_identity",obj({{"world_id",V{world}},{"execution_epoch",V{identity.execution_epoch}},{"revision",V{identity.revision}},{"tick",V{identity.tick}}})},
        {"page",V{page}},{"request_count",V{request_count}},{"history_count",V{history_count}},
        {"requests",V{std::move(pending)}},{"history",V{std::move(history)}}}),{}};
}
}
