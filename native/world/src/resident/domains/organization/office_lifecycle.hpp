// 集中Office唯一字段writer；临时伤病不调用本口，死亡不是五种OfficeTransition之一。
#pragma once
#include "office_authority.hpp"
#include "resident/domains/family/family_life_contract.hpp"
namespace hanmo::resident {
enum class OfficeTransitionKind {install,dismiss,resign,term_handover,accession};
struct VerifiedOfficeTransition {SourceRef source;StableId organization_id,office_id;Integer expected_epoch=0;std::optional<StableId> old_holder,new_holder;OfficeTransitionKind kind=OfficeTransitionKind::install;WorldMoment effective_moment;};
// 原任命/辞免/移交/继位业务唯一producer反读源，不接受前端填holder或任意SourceRef存在证明。
class OfficeTransitionSourceRead {public:virtual ~OfficeTransitionSourceRead()=default;virtual Outcome<VerifiedOfficeTransition> read_office_transition(const SourceRef&,const ReadStamp&)const=0;};
struct OfficeChangeResult {bool changed=false;std::optional<StableId> fact_id;std::vector<StableId> stopped_duty_ids,disabled_role_ids;};
Outcome<OfficeChangeResult> consume_office_transition(CommonStore&,OrganizationState&,const OrganizationCatalog&,const SourceRef&,const ReadStamp&,const OfficeTransitionSourceRead&);
Outcome<OfficeChangeResult> consume_verified_office_death(CommonStore&,OrganizationState&,const OrganizationCatalog&,const StableId& office_id,Integer expected_epoch,const DeathRef&,const ReadStamp&,const VerifiedDeathRead&);
Status validate_office_lifecycle(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const ReadStamp&,const OfficeTransitionSourceRead&,const VerifiedDeathRead&);
}
