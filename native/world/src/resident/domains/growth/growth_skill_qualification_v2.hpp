#include "resident/kernel/source_ref.hpp"
// source/2 真实技艺资格深值。旧 Study reader/1 与经营旧档布局不改。
#pragma once
#include "growth_result_refs.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
class CommonStore;struct CharacterConditionSources;struct GrowthState;struct LifeFixedServiceBoundary;struct LifeFixedActorInputs;
struct LegacyCompletedStudyRef {StableId action_id;};
struct GroupLessonSkillContributionRef {StableId course_id;Integer period_ordinal=0;StableId enrollment_id;};
struct PracticeAwardSkillContributionRef {StableId award_id,skill_id;};
struct VerifiedLegacyTradeGrantRef {StableId original_trade_result_id;};
// v1b§4.1.1 的六支净贡献引用；Training mastery 不进入16技艺XP。
using SkillContributionRefV2=std::variant<StudyLessonResultRef,StudyPerformanceAdjustmentRef,LegacyCompletedStudyRef,GroupLessonSkillContributionRef,PracticeAwardSkillContributionRef,VerifiedLegacyTradeGrantRef>;
struct SkillQualificationBoundaryRefV2 {SourceRef original_source_ref;Integer source_schema=0;};
struct GrowthSkillQualificationV2 {
 GrowthSkillQualificationV2()=default;
 // 原cold decoder可能没有text charge；fresh copy始终重算真实字段，不能复制旧租约数。
 GrowthSkillQualificationV2(const GrowthSkillQualificationV2&);
 // Default construction can allocate the original canonical 0/1 value.
 GrowthSkillQualificationV2(GrowthSkillQualificationV2&&);
 GrowthSkillQualificationV2& operator=(const GrowthSkillQualificationV2&);
 GrowthSkillQualificationV2& operator=(GrowthSkillQualificationV2&&)noexcept;
 void swap(GrowthSkillQualificationV2&)noexcept;
 friend void swap(GrowthSkillQualificationV2& left,GrowthSkillQualificationV2& right)noexcept{left.swap(right);}
 // 非持久内存拥有量，实际冷job字符串构造前取得；字段visitor只访问下列业务值。
 CheckpointMemoryCharge text_memory;
 WorldIdentity source_identity;StableId actor_id,skill_id;
 StableId source_kind;
 StableId origin_ref;SkillQualificationBoundaryRefV2 actual_boundary_ref;WorldMoment observed_at;
 Integer minimum_xp=0,minimum_level=0;
 enum class Coverage{complete,verified_legacy_lower_bound};Coverage coverage_kind=Coverage::complete;
 CheckpointVector<SkillContributionRefV2> contribution_refs;
};
// 本口沿原已落Study sources归集，要求全部已知净增与today严格等值；未知XP writer从不被当lower-bound容错。
// 原尚未落的Group/Practice/旧裁剪Trade不足以解释today时明确拒，完整capability不能据此宣称已齐。
Outcome<GrowthSkillQualificationV2> read_current_growth_skill_qualification_v2(const CharacterConditionSources&,const GrowthState&,const StableId& actor_id,const StableId& skill_id,const SkillQualificationBoundaryRefV2& actual_boundary,const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
// FSL保存实际body/head截面，逐课XP在同一期Condition之前；跨Moment按实际source时点，迟答同M须独立因果证明。
Outcome<GrowthSkillQualificationV2> read_growth_skill_qualification_before_fixed_boundary_v2(const CharacterConditionSources&,const GrowthState&,const StableId& actor_id,const StableId& skill_id,const StableId& actual_boundary_id);
}
namespace hanmo::resident {
Status validate_growth_skill_qualification_at_fixed_boundary_v2(const CharacterConditionSources&,const GrowthState&,const StableId& actual_boundary_id,const GrowthSkillQualificationV2&);
// 原边界实际append后的同candidate填固定skill槽，拒重复、旧head或不完整永久源。
// 缺任何所读技能源整候选回滚；查询与冷读不调用本writer。
Status capture_growth_fixed_skill_inputs_in_candidate(const CharacterConditionSources&,GrowthState&,const StableId& actual_boundary_id,const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
}

namespace hanmo::resident {
// 返回借用的原Terms/PersonalSkill字段；空值只说明这个actor/role原算法未读取16艺。
Outcome<const StableId*> read_growth_fixed_required_skill(const CharacterConditionSources&,const GrowthState&,const LifeFixedServiceBoundary&,const LifeFixedActorInputs&);
}

namespace hanmo::resident {
struct GrowthSkillActualBoundary {WorldMoment effective_at;const ActorConditionHead* condition_before=nullptr;};
// Original consumer owner reads its immutable Work/start/result only, without
// recursively validating the skill value that is embedded in that same row.
class GrowthSkillBoundarySourceRead {
public:virtual ~GrowthSkillBoundarySourceRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Outcome<GrowthSkillActualBoundary> read_actual_skill_boundary(const StableId& actor_id,const SkillQualificationBoundaryRefV2&)const=0;
};
Outcome<GrowthSkillQualificationV2> read_growth_skill_qualification_before_source_v2(const CharacterConditionSources&,const GrowthState&,const StableId& actor_id,const StableId& skill_id,const SkillQualificationBoundaryRefV2&,const GrowthSkillBoundarySourceRead&);
Status validate_growth_skill_qualification_before_source_v2(const CharacterConditionSources&,const GrowthState&,const GrowthSkillQualificationV2&,const GrowthSkillBoundarySourceRead&);
}

namespace hanmo::resident {
struct GrowthVerifiedSkillNetV2 {Integer initial_xp=0,known_net=0,current_xp=0;};
// Complete-source equality without inventing a consumer boundary/qualification.
Outcome<GrowthVerifiedSkillNetV2> read_verified_growth_skill_net_v2(const CharacterConditionSources&,const GrowthState&,const StableId& actor,const StableId& skill);
}
