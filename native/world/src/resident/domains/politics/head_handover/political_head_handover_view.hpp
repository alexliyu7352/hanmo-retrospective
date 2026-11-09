// 只在当前首府生成有限办理候选，私人offer仅双方已参与者可读，不枚举远方人物或别人的意愿。
#pragma once
#include "political_head_handover_business.hpp"
namespace hanmo::resident {
BoundaryValue encode_political_head_handover_quote(const PoliticalHeadHandoverQuote&);
Outcome<BoundaryValue> query_political_head_handovers(const PoliticalHeadHandoverBusinessContext&,const StableId& place,Integer cursor=0);
}
