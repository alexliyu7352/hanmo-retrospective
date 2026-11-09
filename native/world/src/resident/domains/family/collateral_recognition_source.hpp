// 真实旁支认可需原在场双方：提供者本人明确发言，当前玩家另行确认；不等于接续许可。
#pragma once
#include "collateral_control_catalog.hpp"
#include "collateral_family_origin.hpp"
namespace hanmo::resident {
struct CollateralRecognitionSource {
    StableId request_id,first_actor_id,second_actor_id,scenario_id;
    WorldId world_id{};
    Integer terms_version=1,tick=0,revision=0,first_birth_tick=0,second_birth_tick=0;
    CollateralFamilyOrigin ancestry;
    KnownPersonalDialogueProof statement;
    FieldOrder field_order;
};
struct CollateralRecognitionContext {
    BiologicalLegacyBasisContext people;
    const CollateralControlCatalog& catalog;
    const LegacyControlConsentCatalog& original_requests;
    const InteractionCatalog& interactions;
    CollateralControlTemplateRead templates;
    const KnownPersonalDialogueRead* meetings=nullptr;
    // 完整原Family/Legacy机械控制链折叠，不能反调本域认可或整个World验证。
    std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
};
// 只报价、只重读，没有append fact或玩家/资产写；提交方必须显式确认并绑定真实实际结果。
Outcome<CollateralRecognitionSource> preview_collateral_recognition(const CollateralRecognitionContext&,const StableId& first,const StableId& second,const StableId& request);
Status revalidate_collateral_recognition(const CollateralRecognitionContext&,const CollateralRecognitionSource&);
// 仅family_collateral:/family_collateral_recognized/family_collateral_legacy；原payload.tick由本域调用者先核。
Status validate_collateral_recognition_source_before_fact(const CollateralRecognitionContext&,const CollateralRecognitionSource&,const StableId& actual_fact);
bool collateral_recognition_source_equal(const CollateralRecognitionSource&,const CollateralRecognitionSource&);
Status validate_collateral_recognition_source_shape(const CollateralRecognitionSource&);
Outcome<CollateralRecognitionSource> decode_collateral_recognition_source(const BoundaryValue&);
BoundaryValue encode_collateral_recognition_source(const CollateralRecognitionSource&);
Status visit_collateral_recognition_source_checkpoint(CodecVisitor&,CollateralRecognitionSource&);
}
