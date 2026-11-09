// P15.3/21.1唯一五term条款。条款不是生效、派兵、供给、知情或退出已发生证明。
#pragma once
#include "political_passage_decision.hpp"
#include "resident/domains/organization/authority_basis.hpp"
namespace hanmo::resident {
enum class SafeExitManifestPolicy {entered_survivors_and_original_cargo,entered_survivors_and_lawful_trade_cargo};
enum class SafeExitAction {exit_transit,necessary_wait,resupply_for_exit,self_defense};
enum class SafeExitBlockedFallback {hold_and_request_permission};
struct SafeExitPath {
 StableId path_id;ScopeRef inside_start_scope;
 std::vector<PassageBoundaryRef> directed_boundary_refs;StableId outside_end_site;
};
struct SafeExitTerms {
 Integer grace_ticks=0,max_exit_half_days=0;std::vector<SafeExitPath> paths;
 SafeExitManifestPolicy eligible_manifest_policy=SafeExitManifestPolicy::entered_survivors_and_original_cargo;
 bool include_birth_dependents=false;std::vector<SafeExitAction> allowed_actions;
 ActorFoundationRuleRef tariff_rule_ref;
 SafeExitBlockedFallback blocked_fallback=SafeExitBlockedFallback::hold_and_request_permission;
};
enum class TreatyTermKind {truce,passage,mutual_defense,material_aid,submission};
enum class TreatyProhibitedAction {attack,siege,raid,naval_attack,seize_public_assets};
struct TreatyOrganizationPair {StableId first,second;};
struct TreatyTruceTerm {
 std::vector<TreatyOrganizationPair> organization_pairs;std::vector<StableId> jurisdiction_ids;
 std::vector<PassageBoundaryRef> boundary_refs;std::vector<TreatyProhibitedAction> prohibited_actions;
 Integer notice_ticks=0;ActorFoundationRuleRef withdrawal_route_policy;
};
enum class TreatyPassageMode {foot,army,boat};
enum class TreatyCredentialPolicy {official_notice,present_document};
struct TreatyPassageTerm {
 std::vector<StableId> eligible_parties;std::vector<TreatyPassageMode> allowed_modes;
 std::vector<PassageBoundaryRef> boundary_refs;PassageWindow window;
 TreatyCredentialPolicy credential_policy=TreatyCredentialPolicy::official_notice;
 // 明示null表示本条款没有收费；非空仍须原收费源和实际payee权限。
 std::optional<PassageTariff> tariff;SafeExitTerms safe_exit;
};
enum class TreatyDefenseTrigger {external_armed_attack};
struct TreatyMutualDefenseTerm {
 std::vector<StableId> protected_parties,covered_jurisdictions;
 TreatyDefenseTrigger trigger_kind=TreatyDefenseTrigger::external_armed_attack;
 std::vector<StableId> eligible_force_pool_ids;
 Integer max_dispatched_persons=0,max_concurrent_calls=0,dispatch_within_ticks=0,max_service_ticks=0;
 std::vector<SourceRef> supply_contract_refs;BudgetAuthorizationRef budget_ref;
 ActorFoundationRuleRef return_policy;
};
struct TreatyAidItemAmount {StableId item_id;Integer quantity=0;};
struct TreatyAidMoney {StableId currency_id;Integer amount=0;};
enum class TreatyAidScheduleKind {once,periodic};
enum class TreatyAidEstablishmentGate {at_due_tick};
struct TreatyRecordBreach {};
struct TreatyBoundedCompensation {StableId currency_id;Integer maximum_amount=0;StableId obligation_owner;};
using TreatyAidLatePolicy=std::variant<TreatyRecordBreach,TreatyBoundedCompensation>;
struct TreatyMaterialAidTerm {
 StableId source_party;SubjectRef beneficiary;SourceRef delivery_site;
 std::vector<TreatyAidItemAmount> items;std::optional<TreatyAidMoney> money;
 TreatyAidScheduleKind schedule=TreatyAidScheduleKind::once;std::optional<Integer> period_ticks;
 Integer occurrences=0,first_due_tick=0,delivery_deadline_ticks=0;
 SubjectRef transport_cost_payer;BudgetAuthorizationRef budget_ref;TreatyAidLatePolicy late_policy;
 TreatyAidEstablishmentGate establishment_gate=TreatyAidEstablishmentGate::at_due_tick;
};
enum class TreatySharedPower {appoint_local_head,set_tax_ceiling,approve_budget,issue_strategic_order,admit_passage};
enum class TreatySubmissionExit {mutual_release,notice_with_settlement};
struct TreatySubmissionTerm {
 StableId subordinate_org,superior_org;std::vector<StableId> jurisdiction_ids;
 std::vector<TreatySharedPower> shared_powers;StableId retained_policy_id;
 std::vector<SourceRef> command_bindings;std::vector<StableId> tribute_term_ids;
 TreatySubmissionExit exit_policy=TreatySubmissionExit::mutual_release;Integer notice_ticks=0;
};
using TreatyTermPayload=std::variant<TreatyTruceTerm,TreatyPassageTerm,TreatyMutualDefenseTerm,TreatyMaterialAidTerm,TreatySubmissionTerm>;
struct TreatyTerm {StableId term_id;TreatyTermPayload terms;};
enum class TreatyRatificationPolicy {all_parties};
enum class TreatyTerminationMethod {by_expiry,mutual_release,material_breach_after_notice};
struct TreatyTermsV1 {
 StableId treaty_kind;std::vector<StableId> parties;std::vector<TreatyTerm> terms;
 PassageWindow effective_window;TreatyRatificationPolicy ratification_policy=TreatyRatificationPolicy::all_parties;
 std::vector<TreatyTerminationMethod> termination_policy;StableId rule_version;
};
struct TreatyTermsDefinition {StableId id;TreatyTermsV1 terms;};
struct PoliticalTreatyTermsCatalog {CatalogIdentity identity;OrderedTable<TreatyTermsDefinition> definitions;};
// 纯shape门不假定外部引用存在。完整作者门还须走真实原拓扑/组织/预算/力池reader。
Status validate_safe_exit_terms_shape(const SafeExitTerms&);
Status validate_treaty_terms_shape(const TreatyTermsV1&);
class TreatyTermsOriginalRead {public:virtual ~TreatyTermsOriginalRead()=default;
 virtual Status validate_treaty_rule(const StableId& treaty_kind,const StableId& rule_version)const=0;
 virtual Status validate_treaty_party(const StableId&)const=0;
 virtual Status validate_truce_term(const TreatyTruceTerm&)const=0;
 virtual Status validate_passage_term(const TreatyPassageTerm&)const=0;
 virtual Status validate_mutual_defense_term(const TreatyMutualDefenseTerm&)const=0;
 virtual Status validate_material_aid_term(const TreatyMaterialAidTerm&)const=0;
 virtual Status validate_submission_term(const TreatySubmissionTerm&)const=0;
};
Status validate_political_treaty_terms_catalog(const PoliticalTreatyTermsCatalog&,const TreatyTermsOriginalRead&);
// 原Calendar时窗精确谓词；不以floor(M)==until让已经届满的条约继续准入。
Outcome<bool> treaty_window_contains(const PassageWindow&,const WorldMoment&);
Outcome<Integer> material_aid_occurrence_due(const TreatyMaterialAidTerm&,Integer ordinal);
Status visit_safe_exit_terms(CodecVisitor&,SafeExitTerms&);
Status visit_treaty_terms(CodecVisitor&,TreatyTermsV1&);
Status emit_treaty_terms_projection(ProjectionWriter&,std::string_view,const TreatyTermsV1&);
Outcome<BoundaryValue> encode_treaty_terms(const TreatyTermsV1&);
Outcome<TreatyTermsV1> decode_treaty_terms(const BoundaryValue&);
Status emit_political_treaty_terms_catalog_projection(ProjectionWriter&,std::string_view,const PoliticalTreatyTermsCatalog&);
Outcome<BoundaryValue> encode_political_treaty_terms_catalog(const PoliticalTreatyTermsCatalog&);
Outcome<PoliticalTreatyTermsCatalog> decode_political_treaty_terms_catalog(const BoundaryValue&,const CatalogIdentity&,const TreatyTermsOriginalRead&);
}
