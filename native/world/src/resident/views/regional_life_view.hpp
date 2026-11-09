// 地方生活有限投影只读取当场人与合法报告；不创建会话、不获知消息、不变更钱货。
#pragma once
#include "resident/domains/interaction/regional_life_dialogue.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
class EconomicSupplyPolicyRead;class CurrentMomentRead;
using RegionalSharePreview=std::function<Status(const StableId& report,const StableId& speaker,const StableId& listener,const std::vector<StableId>& categories)>;
// 中央借同一候选绑定此解析器；原任务份额与Social分享准入各用唯一来源。
Outcome<std::vector<RegionalOpportunityBinding>> resolve_regional_life_candidates(const WorldRead&,const PreparedCatalog&,const OrganizationRead&,const GrowthLifeRead&,const RegionalSharePreview&,const StableId& observer,const StableId& place,const EconomicSupplyPolicyRead* issued_supply=nullptr,const CurrentMomentRead* actual_clock=nullptr);
// 每页64条，仅当前地点；对白补句来自已成功分享的冻结来源，未打开只显示可询问类别。
Outcome<BoundaryValue> query_regional_life_topics(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const GrowthLifeRead&,const SocialState&,const StableId& observer,const StableId& place,Integer cursor=0);
// 冷边界只核来源引用确实存在；过期会话仍能保留并安全退出，不能因此重绑新来源。
Status validate_regional_life_sources(const WorldRead&,const PreparedCatalog&,const GrowthLifeRead&,const CurrentMomentRead* actual_clock=nullptr);
}
