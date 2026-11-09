// 实际成功结果永久指向唯一岗位认领，不借首保留history的猜测区间恢复已删来源。
#pragma once
#include "political_office_reception_codec.hpp"
namespace hanmo::resident {
// World薄桥必须核真实零时成功Done与唯一PoliticsEffects来源，无Office变更effect。
using PoliticalOfficeReceptionResultMatch=std::function<bool(const CommitResult&,const StableId&)>;
Outcome<StableId> political_office_reception_new_player_source(const PoliticalOfficeReceptionState& before,const PoliticalOfficeReceptionState& after,const StableId& actor,const StableId& command_kind,Integer tick);
Status bind_political_office_reception_player_result(const CommonStore&,PoliticalOfficeReceptionState&,const StableId&,const StableId& actor,Integer request,const CommitResult&,const PoliticalOfficeReceptionResultMatch&);
Status validate_political_office_reception_player_results(const CommonStore&,const PoliticalOfficeReceptionState&,const PoliticalOfficeReceptionResultMatch&);
}
