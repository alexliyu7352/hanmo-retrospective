// 只消费原Family唯一控制链；精确回复事实前界，不保存第二玩家身份或根据今日metadata推旧身份。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <functional>
namespace hanmo::resident {
class CommonStore;struct FamilyState;struct FamilyLegacyState;
using VisitReplyPlayerHistoryRead=std::function<Outcome<StableId>(Integer tick,const StableId& actual_reply_fact)>;
// 调用前完整Family、Birth、Legacy及弟子来源须通过；本口补跨域Common序与实例序同侧，再借原控制读。
Outcome<StableId> read_visit_reply_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& actual_reply_fact);
}
