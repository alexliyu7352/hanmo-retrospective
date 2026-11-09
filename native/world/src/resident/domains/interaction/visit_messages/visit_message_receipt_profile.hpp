// 新独立实际结果段必须显式选择并配原托领撤业务；未选择时旧字节与准入保留。
#pragma once
#include "visit_message_catalog.hpp"
#include "visit_message_receipt_state.hpp"
namespace hanmo::resident {
Outcome<bool> decode_visit_message_receipt_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
// 只读身份选择；完整恢复另将实际独立段与该结果等值核对。
inline Outcome<Integer> visit_message_receipt_selected_version(const std::vector<std::pair<StableId,StableId>>&modules){
 Integer selected=0,original=0;bool seen=false,seen_original=false;for(const auto&row:modules){
  if(row.first==visit_message_receipt_module){if(seen)return {false,{},common_failure("DuplicateVisitMessageReceiptModule").error};seen=true;if(row.second!="1"&&row.second!="2")return {false,{},common_failure("UnsupportedVisitMessageReceiptVersion").error};selected=row.second=="1"?1:2;}
  if(row.first=="visit_messages"){if(seen_original)return {false,{},common_failure("DuplicateVisitMessageOriginalModule").error};seen_original=true;if(row.second!="1"&&row.second!="2"&&row.second!="3")return {false,{},common_failure("VisitMessageReceiptOriginalRequired").error};original=row.second=="1"?1:row.second=="2"?2:3;}
 }
 if(selected&&!original)return {false,{},common_failure("VisitMessageReceiptOriginalRequired").error};return {true,selected,{}};
}
Outcome<Integer> decode_visit_message_receipt_profile_version(const BoundaryValue&,const std::vector<StableId>&);
Outcome<VisitMessageReceiptState> make_initial_visit_message_receipt_state(const CatalogIdentity&,const VisitMessageCatalog&);
// 这里确有独立持久段，区别于纯目录marker；所有入口按相同版本/原业务依赖判定。
Status validate_visit_message_receipt_layout(const std::vector<std::pair<StableId,StableId>>& modules,
    bool selected,bool original_message_catalog,const VisitMessageReceiptState* state);
}
