// 独立来源段选择，不改变原politics访客、命令kind或Effects布局。
#pragma once
#include "political_dismissal_rules.hpp"
namespace hanmo::resident {
Outcome<Integer> political_dismissal_source_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Outcome<std::optional<PoliticalDismissalSourceCatalog>> decode_political_dismissal_source_profile(const BoundaryValue&,const std::vector<StableId>&);
// 未选中仍允许真实旧撤职；选中必须有独立状态，任何伪造本段Common事实均拒。
Status validate_political_dismissal_source_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const PoliticalDismissalSourceState*);
}
