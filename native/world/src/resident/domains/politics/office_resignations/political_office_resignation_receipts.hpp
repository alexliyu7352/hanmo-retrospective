// 首片三个玩家命令永久绑定真正提交结果，裁剪缓存后也不能借旧同tick来源冒新办理。
#pragma once
#include "political_office_resignation_codec.hpp"
namespace hanmo::resident {
// World必须核实际成功Done且effect恰为source；批准离职另核原Office变更effect。
using PoliticalOfficeResignationResultMatch=std::function<bool(const CommitResult&,const StableId& source_fact_id,const StableId& office_change_fact_id)>;
Outcome<StableId> political_office_resignation_new_player_source(const PoliticalOfficeResignationState& before,const PoliticalOfficeResignationState& after,const StableId& actor,const StableId& command_kind,Integer tick);
Status bind_political_office_resignation_player_result(const CommonStore&,PoliticalOfficeResignationState&,const StableId& source_fact_id,const StableId& actor,Integer request,const CommitResult&,const PoliticalOfficeResignationResultMatch&);
// 本口位于原Family控制/政治全史之后；不代替各业务来源和历史本人验证。
Status validate_political_office_resignation_player_results(const CommonStore&,const PoliticalOfficeResignationState&,const PoliticalOfficeResignationResultMatch&);
}
