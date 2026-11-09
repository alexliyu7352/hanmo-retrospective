// 自愿接续的具名本人接受来源；社会收养事实不等于接受控制、赠与或承担旧职事。
#pragma once
#include "succession_personal_assets.hpp"
#include "resident/domains/interaction/dialogue_node_source.hpp"
namespace hanmo::resident {
struct FamilyState;
// 本人只接受作者明确列出的这份私产安排；不得把一般肯定对白解释为任意接续。
struct SuccessionConsentDefinition {
    StableId id,predecessor_actor_id,successor_actor_id,dialogue_id,node_id;
    Integer terms_version=1;
    SuccessionPersonalSelection personal;
};
struct SuccessionConsentCatalog { bool enabled=false;OrderedTable<SuccessionConsentDefinition> definitions; };
// 永久保存双方、实际关系来源和精确批次；职事、知识、公共及产业产权另有具名承接程序。
struct SuccessionConsentSource {
    StableId request_id,relation_fact_id,dialogue_id,node_id,node_kind,session_id,scenario_id;
    WorldId world_id{};
    InteractionContext context;
    Integer terms_version=1,accepted_tick=0,accepted_revision=0,predecessor_birth_tick=0,successor_birth_tick=0;
    SuccessionPersonalTerms personal;
    FieldOrder field_order;
};
struct SuccessionConsentQuote { AuthoredNodeSourceProof source;SuccessionPersonalQuote personal;SuccessionConsentSource agreement; };
// 目录准入固定双方和原节点，严格区分生前接续同意与成年收养同意。
Status validate_succession_consent_catalog(const SuccessionConsentCatalog&,const CommonCatalog&,const InteractionCatalog&);
Outcome<SuccessionConsentCatalog> decode_succession_consent_catalog(const BoundaryValue&,const BoundaryValue&);
// 仅核真实成功收养事实；亲生、旁支及门人尚须各自来源接口，未知关系不予推断。
Outcome<StableId> succession_adoption_basis(const CommonStore&,const FamilyState&,const StableId& predecessor,const StableId& successor,Integer tick);
// 来源报价不切换人物、不转账，也不替代稳定边界上的占用和遗留责任检查。
Outcome<SuccessionConsentQuote> preview_succession_consent(const CommonStore&,const FamilyState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const SuccessionConsentCatalog&,const StableId& predecessor,const StableId& successor,const StableId& request_id);
Status revalidate_succession_consent(const CommonStore&,const FamilyState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const SuccessionConsentCatalog&,const SuccessionConsentQuote&);
// 历史依据保留原日期、人物与条款，不要求历史会话仍打开或前代仍存活。
Status validate_succession_consent_source(const CommonStore&,const FamilyState&,const CommonCatalog&,const InteractionCatalog&,const SuccessionConsentCatalog&,const SuccessionConsentSource&);
bool succession_consent_source_equal(const SuccessionConsentSource&,const SuccessionConsentSource&);
// 保存原同意条款，不在解码时按当前资产重新生成或放宽接受范围。
Outcome<SuccessionConsentSource> decode_succession_consent_source(const BoundaryValue&);
BoundaryValue encode_succession_consent_source(const SuccessionConsentSource&);
Status visit_succession_consent_source(CodecVisitor&,SuccessionConsentSource&);
// 仅显式Family25..32：仍写原字段，接受真实Known narrative的完整context字段序。
Status visit_succession_consent_source_paths(CodecVisitor&,SuccessionConsentSource&);
}
