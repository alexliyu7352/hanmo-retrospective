// 本人拜帖页只给亲自托留/已领读历史，当前接待人当面才可透露待领取的本人拜帖。
#pragma once
#include "visit_message.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_visit_messages(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,Integer cursor=0,Integer offer_cursor=0,const VisitMessageLifeContext* life=nullptr);
}
