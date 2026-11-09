// 公职任期与单Grant窄读口；当前权、历史签发、本人同意和私人Owner各有独立来源。
#pragma once
#include <span>
#include "organization_model.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
// 唯一受信世界读钟：phase取WorldBundle实际阶段M，冷读取已发布M；请求/持久Basis不可提供当前时刻。
class OrganizationAuthorityClock {public:virtual ~OrganizationAuthorityClock()=default;virtual Outcome<WorldMoment> read_organization_authority_now()const=0;};
Outcome<WorldMoment> read_organization_authority_now(const CommonStore&,const OrganizationAuthorityClock* =nullptr);
Outcome<bool> authority_window_contains(Integer valid_from,const std::optional<Integer>& valid_until,const WorldMoment&);
struct OfficeTermRead {
  WorldIdentity source_identity;
  StableId organization_id, office_id;
  Integer epoch = 0;
  std::optional<StableId> holder_actor_id;
  OfficeStatus status = OfficeStatus::vacant;
  OfficeAuthoritySource source;
  ActorFoundationAt effective_at;
};
// S6/C2量纲分量；每项只核同一个原Grant标量limit，不假称原作者配置了分单位限额。
struct MoneyAuthorityAmount {StableId currency_id;std::vector<StableId> wallet_ids;Integer amount=0;};
struct ItemAuthorityAmount {StableId item_id;std::vector<StableId> inventory_ids;Integer amount=0;};
using AuthorityAmountComponent=std::variant<MoneyAuthorityAmount,ItemAuthorityAmount>;
Status validate_authority_amount_components(const CommonStore&,const std::vector<AuthorityAmountComponent>&,const std::vector<ResourceKey>&,Integer grant_amount_limit);
bool same_authority_amount_components(const std::vector<AuthorityAmountComponent>&,const std::vector<AuthorityAmountComponent>&);
// 所有请求资源必须同时由一份grant覆盖；无余额/预算/私人资产委托含义。
struct SingleGrantAuthorityRequest {ActingAuthority acting;StableId command_kind;ScopeRef scope;Integer amount=0;std::vector<ResourceKey> resources;bool requires_delegation=false;std::vector<AuthorityAmountComponent> amount_components;};
struct SingleGrantAuthorityRead {WorldIdentity source_identity;AuthorityProof authority;OfficeAuthoritySource office_source;GrantAuthoritySource grant_source;Grant effective_grant;Integer grant_revision=0;std::vector<AuthorityAmountComponent> amount_components;};
Outcome<OfficeTermRead> read_current_office_term(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const StableId& office_id,const OrganizationAuthorityClock* =nullptr);
// boundary必须是调用方实际业务fact；同tick依据Common事实顺序，绝不按今日holder重解。
Outcome<OfficeTermRead> read_office_term_before_fact(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const StableId& office_id,Integer tick,const StableId& before_fact_id,const OrganizationAuthorityClock* =nullptr);
Outcome<OfficeTermRead> read_office_term_before_fact_at(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const StableId& office_id,const WorldMoment& actual_boundary,const StableId& before_fact_id,const OrganizationAuthorityClock&);
// 原权限检查只读选定Grant，不复制整份Catalog；调用方须已核同一Grant真实revision。
Outcome<AuthorityProof> check_authority_with_single_grant(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const ActingAuthority&,const StableId& command_kind,const ScopeRef&,Integer amount,const Grant&,const OrganizationAuthorityClock* =nullptr);
// Exact request proof comparison vs persistent source binding comparison.
bool same_authority_proof(const AuthorityProof &, const AuthorityProof &);
bool same_authority_grant_binding(const AuthorityProof &,
                                  const AuthorityProof &);
Outcome<SingleGrantAuthorityRead> check_current_grant_authority(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const SingleGrantAuthorityRequest&,const OrganizationAuthorityClock* =nullptr);
// One original Grant must satisfy the primary and every owner-derived term.
// The result remains the primary proof; no merged Grant or persistent sub-basis.
Outcome<SingleGrantAuthorityRead> check_current_grant_authority_requirements(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const SingleGrantAuthorityRequest& primary,std::span<const SingleGrantAuthorityRequest> additional,const OrganizationAuthorityClock* =nullptr);
Status validate_current_grant_authority_requirements(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const SingleGrantAuthorityRequest& primary,std::span<const SingleGrantAuthorityRequest> additional,const SingleGrantAuthorityRead&,const OrganizationAuthorityClock* =nullptr);
Status validate_single_grant_authority(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const SingleGrantAuthorityRequest&,const SingleGrantAuthorityRead&,const OrganizationAuthorityClock* =nullptr);
}
