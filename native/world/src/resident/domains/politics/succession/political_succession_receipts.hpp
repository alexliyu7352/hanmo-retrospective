// 首片四个玩家命令永久绑定真正提交结果，裁剪缓存后也不能借旧同tick来源冒新办理。
#pragma once
#include "political_succession_codec.hpp"
namespace hanmo::resident {
// World必须核实际成功Done且effect恰为source；正式就任另核原Office变更effect。
using PoliticalSuccessionResultMatch=std::function<bool(const CommitResult&,const StableId& source_fact_id,const StableId& office_change_fact_id)>;
Outcome<StableId> political_succession_new_player_source(const PoliticalSuccessionState& before,const PoliticalSuccessionState& after,const StableId& actor,const StableId& command_kind,Integer tick);
Status bind_political_succession_player_result(const CommonStore&,PoliticalSuccessionState&,const StableId& source_fact_id,const StableId& actor,Integer request,const CommitResult&,const PoliticalSuccessionResultMatch&);
// 本口位于原Family控制/政治全史之后；不代替各业务来源和历史本人验证。
Status validate_political_succession_player_results(const CommonStore&,const PoliticalSuccessionState&,const PoliticalSuccessionResultMatch&);
}
