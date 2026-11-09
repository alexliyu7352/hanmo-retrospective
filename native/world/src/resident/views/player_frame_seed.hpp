// 当前玩家首帧的闭合有限组合；由同一worker内固定叶回调读取，不接收任意查询列表或世界快照。
#pragma once
#include "resident/kernel/boundary_dispatch.hpp"
#include <functional>

namespace hanmo::resident {
// 中央回调绑定外层同一source，只调用既有query/preview；叶结果及原错误不得重新解释。
using PlayerFrameLeaf = std::function<Outcome<BoundaryValue>(LiveOperationKind,const BoundaryValue&)>;
inline constexpr std::size_t player_frame_seed_byte_limit=2u*1024u*1024u;
// npc_society只来自已准备目录的typed能力位；所有人物由本人/会面/当前speaker确定。
// 领域Outcome失败原样传播；必要query的{ok:false,...}作为完整根结果原样返回，绝不发布半帧。
// preview的普通拒绝保留在quotes子项。预算超出返回PlayerFrameSeedLimit，不裁剪页或字符串。
Outcome<BoundaryValue> query_player_frame_seed(const PlayerFrameLeaf& leaf,bool npc_society);
} // 只有固定page0，已有64行cursor续页仍走原接口。
