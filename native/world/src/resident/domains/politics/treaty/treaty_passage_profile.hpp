// 新双签模块只有一个显式段；旧外交回执不能借用该特性留下新条约命令。
#pragma once
#include "treaty_passage_model.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t treaty_passage_checkpoint_feature=1024u;
inline bool treaty_passage_command_kind(const StableId& kind){return kind=="treaty_passage_present"||kind=="treaty_passage_sign";}
// 未选条约只核本模块缺席；政治版本约束仅属于已选条约，重复模块仍拒绝。
inline Status validate_treaty_passage_layout(
    bool enabled, const std::vector<std::pair<StableId, StableId>> &modules,
    const TreatyPassageState *state) {
  unsigned treaty = 0, politics = 0, diplomacy = 0, envoy = 0;
  StableId diplomatic_version, envoy_version;
  for (const auto &item : modules) {
    if (item.first == "treaty_passage") {
      if (item.second != "1")
        return common_failure("UnsupportedTreatyPassageVersion");
      if (++treaty != 1)
        return common_failure("DuplicateTreatyPassageModule");
    } else if (item.first == "politics") {
      if ((enabled && item.second != "1") || ++politics != 1)
        return common_failure("TreatyPassageProfileMismatch");
    } else if (item.first == "diplomacy") {
      ++diplomacy;
      diplomatic_version = item.second;
    } else if (item.first == "envoy_tasks") {
      ++envoy;
      envoy_version = item.second;
    }
  }
  if (!enabled)
    return treaty || state ? common_failure("UnexpectedTreatyPassageProfile")
                           : common_success();
  if (treaty != 1 || !state || politics != 1 || diplomacy != 1 || envoy != 1 ||
      (diplomatic_version != "1" && diplomatic_version != "2") ||
      diplomatic_version != envoy_version)
    return common_failure("TreatyPassageProfileMismatch");
  return state->version == 1
             ? common_success()
             : common_failure("UnsupportedTreatyPassageVersion");
}
inline Status validate_treaty_passage_command_history(const CommonStore& common,bool enabled){
    if(enabled)return common_success();
    for(const auto& row:common.state().facts.rows())if(row.second.kind==treaty_passage_delivery_kind||row.second.kind==treaty_passage_signature_kind)return common_failure("TreatyPassageProfileRequired",row.first);
    for(const auto& row:common.state().history)if(treaty_passage_command_kind(row.kind))return common_failure("TreatyPassageProfileRequired",row.result_id);
    for(const auto& row:common.state().results.rows())if(row.second&&treaty_passage_command_kind(row.second->kind))return common_failure("TreatyPassageProfileRequired",row.first);
    return common_success();
}
}
