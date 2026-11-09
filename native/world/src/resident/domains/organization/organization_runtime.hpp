// M5 organization_runtime/1完整组合；grants在codec中借原OrganizationState槽，内存中也不复制。
#pragma once
#include "organization_lifecycle_contract.hpp"
#include "office_lifecycle.hpp"
namespace hanmo::resident {
struct OrganizationGrantVersionRef {StableId id,grant_id;Integer revision=0;SourceRef change_source;};
enum class OrganizationRelationKind {administrative_under,branch_of,political_submission,alliance,protection,military_command_binding};
// 关系条目只绑定原决定，当前权力及解除仍反读那个owner的真实来源。
struct OrganizationRelationBinding {StableId id,organization_id,related_organization_id;OrganizationRelationKind kind=OrganizationRelationKind::administrative_under;SourceRef source_ref;};
struct OrganizationRuntime : OrganizationLifecycleState {
 OrderedTable<VerifiedOfficeTransition> office_transition_sources;
 OrderedTable<OrganizationGrantVersionRef> grant_versions;
 AuthorityBasisRegistry authority_bases;
 OrderedTable<OrganizationRelationBinding> relations;
};
// Explicit physical choice from the locked module list, never inferred from fields.
// 4/5 legacy without runtime keeps its original embedded current_grants wire.
struct OrganizationRuntimeCodecSelection {Integer organization_version=0;bool named_membership=false;bool exact_fact_clock=false;Integer service_effects_schema=0;};
Outcome<OrganizationRuntimeCodecSelection> select_organization_runtime_codec(const CatalogIdentity&);
Status validate_organization_runtime_codec_selection(const CatalogIdentity&,const OrganizationCatalog&);
// Original body dispatch selected by that same locked module list. These are
// only delegations to K3's existing body codecs, never another field walker.
Status visit_organization_runtime_selected_body(CodecVisitor&,OrganizationState&,const CatalogIdentity&);
Status emit_organization_runtime_selected_body_projection(ProjectionWriter&,std::string_view,const OrganizationState&,const CatalogIdentity&);
Outcome<BoundaryValue> encode_organization_runtime_selected_body(const OrganizationState&,const CatalogIdentity&);
Outcome<OrganizationState> decode_organization_runtime_selected_body(const BoundaryValue&,const CatalogIdentity&);
class OrganizationRelationSourceRead {public:virtual ~OrganizationRelationSourceRead()=default;virtual Status validate_organization_relation(const OrganizationRelationBinding&,const ReadStamp&)const=0;};
// 只从唯一原grant_change payload派生不可写引用索引；不能生成Grant或重新签发AuthorityBasis。
Status index_original_grant_versions_in_candidate(const CommonStore&,const OrganizationState&,OrganizationRuntime&);
Status validate_organization_runtime_sources(const CommonStore&,const OrganizationState&,const OrganizationRuntime&,const OrganizationCatalog&,const OrganizationLifecycleCatalog&,const CatalogIdentity&,const ReadStamp&,const OrganizationLifecycleSourceRead&,const OrganizationRelationSourceRead&,const OfficeTransitionSourceRead&,const VerifiedDeathRead&,const AuthorityIssueSourceRead&,const AuthorityBudgetRead*,const WorldMoment&);
Status visit_organization_runtime(CodecVisitor&,OrganizationState&,OrganizationRuntime&,bool named_membership=false);
Status emit_organization_runtime_projection(ProjectionWriter&,std::string_view,const OrganizationState&,const OrganizationRuntime&,bool named_membership=false);
Outcome<BoundaryValue> encode_organization_runtime(const OrganizationState&,const OrganizationRuntime&,bool named_membership=false);
// 冷入口只写私有候选；整个body与runtime完成后才允许世界级发布。
Outcome<OrganizationRuntime> decode_organization_runtime(const BoundaryValue&,OrganizationState& private_candidate,bool named_membership=false);
}
