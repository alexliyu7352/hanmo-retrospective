// 独立回执严格编解码；原业务visitor、索引与Common结果格式均不增加字段。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "visit_message_receipt_state.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include <set>
namespace hanmo::resident {namespace {
template<class A>Status binding(A& a,VisitMessageResultBinding& v){a.field("fact_id",v.fact_id);a.field("producer",v.producer);a.field("request_actor_id",v.request_actor_id);a.field("request_sequence",v.request_sequence);a.field("result_id",v.result_id);a.field("result_revision",v.result_revision);a.field("result_kind",v.result_kind);a.field("from_tick",v.from_tick);a.field("claim_fact_id",v.claim_fact_id);return a.field("intent_id",v.intent_id);}
template<class A>Status state(A& a,VisitMessageReceiptState& v){a.field("version",v.version);if(!visit_message_receipt_version(v.version))return a.reject("UnsupportedVisitMessageReceiptVersion");return dynamic_codec::table(a,"bindings",v.bindings,[&](auto& child,auto& row){auto r=binding(child,row);if(!r.ok)return r;if(row.producer!="player"&&row.producer!="director"&&(v.version!=2||row.producer!="director_actor"))return child.reject("VisitMessageProducerRequired");return child.status();});}
// typed JSON同样拒递归重复键及超限，不能依赖文本parser代守边界。
Status unique(const BoundaryValue& v,std::size_t depth=0){if(depth>CodecVisitor::max_depth)return common_failure("VisitMessageReceiptTooDeep");if(const auto* fields=v.as<BoundaryValue::Dict>()){if(fields->size()>CodecVisitor::max_items)return common_failure("VisitMessageReceiptTooLarge");std::set<StableId> seen;for(const auto& item:*fields){if(!seen.insert(item.first).second)return common_failure("DuplicateVisitMessageReceiptField",item.first);auto r=unique(item.second,depth+1);if(!r.ok)return r;}}if(const auto* rows=v.as<BoundaryValue::Array>()){if(rows->size()>CodecVisitor::max_items)return common_failure("VisitMessageReceiptTooLarge");for(const auto& row:*rows){auto r=unique(row,depth+1);if(!r.ok)return r;}}return common_success();}
}
Status visit_visit_message_receipts_checkpoint(CodecVisitor& codec,VisitMessageReceiptState& value){return state(codec,value);}
BoundaryValue encode_visit_message_receipts(const VisitMessageReceiptState& value){auto r=dynamic_projection::encode(value,[](auto&a,auto&v){return state(a,v);});if(!r.ok){boundary::DecodeFailure error(r.error.original.code,r.error.original.path);error.error=r.error;throw error;}return std::move(r.value);}
Status emit_visit_message_receipts_projection(ProjectionWriter&w,std::string_view name,const VisitMessageReceiptState&value){return dynamic_projection::emit(w,name,value,[](auto&a,auto&v){return state(a,v);});}
Outcome<VisitMessageReceiptState> decode_visit_message_receipts(const BoundaryValue& value){auto r=unique(value);if(!r.ok)return {false,{},r.error};VisitMessageReceiptState out;dynamic_codec::JsonArchive archive(value);r=state(archive,out);if(r.ok)r=archive.finish();return r.ok?Outcome<VisitMessageReceiptState>{true,std::move(out),{}}:Outcome<VisitMessageReceiptState>{false,{},r.error};}
}
