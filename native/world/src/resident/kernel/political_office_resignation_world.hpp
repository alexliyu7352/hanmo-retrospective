// 共享候选：辞任办理短栈自持原辖区、军事和人物控制只读，不把临时引用留给后续世界。
#pragma once
#include "resident/domains/politics/office_resignations/political_office_resignation_profile.hpp"
#include "resident/domains/politics/office_resignations/political_office_resignation_control.hpp"
#include "resident/domains/military/military_model.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class PoliticalOfficeResignationWorldAccess final {
public:
 static Outcome<std::unique_ptr<PoliticalOfficeResignationWorldAccess>> create(const WorldBundle&);
 PoliticalOfficeResignationWorldAccess(const PoliticalOfficeResignationWorldAccess&)=delete;
 PoliticalOfficeResignationWorldAccess& operator=(const PoliticalOfficeResignationWorldAccess&)=delete;
 const PoliticalOfficeResignationContext& context()const{return rules_;}
private:
 explicit PoliticalOfficeResignationWorldAccess(const WorldBundle&);
 const WorldBundle& world_;std::optional<MilitaryAccess> military_;std::optional<PoliticalSuccessionOccupationContext> occupations_;
 PoliticalOfficeResignationTerritoryContext territory_;PoliticalOfficeResignationContext rules_;
};
Status validate_political_office_resignation_world(const WorldBundle&);
Outcome<PoliticalOfficeResignationQuote> prepare_political_office_resignation_world(const WorldBundle&,const StableId&,const PoliticalOfficeResignationCommand&);
Outcome<PoliticsEffects> commit_political_office_resignation_world(WorldBundle&,const StableId&,const PoliticalOfficeResignationCommand&,const PoliticalOfficeResignationQuote&);
Outcome<BoundaryValue> query_political_office_resignation_world(const WorldBundle&,const StableId& place,Integer cursor=0);
Status bind_political_office_resignation_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
