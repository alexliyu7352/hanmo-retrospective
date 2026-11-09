// 有限办理页只展示当前首府所见候选、本人亲历意愿与本人实际收到的原死亡文书。
#pragma once
#include "political_successor_round_business.hpp"
namespace hanmo::resident {
using PoliticalSuccessorRoundKnownNoticeBindings=std::function<Outcome<std::vector<StableId>>(const StableId& observer)>;
struct PoliticalSuccessorRoundViewContext {const PoliticalSuccessorRoundBusinessContext& business;PoliticalSuccessorRoundKnownNoticeBindings known_notice_bindings;};
BoundaryValue encode_political_successor_round_quote(const PoliticalSuccessorRoundQuote&);
Outcome<BoundaryValue> query_political_successor_rounds(const PoliticalSuccessorRoundViewContext&,const StableId& place,Integer cursor=0);
}
