// 显式情景版本仅收紧原招募；能力并集不能替旧剧本自动启用本人声明。
#include "party_recruitment_consent.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
namespace hanmo::resident {
Outcome<bool> decode_party_recruitment_consent_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){try{
    using namespace boundary;object(scenario,"scenario");const auto* profile=scenario.find("party_recruitment_profile");if(!profile){if(scenario.find("party_recruitment_consents"))unsupported("UnexpectedPartyRecruitmentConsentCatalog","scenario");return {true,false,{}};}
    if(string(*profile,"scenario/party_recruitment_profile")!="provider_declaration_v1")unsupported("UnsupportedPartyRecruitmentProfile","scenario/party_recruitment_profile");
    for(const auto* cap:{"party_provider_consent_v1","native_dynamic_world","multi_actor","dialogue","companions"})if(std::find(capabilities.begin(),capabilities.end(),cap)==capabilities.end())unsupported("MissingPartyRecruitmentCapability",cap);
    const auto* dynamic=scenario.find("dynamic_world");const auto* native=dynamic?dynamic->find("profile"):nullptr;if(!native||string(*native,"scenario/dynamic_world/profile")!="local_society_v1")unsupported("PartyRecruitmentNativeProfileRequired","scenario/dynamic_world");
    const auto* block=scenario.find("party_recruitment_consents");if(!block)unsupported("MissingPartyRecruitmentConsentCatalog","scenario");std::set<StableId> seen;for(const auto& row:object(*block,"party_recruitment_consents"))if(!seen.insert(row.first).second)fail("DuplicatePartyRecruitmentField",row.first);fields(*block,{"version","declarations"},{},"party_recruitment_consents");if(get_int(*block,"version","party_recruitment_consents")!=1)unsupported("UnsupportedPartyRecruitmentConsentVersion","party_recruitment_consents/version");if(array(field(*block,"declarations","party_recruitment_consents"),"declarations").size()>4096)fail("PartyRecruitmentDeclarationLimit","declarations");return {true,true,{}};
}catch(const boundary::DecodeFailure& error){return {false,false,error.error};}}
Status validate_party_recruitment_consent_layout(const std::vector<std::pair<StableId,StableId>>& modules,const PartyRecruitmentConsentCatalog* catalog){Integer count=0;for(const auto& pair:modules)if(pair.first=="party_recruitment_consents"){++count;if(pair.second!="1")return common_failure("UnsupportedPartyRecruitmentConsentVersion");}if(count!=(catalog?1:0)||(catalog&&catalog->version!=1))return common_failure("PartyRecruitmentProfileMismatch");return common_success();}
}
