// 辞任薄业务只在同候选原子处理三命令；所有失败由真实外层Common/Organization/本账保存点回滚。
#pragma once
#include "political_office_resignation_receipts.hpp"
namespace hanmo::resident {
Outcome<PoliticsEffects> apply_political_office_resignation_business_in_candidate(CommonStore&,OrganizationState&,PoliticalOfficeResignationState&,const PoliticalOfficeResignationContext&,const StableId&,const PoliticalOfficeResignationCommand&,const PoliticalOfficeResignationQuote&);
BoundaryValue encode_political_office_resignation_quote(const PoliticalOfficeResignationQuote&);
Outcome<BoundaryValue> query_political_office_resignations(const PoliticalOfficeResignationContext&,const StableId& place_id,Integer cursor=0);
}
