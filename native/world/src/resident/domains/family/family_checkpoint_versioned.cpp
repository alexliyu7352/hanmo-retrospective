#include "family_checkpoint_versioned.hpp"
#include <set>
namespace hanmo::resident {namespace {
Status sources_fit(const FamilyState& state,const FamilyCodecSelection& selected){
    for(const auto& row:state.facts.rows()){
        const auto& fact=row.second;
        if((std::holds_alternative<HouseholdControlFact>(fact)&&!selected.household_control)||
           (std::holds_alternative<CharacterCreationFact>(fact)&&!selected.character_creation)||
           (std::holds_alternative<AdultAdoptionFact>(fact)&&!selected.adult_adoption)||
           ((std::holds_alternative<SuccessionConsentFact>(fact)||std::holds_alternative<VoluntarySuccessionFact>(fact))&&!selected.voluntary_succession))
            return common_failure("InvalidFamilyFact",row.first);
        if(const auto* consent=std::get_if<SuccessionConsentFact>(&fact);consent&&consent->agreement.context.kind==std::optional<StableId>{"narrative"}){
            if(!selected.voluntary_paths)return common_failure("VoluntaryPathCodecRequired",row.first);
            const auto& context=consent->agreement.context;
            if(!context.role_id.empty()||context.encounter_id||context.input_error)return common_failure("InvalidVoluntaryPathContext",row.first);
        }
    }return common_success();
}
}
Outcome<FamilyState> decode_family_seed_versioned(const BoundaryValue& world,Integer version,bool extended,bool birth){
    auto shape=inspect_family_codec_version(version);if(!shape.ok)return {false,{},shape.error};const auto& s=shape.value;
    auto decoded=decode_family_seed(world,s.household_control,s.character_creation,extended,s.adult_adoption,s.voluntary_succession,birth);
    if(!decoded.ok)return decoded;auto valid=sources_fit(decoded.value,s);return valid.ok?decoded:Outcome<FamilyState>{false,{},valid.error};
}
Status emit_family_state_versioned_fields(ProjectionWriter& out,const FamilyState& state,Integer version){
    auto shape=inspect_family_codec_version(version);if(!shape.ok)return {false,{},shape.error};
    auto valid=sources_fit(state,shape.value);if(!valid.ok)return valid;
    return emit_family_state_fields(out,state);
}
Status emit_family_state_versioned_projection(ProjectionWriter& out,std::string_view name,const FamilyState& state,Integer version){
    return out.object(name,[&](auto& fields){return emit_family_state_versioned_fields(fields,state,version);});
}
Outcome<BoundaryValue> encode_family_state_versioned(const FamilyState& state,Integer version){
    try{BoundaryValue value;BoundaryProjectionWriter out(value);auto status=emit_family_state_versioned_projection(out,"",state,version);if(status.ok)status=out.finish();if(!status.ok)return {false,{},status.error};return {true,std::move(value),{}};}
    catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
}
