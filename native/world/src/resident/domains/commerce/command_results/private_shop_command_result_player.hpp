// 完整Family/Legacy/弟子控制史之后核真正外层提交者；原NPC业务人物不被重写为玩家。
#pragma once
#include "private_shop_command_results.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct FamilyState;struct FamilyLegacyState;struct DiscipleControlState;struct BiologicalControlState;
Status validate_private_shop_command_result_player_history(const CommonStore&,const PrivateShopState&,const PrivateShopCommandResultsState*,const FamilyState&,const FamilyLegacyState*,PrivateShopSubmissionAuthority authority=PrivateShopSubmissionAuthority::controlled_player,ProjectionWriter* projection=nullptr);
// 显式弟子/亲生控制原提交尾已全验后，逐原activation交叉永久实际结果。
Status validate_private_shop_command_control_anchors(const PrivateShopCommandResultsState*,const FamilyLegacyState&,const DiscipleControlState*,const BiologicalControlState*);
}
