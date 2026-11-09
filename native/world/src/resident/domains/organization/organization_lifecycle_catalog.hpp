// B1.1 作者起点的唯一目录入口。实际资源/权利可安装性仍由原source reader核验。
#pragma once
#include "organization_lifecycle_contract.hpp"
#include <string_view>
namespace hanmo::resident {
class ProjectionWriter;
Status validate_organization_lifecycle_catalog(const OrganizationLifecycleCatalog&,const OrganizationCatalog&);
Outcome<OrganizationLifecycleCatalog> decode_organization_lifecycle_catalog(
 const BoundaryValue&,const CatalogIdentity&,const OrganizationCatalog&);
// 仅作者边界允许省略自引用摘要；锁定后填入真实目录身份，冷存读不走此口。
Outcome<OrganizationLifecycleCatalog> decode_authored_organization_lifecycle_catalog(
 const BoundaryValue&,const CatalogIdentity&,const OrganizationCatalog&);
Outcome<BoundaryValue> encode_organization_lifecycle_catalog(
 const OrganizationLifecycleCatalog&,const OrganizationCatalog&);
Status emit_organization_lifecycle_catalog_projection(ProjectionWriter&,std::string_view,
 const OrganizationLifecycleCatalog&,const OrganizationCatalog&);
// 二进制目录使用同一字段表；identity从受信外层注入，绝不由文件自授。
Status visit_organization_lifecycle_catalog(CodecVisitor&,OrganizationLifecycleCatalog&);
}
