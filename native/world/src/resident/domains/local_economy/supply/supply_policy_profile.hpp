// 两段同选后才允许新命令tag14；旧生产配置仍独占原32位语义，不增旧存档字节。
#pragma once
#include "supply_policy.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t economic_supply_policy_checkpoint_feature=65536u;
// 标题、完整解码和writer使用同一布局门，单边、重复或未知版本一律拒绝。
inline Status validate_economic_supply_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules){
 unsigned economic=0,political=0;bool takeover=false,economy=false,tasks=false,affairs=false;
 for(const auto& module:modules){takeover|=module.first=="political_takeover"&&module.second=="1";economy|=module.first=="local_economy";tasks|=module.first=="tasks";affairs|=module.first=="affairs";
  if(module.first!="economic_supply_policies"&&module.first!="political_supply_adoptions")continue;if(module.second!="1")return common_failure("UnsupportedSupplyPolicyVersion");if(++(module.first=="economic_supply_policies"?economic:political)!=1)return common_failure("DuplicateSupplyPolicyModule");}
 if(!enabled)return economic||political?common_failure("UnexpectedSupplyPolicyProfile"):common_success();
 return economic==1&&political==1&&takeover&&economy&&tasks&&affairs?common_success():common_failure("MissingSupplyPolicyPair");
}
}
