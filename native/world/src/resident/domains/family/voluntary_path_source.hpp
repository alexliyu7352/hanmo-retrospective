// 新生前路径只补原SuccessionConsent的真实来源；唯一接续和分项财物条款仍在原Family账。
#pragma once
#include "voluntary_path_catalog.hpp"
#include "biological_control_origin.hpp"
namespace hanmo::resident {
struct VoluntaryPathSource {
    VoluntaryRelationKind relation_kind=VoluntaryRelationKind::adoption;
    Integer relation_established_tick=0;
    StableId disciple_skill_id;
    std::optional<BiologicalControlOrigin> biological;
    KnownPersonalDialogueProof statement;
    FieldOrder field_order;
};
struct VoluntaryPathContext {
    VoluntaryRelationContext people;
    const VoluntaryPathCatalog& catalog;
    const SuccessionConsentCatalog& original_requests;
    const InteractionCatalog& interactions;
    VoluntaryPathTemplateRead templates;
    VoluntaryOtherDeclarationRead other_declarations;
    const KnownPersonalDialogueRead* meetings=nullptr;
    // 仅原Family/Legacy控制链截面；World保证全局ID/Common前界同侧，不反调完整新路径。
    std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
    // 当前完成时调用原Family.ready/受托任务/已退隐/真实会话退出门，不代替关系或改公职。
    std::function<Status(const StableId&,const StableId&)> stable_parties;
};
struct VoluntaryPathConsentQuote {
    SuccessionConsentSource agreement;
    VoluntaryPathSource path;
    SuccessionPersonalQuote personal;
};
Outcome<VoluntaryPathConsentQuote> preview_voluntary_path_consent(const VoluntaryPathContext&,
    const StableId& predecessor,const StableId& successor,const StableId& request,const StableId& relation_fact={});
Status revalidate_voluntary_path_consent(const VoluntaryPathContext&,const VoluntaryPathConsentQuote&);
// 真family_succession_consent载荷必须已写到同一候选原Family对象；只核原接受前截面。
// 后来人物死亡不抹已发生接受；完成换控仍须再次核双方在世与相同有效关系。
Status validate_voluntary_path_consent_before_fact(const VoluntaryPathContext&,
    const SuccessionConsentSource&,const VoluntaryPathSource&,const StableId& actual_consent_fact);
bool voluntary_path_source_equal(const VoluntaryPathSource&,const VoluntaryPathSource&);
Status validate_voluntary_path_source_shape(const VoluntaryPathSource&);
BoundaryValue encode_voluntary_path_source(const VoluntaryPathSource&);
Outcome<VoluntaryPathSource> decode_voluntary_path_source(const BoundaryValue&);
Status visit_voluntary_path_source(CodecVisitor&,VoluntaryPathSource&);
}
