// 生命周期由当前World候选持有；原军事、经济和任务读口在同一对象身份下组合。
#pragma once
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/politics/takeover/politics_takeover_application.hpp"
#include "resident/domains/politics/takeover/politics_takeover_command.hpp"
#include "resident/domains/politics/takeover/politics_takeover_territory.hpp"
#include "resident/domains/politics/takeover/politics_takeover_fiscal.hpp"
#include "resident/domains/politics/takeover/politics_takeover_views.hpp"
#include <memory>
#include "estate_employment_integration.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
#include "resident/domains/local_economy/supply/supply_policy.hpp"
#include "resident/domains/politics/supply/politics_supply_adoptions.hpp"
namespace hanmo::resident {
struct WorldBundle;
class PoliticalTakeoverWorldAccess final {
public:
    static Outcome<std::unique_ptr<PoliticalTakeoverWorldAccess>> create(
        const WorldBundle&,const PoliticalTakeoverCatalog&,const PoliticalTakeoverState&,const DynamicStepInput* phase=nullptr);
    PoliticalTakeoverWorldAccess(const PoliticalTakeoverWorldAccess&)=delete;
    PoliticalTakeoverWorldAccess& operator=(const PoliticalTakeoverWorldAccess&)=delete;
    const PoliticalTakeoverContext& context()const{return context_;}
    const PoliticalTakeoverOriginalAccess& originals()const{return originals_;}
    const PoliticalTakeoverAccess& responsibilities()const{return responsibilities_;}
    const PoliticalTakeoverTerritoryAccess& territories()const{return territories_;}
    const PoliticalTakeoverFiscalAccess& fiscal()const{return fiscal_;}
    const MilitaryAccess& military_access()const{return military_access_;}
private:
    PoliticalTakeoverWorldAccess(const WorldBundle&,const PoliticalTakeoverCatalog&,const PoliticalTakeoverState&,const DynamicStepInput*);
    MilitaryAccess military_access_;
    std::unique_ptr<EstateEmploymentWorldAccess> employment_;
    std::optional<PrivateShopResponsibilitySourceContext> shop_sources_;
    PoliticalTakeoverOriginalAccess originals_;
    std::unique_ptr<EconomicSupplyPendingAccess> supply_pending_;
    PoliticalTakeoverContext context_;
    PoliticalTakeoverAccess responsibilities_;
    PoliticalTakeoverTerritoryAccess territories_;
    PoliticalTakeoverFiscalAccess fiscal_;
};
// 中央CommandData持具体参数；同原封套绑定身份后才进入业务规则。
Outcome<PoliticalTakeoverQuote> prepare_political_takeover_world_command(
    const WorldBundle&,const PoliticalTakeoverCatalog&,const PoliticalTakeoverState&,
    const StableId& actor,const PoliticalTakeoverDecisionCommand&,const WorldIdentity&);
Outcome<PoliticalTakeoverQuote> prepare_political_takeover_world_command(
    const WorldBundle&,const PoliticalTakeoverCatalog&,const PoliticalTakeoverState&,
    const StableId& actor,const BoundaryValue& params,const WorldIdentity&);
Outcome<StableId> commit_political_takeover_world_command(
    WorldBundle&,const PoliticalTakeoverCatalog&,PoliticalTakeoverState&,
    const StableId& actor,const PoliticalTakeoverQuote&);
Outcome<BoundaryValue> query_political_takeover_world(
    const WorldBundle&,const PoliticalTakeoverCatalog&,const PoliticalTakeoverState&,
    const StableId& observer,const StableId& place);
// 原军事阶段只调用一次；责任执行回调必须与本World共享事务，错误由中央丢弃完整候选。
Outcome<PoliticalTakeoverApplication> apply_political_takeover_world_in_candidate(
    WorldBundle&,const PoliticalTakeoverCatalog&,PoliticalTakeoverState&,
    const MilitaryAccess& original_military_access,const StableId& occupation_id,
    const PoliticalTakeoverPendingApplication&,const DynamicStepInput* phase=nullptr);
}
