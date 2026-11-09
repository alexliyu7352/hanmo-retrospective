// 原柜台有限本人办理页，仅提交原权源已报价命令，不从展示改Role或任职。
#pragma once
#include "political_office_reception_business.hpp"
namespace hanmo::resident {
BoundaryValue encode_political_office_reception_quote(const PoliticalOfficeReceptionQuote&);
Outcome<BoundaryValue> query_political_office_receptions(const PoliticalOfficeReceptionBusinessContext&,const StableId& place,Integer cursor=0);
}
