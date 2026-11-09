#include "resident/domains/military/military_command_contract.hpp"
// 原营地装卸、借调和军令交接的typed入口；行旅请求由中央同候选协调，绝不瞬移或重复搬钱粮。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <string_view>
namespace hanmo::resident {
class CommonStore;class MilitaryReturnSourceRead;
struct MilitaryReturnCargoBuildView {
 const CheckpointVector<std::pair<std::string_view,Integer>>& goods;
 Integer money;
};


struct MilitaryReturnCargo {
 CheckpointMemoryCharge copied_fields;
 std::vector<std::pair<StableId,Integer>> goods;Integer money=0;
 MilitaryReturnCargo()=default;
 explicit MilitaryReturnCargo(const MilitaryReturnCargoBuildView&);
 MilitaryReturnCargo(const MilitaryReturnCargo&);
 MilitaryReturnCargo(MilitaryReturnCargo&&);
 MilitaryReturnCargo& operator=(MilitaryReturnCargo);
 void swap(MilitaryReturnCargo&)noexcept;
};

Outcome<MilitaryReturnCargo> preview_military_return_assets(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const StableId& army_id,const StableId& garrison_id);
struct MilitaryReturnDestination { StableId army_id,destination_settlement_id,return_garrison_id;Integer cost=0; };
std::optional<StableId> military_muster_estate(const MilitaryState&,const StableId& source_id);
StableId military_pending_siege(const MilitaryState&,const StableId& army_id);
std::vector<StableId> military_report_roles(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& campaign_id,bool present_only=true);
Outcome<MilitaryReturnDestination> military_return_destination(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& campaign_id,const StableId& garrison_id={});
// Field等真实连续位置来源不准借旧Common据点零成本短路，政策仍为上一原函数。
Outcome<MilitaryReturnDestination> military_return_destination_from_actual_route(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& campaign_id,const StableId& garrison_id={});
Outcome<MilitaryQuote> preview_military_lifecycle(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryCommand&,const StableId& own_action={});
Outcome<MilitaryEffect> apply_military_lifecycle(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryCommand&);
Status start_military_logistics(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&,const ArmyLogistics&,const MilitaryQuote&);
Outcome<MilitaryEffect> finish_military_logistics(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&);
Status validate_military_logistics(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&,bool finishing=false);
// 占领/解围原收据可即时或在到场后消费；已完成目标重放不改结果。
Status consume_military_campaign_receipts(const CommonRead&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const std::vector<StableId>& receipt_ids,Integer tick);
Outcome<std::vector<MilitaryJourneyRequest>> step_military_campaigns(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,Integer finish_tick,const std::vector<StableId>& completed_receipt_ids={});
Status acknowledge_military_return(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& order,const StableId& receipt);
Status validate_military_lifecycle(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
Status validate_military_lifecycle(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryReturnSourceRead&);
}
