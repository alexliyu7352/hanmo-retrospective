// 只核不可变目录之间的真实责任关联；资源可用性和历史连续性仍由原受领/结算规则裁定。
#pragma once
#include "public_relief.hpp"
namespace hanmo::resident {
struct PublicReliefCatalog;struct PoliticsCatalog;struct PoliticsState;
struct PoliticsGovernanceCatalog;struct PoliticalGovernanceCommissionCommand;
// 初始化与完整候选验证用作者辖区，不用当前换属状态否定已受领历史。
Status validate_public_relief_catalog_links(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PoliticsCatalog&,const PoliticsGovernanceCatalog&);
// 签发预览须另调用S6实际公职/Grant资格；这里只核本次辖区与固定分配计划相容。
Status validate_public_relief_commission_plan(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PoliticsState&,const PoliticsGovernanceCatalog&,const PoliticalGovernanceCommissionCommand&);
}
