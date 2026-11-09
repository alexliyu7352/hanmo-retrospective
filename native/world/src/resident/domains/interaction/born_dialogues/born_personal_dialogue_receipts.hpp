// 玩家打开模板只能绑定本次真实Common结果；缓存裁剪后也不能换挂旧会话或删除永久绑定。
#pragma once
#include "born_personal_dialogue_state.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Outcome<StableId> born_personal_dialogue_new_player_source(const BornPersonalDialogueState* before,const BornPersonalDialogueState* after,const StableId& actor,const StableId& kind,const ReceiptDetails&,Integer tick);
Status bind_born_personal_dialogue_player_result(CommonStore&,BornPersonalDialogueState&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
Status validate_born_personal_dialogue_cached_results(const CommonStore&,const BornPersonalDialogueState*);
}
