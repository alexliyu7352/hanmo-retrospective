#pragma once
#include "task_report_carrier_scope.hpp"
#include "resident/domains/commerce/self_transport_funding.hpp"
namespace hanmo::resident {
// This finite concrete reader owns only the original named-report offer branch.
// All cargo/group/private-mandate offers remain with their actual original owner.
class NativeTaskReportCarrierTermsRead final:public TaskTransportTermsRead {
 const TaskReportCarrierScopeContext& scope_;const SelfTransportFundingContext& funding_;
 const TaskReportCarrierContentSelection* selected_=nullptr;const SelfTransportFundingQuote* quote_=nullptr;
public:
 NativeTaskReportCarrierTermsRead(const TaskReportCarrierScopeContext&s,const SelfTransportFundingContext&f):scope_(s),funding_(f){}
 NativeTaskReportCarrierTermsRead(const TaskReportCarrierScopeContext&s,const SelfTransportFundingContext&f,const TaskReportCarrierContentSelection&selection,const SelfTransportFundingQuote&q):scope_(s),funding_(f),selected_(&selection),quote_(&q){}
 bool bound_to(const CommonStore&,const TaskState&)const noexcept;
 Outcome<TaskCarrierTerms> offered_carrier_terms(const SourceRef&)const override;
 Status validate_current_scope(const TaskCarrierTerms&,const StableId&)const override;
 bool pending_funding_required(const TaskCarrierTerms&)const noexcept override;
 Status validate_prospective_acceptance(const TaskCarrierAgreement&,const TaskCarrierReportTerms*)const override;
 Status validate_frozen_acceptance(const TaskCarrierAgreement&)const override;
 Outcome<TaskCarrierReportTerms> offered_report_delivery_terms(const TaskCarrierTerms&)const override;
 Status validate_report_delivery_acceptance(const TaskCarrierAgreement&,const TaskCarrierReportTerms&)const override;
 Status validate_report_delivery_coverage(const TaskCarrierAgreement&,const TaskCarrierReportTerms*)const override;
 Status validate_registered_continuation(const TaskCarrierAgreement&,const TransportRegistrationSource&,const TransportUnitState&,const ActorGroupInitiator&,const WorldMoment&)const override;
 Status validate_handover_sources(const TaskCarrierAgreement&,const TransportUnitReleaseSource&,const TransportUnitState&)const override;
 Status validate_stop_source(const TaskCarrierAgreement&,const SourceRef&)const override;
};
// The actual accepted Task plan exists before S7 accepts its original funding.
// Only the existing Task funding fields are returned to the one S3 writer.
class NativeTaskReportCarrierFundingWrite final:public TaskCarrierFundingAcceptanceWrite {
 CommonStore&common_;TaskState&tasks_;PrivateTransportFundingState&state_;
 const SelfTransportFundingContext&context_;const SelfTransportFundingQuote&quote_;
 const NativeTaskReportCarrierTermsRead&terms_;
public:
 NativeTaskReportCarrierFundingWrite(CommonStore&c,TaskState&t,PrivateTransportFundingState&s,const SelfTransportFundingContext&x,const SelfTransportFundingQuote&q,const NativeTaskReportCarrierTermsRead&r):common_(c),tasks_(t),state_(s),context_(x),quote_(q),terms_(r){}
 bool bound_to(const CommonStore&,const TaskState&)const noexcept override;
 Status validate_prospective_acceptance(const TaskCarrierAcceptanceQuote&,const WorldMoment&)const override;
 Outcome<TaskCarrierAcceptedFunding> accept_for_transport(const SourceRef&,const WorldMoment&)override;
 Status validate_finalized_transport(const SourceRef&,const WorldMoment&)const override;
};
}
