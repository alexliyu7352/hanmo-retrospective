// 隔离共享候选：岗位接任短栈自持原辖区借用，不保存跨世界临时引用。
#pragma once
#include "resident/domains/politics/office_reception/political_office_reception_profile.hpp"
#include "resident/domains/politics/office_reception/political_office_reception_control.hpp"
#include "resident/domains/politics/office_reception/political_office_reception_view.hpp"
#include "resident/domains/military/military_model.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class PoliticalOfficeReceptionWorldAccess final {
public:
 static Outcome<std::unique_ptr<PoliticalOfficeReceptionWorldAccess>> create(const WorldBundle&);
 PoliticalOfficeReceptionWorldAccess(const PoliticalOfficeReceptionWorldAccess&)=delete;
 PoliticalOfficeReceptionWorldAccess& operator=(const PoliticalOfficeReceptionWorldAccess&)=delete;
 const PoliticalOfficeReceptionBusinessContext& context()const{return context_;}
private:
 explicit PoliticalOfficeReceptionWorldAccess(const WorldBundle&);
 const WorldBundle& world_;std::optional<MilitaryAccess> military_;std::optional<PoliticalSuccessionOccupationContext> occupations_;
 PoliticalSuccessionTerritoryContext territory_;PoliticalOfficeReceptionContext rules_;PoliticalOfficeReceptionBusinessContext context_;
};
Status validate_political_office_reception_world(const WorldBundle&);
Outcome<PoliticalOfficeReceptionQuote> prepare_political_office_reception_world(const WorldBundle&,const StableId&,const PoliticalOfficeReceptionCommand&);
Outcome<PoliticsEffects> commit_political_office_reception_world(WorldBundle&,const StableId&,const PoliticalOfficeReceptionCommand&,const PoliticalOfficeReceptionQuote&);
Outcome<BoundaryValue> query_political_office_reception_world(const WorldBundle&,const StableId& place,Integer cursor=0);
Status bind_political_office_reception_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
