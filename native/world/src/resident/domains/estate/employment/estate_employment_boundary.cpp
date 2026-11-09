// 原预览与提交使用同三类typed命令；所有资金和工资条款只能从实际政策重读。
#include "estate_employment_boundary.hpp"
#include <charconv>
#include "estate_employment_codec.hpp"
#include "../../local_economy/economic_payload_guard.hpp"
namespace hanmo::resident {
Outcome<EstateEmploymentCommand> decode_estate_employment_command(const StableId& kind,const BoundaryValue& params){try{using namespace boundary;
 auto unique=validate_economic_payload_fields(params);if(!unique.ok)return {false,{},unique.error};
 auto id=[&](const char* key){auto value=get_string(params,key,"params");if(value.empty()||value.size()>256)fail("InvalidEmploymentCommandId",key);return value;};
 if(kind=="estate_employment_offer"){fields(params,{"policy_id","worker_actor_id"},{},"params");return {true,EstateEmploymentOffer{id("policy_id"),id("worker_actor_id")},{}};}
 if(kind=="estate_employment_request_offer"){fields(params,{"policy_id"},{},"params");return {true,EstateEmploymentRequestOffer{id("policy_id")},{}};}
 if(kind=="estate_employment_decide"){fields(params,{"contract_id","decision"},{},"params");auto contract=id("contract_id"),decision=id("decision");if(decision!="accept"&&decision!="decline"&&decision!="withdraw"&&decision!="leave"&&decision!="dismiss"&&decision!="request_reply")fail("InvalidEmploymentDecision","params/decision");return {true,EstateEmploymentDecision{contract,decision},{}};}
 return {false,{},common_failure("UnknownEstateEmploymentCommand",kind).error};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
StableId estate_employment_command_kind(const EstateEmploymentCommand& command){return std::holds_alternative<EstateEmploymentOffer>(command)?"estate_employment_offer":std::holds_alternative<EstateEmploymentRequestOffer>(command)?"estate_employment_request_offer":"estate_employment_decide";}
BoundaryValue encode_estate_employment_command(const EstateEmploymentCommand& command){using V=BoundaryValue;if(const auto* offer=std::get_if<EstateEmploymentOffer>(&command))return boundary::obj({{"policy_id",V{offer->policy_id}},{"worker_actor_id",V{offer->worker_actor_id}}});if(const auto* request=std::get_if<EstateEmploymentRequestOffer>(&command))return boundary::obj({{"policy_id",V{request->policy_id}}});const auto& decision=std::get<EstateEmploymentDecision>(command);return boundary::obj({{"contract_id",V{decision.contract_id}},{"decision",V{decision.decision}}});}
BoundaryValue encode_estate_employment_quote(const EstateEmploymentQuote& quote){using V=BoundaryValue;return boundary::obj({{"ok",V{true}},{"duration",V{Integer{0}}},{"kind",V{estate_employment_command_kind(quote.command)}},{"actor_id",V{quote.actor_id}},{"params",encode_estate_employment_command(quote.command)},{"terms",dynamic_economy_codec::encode(quote.terms)},{"resolved_decision",V{quote.resolved_decision}}});}
BoundaryValue encode_estate_employment_receipt(const EstateEmploymentReceipt& receipt){using V=BoundaryValue;return boundary::obj({{"contract_id",V{receipt.contract_id}},{"fact_id",V{receipt.fact_id}},{"obligation_id",V{receipt.obligation_id}},{"wage_receipt_ids",boundary::string_array(receipt.wage_receipt_ids)}});}
Status validate_estate_employment_receipt(const EstateEmploymentState& state,const StableId& kind,const StableId& actor,Integer tick,const EstateReceipt& receipt){
 if(receipt.actor_ids)return common_failure("InvalidEmploymentReceipt");
 const auto* contract=state.contracts.find(receipt.fact_id);StableId expected_actor;Integer expected_tick=-1;
 if(kind=="estate_employment_decide"){const auto* decision=state.decisions.find(receipt.fact_id);if(!decision||decision->actor_id.empty()||(decision->provider_declared&&!decision->meeting))return common_failure("EmploymentReceiptSourceMismatch");contract=state.contracts.find(decision->contract_id);expected_actor=decision->provider_declared?decision->meeting->actor_id:decision->actor_id;expected_tick=decision->tick;}
 else if(kind=="estate_employment_offer"||kind=="estate_employment_request_offer"){if(!contract||contract->offer_from_provider!=(kind=="estate_employment_request_offer"))return common_failure("EmploymentReceiptSourceMismatch");expected_actor=contract->offer_from_provider?contract->terms.worker_actor_id:contract->terms.employer_actor_id;expected_tick=contract->offered_tick;}
 else return common_failure("InvalidEmploymentReceiptKind");
 return contract&&contract->terms.estate_id==receipt.estate_id&&expected_actor==actor&&expected_tick==tick?common_success():common_failure("EmploymentReceiptSourceMismatch");
}

Outcome<StableId> estate_employment_receipt_actor(const CommonState& state,const StableId& key,const StableId& kind,const StableId& result,Integer tick,const StableId& fact){
 auto ordinal=[](const StableId& id,const StableId& prefix)->Integer{if(!prefix.empty()&&id.rfind(prefix+":",0)!=0)return 0;const auto at=id.rfind(':');if(at==StableId::npos)return 0;const auto digits=id.substr(at+1);Integer value=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&value>0&&std::to_string(value)==digits?value:0;};
 const auto split=key.rfind(':');if(split==StableId::npos)return {false,{},common_failure("EmploymentReceiptCacheMismatch").error};const auto actor=key.substr(0,split);const auto request=ordinal(key,{});const auto* sequence=state.request_sequences.find(actor);if(!request||!sequence||request>*sequence)return {false,{},common_failure("EmploymentReceiptCacheMismatch").error};
 const HistoryRecord* selected=nullptr;const HistoryRecord* previous=nullptr;const HistoryRecord* before=nullptr;for(const auto& item:state.history){if(item.result_id==result){if(selected)return {false,{},common_failure("EmploymentReceiptHistoryMismatch").error};selected=&item;before=previous;}previous=&item;}
 if(!selected||selected->actor_id!=actor||selected->kind!=kind||selected->tick!=tick)return {false,{},common_failure("EmploymentReceiptHistoryMismatch").error};
 const auto upper=ordinal(result,"result"),lower=before?ordinal(before->result_id,"result"):0,source=ordinal(fact,{});if(!upper||(before&&!lower)||upper<=lower||source<=lower||source>=upper)return {false,{},common_failure("EmploymentReceiptAllocationMismatch").error};return {true,actor,{}};
}

}
