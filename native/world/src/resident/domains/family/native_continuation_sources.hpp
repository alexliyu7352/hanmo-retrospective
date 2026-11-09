#pragma once
#include "control_continuation.hpp"
#include "family_legacy.hpp"
#include "family_origin_v2.hpp"
#include "disciple_control_sources.hpp"
#include "biological_control_sources.hpp"
#include "collateral_legacy_basis.hpp"
namespace hanmo::resident {
// 待接续地基只借用原控制、人物与死亡来源，不要求后置亲属同意链。
struct NativeContinuationFoundationSources {
 const CommonStore& common;
 const ControlContinuationState& controls;
 const FamilyState& family;
 const FamilyCatalog& family_catalog;
 const CommonCatalog& common_catalog;
 const FamilyDeathState& deaths;
 const FamilyContinuationPeopleRead4& people;
 const FamilyOriginV2Sources& origins;
};
// 反读真实死亡与原控制链；不创建关系、候选人或第二套接续状态。
class NativeContinuationFoundationSourceRead final
    : public ContinuationFoundationSourceRead {
 NativeContinuationFoundationSources source_;
public:
 // 借用同一候选中的原始所有者；调用方负责所有来源的生命期。
 explicit NativeContinuationFoundationSourceRead(
     NativeContinuationFoundationSources source):source_(source){}
 // 核验真实人物、死亡和控制读取器是否指向同一候选。
 bool bound_to(const CommonStore&)const noexcept override;
 // 完整入口额外核验家庭与控制所有者，不能跨候选借用来源。
 bool bound_to(const CommonStore&,const FamilyState&,
               const ControlContinuationState&)const noexcept;
 // 只读取可信世界钟，不把半日Tick伪装成精确时刻。
 Outcome<WorldMoment> actual_now()const override;
 // 沿用原创始人物来源校验；不以空家庭表绕过身份链。
 Status validate_initial_control_actor(const StableId&)const override;
 // 反读原死亡事实，缺失或引用不匹配即失败。
 Outcome<VerifiedDeathView> read_death(const DeathRef&)const override;
 // 由原生命历史读取人物死亡，不另建死亡索引。
 Outcome<std::optional<VerifiedDeathView>>
 read_actor_death(const StableId&)const override;
 // 从原事实之前的控制者与纪元核验待接续记录。
 Status validate_pending_control_origin(const PendingContinuation&)const override;
};
struct NativeContinuationSources {
 const CommonStore& common;const ControlContinuationState& controls;
 const FamilyState& family;const FamilyCatalog& family_catalog;const CommonCatalog& common_catalog;
 const InteractionCatalog& interactions;const FamilyDeathState& deaths;
 const FamilyLegacyState& legacy;const LegacyControlConsentCatalog& legacy_catalog;
 const FamilyContinuationPeopleRead4& people;const FamilyOriginV2Sources& origins;
 const DiscipleControlRelationContext* disciple=nullptr;
 const BiologicalControlDecisionContext* biological=nullptr;
 const CollateralControlDecisionContext* collateral=nullptr;
};
class NativeContinuationOriginalSourceRead final:public ContinuationOriginalSourceRead {
 NativeContinuationSources source_;
 NativeContinuationFoundationSourceRead foundation_;
 Status candidate(const PendingContinuation&,const StableId&,ContinuationPath,const CauseRef&,const ConsentRef&,Integer,const StableId&,const CommonActualFactPrefix* =nullptr,Integer source_revision=-1)const;
 Status legacy_acceptance(const ConsentRef&,const StableId& predecessor,const StableId& before,const CommonActualFactPrefix* =nullptr)const;
public:
 // 完整接续与待接续地基共享同一正文，不放宽完整关系来源要求。
 explicit NativeContinuationOriginalSourceRead(NativeContinuationSources s)
     :source_(s),foundation_({s.common,s.controls,s.family,s.family_catalog,
                            s.common_catalog,s.deaths,s.people,s.origins}){}
 bool bound_to(const CommonStore&)const noexcept override;
 bool bound_to(const CommonStore&,const FamilyState&,const ControlContinuationState&)const noexcept;
 Outcome<WorldMoment> actual_now()const override;
 Status validate_initial_control_actor(const StableId&)const override;
 Outcome<VerifiedDeathView> read_death(const DeathRef&)const override;
 Outcome<std::optional<VerifiedDeathView>> read_actor_death(const StableId&)const override;
 Status validate_pending_control_origin(const PendingContinuation&)const override;
 Status validate_voluntary_preparation(const PendingContinuation&)const override;
 Status validate_consent_source(const ContinuationConsentSource&)const override;
 Status validate_original_consent_registry(const ControlContinuationState&)const override;
 Status validate_current_candidate(const PendingContinuation&,const ContinuationCandidateRef&)const override;
 Status validate_historical_activation(const PendingContinuation&,const ControlActivationRecord&)const override;
 Status validate_historical_boundary_candidate(const PendingContinuation&,const ContinuationBoundaryReceipt&)const override;
 Status validate_historical_retirement_withdrawal(const PendingContinuation&)const override;
 // The actual accepted source determines a single path/ref. This pure read does
 // not allocate, grant consent, inspect unknown relatives or check S0 entry routes.
 Outcome<ContinuationCandidateRef> read_accepted_candidate(const PendingContinuation&,const ConsentRef&)const;
};
// Mechanical control-at-cut projection. Full source validation is performed by
// validate_control_continuation at installation, restore and published exit.
Outcome<StableId> read_control_actor_before_original_fact(const CommonStore&,const ControlContinuationState&,const StableId& initial_actor,const SourceRef&);
// Only the actual original Legacy writer calls this after appending its source.
// A failure aborts the outer private world candidate, including both owners.
class OriginalContinuationConsentWrite {
public:
 virtual ~OriginalContinuationConsentWrite()=default;
 virtual bool bound_to(const CommonStore&,const FamilyLegacyState&,const ControlContinuationState&)const noexcept=0;
 virtual Status consume_original_legacy_decision(const StableId& actual_fact_id)=0;
};
class NativeOriginalContinuationConsentWrite final:public OriginalContinuationConsentWrite {
 const CommonStore& common_;const FamilyLegacyState& legacy_;ControlContinuationState& state_;const ContinuationOriginalSourceRead& sources_;
public:
 NativeOriginalContinuationConsentWrite(const CommonStore& c,const FamilyLegacyState& l,ControlContinuationState& s,const ContinuationOriginalSourceRead& r):common_(c),legacy_(l),state_(s),sources_(r){}
 bool bound_to(const CommonStore&,const FamilyLegacyState&,const ControlContinuationState&)const noexcept override;
 Status consume_original_legacy_decision(const StableId&)override;
};

}
