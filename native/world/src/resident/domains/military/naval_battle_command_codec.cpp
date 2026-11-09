#include "resident/checkpoint/projection_writer.hpp"
// 公开四种零时长战斗意图，各自只引用原回合/计划事实，不把资产账复制进回执。
#include "naval_battle_command_codec.hpp"
#include "naval_json_guard.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident::naval_battle_command_codec {
namespace {
using V=BoundaryValue;
// 版本来自外层实际 selected_checkpoint_domains，不能从收到的命令或回执推测。
Status enabled(Integer version){return (version==2||version==3||version==4||version==5||version==6)?common_success():common_failure("NavalCombatProfileRequired");}
bool instance(const StableId& id,const StableId& prefix){if(id.rfind(prefix+":",0)!=0)return false;Integer n=0;const auto first=id.data()+prefix.size()+1;auto r=std::from_chars(first,id.data()+id.size(),n);return r.ec==std::errc{}&&r.ptr==id.data()+id.size()&&n>0&&id==prefix+":"+std::to_string(n);}
Status valid(const NavalBattleCommandReceipt& r){
 const StableId prefix=r.kind=="naval_battle_plan"?"naval_plan":r.kind=="naval_capture"?"naval_capture":r.kind=="naval_claim_decline"?"naval_decline":r.kind=="naval_battle_continue"?"naval_continue":"";
 if(prefix.empty()||!naval_request_key_valid(r.request_key)||!instance(r.receipt_id,prefix)||!instance(r.battle_id,"naval_battle")||r.actor_id.empty()||r.tick<0||(r.kind=="naval_battle_plan"?!r.round_source_id.empty():!instance(r.round_source_id,"naval_round")))return common_failure("InvalidNavalBattleReceipt",r.receipt_id);
 if(!r.field_order.empty()){std::set<StableId> fields={"kind","request_key","receipt_id","battle_id","round_source_id","actor_id","tick"};if(fields.size()!=r.field_order.size())return common_failure("InvalidNavalBattleReceiptFieldOrder");for(const auto& key:r.field_order)if(fields.erase(key)!=1)return common_failure("InvalidNavalBattleReceiptFieldOrder",key);}return common_success();
}
bool same(const NavalBattleCommandReceipt& a,const NavalBattleCommandReceipt& b){return a.kind==b.kind&&a.request_key==b.request_key&&a.receipt_id==b.receipt_id&&a.battle_id==b.battle_id&&a.round_source_id==b.round_source_id&&a.actor_id==b.actor_id&&a.tick==b.tick;}
}
// 只认四种正式意图，history-only 验证也使用本函数识别禁用命令。
bool command_kind(const StableId& kind){return kind=="naval_battle_plan"||kind=="naval_capture"||kind=="naval_claim_decline"||kind=="naval_battle_continue";}
StableId command_kind(const NavalBattleCommand& command){return std::visit([](const auto& row)->StableId{using T=std::decay_t<decltype(row)>;if constexpr(std::is_same_v<T,NavalPlanCommand>)return "naval_battle_plan";else if constexpr(std::is_same_v<T,NavalCaptureCommand>)return "naval_capture";else if constexpr(std::is_same_v<T,NavalClaimDeclineCommand>)return "naval_claim_decline";else return "naval_battle_continue";},command);}
// 命令不能指定结果、权限、世界时刻或来源 ID；这些都由真实事务生成。
Outcome<NavalBattleCommand> decode_command(const V& value,Integer version){auto ready=enabled(version);if(!ready.ok)return {false,{},ready.error};auto unique=validate_naval_json_unique_keys(value,"naval/command");if(!unique.ok)return {false,{},unique.error};try{
 const auto kind=boundary::get_string(value,"kind","naval_command");if(!command_kind(kind))return {false,{},common_failure("UnknownNavalBattleCommand",kind).error};
 if(kind=="naval_battle_plan"){boundary::fields(value,{"kind","battle_id","request_key","orders"},{},kind);auto orders=naval_battle_codec::decode_orders(boundary::field(value,"orders",kind));if(!orders.ok)return {false,{},orders.error};NavalPlanCommand c;c.battle_id=boundary::get_string(value,"battle_id",kind);c.request_key=boundary::get_string(value,"request_key",kind);c.orders=std::move(orders.value);return {true,NavalBattleCommand{std::move(c)},{}};}
 if(kind=="naval_battle_continue"){boundary::fields(value,{"kind","battle_id","round_source_id","request_key"},{},kind);return {true,NavalBattleCommand{NavalContinueCommand{boundary::get_string(value,"battle_id",kind),boundary::get_string(value,"round_source_id",kind),boundary::get_string(value,"request_key",kind)}},{}};}
 if(kind=="naval_capture"){boundary::fields(value,{"kind","battle_id","round_source_id","request_key","vessel_id","captor_unit_id","assignments"},{},kind);auto rows=naval_battle_codec::decode_assignments(boundary::field(value,"assignments",kind));if(!rows.ok)return {false,{},rows.error};return {true,NavalBattleCommand{NavalCaptureCommand{boundary::get_string(value,"battle_id",kind),boundary::get_string(value,"round_source_id",kind),boundary::get_string(value,"request_key",kind),boundary::get_string(value,"vessel_id",kind),boundary::get_string(value,"captor_unit_id",kind),std::move(rows.value)}},{}};}
 boundary::fields(value,{"kind","battle_id","round_source_id","request_key","vessel_id","captor_unit_id"},{},kind);return {true,NavalBattleCommand{NavalClaimDeclineCommand{boundary::get_string(value,"battle_id",kind),boundary::get_string(value,"round_source_id",kind),boundary::get_string(value,"request_key",kind),boundary::get_string(value,"vessel_id",kind),boundary::get_string(value,"captor_unit_id",kind)}},{}};
 }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// JSON 冷边界与 typed 边界保存同七字段，原序只影响字节而不改变事实身份。
Status emit_receipt_fields(ProjectionWriter& w,const NavalBattleCommandReceipt& r,Integer version){auto gate=enabled(version);if(!gate.ok)return w.reject(gate.error.original.code,"naval_receipt");auto checked=valid(r);if(!checked.ok)return w.reject(checked.error.original.code,checked.error.original.path);w.field("kind",r.kind);w.field("request_key",r.request_key);w.field("receipt_id",r.receipt_id);w.field("battle_id",r.battle_id);w.field("round_source_id",r.round_source_id);w.field("actor_id",r.actor_id);w.field("tick",r.tick);w.order_fields(r.field_order);return w.status();}
// 复用本编码器的原七字段投影；校验或输出失败时保留原边界错误。
BoundaryValue encode_receipt(const NavalBattleCommandReceipt &r,
                             Integer version) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto written = sink.object("", [&](auto &w) {
    return naval_battle_command_codec::emit_receipt_fields(w, r, version);
  });
  if (!written.ok)
    boundary::fail(written.error.original.code, written.error.original.path);
  auto finished = sink.finish();
  if (!finished.ok)
    boundary::fail(finished.error.original.code, finished.error.original.path);
  return out;
}
Outcome<NavalBattleCommandReceipt> decode_receipt(const V& value,Integer version){auto gate=enabled(version);if(!gate.ok)return {false,{},gate.error};auto unique=validate_naval_json_unique_keys(value,"naval/receipt");if(!unique.ok)return {false,{},unique.error};try{const StableId p="naval_receipt";boundary::fields(value,{"kind","request_key","receipt_id","battle_id","round_source_id","actor_id","tick"},{},p);NavalBattleCommandReceipt r;r.kind=boundary::get_string(value,"kind",p);r.request_key=boundary::get_string(value,"request_key",p);r.receipt_id=boundary::get_string(value,"receipt_id",p);r.battle_id=boundary::get_string(value,"battle_id",p);r.round_source_id=boundary::get_string(value,"round_source_id",p);r.actor_id=boundary::get_string(value,"actor_id",p);r.tick=boundary::get_int(value,"tick",p);r.field_order=boundary::order(value);auto checked=valid(r);if(!checked.ok)return {false,{},checked.error};return {true,std::move(r),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_receipt(CodecVisitor& c,NavalBattleCommandReceipt& r,Integer version){auto gate=enabled(version);if(!gate.ok)return c.reject(gate.error.original.code);c.field("kind",r.kind);c.field("request_key",r.request_key);c.field("receipt_id",r.receipt_id);c.field("battle_id",r.battle_id);c.field("round_source_id",r.round_source_id);c.field("actor_id",r.actor_id);c.field("tick",r.tick);common_checkpoint::strings(c,"field_order",r.field_order);if(!c.status().ok)return c.status();auto checked=valid(r);return checked.ok?c.status():c.reject(checked.error.original.code,checked.error.original.path);}
BoundaryValue encode_quote(const NavalBattleCommandQuote& q,Integer version){auto gate=enabled(version);if(!gate.ok)boundary::fail(gate.error.original.code,"naval_quote");V::Dict fields={{"replayed",V{q.replayed}},{"kind",V{q.receipt.kind}},{"battle_id",V{q.receipt.battle_id}}};if(q.replayed)fields.emplace_back("receipt",encode_receipt(q.receipt,version));return boundary::obj({{"ok",V{true}},{"duration",V{Integer{0}}},{"naval_battle",V{std::move(fields)}}});}
// 源事实到回执的投影没有独立可写权限字段，也不会冒充新的半日行动。
NavalBattleCommandReceipt receipt(const NavalPlanFact& f){return {"naval_battle_plan",f.request_key,f.id,f.battle_id,"",f.actor_id,f.tick,{}};}
NavalBattleCommandReceipt receipt(const NavalCaptureFact& f){return {"naval_capture",f.request_key,f.id,f.battle_id,f.round_source_id,f.actor_id,f.tick,{}};}
NavalBattleCommandReceipt receipt(const NavalClaimDeclineFact& f){return {"naval_claim_decline",f.request_key,f.id,f.battle_id,f.round_source_id,f.actor_id,f.tick,{}};}
NavalBattleCommandReceipt receipt(const NavalContinueFact& f){return {"naval_battle_continue",f.request_key,f.id,f.battle_id,f.round_source_id,f.actor_id,f.tick,{}};}
// 旧资产 profile 即使 result 已清退，仍不能借一条 history 冒充战斗已启用。
Status validate_history_version(const CommonState& state,Integer version){for(const auto& h:state.history)if(command_kind(h.kind)&&version!=2&&version!=3&&version!=4&&version!=5&&version!=6)return common_failure("NavalCombatProfileRequired","history/"+h.result_id);return common_success();}
Status validate_cached_receipt(const NavalBattleState& state,const NavalBattleCommandReceipt& r,const StableId& actor,Integer version){auto gate=enabled(version);if(!gate.ok)return gate;auto checked=valid(r);if(!checked.ok)return checked;if(actor!=r.actor_id)return common_failure("NavalBattleCachedActorMismatch",r.receipt_id);Integer matches=0;bool equal=false;auto check=[&](const auto& rows){for(const auto& f:rows)if(f.id==r.receipt_id){++matches;equal=same(r,receipt(f));}};check(state.plans);check(state.captures);check(state.declines);check(state.continuations);return matches==1&&equal?common_success():common_failure("NavalBattleCachedSourceMismatch",r.receipt_id);}
}
