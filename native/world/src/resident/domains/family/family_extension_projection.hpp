// Original Family extension JSON fields, shared by export and no-tree shape.
#pragma once
#include "family_projection.hpp"
#include "family_source_projection.hpp"
namespace hanmo::resident {
struct FamilyDeathState;struct FamilyDeathRecord;
struct FamilyBequestState;struct FamilyBequestDeclaration;struct FamilyBequestRevocation;struct FamilyBequestDeclarationSource;
struct DiscipleControlState;struct BiologicalControlState;struct CollateralControlState;
Status emit_family_death_fields(ProjectionWriter&,const FamilyDeathRecord&);
Status emit_family_deaths_fields(ProjectionWriter&,const FamilyDeathState&);
Status emit_family_bequest_source_fields(ProjectionWriter&,const FamilyBequestDeclarationSource&,Integer selected_version=1);
Status emit_family_bequest_declaration_fields(ProjectionWriter&,const FamilyBequestDeclaration&,Integer selected_version=1);
Status emit_family_bequest_revocation_fields(ProjectionWriter&,const FamilyBequestRevocation&);
Status emit_family_bequests_fields(ProjectionWriter&,const FamilyBequestState&,Integer selected_version=1);
Status emit_disciple_control_state_fields(ProjectionWriter&,const DiscipleControlState&);
Status emit_biological_control_state_fields(ProjectionWriter&,const BiologicalControlState&);
Status emit_collateral_control_state_fields(ProjectionWriter&,const CollateralControlState&);
struct MarriageStatementSource;
Status emit_marriage_statement_source_fields(ProjectionWriter&,const MarriageStatementSource&);
struct BequestDisclosureSource;
Status emit_bequest_disclosure_source_fields(ProjectionWriter&,const BequestDisclosureSource&,Integer selected_version=1);
struct BirthArrangementSource;
Status emit_birth_arrangement_source_fields(ProjectionWriter&,const BirthArrangementSource&,Integer selected_version=1);
struct BirthCareTerms;
Status emit_birth_care_terms_fields(ProjectionWriter&,const BirthCareTerms&);
struct ChildCareReliefSource;
Status emit_child_care_relief_source_fields(ProjectionWriter&,const ChildCareReliefSource&);
struct ChildCareReliefTerms;
Status emit_child_care_relief_terms_fields(ProjectionWriter&,const ChildCareReliefTerms&);
struct InnateParentSource;
Status emit_innate_parent_source_fields(ProjectionWriter&,const InnateParentSource&);
struct BirthInnateResult;
Status emit_birth_innate_result_fields(ProjectionWriter&,const BirthInnateResult&);
struct BirthProposal;
Status emit_birth_proposal_fields(ProjectionWriter&,const BirthProposal&,Integer selected_version=1);
struct BirthReply;
Status emit_birth_reply_fields(ProjectionWriter&,const BirthReply&,Integer selected_version=1);
struct BirthClosure;
Status emit_birth_closure_fields(ProjectionWriter&,const BirthClosure&);
struct FamilyBirthRecord;
Status emit_family_birth_record_fields(ProjectionWriter&,const FamilyBirthRecord&,Integer selected_version=1);
struct ChildMaturationRecord;
Status emit_child_maturation_fields(ProjectionWriter&,const ChildMaturationRecord&);
struct ChildCareReceipt;
Status emit_child_care_receipt_fields(ProjectionWriter&,const ChildCareReceipt&);
struct ChildCareStart;
Status emit_child_care_start_fields(ProjectionWriter&,const ChildCareStart&);
struct FamilyBirthState;
Status emit_family_birth_fields(ProjectionWriter&,const FamilyBirthState&,Integer selected_version=1);
struct ChildCareReliefAgreement;
Status emit_child_care_relief_agreement_fields(ProjectionWriter&,const ChildCareReliefAgreement&);
struct ChildCareReliefStart;
Status emit_child_care_relief_start_fields(ProjectionWriter&,const ChildCareReliefStart&);
struct ChildCareReliefReceipt;
Status emit_child_care_relief_receipt_fields(ProjectionWriter&,const ChildCareReliefReceipt&);
struct ChildCareReliefClosure;
Status emit_child_care_relief_closure_fields(ProjectionWriter&,const ChildCareReliefClosure&);
struct ChildCareReliefState;
Status emit_child_care_relief_fields(ProjectionWriter&,const ChildCareReliefState&);
struct MarriageProposal;
Status emit_marriage_proposal_fields(ProjectionWriter&,const MarriageProposal&);
struct MarriageReply;
Status emit_marriage_reply_fields(ProjectionWriter&,const MarriageReply&);
struct MarriageDissolution;
Status emit_marriage_dissolution_fields(ProjectionWriter&,const MarriageDissolution&);
struct FamilyMarriageState;
Status emit_family_marriage_fields(ProjectionWriter&,const FamilyMarriageState&);
struct FamilyBequestDisclosure;
Status emit_family_bequest_disclosure_fields(ProjectionWriter&,const FamilyBequestDisclosure&,Integer selected_version=1);
struct FamilyInheritanceAcceptance;
Status emit_family_inheritance_acceptance_fields(ProjectionWriter&,const FamilyInheritanceAcceptance&,Integer selected_version=1);
struct FamilyInheritanceSettlement;
Status emit_family_inheritance_settlement_fields(ProjectionWriter&,const FamilyInheritanceSettlement&,Integer selected_version=1);
struct FamilyInheritanceState;
Status emit_family_inheritance_fields(ProjectionWriter&,const FamilyInheritanceState&,Integer selected_version=1);
struct LegacyControlDecisionSource;struct FamilyLegacyState;struct FamilyLegacyConsentFact;struct FamilyLegacyRevocationFact;struct FamilyLegacyActivationFact;
// Explicit existing family_legacy physical selector 1, 2 or 3; never guessed from optionals.
Status emit_legacy_control_decision_source_fields(ProjectionWriter&,const LegacyControlDecisionSource&,Integer selected_version);
Status emit_family_legacy_consent_fields(ProjectionWriter&,const FamilyLegacyConsentFact&,Integer selected_version);
Status emit_family_legacy_revocation_fields(ProjectionWriter&,const FamilyLegacyRevocationFact&,Integer selected_version);
Status emit_family_legacy_activation_fields(ProjectionWriter&,const FamilyLegacyActivationFact&,Integer selected_version);
Status emit_family_legacy_fields(ProjectionWriter&,const FamilyLegacyState&,Integer selected_version);
namespace family_projection {
template<class T>Status control_commit_fields(ProjectionWriter& w,const T& b){w.field("result_id",b.result_id);w.field("actor_id",b.actor_id);w.field("kind",b.kind);w.field("request_seq",b.request_seq);w.field("revision",b.revision);return w.order_fields(b.field_order);}
}
}
