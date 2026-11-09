// Finite original visit leaves. Capture checks source payloads only; adoption,
// command receipts and Life4 effects retain their separate full cold validators.
#pragma once
#include "visit_message_life.hpp"
#include "visit_message_receipts.hpp"
#include "visit_reception_director.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_read.hpp"
#include "resident/kernel/dynamic_result_time2.hpp"
namespace hanmo::resident {
struct VisitOriginalResultSources {
 const CommonStore& common;
 const VisitMessageLifeContext* messages=nullptr;
 const VisitReplyReadContext* replies=nullptr;
 const VisitReceptionRead* reception=nullptr;
 const VisitMessageReceiptState* receipts=nullptr;
 const VisitMessageReceiptRead* receipt_sources=nullptr;
};
bool native_visit_original_result_kind(const FactRecord&);
class NativeVisitOriginalResultRead final:public ExactResultOwnerRead {
 VisitOriginalResultSources x_;
public:explicit NativeVisitOriginalResultRead(VisitOriginalResultSources x):x_(x){}
 Outcome<ExactOwnerFactRead>read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment>read_selected_source_moment(const MomentOwnerSource&)const override;
};
}
