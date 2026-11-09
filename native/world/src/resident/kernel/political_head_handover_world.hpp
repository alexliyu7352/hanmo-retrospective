// 隔离共享候选：生前交接短栈自持原占领/首府借口，不留跨候选或跨时钟的临时引用。
#pragma once
#include "resident/domains/politics/head_handover/political_head_handover_profile.hpp"
#include "resident/domains/politics/head_handover/political_head_handover_control.hpp"
#include "resident/domains/politics/head_handover/political_head_handover_view.hpp"
#include "resident/domains/military/military_model.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class PoliticalHeadHandoverWorldAccess final {
public:
 static Outcome<std::unique_ptr<PoliticalHeadHandoverWorldAccess>> create(const WorldBundle&);
 PoliticalHeadHandoverWorldAccess(const PoliticalHeadHandoverWorldAccess&)=delete;
 PoliticalHeadHandoverWorldAccess& operator=(const PoliticalHeadHandoverWorldAccess&)=delete;
 const PoliticalHeadHandoverBusinessContext& context()const{return context_;}
private:
 explicit PoliticalHeadHandoverWorldAccess(const WorldBundle&);
 const WorldBundle& world_;
 std::optional<MilitaryAccess> military_;
 std::optional<PoliticalSuccessionOccupationContext> occupations_;
 PoliticalSuccessionTerritoryContext territory_;
 PoliticalHeadHandoverContext rules_;
 PoliticalHeadHandoverBusinessContext context_;
};
Status validate_political_head_handover_world(const WorldBundle&);
Outcome<PoliticalHeadHandoverQuote> prepare_political_head_handover_world(const WorldBundle&,const StableId&,const PoliticalHeadHandoverCommand&);
Outcome<PoliticsEffects> commit_political_head_handover_world(WorldBundle&,const StableId&,const PoliticalHeadHandoverCommand&,const PoliticalHeadHandoverQuote&);
Outcome<BoundaryValue> query_political_head_handover_world(const WorldBundle&,const StableId& place,Integer cursor=0);
Status bind_political_head_handover_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
