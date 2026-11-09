// 隔离共享候选：短栈持有原军事、首府与通知读，候选替换后不缓存旧引用。
#pragma once
#include "resident/domains/politics/succession/political_succession_profile.hpp"
#include "resident/domains/politics/succession/political_succession_control.hpp"
#include "resident/domains/politics/succession/political_succession_view.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class PoliticalSuccessionWorldAccess final {
public:
 static Outcome<std::unique_ptr<PoliticalSuccessionWorldAccess>> create(const WorldBundle&);
 PoliticalSuccessionWorldAccess(const PoliticalSuccessionWorldAccess&)=delete;
 PoliticalSuccessionWorldAccess& operator=(const PoliticalSuccessionWorldAccess&)=delete;
 const PoliticalSuccessionContext& context()const{return context_;}
 PoliticalSuccessionViewContext view()const;
private:
 PoliticalSuccessionWorldAccess(const WorldBundle&,TaskMessageNoticeServices);
 const WorldBundle& world_;
 std::optional<MilitaryAccess> military_;
 std::optional<PoliticalSuccessionOccupationContext> occupations_;
 PoliticalSuccessionTerritoryContext territory_;
 TaskMessageNoticeServices notices_;
 PoliticalSuccessionNominationContext nominations_;
 PoliticalSuccessionAccessionSourceContext sources_;
 PoliticalSuccessionContext context_;
};
Status initialize_political_succession_world_in_candidate(WorldBundle&);
Status validate_political_succession_world(const WorldBundle&);
Outcome<PoliticalSuccessionQuote> prepare_political_succession_world(const WorldBundle&,const StableId&,const PoliticalSuccessionCommand&);
Outcome<PoliticsEffects> commit_political_succession_world(WorldBundle&,const StableId&,const PoliticalSuccessionCommand&,const PoliticalSuccessionQuote&);
Outcome<BoundaryValue> query_political_succession_world(const WorldBundle&,const StableId& place,Integer cursor=0);
Status bind_political_succession_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
