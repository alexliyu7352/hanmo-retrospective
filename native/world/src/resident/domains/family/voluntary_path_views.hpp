#pragma once
#include "voluntary_path_original_commands.hpp"
namespace hanmo::resident {
// 总计每页16条。当前真实SAY才显示新接受项，既有plan只沿本人参与索引投影。
// 不从族谱/隐藏人物生成候选；死后控制许可有自己的原页面。
Outcome<BoundaryValue> query_voluntary_path_page(const VoluntaryPathContext&,const VoluntaryPathState&,
    const InteractionState&,const InteractionAccess&,Integer page);
}
