#include <string_view>
// P3.2唯一持久权源；只解释实际签发，不携带ReadStamp或资源版本形式的永续写权。
#pragma once
#include "office_authority.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct MomentOwnerSource;

struct BudgetAuthorizationRef {
  StableId authorization_id;
  Integer authorization_revision = 0;
  SourceRef approval_source;
};
struct ParentGrantRef {
  StableId grant_id;
  Integer grant_revision = 0;
  GrantAuthoritySource authority_source;
};
struct AuthorityBasis {
  WorldIdentity source_identity;
  StableId actor_id, organization_id, office_id;
  Integer office_epoch = 0;
  StableId grant_id;
  Integer grant_revision = 0;
  std::optional<StableId> delegation_id;
  std::optional<ParentGrantRef> parent_grant_ref;
  StableId command_kind;
  ScopeRef scope;
  std::vector<ResourceKey> resources;
  Integer amount = 0, amount_limit = 0;
  std::vector<AuthorityAmountComponent> amount_components;
  std::optional<BudgetAuthorizationRef> budget_ref;
  Integer valid_from = 0;
  std::optional<Integer> valid_until;
  StableId continuity_policy_id;
  Integer issued_tick = 0, issued_fact_sequence = 0;
  ActorFoundationAt issued_at;
  SourceRef issued_source;
  OfficeAuthoritySource office_source;
  GrantAuthoritySource grant_source;
};
struct AuthorityIssueSourceView {
  SourceRef source;
  StableId actor_id, command_kind;
  ActorFoundationAt occurred_at;
  std::vector<AuthorityAmountComponent> amount_components;
};
class AuthorityIssueSourceRead : public OrganizationAuthorityClock {
public:
  virtual ~AuthorityIssueSourceRead() = default;
  virtual Outcome<AuthorityIssueSourceView>
  read_authority_issue_source(const SourceRef &) const = 0;
};
// S6原批准owner提供真预算；此口不能从Grant/钱包余额发明公共预算。
class AuthorityBudgetRead {
public:
  virtual ~AuthorityBudgetRead() = default;
  virtual Status
  validate_budget_authorization(const BudgetAuthorizationRef &,
                                const AuthorityBasis &) const = 0;
};
// 原业务事实已实际写入后，核与pre-apply真单Grant读值相同的历史，才形成持久Basis。
Outcome<AuthorityBasis> make_authority_basis_for_fact(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const CatalogIdentity &, const SingleGrantAuthorityRequest &,
    const SingleGrantAuthorityRead &, const SourceRef &,
    const AuthorityIssueSourceRead &,
    const std::optional<BudgetAuthorizationRef> & = {},
    const AuthorityBudgetRead * = nullptr);
Status validate_historical_authority_basis(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const CatalogIdentity &, const AuthorityBasis &,
    const AuthorityIssueSourceRead &, const AuthorityBudgetRead * = nullptr);
// Borrowed from the original catalog or original historical GrantChangePayload.
// Initial authored roots have runtime=null; no synthetic runtime row is made.
struct HistoricalAuthorityGrantRead {
  const Grant *terms = nullptr;
  const GrantRuntimeRecord *runtime = nullptr;
  const GrantAuthoritySource *authority_source = nullptr;
};
Outcome<HistoricalAuthorityGrantRead> read_historical_authority_grant(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const CatalogIdentity &, const AuthorityBasis &,
    const AuthorityIssueSourceRead &, const AuthorityBudgetRead * = nullptr);
// Re-read one Proof /2 at its actual original business fact boundary.
// Returned terms come from the original authored or GrantChange payload.
Outcome<Grant>
read_historical_authority_proof(const CommonStore &, const OrganizationState &,
                                const OrganizationCatalog &,
                                const AuthorityProof &, const SourceRef &,
                                const OrganizationAuthorityClock &);
// 明示条约使者实际签署/2采用边界，不是任意M权限getter。
// 只反读原Grant与原Office；不是完整使用授权。实际command/scope/resources/caps、
// 机构/个人连续性与使者生命/受领仍由原业务owner核，不把借读成功当准入。
struct ActualTreatyUseGrantRead {
  const Grant *terms = nullptr;
  const GrantRuntimeRecord *runtime = nullptr;
  const OrganizationFact *grant_change = nullptr;
  const OfficeState *authored_office = nullptr;
  const OrganizationFact *office_change = nullptr;
  const StableId *holder_actor_id = nullptr;
  Integer office_epoch = 0;
  OfficeStatus office_status = OfficeStatus::vacant;
};
struct FactRecord;
// 原业务owner只回不可变实际签署行的借用，不回完整Basis/Grant验证，防采用环。
struct TreatyEnvoyActualUseSourceView {
  const FactRecord *actual_head = nullptr;
  const MomentOwnerSource *actual_mandate = nullptr;
  const StableId *envoy_actor_id = nullptr;
  const WorldMoment *signed_at = nullptr;
};
class TreatyEnvoyActualUseSourceRead : public OrganizationAuthorityClock {
public:
  virtual bool bound_to(const CommonStore &,
                        const OrganizationState &) const noexcept = 0;
  virtual Outcome<TreatyEnvoyActualUseSourceView>
  read_actual_envoy_signature2(const MomentOwnerSource &) const = 0;
};
Outcome<ActualTreatyUseGrantRead>
read_authority_grant_before_treaty_envoy_signature2(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const CatalogIdentity &, const AuthorityBasis &actual_mandate_issue,
    const AuthorityIssueSourceRead &mandate_issue_sources,
    const MomentOwnerSource &actual_signature, const WorldMoment &signed_at,
    const TreatyEnvoyActualUseSourceRead &signature_raw_sources,
    const AuthorityBudgetRead * = nullptr);
Status visit_authority_basis(CodecVisitor &, AuthorityBasis &);
Outcome<BoundaryValue> encode_authority_basis(const AuthorityBasis &);
Outcome<AuthorityBasis> decode_authority_basis(const BoundaryValue &);
Status emit_authority_basis_projection(ProjectionWriter &, std::string_view,
                                       const AuthorityBasis &);
// 许可2只借实际四类司法处分，不把裸tick或任意业务头当权源。
using ActualShopLicensingGrantRead = ActualTreatyUseGrantRead;
struct ActualShopLicensingAuthoritySourceView {
  const FactRecord *actual_head = nullptr;
  const ActingAuthority *actor = nullptr;
  const StableId *grant_id = nullptr;
  const WorldMoment *occurred_at = nullptr;
};
class ActualShopLicensingAuthoritySourceRead
    : public OrganizationAuthorityClock {
public:
  virtual bool bound_to(const CommonStore &,
                        const OrganizationState &) const noexcept = 0;
  virtual Outcome<ActualShopLicensingAuthoritySourceView>
  read_actual_shop_authority2(const MomentOwnerSource &) const = 0;
};
Outcome<ActualShopLicensingGrantRead>
read_authority_grant_before_shop_licensing2(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const CatalogIdentity &, const MomentOwnerSource &,
    const ActualShopLicensingAuthoritySourceRead &);
} // namespace hanmo::resident
