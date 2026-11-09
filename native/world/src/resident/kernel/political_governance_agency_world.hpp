// 代理持者只借同候选原账；phase来源与当前新签发分开，不向原Task或赈济保存临时引用。
#pragma once
#include "resident/domains/politics/governance_agencies/political_governance_agency_profile.hpp"
#include "resident/domains/politics/governance_agencies/political_governance_agency_control.hpp"
#include "resident/domains/politics/governance_agencies/political_governance_agency_access.hpp"
#include "resident/domains/politics/governance_agencies/political_governance_agency_view.hpp"
#include "resident/domains/military/military_model.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class PoliticalGovernanceAgencyWorldAccess final {
public:
 // 未选择模块返回空持者，半套目录/状态或坏phase仍严格报错。
 static Outcome<std::unique_ptr<PoliticalGovernanceAgencyWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr);
 PoliticalGovernanceAgencyWorldAccess(const PoliticalGovernanceAgencyWorldAccess&)=delete;
 PoliticalGovernanceAgencyWorldAccess& operator=(const PoliticalGovernanceAgencyWorldAccess&)=delete;
 const PoliticalGovernanceAgencyAccess& read()const{return read_;}
 const PoliticalGovernanceAgencyBusinessContext& context()const{return context_;}
private:
 PoliticalGovernanceAgencyWorldAccess(const WorldBundle&,const DynamicStepInput*);
 const WorldBundle& world_;std::optional<MilitaryAccess> military_;std::optional<PoliticalSuccessionOccupationContext> occupations_;
 PoliticalGovernanceAgencyTerritoryContext territory_;PoliticalGovernanceAgencyContext rules_;PoliticalGovernanceAgencyAccess read_;PoliticalGovernanceAgencyBusinessContext context_;
};
Status validate_political_governance_agency_world(const WorldBundle&);
// 显式代理世界的原完整批准/治理任务/实付门在完整控制史后重走；不把新账替代旧合同。
Status validate_agency_governance_dependencies_world(const WorldBundle&);
Outcome<PoliticalGovernanceAgencyQuote> prepare_political_governance_agency_world(const WorldBundle&,const StableId&,const PoliticalGovernanceAgencyCommand&);
Outcome<PoliticsEffects> commit_political_governance_agency_world(WorldBundle&,const StableId&,const PoliticalGovernanceAgencyCommand&,const PoliticalGovernanceAgencyQuote&);
Outcome<BoundaryValue> query_political_governance_agency_world(const WorldBundle&,const StableId&,Integer cursor=0);
Status bind_political_governance_agency_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
