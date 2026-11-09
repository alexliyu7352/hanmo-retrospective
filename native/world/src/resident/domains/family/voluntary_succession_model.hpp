// 接续计划先取得本人对具体条款的接受，再由当前玩家在稳定边界确认移交。
#pragma once
#include "succession_consent_source.hpp"
namespace hanmo::resident {
// 撤销不收取或退回不存在的款项；完成只指向同一世界的唯一控制交接事实。
struct SuccessionPlanConclusion { StableId kind;Integer tick=0;std::optional<StableId> succession_fact_id;FieldOrder field_order; };
struct SuccessionConsentFact { StableId kind="family_succession_consent";SuccessionConsentSource agreement;std::optional<SuccessionPlanConclusion> conclusion;FieldOrder field_order; };
struct VoluntarySuccessionFact { StableId kind="family_voluntary_succession",consent_fact_id;Integer tick=0;FieldOrder field_order; };
// 控制链只借原事实中的前后代，不生成第二份关系或资产账。
struct FamilySuccessionStep { StableId predecessor_actor_id,successor_actor_id;Integer tick=0; };
Outcome<SuccessionConsentFact> decode_succession_consent_fact(const BoundaryValue&);
BoundaryValue encode_succession_consent_fact(const SuccessionConsentFact&);
Status visit_succession_consent_fact(CodecVisitor&,SuccessionConsentFact&);
Status visit_succession_consent_fact_paths(CodecVisitor&,SuccessionConsentFact&);
Outcome<VoluntarySuccessionFact> decode_voluntary_succession_fact(const BoundaryValue&);
BoundaryValue encode_voluntary_succession_fact(const VoluntarySuccessionFact&);
Status visit_voluntary_succession_fact(CodecVisitor&,VoluntarySuccessionFact&);
}
