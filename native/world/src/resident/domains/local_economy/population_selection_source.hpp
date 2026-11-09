// 原P02 Selection的具名只读身份；没有第二事实、人数或权限表。
#pragma once
#include "population_transport.hpp"
namespace hanmo::resident {
inline constexpr const char* population_selection_source_kind="population_selection";
inline constexpr Integer population_selection_source_schema=1;
// 返回原保存行。权限仍由原接受业务/释放来源验证，此读取本身不授予物化/移人权。
Outcome<const PopulationSelection*> read_population_selection_source(const LocalEconomyState&,const PopulationTransportState&,const SourceRef&,Integer source_schema=population_selection_source_schema);
Outcome<SourceRef> population_selection_source_ref(const LocalEconomyState&,const PopulationTransportState&,const StableId& selection_id);
}
