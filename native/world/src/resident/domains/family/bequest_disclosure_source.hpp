// 生前当面公开只授予该受益人知晓确切遗赠条款，不代表同意受领或改变财产。
#pragma once
#include "family_bequest.hpp"
#include "resident/domains/interaction/dialogue_node_source.hpp"
namespace hanmo::resident {
struct BequestDisclosureSource {
    StableId declaration_id,donor_actor_id,beneficiary_actor_id,dialogue_id,node_id,node_kind,session_id,scenario_id;
    WorldId world_id{};InteractionContext context;Integer tick=0,revision=0;std::optional<WorldMoment> occurred_at;FieldOrder field_order;
};
struct BequestDisclosureQuote { AuthoredNodeSourceProof meeting;BequestDisclosureSource source; };
// 原账及控制历史先由调用者完整验证；本命令明确由当前在世遗赠人本人主动公开。
Outcome<BequestDisclosureQuote> preview_bequest_disclosure(const CommonStore&,const FamilyBequestState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& declaration_id,Integer selected_version=1,const FamilyContinuationPeopleRead4* people=nullptr);
Status revalidate_bequest_disclosure(const CommonStore&,const FamilyBequestState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const BequestDisclosureQuote&,Integer selected_version=1,const FamilyContinuationPeopleRead4* people=nullptr);
// 历史会面可以关闭，人物后来死亡不抹去已知内容；冷读以原具名事实序核生前公开。
Status validate_bequest_disclosure_source(const CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionCatalog&,const FamilyBequestPlayerRead&,const BequestDisclosureSource&,const StableId& disclosure_fact_id,Integer selected_version=1,const FamilyContinuationPeopleRead4* people=nullptr);
bool bequest_disclosure_source_equal(const BequestDisclosureSource&,const BequestDisclosureSource&);
Outcome<BequestDisclosureSource> decode_bequest_disclosure_source(const BoundaryValue&,Integer selected_version=1);
BoundaryValue encode_bequest_disclosure_source(const BequestDisclosureSource&,Integer selected_version=1);
Status visit_bequest_disclosure_source(CodecVisitor&,BequestDisclosureSource&,Integer selected_version=1);
}
