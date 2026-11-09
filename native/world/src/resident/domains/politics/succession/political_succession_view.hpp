// 有限办理页只展示当前首府所见候选、本人亲历意愿与本人实际收到的原死亡文书。
#pragma once
#include "political_succession_business.hpp"
namespace hanmo::resident {
using PoliticalSuccessionKnownNoticeBindings=std::function<Outcome<std::vector<StableId>>(const StableId& observer)>;
struct PoliticalSuccessionViewContext {const PoliticalSuccessionContext& business;PoliticalSuccessionKnownNoticeBindings known_notice_bindings;};
BoundaryValue encode_political_succession_quote(const PoliticalSuccessionQuote&);
Outcome<BoundaryValue> query_political_successions(const PoliticalSuccessionViewContext&,const StableId& place,Integer cursor=0);
}
