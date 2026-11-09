// 隔离共享候选：生前交接短栈自持原占领/首府借口，不留跨候选或跨时钟的临时引用。
#pragma once
#include "resident/domains/politics/term_handover/political_term_handover_profile.hpp"
#include "resident/domains/politics/term_handover/political_term_handover_control.hpp"
#include "resident/domains/politics/term_handover/political_term_handover_view.hpp"
#include "resident/domains/military/military_model.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class PoliticalTermHandoverWorldAccess final {
public:
 static Outcome<std::unique_ptr<PoliticalTermHandoverWorldAccess>> create(const WorldBundle&);
 PoliticalTermHandoverWorldAccess(const PoliticalTermHandoverWorldAccess&)=delete;
 PoliticalTermHandoverWorldAccess& operator=(const PoliticalTermHandoverWorldAccess&)=delete;
 const PoliticalTermHandoverBusinessContext& context()const{return context_;}
private:
 explicit PoliticalTermHandoverWorldAccess(const WorldBundle&);
 const WorldBundle& world_;
 std::optional<MilitaryAccess> military_;
 std::optional<PoliticalSuccessionOccupationContext> occupations_;
 PoliticalSuccessionTerritoryContext territory_;
 PoliticalHeadSourceContext heads_;
 PoliticalTermHandoverContext rules_;
 PoliticalTermHandoverBusinessContext context_;
};
// 原新轮holder可借此纯来源，返回值只捕获同World，不反调本域完整验证。
PoliticalTermHandoverHeadRead political_term_handover_head_read_world(const WorldBundle&);
Status validate_political_term_handover_world(const WorldBundle&);
Outcome<PoliticalTermHandoverQuote> prepare_political_term_handover_world(const WorldBundle&,const StableId&,const PoliticalTermHandoverCommand&);
Outcome<PoliticsEffects> commit_political_term_handover_world(WorldBundle&,const StableId&,const PoliticalTermHandoverCommand&,const PoliticalTermHandoverQuote&);
Outcome<BoundaryValue> query_political_term_handover_world(const WorldBundle&,const StableId& place,Integer cursor=0);
Status bind_political_term_handover_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
