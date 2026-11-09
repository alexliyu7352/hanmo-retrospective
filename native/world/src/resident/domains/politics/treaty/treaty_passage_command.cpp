// 报价只冻结本次明确命令，提交时重核同一world、同一人和真实会面，不依赖UI传回许可。
#include "treaty_passage_command.hpp"
#include "treaty_passage_detail.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
namespace hanmo::resident {
StableId treaty_passage_command_kind(const TreatyPassageCommand& command){return std::holds_alternative<TreatyPassagePresentCommand>(command)?"treaty_passage_present":"treaty_passage_sign";}
BoundaryValue encode_treaty_passage_command(const TreatyPassageCommand& command){return std::visit([](const auto& c){using T=std::decay_t<decltype(c)>;if constexpr(std::is_same_v<T,TreatyPassagePresentCommand>)return encode_treaty_passage_present(c);else return encode_treaty_passage_sign(c);},command);}
Outcome<TreatyPassageCommand> decode_treaty_passage_command(const StableId& kind,const BoundaryValue& value){if(kind=="treaty_passage_present"){auto result=decode_treaty_passage_present(value);if(!result.ok)return {false,{},result.error};return {true,std::move(result.value),{}};}if(kind=="treaty_passage_sign"){auto result=decode_treaty_passage_sign(value);if(!result.ok)return {false,{},result.error};return {true,std::move(result.value),{}};}return {false,{},common_failure("UnsupportedTreatyPassageCommand",kind,ErrorKind::unsupported_capability).error};}
BoundaryValue encode_treaty_passage_quote(const TreatyPassageQuote& quote){using V=BoundaryValue;StableId world;const char* hex="0123456789abcdef";for(auto byte:quote.identity.world_id){world.push_back(hex[byte>>4]);world.push_back(hex[byte&15]);}return boundary::obj({{"identity",boundary::obj({{"world_id",V{world}},{"execution_epoch",V{quote.identity.execution_epoch}},{"revision",V{quote.identity.revision}},{"tick",V{quote.identity.tick}}})},{"actor_id",V{quote.actor_id}},{"duration",V{quote.duration}},{"command_kind",V{treaty_passage_command_kind(quote.command)}},{"command",encode_treaty_passage_command(quote.command)}});}
Outcome<TreatyPassageQuote> preview_treaty_passage_command(const TreatyPassageContext& c,const StableId& actor,const TreatyPassageCommand& command){auto result=std::visit([&](const auto& typed)->Status{using T=std::decay_t<decltype(typed)>;if constexpr(std::is_same_v<T,TreatyPassagePresentCommand>){auto q=preview_treaty_passage_presentation(c,actor,typed);return q.ok?common_success():Status{false,{},q.error};}else{auto q=preview_treaty_passage_signature(c,actor,typed);return q.ok?common_success():Status{false,{},q.error};}},command);if(!result.ok)return {false,{},result.error};return {true,{c.diplomacy.common.state().identity,actor,command,0},{}};}
Outcome<StableId> apply_treaty_passage_command_in_transaction(CommonStore& common,TreatyPassageState& state,const TreatyPassageContext& c,const StableId& actor,const TreatyPassageCommand& command,const TreatyPassageQuote& quote){
    if(!treaty_passage_detail::same_identity(quote.identity,common.state().identity)||quote.actor_id!=actor||quote.duration!=0||quote.command.index()!=command.index()||!compare_boundary(encode_treaty_passage_command(quote.command),encode_treaty_passage_command(command)).exact())return {false,{},common_failure("StaleTreatyPassageQuote").error};
    return std::visit([&](const auto& typed)->Outcome<StableId>{using T=std::decay_t<decltype(typed)>;if constexpr(std::is_same_v<T,TreatyPassagePresentCommand>){auto q=preview_treaty_passage_presentation(c,actor,typed);if(!q.ok)return {false,{},q.error};return present_treaty_passage_in_transaction(common,state,c,actor,typed,q.value);}else{auto q=preview_treaty_passage_signature(c,actor,typed);if(!q.ok)return {false,{},q.error};return sign_treaty_passage_in_transaction(common,state,c,actor,typed,q.value);}},command);
}
Status validate_treaty_passage_receipt_sources(const TreatyPassageContext& c,const std::function<bool(const CommitResult&)>& is_done){
 const auto& common=c.diplomacy.common.state();std::set<StableId> matched;
 // 缓存键是actor:request_sequence，不是result_id；已按原64项规则淘汰的回执不再要求存在。
 for(const auto& cached:common.results.rows()){
  if(!cached.second||!treaty_passage_command_kind(cached.second->kind))continue;const auto& result=*cached.second;
  const auto split=cached.first.rfind(':');if(split==StableId::npos||!is_done||!is_done(result))return common_failure("InvalidTreatyPassageReceipt",cached.first);
  const auto actor=cached.first.substr(0,split);auto request=organization_instance_sequence(cached.first,actor);const auto* last=common.request_sequences.find(actor);if(!request||!last||*request>*last)return common_failure("InvalidTreatyPassageReceipt",cached.first);
  const HistoryRecord* history=nullptr;const HistoryRecord* lower=nullptr;const HistoryRecord* previous=nullptr;
  for(const auto& entry:common.history){if(entry.result_id==result.result_id){if(history)return common_failure("DuplicateTreatyPassageReceiptHistory",cached.first);history=&entry;lower=previous;}previous=&entry;}
  if(!history||history->kind!=result.kind||history->tick!=result.tick||history->actor_id!=actor)return common_failure("TreatyPassageReceiptHistoryMismatch",cached.first);
  auto upper_sequence=organization_instance_sequence(result.result_id,"result"),lower_sequence=lower?organization_instance_sequence(lower->result_id,"result"):std::optional<Integer>{0};if(!upper_sequence||!lower_sequence||*upper_sequence<=*lower_sequence)return common_failure("InvalidTreatyPassageReceiptOrder",cached.first);
  StableId source;unsigned count=0;const auto take=[&](const auto& record,const char* prefix,const StableId& signer){auto id=organization_instance_sequence(record.id,prefix);if(id&&*id>*lower_sequence&&*id<*upper_sequence&&record.tick==result.tick&&signer==actor){source=record.id;++count;}};
  if(result.kind=="treaty_passage_present")for(const auto& record:c.state.deliveries.rows())take(record.second,"treaty_passage_document",record.second.contact.actor_id);
  else for(const auto& record:c.state.signatures.rows())take(record.second,"treaty_passage_signature",record.second.authority.actor_id);
  if(count!=1||!matched.insert(source).second)return common_failure("TreatyPassageReceiptSourceMismatch",cached.first);
 }
 return common_success();
}

}
