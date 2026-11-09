// 新供给经营与政治绑定同启用，不能借旧接管段单独放行assume。
#pragma once
#include "politics_supply_adoptions.hpp"
namespace hanmo::resident {
inline Status validate_political_supply_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const PoliticalSupplyAdoptionState* state,std::optional<Integer> economic_version={}){
    unsigned political=0,economic=0;bool takeover=false;
    for(const auto& item:modules){if(item.first=="political_takeover"&&item.second=="1")takeover=true;if(item.first!=political_supply_adoption_module&&item.first!="economic_supply_policies")continue;if(item.second!="1")return common_failure("UnsupportedSupplyPolicyVersion");if(++(item.first==political_supply_adoption_module?political:economic)!=1)return common_failure("DuplicateSupplyPolicyModule");}
    if(!enabled)return political||economic||state||economic_version?common_failure("UnexpectedSupplyPolicyProfile"):common_success();
    if(political!=1||economic!=1||!state||!economic_version||!takeover)return common_failure("MissingSupplyPolicyPair");return state->version==1&&*economic_version==1?common_success():common_failure("UnsupportedSupplyPolicyVersion");
}
}
