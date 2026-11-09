// 一个栈对象持有同候选外交、接管与文书读口；不跨NPC回滚缓存子对象地址。
#pragma once
#include "political_takeover_integration.hpp"
#include "resident/domains/politics/treaty/treaty_passage_access.hpp"
#include "resident/domains/politics/treaty/treaty_passage_command.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_reports.hpp"
#include "resident/domains/tasks/envoy_task_rules.hpp"
namespace hanmo::resident {
class TreatyPassageWorldAccess final {
public:
 static Outcome<std::unique_ptr<TreatyPassageWorldAccess>> create(const WorldBundle&);
 const TreatyPassageContext& context()const{return context_;}
private:
 TreatyPassageWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalTakeoverWorldAccess>);
 std::unique_ptr<PoliticalTakeoverWorldAccess> takeover_;
 InteractionAccess interaction_;
 DiplomaticPresentRead present_;
 DiplomaticMissionAccess missions_;
 std::optional<DiplomaticReportAccess> reports_;
 std::unique_ptr<EnvoyTaskRead> envoy_;
 TreatyPassageTerritoryAccess territory_;
 TreatyPassageContext context_;
};
Status validate_treaty_passage_world(const WorldBundle&);
Outcome<TreatyPassageQuote> prepare_treaty_passage_world(const WorldBundle&,const StableId&,const TreatyPassageCommand&);
Outcome<StableId> commit_treaty_passage_world(WorldBundle&,const StableId&,const TreatyPassageCommand&,const TreatyPassageQuote&);
Outcome<bool> treaty_passage_world_admission(const WorldBundle&,const StableId&,const StableId&,const StableId&);
Outcome<BoundaryValue> merge_treaty_passage_world_page(const WorldBundle&,const StableId&,BoundaryValue);
}
