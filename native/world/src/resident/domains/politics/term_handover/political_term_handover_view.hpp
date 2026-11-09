// 只在当前首府生成有限办理候选，私人offer仅双方已参与者可读，不枚举远方人物或别人的意愿。
#pragma once
#include "political_term_handover_business.hpp"
namespace hanmo::resident {
BoundaryValue encode_political_term_handover_quote(const PoliticalTermHandoverQuote&);
Outcome<BoundaryValue> query_political_term_handovers(const PoliticalTermHandoverBusinessContext&,const StableId& place,Integer cursor=0);
}
