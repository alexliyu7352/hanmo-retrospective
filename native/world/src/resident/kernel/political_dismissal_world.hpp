// 共享候选：短栈自持原Military/辖区读，原政治报价完成后才进入唯一撤职writer。
#pragma once
#include "resident/domains/politics/dismissal_sources/political_dismissal_profile.hpp"
#include "resident/domains/politics/dismissal_sources/political_dismissal_control.hpp"
#include "resident/domains/military/military_model.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class PoliticalDismissalWorldAccess final {
public:
 static Outcome<std::unique_ptr<PoliticalDismissalWorldAccess>> create(const WorldBundle&);
 PoliticalDismissalWorldAccess(const PoliticalDismissalWorldAccess&)=delete;
 PoliticalDismissalWorldAccess& operator=(const PoliticalDismissalWorldAccess&)=delete;
 const PoliticalDismissalContext& context()const{return rules_;}
private:
 explicit PoliticalDismissalWorldAccess(const WorldBundle&);
 const WorldBundle& world_;std::optional<MilitaryAccess> military_;std::optional<PoliticalSuccessionOccupationContext> occupations_;
 PoliticalDismissalTerritoryContext territory_;PoliticalDismissalContext rules_;
};
Status validate_political_dismissal_world(const WorldBundle&);
Status preview_political_dismissal_world(const WorldBundle&,const StableId&,const PoliticalDismissCommand&);
Outcome<PoliticsEffects> commit_political_dismissal_world(WorldBundle&,const StableId&,const PoliticalDismissCommand&);
Status bind_political_dismissal_result_world(WorldBundle&,const StableId&,const StableId&,Integer,const CommitResult&);
}
