// 成年收养的明确本人请求来源；不创建人物，不转移财产或玩家控制权。
#pragma once
#include "resident/domains/interaction/dialogue_node_source.hpp"

namespace hanmo::resident {
// 只有这份具名声明才赋予作者节点“被收养人本人提出收养请求”的语义。
// 一般交谈、同地、好感和拜师均不能替代该请求；继任须另外取得接受来源。
struct AdultAdoptionRequestDefinition {
    StableId id,adopter_actor_id,adoptee_actor_id,dialogue_id,node_id;
    Integer terms_version=1;
};
struct AdultAdoptionRequestCatalog {
    bool enabled=false;
    OrderedTable<AdultAdoptionRequestDefinition> definitions;
};
// 永久来源保留原人物和生日，不包含临时execution_epoch，也不承诺继任/赠与。
struct AdultAdoptionAgreement {
    StableId request_id,adopter_actor_id,adoptee_actor_id;
    StableId dialogue_id,node_id,node_kind,session_id,scenario_id;
    WorldId world_id{};
    InteractionContext context;
    Integer terms_version=1,accepted_tick=0,accepted_revision=0;
    Integer adopter_birth_tick=0,adoptee_birth_tick=0;
    FieldOrder field_order;
};
struct AdultAdoptionRequestQuote {
    AuthoredNodeSourceProof source;
    AdultAdoptionAgreement agreement;
};
// 只记录新的社会收养关系；原Actor创建事实和生物亲缘来源保持独立。
struct AdultAdoptionFact {
    StableId kind="family_adult_adoption";
    AdultAdoptionAgreement agreement;
    FieldOrder field_order;
};
class CodecVisitor;
struct BoundaryValue;
Outcome<AdultAdoptionRequestCatalog> decode_adult_adoption_request_catalog(const BoundaryValue&,const BoundaryValue&);
AdultAdoptionFact decode_adult_adoption_fact(const BoundaryValue&);
BoundaryValue encode_adult_adoption_fact(const AdultAdoptionFact&);
Status visit_adult_adoption_fact(CodecVisitor&,AdultAdoptionFact&);
bool adult_adoption_agreement_equal(const AdultAdoptionAgreement&,const AdultAdoptionAgreement&);
Status validate_adult_adoption_request_catalog(const AdultAdoptionRequestCatalog&,const CommonCatalog&,const InteractionCatalog&);
// 当前命令本人是收养人；被收养人的明确请求必须来自其当前真实会话节点。
// 这里只核自愿来源，最终收养仍须核Family亲缘/重复/循环，并原子记录同一事实。
Outcome<AdultAdoptionRequestQuote> preview_adult_adoption_request(
    const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,
    const AdultAdoptionRequestCatalog&,const StableId& adopter,const StableId& adoptee,const StableId& request_id);
Status revalidate_adult_adoption_request(
    const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,
    const AdultAdoptionRequestCatalog&,const AdultAdoptionRequestQuote&);
// 历史只核当时成年人及具名条款/永久来源；不要求旧会话、在场或当事人仍存活。
Status validate_adult_adoption_agreement_source(
    const CommonState&,const CommonCatalog&,const InteractionCatalog&,const AdultAdoptionRequestCatalog&,
    const AdultAdoptionAgreement&);
}
