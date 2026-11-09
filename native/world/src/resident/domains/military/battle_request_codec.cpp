#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_writer.hpp"
// 回执的原序、接受/拒绝形状与原计划统一编码；损坏输入只落到未发布候选。
#include "battle_request_codec.hpp"
#include "battle_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident::military_request_codec {
namespace {
using V=BoundaryValue;using D=V::Dict;
// 业务校验失败同时用于JSON与检查点，不给不同导入路径不同的宽松边界。
Status fail(const StableId& field){return common_failure("InvalidMilitaryRequestReceipt",field);}
// typed字段顺序不能藏缺键/重复键；空序列表示本域固定规范顺序。
bool keys_valid(const FieldOrder& keys,std::initializer_list<StableId> required,const StableId& extra={}){if(keys.empty())return true;std::set<StableId> expected(required);if(!extra.empty())expected.insert(extra);if(keys.size()!=expected.size())return false;for(const auto& key:keys)if(expected.erase(key)!=1)return false;return expected.empty();}
// 各请求只保原始公开目标，不持玩家未提交草案。
Status request_emit(ProjectionWriter& w,const MilitaryBattleRequest& r){w.field("battle_id",r.battle_id);w.field("request_key",r.request_key);w.field("request_kind",r.request_kind);w.field("unit_id",r.unit_id);w.field("target_unit_id",r.target_unit_id);w.order_fields(r.field_order);return w.status();}
// 请求字段必须完整且无额外字段，保持输入字典顺序供无改动冷读。
void request_read(const V& v,MilitaryBattleRequest& r,const StableId& p){boundary::fields(v,{"battle_id","request_key","request_kind","unit_id","target_unit_id"},{},p);r.field_order=domain_codec::order(v);r.battle_id=boundary::get_string(v,"battle_id",p);r.request_key=boundary::get_string(v,"request_key",p);r.request_kind=boundary::get_string(v,"request_kind",p);r.unit_id=boundary::get_string(v,"unit_id",p);r.target_unit_id=boundary::get_string(v,"target_unit_id",p);}
// 拒绝无响应计划字段，采纳才携一条实际计划。
Status decision_emit(ProjectionWriter& w,const MilitaryRequestDecision& r){w.field("accepted",r.accepted);w.field("request_kind",r.request_kind);w.field("reason",r.reason);if(r.accepted){w.field("response_unit_id",r.response_unit_id);w.object("response_order",[&](auto& child){return battle_codec::emit_fields(child,r.response_order);});}w.order_fields(r.field_order);return w.status();}
// 存在位与accepted严格对应；不能将一份藏有命令的拒绝当普通回执。
void decision_read(const V& v,MilitaryRequestDecision& r,const StableId& p){r={};r.accepted=boundary::boolean(boundary::field(v,"accepted",p),p+"/accepted");if(r.accepted)boundary::fields(v,{"accepted","request_kind","reason","response_unit_id","response_order"},{},p);else boundary::fields(v,{"accepted","request_kind","reason"},{},p);r.field_order=domain_codec::order(v);r.request_kind=boundary::get_string(v,"request_kind",p);r.reason=boundary::get_string(v,"reason",p);if(r.accepted){r.response_unit_id=boundary::get_string(v,"response_unit_id",p);battle_codec::decode(boundary::field(v,"response_order",p),r.response_order,p+"/response_order");}}
// 访客显式访问唯一请求对象；字符串及集合上限由通用有界访客执行。
Status request_visit(CodecVisitor& c,MilitaryBattleRequest& r){c.field("battle_id",r.battle_id);c.field("request_key",r.request_key);c.field("request_kind",r.request_kind);c.field("unit_id",r.unit_id);c.field("target_unit_id",r.target_unit_id);domain_codec::visit_order(c,r.field_order);return c.status();}
// 拒绝的隐藏响应字段在读入时清空；错误标签仍由原访客拒绝。
Status decision_visit(CodecVisitor& c,MilitaryRequestDecision& r){c.field("accepted",r.accepted);c.field("request_kind",r.request_kind);c.field("reason",r.reason);if(r.accepted){c.field("response_unit_id",r.response_unit_id);c.object("response_order",[&](auto& codec){return battle_codec::visit(codec,r.response_order);});}else if(c.reading()){r.response_unit_id.clear();r.response_order={};}domain_codec::visit_order(c,r.field_order);return c.status();}
}
// 回执必须保三种独立意图；历史战法合法性与实际权源另由世界冷validator核对。
Status validate(const MilitaryRequestReceipt& r){
 if(r.id.empty()||r.actor_id.empty()||r.commander_actor_id.empty()||r.actor_id==r.commander_actor_id||r.army_id.empty()||r.office_id.empty()||r.formation_source.empty()||r.appointment_source.empty()||r.tick<0||r.round<1)return fail("identity");
 const auto& q=r.request;const auto& d=r.decision;
 if(!keys_valid(r.field_order,{"id","actor_id","commander_actor_id","army_id","office_id","formation_source","appointment_source","tick","round","request","decision"},r.previous_order?"previous_order":"")||!keys_valid(q.field_order,{"battle_id","request_key","request_kind","unit_id","target_unit_id"})||!(d.accepted?keys_valid(d.field_order,{"accepted","request_kind","reason","response_unit_id","response_order"}):keys_valid(d.field_order,{"accepted","request_kind","reason"})))return fail("field_order");
 if(!q.fields_valid||q.battle_id.empty()||q.request_key.empty()||q.request_key.size()>4096||q.unit_id.empty()||d.request_kind!=q.request_kind)return fail("request");
 if(q.request_kind!="support"&&q.request_kind!="withdrawal"&&q.request_kind!="focus_target")return fail("request_kind");
 if((q.request_kind=="focus_target")==q.target_unit_id.empty())return fail("target_unit_id");
 if(d.accepted){
  if(d.response_unit_id.empty()||d.response_unit_id==q.unit_id||!r.previous_order)return fail("response");
  const auto expected=q.request_kind=="support"?"MilitarySupportAccepted":q.request_kind=="withdrawal"?"MilitaryWithdrawalApprovedWithCover":"MilitaryFocusAccepted";
  if(d.reason!=expected||d.response_order.action!=(q.request_kind=="focus_target"?"attack":"tactic")||d.response_order.target_id!=(q.request_kind=="focus_target"?q.target_unit_id:q.unit_id))return fail("decision");
 }else{
  if(r.previous_order||!d.response_unit_id.empty()||!d.response_order.action.empty())return fail("refusal");
  bool reason=q.request_kind=="focus_target"?d.reason=="MilitaryNoLegalAttack":d.reason=="MilitaryNoLegalSupport";
  if(q.request_kind=="withdrawal")reason=reason||d.reason=="MilitaryWithdrawalForbidden"||d.reason=="MilitaryCommanderMaintainsObjective"||d.reason=="MilitaryRetreatExitUnreachable";
  if(!reason)return fail("reason");
 }
 return common_success();
}
// 输出只编码一份回执，不包含世界、AI全计划或别人的私有状态。
Status emit_fields(ProjectionWriter& w,const MilitaryRequestReceipt& r,bool apply_order){
#define FIELD(n) w.field(#n,r.n)
FIELD(id);FIELD(actor_id);FIELD(commander_actor_id);FIELD(army_id);FIELD(office_id);FIELD(formation_source);FIELD(appointment_source);FIELD(tick);FIELD(round);
#undef FIELD
w.object("request",[&](auto& child){return request_emit(child,r.request);});w.object("decision",[&](auto& child){return decision_emit(child,r.decision);});if(r.previous_order)w.object("previous_order",[&](auto& child){return battle_codec::emit_fields(child,*r.previous_order);});if(apply_order)w.order_fields(r.field_order);return w.status();}
BoundaryValue encode(const MilitaryRequestReceipt& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& w){return emit_fields(w,r);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
// JSON导入不允许多余字段或缺失的采纳前计划，损坏记录不静默修复。
void decode(const BoundaryValue& v,MilitaryRequestReceipt& r,const StableId& p){r={};boundary::fields(v,{"id","actor_id","commander_actor_id","army_id","office_id","formation_source","appointment_source","tick","round","request","decision"},{"previous_order"},p);r.field_order=domain_codec::order(v);r.id=boundary::get_string(v,"id",p);r.actor_id=boundary::get_string(v,"actor_id",p);r.commander_actor_id=boundary::get_string(v,"commander_actor_id",p);r.army_id=boundary::get_string(v,"army_id",p);r.office_id=boundary::get_string(v,"office_id",p);r.formation_source=boundary::get_string(v,"formation_source",p);r.appointment_source=boundary::get_string(v,"appointment_source",p);r.tick=boundary::get_int(v,"tick",p);r.round=boundary::get_int(v,"round",p);request_read(boundary::field(v,"request",p),r.request,p+"/request");decision_read(boundary::field(v,"decision",p),r.decision,p+"/decision");if(auto previous=v.find("previous_order")){r.previous_order.emplace();battle_codec::decode(*previous,*r.previous_order,p+"/previous_order");}auto valid=validate(r);if(!valid.ok)boundary::fail(valid.error.original.code,p+"/"+valid.error.original.path);}
// 二进制访客完成后立即核业务形状，外层仍须finish和世界跨域validator后才发布。
Status visit(CodecVisitor& c,MilitaryRequestReceipt& r){c.field("id",r.id);c.field("actor_id",r.actor_id);c.field("commander_actor_id",r.commander_actor_id);c.field("army_id",r.army_id);c.field("office_id",r.office_id);c.field("formation_source",r.formation_source);c.field("appointment_source",r.appointment_source);c.field("tick",r.tick);c.field("round",r.round);c.object("request",[&](auto& codec){return request_visit(codec,r.request);});c.object("decision",[&](auto& codec){return decision_visit(codec,r.decision);});c.optional("previous_order",r.previous_order,[](auto& codec,auto& value){return battle_codec::visit(codec,value);});domain_codec::visit_order(c,r.field_order);if(!c.status().ok)return c.status();auto valid=validate(r);return valid.ok?c.status():c.reject(valid.error.original.code,valid.error.original.path);}
// 命令结果由唯一事实投影，不把统帅采纳前私有计划复制到公共回执。
MilitaryRequestAnswer answer(const MilitaryRequestReceipt& r){MilitaryRequestAnswer a;a.receipt_id=r.id;a.battle_id=r.request.battle_id;a.request_key=r.request.request_key;a.request_kind=r.request.request_kind;a.unit_id=r.request.unit_id;a.target_unit_id=r.request.target_unit_id;a.commander_actor_id=r.commander_actor_id;a.round=r.round;a.tick=r.tick;a.accepted=r.decision.accepted;a.reason=r.decision.reason;a.response_unit_id=r.decision.response_unit_id;if(a.accepted)a.response_order=r.decision.response_order;return a;}
// 公开缓存同样保持三种独立意图及严格接受形状，不能藏第二条命令。
Status validate_answer(const MilitaryRequestAnswer& a){if(!keys_valid(a.field_order,{"receipt_id","battle_id","request_key","request_kind","unit_id","target_unit_id","commander_actor_id","round","tick","accepted","reason","response_unit_id"},a.response_order?"response_order":""))return fail("answer_field_order");if(a.receipt_id.empty()||a.battle_id.empty()||a.request_key.empty()||a.request_key.size()>4096||a.unit_id.empty()||a.commander_actor_id.empty()||a.round<1||a.tick<0)return fail("answer_identity");if(a.request_kind!="support"&&a.request_kind!="withdrawal"&&a.request_kind!="focus_target")return fail("answer_kind");if((a.request_kind=="focus_target")==a.target_unit_id.empty())return fail("answer_target");if(a.accepted){if(a.response_unit_id.empty()||a.response_unit_id==a.unit_id||!a.response_order)return fail("answer_response");const auto expected=a.request_kind=="support"?"MilitarySupportAccepted":a.request_kind=="withdrawal"?"MilitaryWithdrawalApprovedWithCover":"MilitaryFocusAccepted";if(a.reason!=expected||a.response_order->action!=(a.request_kind=="focus_target"?"attack":"tactic")||a.response_order->target_id!=(a.request_kind=="focus_target"?a.target_unit_id:a.unit_id))return fail("answer_decision");}else{if(!a.response_unit_id.empty()||a.response_order)return fail("answer_refusal");bool reason=a.request_kind=="focus_target"?a.reason=="MilitaryNoLegalAttack":a.reason=="MilitaryNoLegalSupport";if(a.request_kind=="withdrawal")reason=reason||a.reason=="MilitaryWithdrawalForbidden"||a.reason=="MilitaryCommanderMaintainsObjective"||a.reason=="MilitaryRetreatExitUnreachable";if(!reason)return fail("answer_reason");}return common_success();}
// 公开字段由唯一投影集中生成，Main无需解析完整历史权源。
Status emit_answer_fields(ProjectionWriter& w,const MilitaryRequestAnswer& a){w.field("receipt_id",a.receipt_id);w.field("battle_id",a.battle_id);w.field("request_key",a.request_key);w.field("request_kind",a.request_kind);w.field("unit_id",a.unit_id);w.field("target_unit_id",a.target_unit_id);w.field("commander_actor_id",a.commander_actor_id);w.field("round",a.round);w.field("tick",a.tick);w.field("accepted",a.accepted);w.field("reason",a.reason);w.field("response_unit_id",a.response_unit_id);if(a.response_order)w.object("response_order",[&](auto& x){return battle_codec::emit_fields(x,*a.response_order);});w.order_fields(a.field_order);return w.status();}
BoundaryValue encode_answer(const MilitaryRequestAnswer& a){BoundaryValue out;BoundaryProjectionWriter sink(out);auto written=sink.object("",[&](auto& w){return emit_answer_fields(w,a);});if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);auto finished=sink.finish();if(!finished.ok)boundary::fail(finished.error.original.code,finished.error.original.path);return out;}
// 公共缓存导入必须和领域完整回执一样严格拒绝未知字段。
void decode_answer(const BoundaryValue& v,MilitaryRequestAnswer& a,const StableId& p){a={};boundary::fields(v,{"receipt_id","battle_id","request_key","request_kind","unit_id","target_unit_id","commander_actor_id","round","tick","accepted","reason","response_unit_id"},{"response_order"},p);a.field_order=domain_codec::order(v);a.receipt_id=boundary::get_string(v,"receipt_id",p);a.battle_id=boundary::get_string(v,"battle_id",p);a.request_key=boundary::get_string(v,"request_key",p);a.request_kind=boundary::get_string(v,"request_kind",p);a.unit_id=boundary::get_string(v,"unit_id",p);a.target_unit_id=boundary::get_string(v,"target_unit_id",p);a.commander_actor_id=boundary::get_string(v,"commander_actor_id",p);a.round=boundary::get_int(v,"round",p);a.tick=boundary::get_int(v,"tick",p);a.accepted=boundary::boolean(boundary::field(v,"accepted",p),p+"/accepted");a.reason=boundary::get_string(v,"reason",p);a.response_unit_id=boundary::get_string(v,"response_unit_id",p);if(auto order=v.find("response_order")){a.response_order.emplace();battle_codec::decode(*order,*a.response_order,p+"/response_order");}auto valid=validate_answer(a);if(!valid.ok)boundary::fail(valid.error.original.code,p+"/"+valid.error.original.path);}
// 共同回执由外层feature128门控制，本访客不自行推断或升级旧格式。
Status visit_answer(CodecVisitor& c,MilitaryRequestAnswer& a){c.field("receipt_id",a.receipt_id);c.field("battle_id",a.battle_id);c.field("request_key",a.request_key);c.field("request_kind",a.request_kind);c.field("unit_id",a.unit_id);c.field("target_unit_id",a.target_unit_id);c.field("commander_actor_id",a.commander_actor_id);c.field("round",a.round);c.field("tick",a.tick);c.field("accepted",a.accepted);c.field("reason",a.reason);c.field("response_unit_id",a.response_unit_id);c.optional("response_order",a.response_order,[](auto& codec,auto& order){return battle_codec::visit(codec,order);});domain_codec::visit_order(c,a.field_order);if(!c.status().ok)return c.status();auto valid=validate_answer(a);return valid.ok?c.status():c.reject(valid.error.original.code,valid.error.original.path);}

}
