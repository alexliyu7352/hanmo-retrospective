// 本人回信页只列亲自决定/真实实领，未领内容不在预览或远方列表泄露。
#pragma once
#include "visit_reply_read.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_visit_replies(const VisitReplyReadContext&,const InteractionState&,const InteractionAccess&,Integer cursor=0);
BoundaryValue encode_visit_reply_quote(const VisitReplyQuote&);
}
