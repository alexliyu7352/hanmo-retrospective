#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "repair_service_codec_archive.hpp"
#include <string_view>
#include "resident/domains/craft/item_service.hpp"
namespace hanmo::resident {
namespace {
template<class F>Status repair_service_original_table_members(F&&f){
 Status out=common_success();auto table=[&](const char*kind,auto member){if(out.ok)out=f(kind,member);};
 table("repair_service_offered",&RepairServiceContractsState::offers);
 table("repair_service_assented",&RepairServiceContractsState::assents);
 table("repair_service_contract_accepted",&RepairServiceContractsState::contracts);
 table("repair_custody_received",&RepairServiceContractsState::custody_receipts);
 table("repair_service_charge",&RepairServiceContractsState::charges);
 table("repair_service_paid",&RepairServiceContractsState::payments);
 table("repair_service_termination_settled",&RepairServiceContractsState::termination_settlements);
 table("repair_item_returned",&RepairServiceContractsState::returns);
 return out;
}
template<class F>Status repair_service_original_tables(const RepairServiceContractsState&s,F&&f){return repair_service_original_table_members([&](const char*kind,auto member){return f(kind,s.*member);});}
template<class T>Status repair_service_owned_payload_identity(const T&row){
 if constexpr(std::is_same_v<T,RepairCustodyReceivedSource>||std::is_same_v<T,RepairItemReturnedSource>){
  auto checked=validate_original_unique_item_transfer_effect_shape(row.actual_transfer);if(!checked.ok)return checked;
  const auto&e=row.actual_transfer;
  if(row.actual_item_selection.inventory_id!=e.destination_inventory_id||row.actual_item_selection.lot_id!=e.lot_id||row.actual_item_selection.quantity!=1||row.actual_condition_head.instance_id!=e.instance_id||row.original_resource_effect_refs.size()!=1)return common_failure("RepairOwnedTransferPayloadIdentityMismatch",row.id);
  const auto&ref=row.original_resource_effect_refs.front();
  constexpr const char*kind=std::is_same_v<T,RepairCustodyReceivedSource>?"repair_custody_item_transfer":"repair_return_item_transfer";
  if(ref.source_schema!=1||ref.source_ref.domain!="commerce"||ref.source_ref.kind!=kind||ref.source_ref.id!=row.id)return common_failure("RepairOwnedTransferPayloadIdentityMismatch",row.id);
 }
 return common_success();
}
}
bool is_repair_service_original_fact_kind(std::string_view kind){bool found=false;repair_service_original_table_members([&](const char*name,auto){if(kind==name)found=true;return common_success();});return found;}
Outcome<Integer> read_repair_service_original_payload_schema(const CommonStore&common,const ItemInstancesState&items,const FactRecord&head){
 const auto*actual=common.state().facts.find(head.id);
 if(head.id.empty()||!actual||actual->id!=head.id||actual->kind!=head.kind||actual->domain!=head.domain||head.domain!="commerce")return {false,0,common_failure("RepairServiceOriginalHeaderRequired",head.id).error};
 if(!is_repair_service_original_fact_kind(head.kind))return {false,0,common_failure("UnsupportedRepairServiceSource",head.id).error};
 std::size_t matched=0;const auto checked=repair_service_original_tables(items.repair_service_contracts,[&](const char*kind,const auto&rows){
  for(const auto&row:rows.rows())if(row.first==head.id||row.second.id==head.id){
   if(row.first!=head.id||row.second.id!=head.id||head.kind!=kind)return common_failure("RepairServiceOriginalPayloadIdentityMismatch",head.id);
   ++matched;auto valid=repair_service_owned_payload_identity(row.second);if(!valid.ok)return valid;
  }
  return common_success();
 });
 if(!checked.ok)return {false,0,checked.error};
 if(matched!=1)return {false,0,common_failure("RepairServiceOriginalPayloadRequired",head.id).error};
 return {true,1,{}};
}
Status visit_repair_service_original_fact_headers(const CommonStore&common,const ItemInstancesState&items,RepairServiceOriginalFactHeaderSink&sink){
 auto checked=repair_service_original_tables(items.repair_service_contracts,[&](const char*kind,const auto&rows){
  for(const auto&row:rows.rows()){
   const auto*head=common.state().facts.find(row.first);
   if(!head||head->id!=row.first||head->domain!="commerce"||head->kind!=kind||row.second.id!=row.first)return common_failure("RepairServiceOriginalHeaderRequired",row.first);
   auto schema=read_repair_service_original_payload_schema(common,items,*head);if(!schema.ok)return Status{false,{},schema.error};
  }
  return common_success();
 });
 if(!checked.ok)return checked;
 for(const auto&row:common.state().facts.rows())if(row.second.domain=="commerce"){
  // The actual transfer is one owned child of its actual custody/return row,
  // never a second Common fact or an additional coverage count.
  if(row.second.kind=="repair_custody_item_transfer"||row.second.kind=="repair_return_item_transfer")return common_failure("RepairOwnedTransferCannotBeCommonHeader",row.first);
  if(is_repair_service_original_fact_kind(row.second.kind)){
   if(row.first!=row.second.id)return common_failure("RepairServiceOriginalHeaderIdentityMismatch",row.first);
   auto schema=read_repair_service_original_payload_schema(common,items,row.second);if(!schema.ok)return {false,{},schema.error};
  }
 }
 for(const auto&row:common.state().facts.rows())if(row.second.domain=="commerce"&&is_repair_service_original_fact_kind(row.second.kind)){checked=sink.append(row.second,1);if(!checked.ok)return checked;}
 return common_success();
}
bool repair_service_contracts_empty(const RepairServiceContractsState&s)noexcept{return s.offers.rows().empty()&&s.assents.rows().empty()&&s.contracts.rows().empty()&&s.custody_receipts.rows().empty()&&s.charges.rows().empty()&&s.payments.rows().empty()&&s.termination_settlements.rows().empty()&&s.returns.rows().empty();}
Outcome<RepairServiceContractsState> decode_repair_service_contracts(const BoundaryValue&v){try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<RepairServiceContractsState>();RepairServiceContractsState out;repair_service_codec::decode(v,out,"/repair_service_contracts");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_repair_service_contracts(const RepairServiceContractsState&v){return repair_service_codec::encode(v);}
Status emit_repair_service_contracts_fields(ProjectionWriter&w,const RepairServiceContractsState&v){repair_service_codec::Project f{w};repair_service_codec::record(f,const_cast<RepairServiceContractsState&>(v));return f.result.ok?w.status():f.result;}
Status emit_repair_service_contracts(ProjectionWriter&w,std::string_view name,const RepairServiceContractsState&v){return repair_service_codec::emit(w,name,v);}
Status read_repair_service_contracts_json_fields(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,RepairServiceContractsState& value,std::string_view path){return repair_service_codec::json_value(archive,input,value,path);}
Status visit_repair_service_contracts(CodecVisitor&c,RepairServiceContractsState&v){repair_service_codec::Visit f{c};repair_service_fields::fields(f,v);return f.result.ok?c.status():f.result;}
Status emit_repair_service_original_payload_fields(ProjectionWriter&w,const CommonStore&common,const ItemInstancesState&items,const FactRecord&head){
 const auto schema=read_repair_service_original_payload_schema(common,items,head);if(!schema.ok)return {false,{},schema.error};
 try{return repair_service_original_tables(items.repair_service_contracts,[&](const char*kind,const auto&table)->Status{
  if(head.kind!=kind)return common_success();const auto&row=*table.find(head.id);using T=std::decay_t<decltype(row)>;
  repair_service_codec::Project f{w};repair_service_codec::record(f,const_cast<T&>(row));return f.result.ok?w.status():f.result;
 });}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}
}
}
namespace hanmo::resident {Status emit_repair_service_original_payload(ProjectionWriter&w,const StableId&name,const CommonStore&c,const ItemInstancesState&i,const FactRecord&h){return w.object(name,[&](auto&out){return emit_repair_service_original_payload_fields(out,c,i,h);});}}
namespace hanmo::resident::repair_service_codec {
template<class T>bool equal(const T&,const T&);
struct CompareFields {
 struct Entry{const char*name=nullptr;const void*value=nullptr;bool(*compare)(const void*,const void*)=nullptr;};
 std::array<Entry,32>entries{};std::size_t count=0,position=0;bool second=false,result=true;
 template<class T>static bool values(const void*a,const void*b){return equal(*static_cast<const T*>(a),*static_cast<const T*>(b));}
 template<class T>void operator()(const char*name,const T&v){if(!result)return;if(!second){if(count==entries.size()){result=false;return;}entries[count++]={name,&v,&values<T>};}else{if(position==count){result=false;return;}const auto&e=entries[position++];result=std::string_view(e.name)==name&&e.compare==&values<T>&&e.compare(e.value,&v);}}
};
template<class T>bool equal(const T&a,const T&b){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_enum_v<T>)return a==b;
 else if constexpr(std::is_same_v<T,Owner>)return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;
 else if constexpr(std::is_same_v<T,WorldMoment>){const auto cmp=compare_world_moments(a,b);return cmp.ok&&!cmp.value;}
 else if constexpr(std::is_same_v<T,MomentOwnerSource>)return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);
 else if constexpr(std::is_same_v<T,SourceRef>)return same_source_ref(a,b);
 else if constexpr(std::is_same_v<T,CargoSelection>)return a.inventory_id==b.inventory_id&&a.lot_id==b.lot_id&&a.quantity==b.quantity;
 else if constexpr(std::is_same_v<T,FacilitySlotRef>)return a.provider_domain==b.provider_domain&&a.facility_id==b.facility_id&&a.slot_kind==b.slot_kind;
 else if constexpr(Optional<T>::value)return a.has_value()==b.has_value()&&(!a||equal(*a,*b));
 else if constexpr(Vector<T>::value){if(a.size()!=b.size())return false;for(std::size_t n=0;n<a.size();++n)if(!equal(a[n],b[n]))return false;return true;}
 else{CompareFields fields;record(fields,const_cast<T&>(a));fields.second=true;record(fields,const_cast<T&>(b));return fields.result&&fields.count==fields.position;}
}
}
namespace hanmo::resident {bool same_repair_service_quoted_terms(const RepairServiceQuotedTerms&a,const RepairServiceQuotedTerms&b){return repair_service_codec::equal(a,b);}
bool same_repair_service_inspection(const RepairServiceAcceptanceInspection&a,const RepairServiceAcceptanceInspection&b){return repair_service_codec::equal(a,b);}
bool same_repair_service_contract_terms(const RepairServiceContractTerms&a,const RepairServiceContractTerms&b){return repair_service_codec::equal(a,b);}}
