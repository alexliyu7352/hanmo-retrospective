// 经济新政策先借纯政治责任/签权，再构造本域完整读；各成员严格同World候选保活。
#pragma once
#include "resident/domains/local_economy/supply/supply_policy_view.hpp"
#include "resident/domains/local_economy/supply/supply_policy_profile.hpp"
#include "resident/domains/politics/supply/politics_supply_authority.hpp"
#include "resident/domains/politics/supply/politics_supply_adoptions.hpp"
#include "political_takeover_integration.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class EconomicSupplyWorldAccess final {
public:
 static Outcome<std::unique_ptr<EconomicSupplyWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr);
 const EconomicSupplyPolicyContext& context()const{return context_;}
 const PoliticalTakeoverContext& political_context()const{return takeover_->context();}
 const EconomicSupplyPolicyAccess& read()const{return read_;}
 const PoliticalTakeoverRead& responsibilities()const{return takeover_->responsibilities();}
private:
 EconomicSupplyWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalTakeoverWorldAccess>,const DynamicStepInput*);
 std::unique_ptr<PoliticalTakeoverWorldAccess> takeover_;
 PoliticalSupplyPolicyAccess authority_;PoliticalSupplyAdoptionAccess adoptions_;
 EconomicSupplyPolicyContext context_;EconomicSupplyPolicyAccess read_;
};
Status validate_economic_supply_world(const WorldBundle&);
Outcome<EconomicSupplyPolicyQuote> prepare_economic_supply_world(const WorldBundle&,const StableId&,const IssueEconomicSupplyPolicyCommand&);
Outcome<LocalEconomyEffects> commit_economic_supply_world(WorldBundle&,const StableId&,const IssueEconomicSupplyPolicyCommand&,const EconomicSupplyPolicyQuote&);
Outcome<BoundaryValue> query_economic_supply_world(const WorldBundle&,const LocalEconomyQuery&);
// 零时/实际占领只撤新认领资格，不生成观察、Need、ID、付款或新权源。
Status project_economic_supply_world_in_candidate(WorldBundle&,const DynamicStepInput* phase=nullptr);
Outcome<std::vector<SupplyObservation>> refresh_economic_supply_world(WorldBundle&);
}
