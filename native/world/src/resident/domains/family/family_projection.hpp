// 原 Family JSON 输出面；同 emit 驱动实际导出与无树 shape，binary 原访客不改版。
#pragma once
#include "family_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <array>
namespace hanmo::resident {
Status emit_family_state_fields(ProjectionWriter&,const FamilyState&);
Status emit_family_state_projection(ProjectionWriter&,std::string_view,const FamilyState&);
// Same original row fields, merged only under the actual Common header.
Status emit_family_action_payload_fields(ProjectionWriter&,const FamilyState&,const ActionCoreView&);
Status emit_family_fact_payload_fields(ProjectionWriter&,const FamilyState&,const FactRecord&);
Status emit_family_additional_state_fields(ProjectionWriter&,const FamilyState&);
Status emit_character_creation_draft_fields(ProjectionWriter&,const CharacterCreationDraft&,bool extended);
Status emit_character_creation_fact_fields(ProjectionWriter&,const CharacterCreationFact&,bool extended);
Status emit_household_control_fact_fields(ProjectionWriter&,const HouseholdControlFact&);
Status emit_adult_adoption_fact_fields(ProjectionWriter&,const AdultAdoptionFact&);
Status emit_succession_personal_terms_fields(ProjectionWriter&,const SuccessionPersonalTerms&);
Status emit_succession_consent_source_fields(ProjectionWriter&,const SuccessionConsentSource&);
Status emit_succession_consent_fact_fields(ProjectionWriter&,const SuccessionConsentFact&);
Status emit_voluntary_succession_fact_fields(ProjectionWriter&,const VoluntarySuccessionFact&);
namespace family_projection {
template<class F>BoundaryValue encode(F emit){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",emit);if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
inline Status world_hex(ProjectionWriter& writer,std::string_view name,const WorldId& id){constexpr char digits[]="0123456789abcdef";std::array<char,32> text{};for(std::size_t n=0;n<id.size();++n){text[2*n]=digits[id[n]>>4];text[2*n+1]=digits[id[n]&15];}return writer.text(name,std::string_view(text.data(),text.size()));}
}
}
