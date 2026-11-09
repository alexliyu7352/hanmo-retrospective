// 占领链、公共清单和产权变更的原生入口；失败由外层丢弃整个世界候选。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "occupation_reservations.hpp"
namespace hanmo::resident {
struct DynamicStepInput;
class CommonStore;
Outcome<MilitaryOwnership> military_garrison_owner(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& garrison,Integer limit_tick=-1);
Outcome<StableId> military_historical_owner(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& garrison,const StableId& occupation);
Outcome<MilitaryPublicOwnership> military_public_owner(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& field,const StableId& container);
Status validate_occupation_receipt(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& receipt,Integer limit_tick);
// 只核指定已应用旧占领的共同事实、正式战果和完整资产清单；不授予当前公库权，也不遍历其他未应用占领。
Status validate_applied_occupation_manifest(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& receipt);
// 仅原单半日候选可读取to_tick；稳定冷入口仍严格采用Common当前tick。
Status validate_applied_occupation_manifest(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& receipt,const DynamicStepInput&);
Outcome<bool> preview_military_occupation(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& receipt);
Outcome<OccupationEffect> apply_military_occupation(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& receipt);
// 仅真实接管组合栈提供逐预约域证明；旧签名固定无许可。
Outcome<bool> preview_military_occupation(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& receipt,const OccupationReservationRead&);
Outcome<OccupationEffect> apply_military_occupation(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& receipt,const OccupationReservationRead&);
Status validate_military_ownership(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
// 仅同一半日候选、clock_commit前核到终点；冷档仍走上面的当前tick严格入口。
Status validate_military_ownership(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const DynamicStepInput&);
}
