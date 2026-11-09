#include "adult_adoption.hpp"
#include <charconv>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool fact_id(const StableId& value,Integer next){
    if(value.rfind("family:",0)!=0)return false;
    const auto suffix=value.substr(7);Integer n=0;
    const auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),n);
    return parsed.ec==std::errc{}&&parsed.ptr==suffix.data()+suffix.size()&&n>0&&n<next&&std::to_string(n)==suffix;
}
Outcome<StableId> parent_of(const FamilyState& family,const StableId& child){
    const auto* id=family.children.find(child);if(!id)return {true,{}, {}};
    const auto* value=family.facts.find(*id);
    if(value){
        if(const auto* f=std::get_if<AdoptionFact>(value);f&&f->kind=="family_adoption"&&f->success&&f->child_actor_id==child)return {true,f->guardian_actor_id,{}};
        if(const auto* f=std::get_if<AdultAdoptionFact>(value);f&&f->kind=="family_adult_adoption"&&f->agreement.adoptee_actor_id==child)return {true,f->agreement.adopter_actor_id,{}};
    }
    return {false,{},common_failure("FamilyAdoptionSourceMismatch",child).error};
}
Status relation_available(const FamilyState& family,const StableId& adopter,const StableId& adoptee){
    if(adopter==adoptee)return common_failure("FamilySelfAdoption",adopter);
    if(family.children.find(adoptee))return common_failure("FamilyAlreadyAdopted",adoptee);
    std::set<StableId> seen;auto cursor=adopter;
    while(!cursor.empty()){
        if(cursor==adoptee||!seen.insert(cursor).second)return common_failure("FamilyAdoptionCycle",adoptee);
        auto parent=parent_of(family,cursor);if(!parent.ok)return {false,{},parent.error};cursor=std::move(parent.value);
    }
    return common_success();
}
}
Status validate_adult_adoptions(const CommonStore& common,const FamilyState& family,const CommonCatalog& shared,const InteractionCatalog& interaction,const AdultAdoptionRequestCatalog* requests){
    std::set<StableId> adoptees;
    std::set<std::tuple<StableId,StableId,StableId,StableId>> consumed;
    for(const auto& row:family.facts.rows())if(const auto* f=std::get_if<AdultAdoptionFact>(&row.second)){
        if(!requests||!requests->enabled)return common_failure("AdultAdoptionCapabilityRequired",row.first);
        const auto* head=common.state().facts.find(row.first);const auto& a=f->agreement;
        if(!fact_id(row.first,common.state().next_instance_id)||!head||head->domain!="family"||head->kind!="family_adult_adoption"||f->kind!=head->kind)return common_failure("InvalidAdultAdoptionFact",row.first);
        auto valid=validate_adult_adoption_agreement_source(common.state(),shared,interaction,*requests,a);if(!valid.ok)return valid;
        const auto* source=family.children.find(a.adoptee_actor_id);
        if(!source||*source!=row.first||!adoptees.insert(a.adoptee_actor_id).second)return common_failure("AdultAdoptionRelationMismatch",row.first);
        if(!consumed.emplace(a.request_id,a.session_id,a.adopter_actor_id,a.adoptee_actor_id).second)return common_failure("AdultAdoptionRequestConsumed",row.first);
    }
    for(const auto& row:common.state().facts.rows())if(row.second.domain=="family"&&row.second.kind=="family_adult_adoption"){
        const auto* value=family.facts.find(row.first);if(!value||!std::holds_alternative<AdultAdoptionFact>(*value))return common_failure("MissingAdultAdoptionFact",row.first);
    }
    // 两种收养沿同一children索引核环；未知生物亲缘没有被补成已知关系。
    for(const auto& row:family.children.rows()){
        std::set<StableId> seen;auto cursor=row.first;
        while(!cursor.empty()){
            if(!seen.insert(cursor).second)return common_failure("FamilyAdoptionCycle",row.first);
            auto parent=parent_of(family,cursor);if(!parent.ok)return {false,{},parent.error};cursor=std::move(parent.value);
        }
    }
    return common_success();
}
Outcome<AdultAdoptionRequestQuote> preview_adult_adoption(const CommonStore& common,const FamilyState& family,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const AdultAdoptionRequestCatalog& requests,const StableId& adopter,const StableId& adoptee,const StableId& request_id){
    if(!interaction.common)return {false,{},common_failure("MissingAdultAdoptionCommonCatalog").error};
    auto valid=validate_adult_adoptions(common,family,*interaction.common,interaction,&requests);if(!valid.ok)return {false,{},valid.error};
    valid=relation_available(family,adopter,adoptee);if(!valid.ok)return {false,{},valid.error};
    return preview_adult_adoption_request(common,state,interaction,access,requests,adopter,adoptee,request_id);
}
Outcome<StableId> accept_adult_adoption(CommonStore& common,FamilyState& family,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const AdultAdoptionRequestCatalog& requests,const AdultAdoptionRequestQuote& quote){
    auto valid=revalidate_adult_adoption_request(common,state,interaction,access,requests,quote);if(!valid.ok)return {false,{},valid.error};
    auto fresh=preview_adult_adoption(common,family,state,interaction,access,requests,quote.agreement.adopter_actor_id,quote.agreement.adoptee_actor_id,quote.agreement.request_id);
    if(!fresh.ok)return {false,{},fresh.error};
    if(!adult_adoption_agreement_equal(quote.agreement,fresh.value.agreement))return {false,{},common_failure("StaleAdultAdoptionRequest").error};
    auto point=common.begin_savepoint();if(!point.ok)return {false,{},point.error};auto candidate=family;
    auto abort=[&](Status error)->Outcome<StableId>{auto rolled=common.rollback_savepoint(point.value);return {false,{},rolled.ok?error.error:rolled.error};};
    auto id=common.next_id("family");if(!id.ok)return abort({false,{},id.error});
    AdultAdoptionFact fact;fact.agreement=quote.agreement;
    if(!candidate.facts.insert(id.value,std::move(fact))||!candidate.children.insert(quote.agreement.adoptee_actor_id,id.value))return abort(common_failure("DuplicateAdultAdoptionFact"));
    valid=common.append_fact({id.value,"family_adult_adoption","family"});if(!valid.ok)return abort(valid);
    valid=validate_adult_adoptions(common,candidate,*interaction.common,interaction,&requests);if(!valid.ok)return abort(valid);
    valid=common.commit_savepoint(point.value);if(!valid.ok)return {false,{},valid.error};
    family=std::move(candidate);return {true,id.value,{}};
}
}
