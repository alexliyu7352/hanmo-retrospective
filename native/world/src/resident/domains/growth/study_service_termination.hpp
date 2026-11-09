// ST原StudyFact同槽真实终止。真实Common释放由世界原release_action接入；结果后绑定沿原提交尾。
#pragma once
#include "study_service_sources.hpp"
#include "resident/domains/family/family_life_contract.hpp"
namespace hanmo::resident {
struct CommitResult;
class StudyOriginalActionReleaseWrite {
public:virtual ~StudyOriginalActionReleaseWrite()=default;
 virtual Status release_original_study_action(const StableId& action_id)=0;
};
// 内部真实Death依赖调用；无UI取消/任意reason。再次调用只读回已经核真的同槽。
Outcome<const StudyServiceTerminalV2*> stop_study_for_verified_death_in_candidate(CommonStore&,GrowthState&,const GrowthCatalog&,const ActorFoundationCatalog&,const GrowthAccess&,const StableId& action_id,const StudyVerifiedParticipantDeath&,const WorldMoment& actual_moment,const ReadStamp&,const VerifiedDeathRead&,StudyOriginalActionReleaseWrite&);
// 只在原writer共享实现中调用；保分段迟答，非分段仍原收束结算一次。
Status settle_original_study_termination_in_candidate(CommonStore&,GrowthState&,const GrowthCatalog&,const StableId& action_id,const WorldMoment& actual_moment);
Outcome<const StudyServiceTerminalV2*> read_study_service_termination(const CommonStore&,const GrowthState&,const GrowthCatalog&,const ActorFoundationCatalog&,const GrowthAccess&,const StudyTerminal&,const WorldMoment& actual_now,const ReadStamp&,const VerifiedDeathRead&);
// before/after必须是本次真实世界候选；核真实cache对象地址/history尾后绑定新槽，拒旧源修改。
Status bind_study_service_release_result_in_candidate(const GrowthState& before,CommonStore&,GrowthState& after,const StableId& actor_id,Integer request_seq,const CommitResult&,const WorldMoment& actual_from,const WorldMoment& actual_to);
Status validate_study_service_release_commit_bindings(const CommonStore&,const GrowthState&,const WorldMoment& published_now);
}
namespace hanmo::resident {
struct StudyLessonChargeView {const StudyFact* original_study=nullptr;const StudyLesson* original_lesson=nullptr;const LessonCapabilitySource* actual_lesson_source=nullptr;Integer amount=0;};
// 原成功逐课recipe已实付的只读源；没有新PaymentLeg、预收款或欠款。
Outcome<StudyLessonChargeView> read_study_lesson_charge(const CharacterConditionSources&,const GrowthState&,const StudyLessonCharge&);
}
