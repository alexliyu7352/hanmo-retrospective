#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "family_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
// 计划、撤回和完成事实分开保存；旧家事格式没有新variant或隐式存在位。
#include "voluntary_succession_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 可选字段仅在真实存在时纳入键序，坏档不能漏写或重复终态引用。
bool valid_order(const FieldOrder& order,const std::set<StableId>& expected){return order.empty()||(order.size()==expected.size()&&std::set<StableId>(order.begin(),order.end())==expected);}
// 终态形状明确，不以未知kind退化为“取消”。
Status valid_conclusion(const SuccessionPlanConclusion& value){if(value.tick<0||(value.kind!="cancelled"&&value.kind!="completed")||(value.kind=="completed")!=value.succession_fact_id.has_value()||(value.succession_fact_id&&value.succession_fact_id->empty()))return common_failure("InvalidSuccessionPlanConclusion");return common_success();}
// 只有已完成的终态可带唯一交接事实引用。
SuccessionPlanConclusion decode_conclusion(const V& value){using namespace boundary;fields(value,{"kind","tick"},{"succession_fact_id"},"succession/conclusion");SuccessionPlanConclusion result;result.kind=get_string(value,"kind","");result.tick=get_int(value,"tick","");if(const auto* id=value.find("succession_fact_id"))result.succession_fact_id=string(*id,"succession_fact_id");result.field_order=order(value);auto valid=valid_conclusion(result);if(!valid.ok)fail(valid.error.original.code,"succession/conclusion");return result;}
// 原终态直接导出，不因再次打开页面产生新日期。

// typed终态核完整键序与形状，业务历史再核原计划及Common事实先后。
Status visit_conclusion(CodecVisitor& c,SuccessionPlanConclusion& value){c.field("kind",value.kind);c.field("tick",value.tick);c.optional("succession_fact_id",value.succession_fact_id,[](CodecVisitor& p,StableId& id){return p.field("value",id);});common_checkpoint::strings(c,"field_order",value.field_order);if(!c.status().ok)return c.status();auto keys=std::set<StableId>{"kind","tick"};if(value.succession_fact_id)keys.insert("succession_fact_id");if(!valid_order(value.field_order,keys))return c.reject("BoundaryFieldOrderMismatch");auto valid=valid_conclusion(value);return valid.ok?c.status():c.reject(valid.error.original.code);}
}
// 同意与最终交割分离，未完成计划没有杜撰的completed_tick。
Outcome<SuccessionConsentFact> decode_succession_consent_fact(const BoundaryValue& value){try{auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};using namespace boundary;fields(value,{"kind","agreement"},{"conclusion"},"succession_consent_fact");SuccessionConsentFact result;result.kind=get_string(value,"kind","");if(result.kind!="family_succession_consent")fail("InvalidSuccessionPlanFact","");auto source=decode_succession_consent_source(field(value,"agreement",""));if(!source.ok)return {false,{},source.error};result.agreement=std::move(source.value);if(const auto* end=value.find("conclusion"))result.conclusion=decode_conclusion(*end);result.field_order=order(value);return {true,std::move(result),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 导出完整接受条款及实际终态，不把未交割计划混入成功接续链。
Status emit_succession_consent_fact_fields(ProjectionWriter& out,const SuccessionConsentFact& value){out.field("kind",value.kind);out.object("agreement",[&](auto& w){return emit_succession_consent_source_fields(w,value.agreement);});if(value.conclusion)out.object("conclusion",value.conclusion->field_order,[&](auto& w){const auto& c=*value.conclusion;w.field("kind",c.kind);w.field("tick",c.tick);if(c.succession_fact_id)w.field("succession_fact_id",*c.succession_fact_id);return w.status();});return out.order_fields(value.field_order);}
BoundaryValue encode_succession_consent_fact(const SuccessionConsentFact& value){return family_projection::encode([&](auto& w){return emit_succession_consent_fact_fields(w,value);});}
// 新variant只有显式新版Family reader才能调用。
namespace {
Status visit_consent_fact(CodecVisitor& c,SuccessionConsentFact& value,bool paths){c.field("kind",value.kind);c.object("agreement",[&](CodecVisitor& p){return paths?visit_succession_consent_source_paths(p,value.agreement):visit_succession_consent_source(p,value.agreement);});c.optional("conclusion",value.conclusion,visit_conclusion);common_checkpoint::strings(c,"field_order",value.field_order);if(!c.status().ok)return c.status();if(value.kind!="family_succession_consent")return c.reject("InvalidSuccessionPlanFact");auto keys=std::set<StableId>{"kind","agreement"};if(value.conclusion)keys.insert("conclusion");return valid_order(value.field_order,keys)?c.status():c.reject("BoundaryFieldOrderMismatch");}
}
Status visit_succession_consent_fact(CodecVisitor& c,SuccessionConsentFact& value){return visit_consent_fact(c,value,false);}
Status visit_succession_consent_fact_paths(CodecVisitor& c,SuccessionConsentFact& value){return visit_consent_fact(c,value,true);}
// 最终交接引用既有同意事实，不复制资产条款造成两份可修改的真值。
Outcome<VoluntarySuccessionFact> decode_voluntary_succession_fact(const BoundaryValue& value){try{auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};using namespace boundary;fields(value,{"kind","consent_fact_id","tick"},{},"voluntary_succession_fact");VoluntarySuccessionFact result;result.kind=get_string(value,"kind","");result.consent_fact_id=get_string(value,"consent_fact_id","");result.tick=get_int(value,"tick","");result.field_order=order(value);if(result.kind!="family_voluntary_succession"||result.consent_fact_id.empty()||result.tick<0)fail("InvalidVoluntarySuccessionFact","");return {true,std::move(result),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 结果日期是实际换人时刻，前代收养或同意日期不会被改写。
Status emit_voluntary_succession_fact_fields(ProjectionWriter& out,const VoluntarySuccessionFact& value){out.field("kind",value.kind);out.field("consent_fact_id",value.consent_fact_id);out.field("tick",value.tick);return out.order_fields(value.field_order);}
BoundaryValue encode_voluntary_succession_fact(const VoluntarySuccessionFact& value){return family_projection::encode([&](auto& w){return emit_voluntary_succession_fact_fields(w,value);});}
// 冷读取拒绝未知kind、空来源及非法字段序。
Status visit_voluntary_succession_fact(CodecVisitor& c,VoluntarySuccessionFact& value){c.field("kind",value.kind);c.field("consent_fact_id",value.consent_fact_id);c.field("tick",value.tick);common_checkpoint::strings(c,"field_order",value.field_order);if(!c.status().ok)return c.status();if(value.kind!="family_voluntary_succession"||value.consent_fact_id.empty()||value.tick<0)return c.reject("InvalidVoluntarySuccessionFact");return valid_order(value.field_order,{"kind","consent_fact_id","tick"})?c.status():c.reject("BoundaryFieldOrderMismatch");}
}
