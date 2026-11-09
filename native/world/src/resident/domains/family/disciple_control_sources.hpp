// 显式弟子政策只扩原控制许可的关系依据，原family_legacy记录/激活/历史玩家仍是唯一账。
#pragma once
#include "apprenticeship_source.hpp"
#include "family_apprenticeship.hpp"
#include "legacy_control_consent.hpp"
namespace hanmo::resident {
// 精确引用已选择的原本人控制请求；技艺只定位原关系，不授经验或教学资格。
struct DiscipleControlRequestBasis {StableId request_id,skill_id;};
struct DiscipleControlCatalog {bool enabled=false;OrderedTable<DiscipleControlRequestBasis> requests;};
struct DiscipleControlRelationSource {
    WorldIdentity identity;
    StableId request_id,relation_fact_id,predecessor_actor_id,successor_actor_id,skill_id,before_fact_id;
    Integer observed_tick=0;
};
// 临时同候选来源，不持有第二份关系/同意/控制状态，不在调用者栈结束后继续借用。
struct DiscipleControlRelationContext {
    const CommonStore& common;
    const CommonCatalog& common_catalog;
    const FamilyApprenticeshipState& apprenticeships;
    const LegacyControlConsentCatalog& original_requests;
    const DiscipleControlCatalog& catalog;
    ApprenticeshipIdentityRead people;
};
Outcome<DiscipleControlCatalog> decode_disciple_control_catalog(const BoundaryValue&);
Status validate_disciple_control_catalog(const DiscipleControlCatalog&,const LegacyControlConsentCatalog&,const CommonCatalog&);
// 正规准入再核原请求由候选私人岗位的两个本人 say 承载，不将玩家选项当候选签名。
Status validate_disciple_control_catalog(const DiscipleControlCatalog&,const LegacyControlConsentCatalog&,const CommonCatalog&,const InteractionCatalog&);
// 热端只可选唯一当前有效、相同双方/技艺的真实关系；多个候选不取latest。
Outcome<DiscipleControlRelationSource> select_current_disciple_control_relation(const DiscipleControlRelationContext&,const StableId& request_id);
// 撤回只指向原许可的确切关系；退出师门不剥夺本人收回许可的出口。
Outcome<DiscipleControlRelationSource> read_current_disciple_control_revocation_relation(const DiscipleControlRelationContext&,const StableId& request_id,const StableId& relation_fact_id);
// 冷端固定原许可保存的关系ID与真正许可事实前界；不重新选择后来关系来补旧许可。
Outcome<DiscipleControlRelationSource> read_disciple_control_relation_before_consent(const DiscipleControlRelationContext&,const StableId& request_id,const StableId& relation_fact_id,Integer tick,const StableId& exact_consent_or_revocation_fact);
}
