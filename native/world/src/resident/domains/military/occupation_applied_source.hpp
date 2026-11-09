// 本次真实占领应用的阶段来源；仅包装原写口，不提供未来占领的普通读取接口。
#pragma once
#include "occupation_rules.hpp"
namespace hanmo::resident {
struct OccupationAppliedSource {
    WorldIdentity world;
    StableId occupation_id,siege_id,campaign_id,army_id,stronghold_id,garrison_id;
    StableId from_organization_id,to_organization_id,previous_occupation_id;
    Integer source_tick=0,effective_tick=0;
    OccupationAssetManifest assets;
};
struct OccupationPhaseApplication {
    OccupationEffect effect;
    std::optional<OccupationAppliedSource> source;
};
// 调用者必须在同一整体世界候选中登记责任；晚失败丢弃该候选，不能单独提交军事资产。
Outcome<OccupationPhaseApplication> apply_military_occupation_with_source(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const WorldIdentity&,const StableId& occupation_id);
Outcome<OccupationPhaseApplication> apply_military_occupation_with_source(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const WorldIdentity&,const StableId& occupation_id,const OccupationReservationRead&);
}
