// 显式Family25..32下原三个命令的薄适配；旧Family1..16仍走其原公开入口。
#pragma once
#include "voluntary_path_commands.hpp"
namespace hanmo::resident {
Outcome<FamilyQuote> preview_original_voluntary_path_command(const VoluntaryPathContext&,const VoluntaryPathState&,
    const InteractionState&,const InteractionAccess&,const StableId& actor,const FamilyCommand&);
Outcome<FamilyEffect> apply_original_voluntary_path_command(CommonStore&,FamilyState&,VoluntaryPathState&,
    const VoluntaryPathContext&,const InteractionState&,const InteractionAccess&,const StableId& actor,
    const FamilyCommand&,const FamilyQuote&);
}
