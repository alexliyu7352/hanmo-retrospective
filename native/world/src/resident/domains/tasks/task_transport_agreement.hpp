// S3 transport acceptance is a real bounded agreement source, not inferred from a Shipment status string.
#pragma once
#include <string_view>
#include "task_report_carriage_types.hpp"
#include "resident/model/commitment_ref.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/domains/travel/group_transport_registration.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
namespace hanmo::resident {
struct TaskState;
struct TaskCarrierSelectionTerms {StableId selection_id;std::vector<StableId> releasable_allocation_ids;};
struct TaskCarrierTerms {
 SourceRef original_terms_source,original_business_source;TermsRevisionRef terms_revision;
 SubjectRef principal,carrier_subject;ActorGroupInitiator initiator;
 StableId provider_actor_id,role_id,dialogue_id,accept_node_id;
 std::vector<TaskCarrierSelectionTerms> population_selections;std::vector<StableId> named_participants,vehicle_refs,inventory_ids;
 StableId custody_ref,destination_site_id;TransportExpenseAuthority expense_authority;
 std::vector<SourceRef> cargo_authority_refs,vehicle_authority_refs,participant_consent_refs,payment_source_refs,passage_source_refs;
 Integer labor_until_tick=0;
};
struct TaskCarrierAcceptanceContact {StableId actor_id,provider_actor_id,session_id,place_id,role_id,dialogue_id,node_id;Integer tick=0;};
struct TaskCarrierAgreement {
 StableId id;Integer accepted_scope_revision=1;TaskCarrierTerms terms;TaskCarrierAcceptanceContact contact;WorldMoment accepted_at;
 // Only explicit accepted amendment/termination may stop new dispatch. Existing custody is not undone.
 std::optional<SourceRef> stopped_source;
};
struct TaskTransportAgreementState {Integer version=1;OrderedTable<TaskCarrierAgreement> agreements;OrderedTable<TaskReportCarriage> report_carriages;OrderedTable<TaskReportHandoverSource> report_handover_sources;};
class TaskTransportTermsRead {public:virtual ~TaskTransportTermsRead()=default;
 virtual Outcome<TaskCarrierTerms> offered_carrier_terms(const SourceRef&)const=0;
 virtual Status validate_current_scope(const TaskCarrierTerms&,const StableId&accepting_actor)const=0;
 // Re-read the original continuing task/contract and current Actor authority
 // after registration. Population selections are already materialized here;
 // the new-registration admission predicate must not be reused.
 virtual Status validate_registered_continuation(const TaskCarrierAgreement&,const TransportRegistrationSource&,const TransportUnitState&,const ActorGroupInitiator&,const WorldMoment&)const=0;
 virtual Status validate_frozen_acceptance(const TaskCarrierAgreement&)const=0;
 // Only the explicit same-command funding branch may use a source-free quote.
 // A published acceptance always goes through validate_frozen_acceptance.
 virtual bool pending_funding_required(const TaskCarrierTerms&)const noexcept{return false;}
 virtual Status validate_prospective_acceptance(const TaskCarrierAgreement&a,const TaskCarrierReportTerms*)const{return validate_frozen_acceptance(a);}
 virtual Status validate_handover_sources(const TaskCarrierAgreement&,const TransportUnitReleaseSource&,const TransportUnitState&)const=0;
 virtual Status validate_stop_source(const TaskCarrierAgreement&,const SourceRef&)const=0;
 // Explicit MC1 /2 branch, never an empty successful fallback for old handlers.
 virtual Outcome<TaskCarrierReportTerms> offered_report_delivery_terms(const TaskCarrierTerms&)const{return {false,{},common_failure("CarrierReportTermsReadRequired").error};}
 virtual Status validate_report_delivery_acceptance(const TaskCarrierAgreement&,const TaskCarrierReportTerms&)const{return common_failure("CarrierReportAcceptanceReadRequired");}
 // Original immutable offered terms determine whether this agreement carries a
 // Report. null is valid only for a genuinely non-report original acceptance;
 // removing the /2 tail must not turn a report contract into an ordinary one.
 virtual Status validate_report_delivery_coverage(const TaskCarrierAgreement&,const TaskCarrierReportTerms*)const{return common_failure("CarrierReportOriginalCoverageReadRequired");}
};
// Borrow the actual accepted source without calling expense/Work/transport
// validators back. This source-local cut is for those original owners; it does
// not by itself establish payment authority or validate the offered terms.
struct TaskCarrierAcceptedScopeView {
 const TaskCarrierAgreement* agreement=nullptr;
 const TaskCarrierReportTerms* report_delivery=nullptr;
};
Outcome<TaskCarrierAcceptedScopeView> read_original_task_carrier_acceptance(const CommonStore&,const TaskState&,const SourceRef&,Integer source_schema);
struct AcceptTaskCarrierAgreement {SourceRef original_terms_source;};
struct TaskCarrierAcceptanceQuote {WorldIdentity identity;TaskCarrierTerms terms;AuthoredProviderSourceProof consent;std::optional<TaskCarrierReportTerms> report_delivery;bool funding_finalization_required=false;};
struct TaskCarrierAcceptedFunding {
 TransportExpenseAuthority expense_authority;std::vector<SourceRef> payment_source_refs;
};
class TaskCarrierFundingAcceptanceWrite {
public:virtual ~TaskCarrierFundingAcceptanceWrite()=default;
 virtual bool bound_to(const CommonStore&,const TaskState&)const noexcept=0;
 virtual Status validate_prospective_acceptance(const TaskCarrierAcceptanceQuote&,const WorldMoment&)const=0;
 // The Task source and report scope exist now, but its funding binding is still
 // private to this same outer candidate. Never use that scope as payment proof.
 virtual Outcome<TaskCarrierAcceptedFunding> accept_for_transport(const SourceRef&actual_task_acceptance,const WorldMoment&)=0;
 virtual Status validate_finalized_transport(const SourceRef&actual_task_acceptance,const WorldMoment&)const=0;
};
Outcome<TaskCarrierAcceptanceQuote> preview_task_carrier_agreement(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const TaskTransportTermsRead&,const StableId&actor,const AcceptTaskCarrierAgreement&);
Outcome<TaskCarrierAcceptanceQuote> preview_task_report_carrier_agreement(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const TaskTransportTermsRead&,const StableId&actor,const AcceptTaskCarrierAgreement&);
Outcome<SourceRef> accept_task_carrier_agreement_in_candidate(CommonStore&,TaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const TaskTransportTermsRead&,const TaskCarrierAcceptanceQuote&,const WorldMoment&,const CurrentMomentRead&,TaskCarrierFundingAcceptanceWrite* funding=nullptr);
Status stop_task_carrier_agreement_in_candidate(TaskState&,const StableId&,const SourceRef&actual_stop,const TaskTransportTermsRead&);
class NativeTaskTransportRegistrationRead final:public TransportRegistrationBusinessRead {
 const CommonStore&common_;const TaskState&tasks_;const TransportRegistrationState&registrations_;const TaskTransportTermsRead&sources_;const CurrentMomentRead& clock_;
public:
 NativeTaskTransportRegistrationRead(const CommonStore&c,const TaskState&t,const TransportRegistrationState&r,const TaskTransportTermsRead&s,const CurrentMomentRead&clock):common_(c),tasks_(t),registrations_(r),sources_(s),clock_(clock){}
 Outcome<TransportRegistrationAuthority> read_registration_authority(const SourceRef&)const override;
 Status validate_registration_authority(const TransportRegistrationAuthority&)const override;
 Status validate_registered_initiator(const TransportRegistrationSource&,const TransportUnitState&,const GroupInitiator&,const SubjectRef&,const WorldMoment&)const override;
 Outcome<std::vector<StableId>> read_releasable_labor_allocations(const StableId&,const SourceRef&)const override;
 Status validate_transport_handover(const TransportUnitReleaseSource&,const TransportUnitState&)const override;
};
Outcome<WorldMoment> read_task_carrier_agreement_moment(const CommonStore&,const TaskState&,const SourceRef&,Integer schema);
Status validate_task_transport_agreements(const CommonStore&,const TaskState&,const TaskTransportTermsRead&,const CurrentMomentRead&);
Outcome<TaskTransportAgreementState> decode_task_transport_agreements(const BoundaryValue&,Integer selected_version=1);
Outcome<BoundaryValue> encode_task_transport_agreements(const TaskTransportAgreementState&);
Status visit_task_transport_agreements_checkpoint(CodecVisitor&,TaskTransportAgreementState&);
class ProjectionWriter;
Outcome<bool> emit_task_report_carriage_fact_payload_fields(ProjectionWriter&,const TaskState&,const FactRecord&);
Status emit_task_transport_agreements_projection(ProjectionWriter&,std::string_view,const TaskTransportAgreementState&);
}
