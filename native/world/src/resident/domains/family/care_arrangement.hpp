// P24.1 的实际照护同意。接受不建立占用、不移动儿童、不写满意。
#pragma once
#include "care_requirement_contract.hpp"
#include "family_continuation_people4.hpp"
#include "profile_control_eligibility.hpp"
#include "resident/domains/actor_foundation/character_expression.hpp"
namespace hanmo::resident {
// H 是本次明确同意的交接工期；职责窗口仍为原 Calendar，不改成服务累计量。
struct CareArrangementTerms {
 Integer handover_half_days=0,from_tick=0;
 std::optional<Integer> until_tick;
 std::vector<StableId> allowed_place_ids;
 std::vector<MomentOwnerSource> allowed_dependency_refs;
 std::vector<MomentOwnerSource> provision_arrangement_refs;
 std::vector<MomentOwnerSource> fee_arrangement_refs;
 std::vector<MomentOwnerSource> guardianship_permission_refs;
};
// 机构只能指定实际真人；机构任命/授权原结果仅保引用，不复制权源。
struct CareProviderIdentity {
 SubjectRef provider;StableId carer_actor_id;
 std::optional<MomentOwnerSource> institutional_assignment_ref;
};
struct AcceptedCaregiver {
 StableId accepted_id,requirement_id,subject_actor_id;
 CareProviderIdentity provider;
 SourceRef requirement_transition_ref;
 CareArrangementTerms terms;
 WorldMoment accepted_at;ActorConditionHead acceptance_condition_head;
};
// 外域供养、费用、地点/依附、监护与机构指定各由原 owner 反读。即使某类
// 引用为空也必须验证其真实条款允许为空；不得以空向量默认免费/无须许可。
class CareArrangementOriginalRead {
public:virtual ~CareArrangementOriginalRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Status validate_current_care_arrangement(const StableId& child,
  const CareProviderIdentity&,const CareArrangementTerms&,const WorldMoment&)const=0;
 virtual Status validate_care_arrangement_before_acceptance(const AcceptedCaregiver&,
  const SourceRef& actual_acceptance)const=0;
};
struct CareAcceptanceContext {
 const CareGoalSourceContext& goals;
 const OrderedTable<AcceptedCaregiver>& accepted;
 const FamilyContinuationPeopleRead4& people;
 const ProfileControlEligibilitySources& controls;
 const CareArrangementOriginalRead& originals;
 const CharacterConditionSources& body;
};
struct AcceptCareArrangementRequest {
 StableId requirement_id;CareProviderIdentity provider;CareArrangementTerms terms;
};
struct CareAcceptanceResult {SourceRef accepted_ref;bool already_accepted=false;};
Status validate_care_arrangement_terms_shape(const CareArrangementTerms&);
bool care_arrangement_terms_equal(const CareArrangementTerms&,const CareArrangementTerms&);
SourceRef care_acceptance_source(const StableId& accepted_id);
Outcome<Integer> care_acceptance_source_schema(const SourceRef&);
// 当前已实现本人玩家命令分支；历史按接受 Common 头前的控制链与生死反验。
// NPC 必须接实际原接受 route，不能把机构/他人姓名当成本次发令者。
Outcome<CareAcceptanceResult> accept_care_arrangement_in_candidate(CommonStore&,
 OrderedTable<AcceptedCaregiver>&,const CareAcceptanceContext&,
 const StableId& actual_command_actor,const AcceptCareArrangementRequest&,
 const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
Outcome<const AcceptedCaregiver*> read_accepted_caregiver(const CareAcceptanceContext&,const SourceRef&);
Status validate_accepted_caregiver_sources(const CareAcceptanceContext&);
// 仅反读已接受的原条款；是否已撤回、到场和适用由真实交接/持续覆盖源再核。
Outcome<Integer> read_accepted_care_handover_half_days(const CareAcceptanceContext&,const SourceRef&);
}
