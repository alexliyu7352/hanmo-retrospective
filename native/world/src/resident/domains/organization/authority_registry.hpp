// M-ORG唯一持久权源槽。业务consumer仅存引用；不把相同Basis镜像进预算或通行结果。
#pragma once
#include "authority_basis.hpp"
namespace hanmo::resident {
// 同一次实际签发源为唯一键；schema明确固定，不由是否存在字段猜版本。
struct AuthorityBasisRef {SourceRef issued_source;Integer source_schema=1;};
using AuthorityBasisRegistry=OrderedTable<AuthorityBasis>;
// OrganizationRuntime持有一个AuthorityBasisRegistry authority_bases。
// 仅原签发成功的同candidate调用：真实builder验证后直接写唯一registry。
Outcome<AuthorityBasisRef> register_authority_basis_for_fact(CommonStore&,AuthorityBasisRegistry&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const SingleGrantAuthorityRequest&,const SingleGrantAuthorityRead&,const SourceRef&,const AuthorityIssueSourceRead&,const std::optional<BudgetAuthorizationRef>& = {},const AuthorityBudgetRead* =nullptr);
// 返回借用值，不复制资源列表。冷读仍向真实签发owner反验历史。
Outcome<const AuthorityBasis*> read_registered_authority_basis(const CommonStore&,const AuthorityBasisRegistry&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const AuthorityBasisRef&,const AuthorityIssueSourceRead&,const AuthorityBudgetRead* =nullptr);
Status validate_authority_basis_registry(const CommonStore&,const AuthorityBasisRegistry&,const OrganizationState&,const OrganizationCatalog&,const CatalogIdentity&,const AuthorityIssueSourceRead&,const AuthorityBudgetRead* =nullptr);
Status visit_authority_basis_ref(CodecVisitor&,AuthorityBasisRef&);
}
