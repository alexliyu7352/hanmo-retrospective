// 工程只选择独立Military17；uint32既有位不扩容，不借任何其他领域能力位。
#pragma once
#include "military_tactic_learning_profile.hpp"
#include "military_siege_engineering_contract.hpp"
namespace hanmo::resident {
inline Status validate_military_siege_engineering_modules(const std::vector<std::pair<StableId,StableId>>& modules){
 auto valid=validate_military_tactic_learning_modules(modules);if(!valid.ok)return valid;unsigned chosen=0,prior=0;
 for(const auto& m:modules){if(m.first==military_siege_engineering_module){if(m.second!="1")return common_failure("MilitarySiegeEngineeringProfileMismatch");++chosen;}if(m.first==military_tactic_learning_module&&m.second=="1")++prior;}
 if(chosen>1)return common_failure("MilitarySiegeEngineeringProfileMismatch");return chosen&&prior!=1?common_failure("MilitarySiegeEngineeringPrerequisiteRequired"):common_success();
}
inline Status validate_military_siege_engineering_checkpoint_layout(std::uint32_t version,const std::vector<std::pair<StableId,StableId>>& modules){
 auto valid=validate_military_siege_engineering_modules(modules);if(!valid.ok)return valid;const bool present=std::find(modules.begin(),modules.end(),std::make_pair(StableId{military_siege_engineering_module},StableId{"1"}))!=modules.end();
 if(version<1||version>17||(version==17)!=present)return common_failure("MilitarySiegeEngineeringCodecVersionMismatch");return validate_military_tactic_learning_checkpoint_layout(version==17?16u:version,modules);
}
}
