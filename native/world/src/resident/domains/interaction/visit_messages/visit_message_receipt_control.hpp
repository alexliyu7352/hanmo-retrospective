// 本人身份只取原Family唯一控制链，后来接续不能把原玩家业务改成NPC空回执。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <functional>
namespace hanmo::resident {
class CommonStore;struct FamilyState;struct FamilyLegacyState;
using VisitMessageAdvanceSubmitterRead=std::function<Outcome<StableId>(Integer tick,const StableId& actual_message_fact)>;
using VisitMessagePlayerHistoryRead=std::function<Outcome<StableId>(Integer tick,const StableId& actual_message_fact)>;
// 仅原托/领/撤三种实际头；完整Family/Birth/Legacy及弟子/Bio来源先验由World晚序持有。
Outcome<StableId> read_visit_message_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& actual_message_fact);
// 独立访帖首片无全控制result组：本半日真实换控前后的混合Advance不推断提交者。
// 热端在原NPC预览前据此延后本轮办理；冷端只能消费没有先前同tick换控的事实截面。
Outcome<bool> visit_message_npc_receipt_available(const CommonStore&,const FamilyState&,const FamilyLegacyState*);
Outcome<StableId> read_visit_message_advance_submitter_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& actual_message_fact);
}
