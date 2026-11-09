#include "resident/domains/growth/growth_cards.hpp"
#include <string_view>
// 七卡canonical取得接缝：旧真实源只映射，不重授本领、经验或解锁另一套人物。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "growth_actor_cards.hpp"
#include "growth_insight_cards.hpp"
#include "resident/domains/family/character_creation.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
class ProjectionWriter;class CurrentMomentRead;



// 三项组成真正一次身份；不含execution_epoch、当前控制人或可刷新的查询时刻。
struct CanonicalCardAcquisitionIdentity {WorldId world_id{};StableId actor_id,card_id;};
// 旧owned仍是唯一取得authority，此引用不复制其本领效果。
struct ExistingGrowthCardAcquisitionSource {StableId actor_id,card_id;};
struct ActorUnlockEvidenceV1 {GrowthActorUnlockEvidence evidence;};
struct ActorUnlockEvidenceV2 {GrowthActorUnlockEvidence evidence;};
struct CampTrainingExperienceV1 {GrowthCampTrainingExperience evidence;};
// 只开放已有真实producer的闭合来源；新来源须连实际writer和validator后才能加入此联合。
using LegacyCardAcquisitionSource=std::variant<ExistingGrowthCardAcquisitionSource,ActorUnlockEvidenceV1,ActorUnlockEvidenceV2,CampTrainingExperienceV1>;
struct CardAcquisitionReceiptRefV2 {StableId receipt_id;};
struct LegacyCardSourceMappingRefV1 {StableId mapping_id;};
// S0 outbox只收这两种S4唯一记录引用；不接受任意domain/kind/source字符串。
using ProfileAcquisitionSource=std::variant<CardAcquisitionReceiptRefV2,LegacyCardSourceMappingRefV1>;
struct LegacyCardSourceMapping {
    StableId mapping_id,actor_id,card_id;LegacyCardAcquisitionSource legacy_source;
    CanonicalCardAcquisitionIdentity canonical_acquisition_identity;
    std::optional<StableId> profile_delivery_source_ref;
};
// 新取得原来源保持不同variant，不能把原初态与训练完成互冒。
struct ScenarioInitialCardGrant {StableId scenario_id,actor_id,card_id;};
struct GrowthTrainingCardCompletion {StableId fact_id,actor_id,card_id,personal_skill_id;};
using CardAcquisitionBasis=std::variant<ScenarioInitialCardGrant,GrowthTrainingCardCompletion,ActorUnlockEvidenceV1,ActorUnlockEvidenceV2,CampTrainingExperienceV1>;
struct CardAcquisitionReceiptV2 {
    StableId receipt_id,actor_id,card_id;Integer definition_version=1;GrowthCardCategory category=GrowthCardCategory::technique;
    ActorFoundationAt acquired_at;CharacterCreationMode mode=CharacterCreationMode::standard;
    CardAcquisitionBasis acquisition_basis;std::optional<StableId> applied_local_effect_ref;
};
struct CardAcquisitionState {
    OrderedTable<CardAcquisitionReceiptV2> card_acquisitions;
    OrderedTable<LegacyCardSourceMapping> legacy_card_source_mappings;
};
// 完整拥有读值供outbox，不含当前可变XP/掌握或伪造的Common事实头。
struct CardAcquisitionReadV2 {
    CanonicalCardAcquisitionIdentity canonical_identity;ProfileAcquisitionSource source;
    GrowthCardCategory category=GrowthCardCategory::technique;Integer definition_version=1;
    ActorFoundationAt acquired_at;CharacterCreationMode mode=CharacterCreationMode::standard;
    std::optional<StableId> actor_unlock_target;std::optional<StableId> applied_local_effect_ref;
};
// 跨世界profile拥有原完整取得来源；不是另一奖励源，不能用于恢复本局当前本领。
struct ResolvedLegacyCardAcquisition {LegacyCardSourceMapping mapping;std::optional<GrowthCardAcquisition> existing_growth_card_acquisition;};
struct ResolvedProfileAcquisition {CardAcquisitionReadV2 canonical;std::variant<CardAcquisitionReceiptV2,ResolvedLegacyCardAcquisition> actual_source;};
// 各参数必须借同一已验证候选，原领域完整验证在前；只有原实际源决定取得。
struct CardAcquisitionSources {
    const CommonState& common;const CommonCatalog& shared;const GrowthState& growth;const GrowthCatalog& growth_catalog;
    const TaskState& tasks;const TaskCatalog& task_catalog;const GrowthActorCardCatalog& actor_cards;
    const MilitaryState& military;const GrowthInsightCardCatalog& insight_cards;
    CharacterCreationMode mode=CharacterCreationMode::standard;
    const EnvoyTaskState* envoy=nullptr;const TaskEnvoyRead* envoy_read=nullptr;
    const CurrentMomentRead* actual_clock=nullptr;
};
// 在原成功候选内给实际旧源建立一次映射；cold decode和查询不得调用此writer。
// Common ID由外层完整WorldBundle候选保护；不得单独提交Common保存点。
Outcome<std::vector<LegacyCardSourceMappingRefV1>> bind_verified_legacy_card_acquisitions(CommonStore&,CardAcquisitionState&,const CardAcquisitionSources&,const StableId& actor_id);
// Exact At is read from the original immutable terminal publication/FSL chain.
Outcome<ActorFoundationAt> read_growth_training_card_acquired_at(const CardAcquisitionSources&,const StableId& training_id);
// 新训练取得改派时只写canonical receipt，不能同一actor/card再写旧owned。
Outcome<ProfileAcquisitionSource> record_verified_card_acquisition(CommonStore&,CardAcquisitionState&,const CardAcquisitionSources&,const StableId& card_id,const CardAcquisitionBasis&,const ActorFoundationAt&);
Outcome<CardAcquisitionReadV2> read_card_acquisition_v2(const CardAcquisitionSources&,const CardAcquisitionState&,const ProfileAcquisitionSource&);
// 不依控制路由的历史验证保旧成果；公开有界分页另严格核本人及完整ReadStamp。
Status validate_card_acquisitions(const CardAcquisitionSources&,const CardAcquisitionState&);
struct CardAcquisitionPage {WorldIdentity source_identity;StableId actor_id;Integer offset=0,limit=64,total=0;std::vector<CardAcquisitionReadV2> rows;};
Outcome<CardAcquisitionPage> read_controlled_card_acquisitions(const CardAcquisitionSources&,const CardAcquisitionState&,const StableId& actor_id,Integer offset=0,Integer limit=64);
// 只在已提交world真实reader核准后获取；S14可拥有完整结果而不依赖旧世界文件仍存在。
Outcome<ResolvedProfileAcquisition> resolve_profile_acquisition(const CardAcquisitionSources&,const CardAcquisitionState&,const ProfileAcquisitionSource&);
// 冷shape验证只核封闭结构与自洽，不替代生产时世界源核验；S14入口仍须上述producer。
Status validate_resolved_profile_acquisition_shape(const ResolvedProfileAcquisition&);
Status visit_resolved_profile_acquisition(CodecVisitor&,ResolvedProfileAcquisition&);
Outcome<BoundaryValue> encode_resolved_profile_acquisition(const ResolvedProfileAcquisition&);
Outcome<ResolvedProfileAcquisition> decode_resolved_profile_acquisition(const BoundaryValue&);
// 源/状态双向codec保存完整typed证据；unknown tag、缺字段和多余字段严格拒绝。
Status visit_profile_acquisition_source(CodecVisitor&,ProfileAcquisitionSource&);
Status visit_card_acquisitions(CodecVisitor&,CardAcquisitionState&);
// 只写完整已核canonical源及其实际原取得载荷，供S0指纹；不能拿裸引用ID代完整来源摘要。
Status visit_resolved_profile_acquisition_source(CodecVisitor&,const CardAcquisitionSources&,const CardAcquisitionState&,const ProfileAcquisitionSource&);
Outcome<BoundaryValue> encode_profile_acquisition_source(const ProfileAcquisitionSource&);
Outcome<ProfileAcquisitionSource> decode_profile_acquisition_source(const BoundaryValue&);
Outcome<BoundaryValue> encode_card_acquisitions(const CardAcquisitionState&);
Outcome<CardAcquisitionState> decode_card_acquisitions(const BoundaryValue&);
Status emit_resolved_profile_acquisition_projection(ProjectionWriter&,std::string_view,const ResolvedProfileAcquisition&);
Status emit_card_acquisitions_projection(ProjectionWriter&,std::string_view,const CardAcquisitionState&);
Status emit_profile_acquisition_source_projection(ProjectionWriter&,std::string_view,const ProfileAcquisitionSource&);
} // S14只消费已提交的S0 outbox，不能通过此源改变本局取得。
