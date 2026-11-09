// 隔离共享候选：短栈持有原军事、首府与通知读，候选替换后不缓存旧引用。
#pragma once
#include "resident/domains/politics/successor_rounds/political_successor_round_profile.hpp"
#include "resident/domains/politics/successor_rounds/political_successor_round_control.hpp"
#include "resident/domains/politics/successor_rounds/political_successor_round_view.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class PoliticalSuccessorRoundWorldAccess final {
public:
 static Outcome<std::unique_ptr<PoliticalSuccessorRoundWorldAccess>> create(const WorldBundle&);
 PoliticalSuccessorRoundWorldAccess(const PoliticalSuccessorRoundWorldAccess&)=delete;
 PoliticalSuccessorRoundWorldAccess& operator=(const PoliticalSuccessorRoundWorldAccess&)=delete;
 const PoliticalSuccessorRoundBusinessContext& context()const{return context_;}
 PoliticalSuccessorRoundViewContext view()const;
private:
 PoliticalSuccessorRoundWorldAccess(const WorldBundle&,TaskMessageNoticeServices);
 const WorldBundle& world_;
 std::optional<MilitaryAccess> military_;
 std::optional<PoliticalSuccessionOccupationContext> occupations_;
 PoliticalSuccessionTerritoryContext territory_;
 TaskMessageNoticeServices notices_;
 PoliticalHeadSourceContext heads_;
 PoliticalSuccessorRoundContext nominations_;
 PoliticalSuccessorRoundAccessionSourceContext sources_;
 PoliticalSuccessorRoundBusinessContext context_;
};
Status initialize_political_successor_round_world_in_candidate(WorldBundle&);
Status validate_political_successor_round_world(const WorldBundle&);
Outcome<PoliticalSuccessorRoundQuote> prepare_political_successor_round_world(const WorldBundle&,const StableId&,const PoliticalSuccessorRoundCommand&);
Outcome<PoliticsEffects> commit_political_successor_round_world(WorldBundle&,const StableId&,const PoliticalSuccessorRoundCommand&,const PoliticalSuccessorRoundQuote&);
Outcome<BoundaryValue> query_political_successor_round_world(const WorldBundle&,const StableId& place,Integer cursor=0);
Status bind_political_successor_round_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
