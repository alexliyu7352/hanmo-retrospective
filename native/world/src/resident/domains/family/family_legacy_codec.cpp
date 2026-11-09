// 遗留控制三账的独立严格冷边界；不存在资产默认值、隐式接受或丢弃未知字段。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "family_legacy.hpp"
#include "succession_personal_assets.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
#include "family_extension_projection.hpp"
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
enum class LegacyFormat { original1,biological2,collateral3,declared_age4 };
// 只有空键序或完整唯一键序合法，禁止冷恢复替损坏来源重新排序。
bool ordered(const FieldOrder& value,const std::set<StableId>& keys){return value.empty()||(value.size()==keys.size()&&std::set<StableId>(value.begin(),value.end())==keys);}
// 引用格式的具体实例号和先后由业务账验证，此层先拒空值与超长值。
bool reference(const StableId& id){return !id.empty()&&id.size()<=256;}
// 三类永久事实不能互相冒名，也不能把撤回节点解码为同意。
Status shape(const FamilyLegacyConsentFact& f){return f.kind=="family_legacy_control_consent"&&f.source.decision=="accept"&&ordered(f.field_order,{"kind","source"})?common_success():common_failure("InvalidLegacyControlConsentFact");}
Status shape(const FamilyLegacyRevocationFact& f){return f.kind=="family_legacy_control_revocation"&&f.source.decision=="revoke"&&reference(f.consent_fact_id)&&ordered(f.field_order,{"kind","consent_fact_id","source"})?common_success():common_failure("InvalidLegacyControlRevocation");}
Status shape(const FamilyLegacyActivationFact& f){return f.kind=="family_legacy_control_activated"&&reference(f.consent_fact_id)&&reference(f.death_fact_id)&&f.tick>=0&&ordered(f.field_order,{"kind","consent_fact_id","death_fact_id","tick"})?common_success():common_failure("InvalidLegacyControlActivation");}
// 嵌套来源使用同一受检JSON解码口，首错保持，不接受部分填充的决定。
LegacyControlDecisionSource source(const V& v,LegacyFormat format){auto decoded=format==LegacyFormat::original1?decode_legacy_control_decision_source(v):format==LegacyFormat::biological2?decode_legacy_control_decision_source_v2(v):format==LegacyFormat::collateral3?decode_legacy_control_decision_source_v3(v):decode_legacy_control_decision_source_v4(v);if(!decoded.ok)boundary::fail(decoded.error.original.code,decoded.error.original.path);return std::move(decoded.value);}
// 已知键集不包含任何资产或授职字段。
FamilyLegacyConsentFact consent(const V& v,LegacyFormat format){using namespace boundary;fields(v,{"kind","source"},{},"legacy/consent");FamilyLegacyConsentFact f;f.kind=get_string(v,"kind","");f.source=source(field(v,"source",""),format);f.field_order=order(v);return f;}
FamilyLegacyRevocationFact revocation(const V& v,LegacyFormat format){using namespace boundary;fields(v,{"kind","consent_fact_id","source"},{},"legacy/revocation");FamilyLegacyRevocationFact f;f.kind=get_string(v,"kind","");f.consent_fact_id=get_string(v,"consent_fact_id","");f.source=source(field(v,"source",""),format);f.field_order=order(v);return f;}
FamilyLegacyActivationFact activation(const V& v){using namespace boundary;fields(v,{"kind","consent_fact_id","death_fact_id","tick"},{},"legacy/activation");FamilyLegacyActivationFact f;f.kind=get_string(v,"kind","");f.consent_fact_id=get_string(v,"consent_fact_id","");f.death_fact_id=get_string(v,"death_fact_id","");f.tick=get_int(v,"tick","");f.field_order=order(v);return f;}
// 输出保原来源，不从当前玩家反填接受人。
V encode(const FamilyLegacyConsentFact& f,LegacyFormat format){return family_projection::encode([&](auto& w){return emit_family_legacy_consent_fields(w,f,static_cast<Integer>(format)+1);});}
V encode(const FamilyLegacyRevocationFact& f,LegacyFormat format){return family_projection::encode([&](auto& w){return emit_family_legacy_revocation_fields(w,f,static_cast<Integer>(format)+1);});}
V encode(const FamilyLegacyActivationFact& f,LegacyFormat format){return family_projection::encode([&](auto& w){return emit_family_legacy_activation_fields(w,f,static_cast<Integer>(format)+1);});}
// typed与JSON共享相同形状约束；截断及未知标签由原visitor拒绝。
Status visit(CodecVisitor& c,FamilyLegacyConsentFact& f,LegacyFormat format){c.field("kind",f.kind);c.object("source",[&](CodecVisitor& q){return format==LegacyFormat::original1?visit_legacy_control_decision_source(q,f.source):format==LegacyFormat::biological2?visit_legacy_control_decision_source_v2(q,f.source):format==LegacyFormat::collateral3?visit_legacy_control_decision_source_v3(q,f.source):visit_legacy_control_decision_source_v4(q,f.source);});common_checkpoint::strings(c,"field_order",f.field_order);if(!c.status().ok)return c.status();auto v=shape(f);return v.ok?c.status():c.reject(v.error.original.code);}
Status visit(CodecVisitor& c,FamilyLegacyRevocationFact& f,LegacyFormat format){c.field("kind",f.kind);c.field("consent_fact_id",f.consent_fact_id);c.object("source",[&](CodecVisitor& q){return format==LegacyFormat::original1?visit_legacy_control_decision_source(q,f.source):format==LegacyFormat::biological2?visit_legacy_control_decision_source_v2(q,f.source):format==LegacyFormat::collateral3?visit_legacy_control_decision_source_v3(q,f.source):visit_legacy_control_decision_source_v4(q,f.source);});common_checkpoint::strings(c,"field_order",f.field_order);if(!c.status().ok)return c.status();auto v=shape(f);return v.ok?c.status():c.reject(v.error.original.code);}
Status visit(CodecVisitor& c,FamilyLegacyActivationFact& f,LegacyFormat){c.field("kind",f.kind);c.field("consent_fact_id",f.consent_fact_id);c.field("death_fact_id",f.death_fact_id);c.field("tick",f.tick);common_checkpoint::strings(c,"field_order",f.field_order);if(!c.status().ok)return c.status();auto v=shape(f);return v.ok?c.status():c.reject(v.error.original.code);}
// 结构解码不静默覆盖相同ID；业务validator另外核跨表唯一与Common头。
template<class T,class Read> void decode_table(const V& v,OrderedTable<T>& table,Read read){for(const auto& row:boundary::object(v,"legacy/table")){auto f=read(row.second);auto valid=shape(f);if(!valid.ok)boundary::fail(valid.error.original.code,valid.error.original.path);if(!reference(row.first)||!table.insert(row.first,std::move(f)))boundary::fail("InvalidFamilyLegacyFact",row.first);}}
// 编码也拒绝内存中的坏形状，而非等下一次冷读才暴露。
template<class T> V encode_table(const OrderedTable<T>& table,LegacyFormat format){V::Dict out;for(const auto& row:table.rows()){auto valid=shape(row.second);if(!valid.ok)boundary::fail(valid.error.original.code,valid.error.original.path);if(!reference(row.first))boundary::fail("InvalidFamilyLegacyFact",row.first);out.emplace_back(row.first,encode(row.second,format));}return V{std::move(out)};}
}
namespace {
// 递归唯一键检查在任何first-match字段读取前执行。
Outcome<FamilyLegacyState> decode_state(const BoundaryValue& value,LegacyFormat format){try{auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};using namespace boundary;fields(value,{"consents","revocations","activations"},{},"family_legacy");FamilyLegacyState out;decode_table(field(value,"consents",""),out.consents,[&](const V& v){return consent(v,format);});decode_table(field(value,"revocations",""),out.revocations,[&](const V& v){return revocation(v,format);});decode_table(field(value,"activations",""),out.activations,activation);out.field_order=order(value);return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原插入序完整保持，旧域与未启用profile从不调用本路径。
BoundaryValue encode_state(const FamilyLegacyState& state,LegacyFormat format){return family_projection::encode([&](auto& w){return emit_family_legacy_fields(w,state,static_cast<Integer>(format)+1);});}
// family_legacy/1固定三表次序，末尾字节和字段元数据仍由原封套严格验证。
Status visit_state(CodecVisitor& c,FamilyLegacyState& s,LegacyFormat format){common_checkpoint::table(c,"consents",s.consents,[&](CodecVisitor& v,FamilyLegacyConsentFact& f){return visit(v,f,format);});common_checkpoint::table(c,"revocations",s.revocations,[&](CodecVisitor& v,FamilyLegacyRevocationFact& f){return visit(v,f,format);});common_checkpoint::table(c,"activations",s.activations,[&](CodecVisitor& v,FamilyLegacyActivationFact& f){return visit(v,f,format);});common_checkpoint::strings(c,"field_order",s.field_order);if(!c.status().ok)return c.status();return ordered(s.field_order,{"consents","revocations","activations"})?c.status():c.reject("BoundaryFieldOrderMismatch");}
}
Status emit_family_legacy_consent_fields(ProjectionWriter& w,const FamilyLegacyConsentFact& f,Integer version){w.field("kind",f.kind);w.object("source",[&](auto& out){return emit_legacy_control_decision_source_fields(out,f.source,version);});return w.order_fields(f.field_order);}
Status emit_family_legacy_revocation_fields(ProjectionWriter& w,const FamilyLegacyRevocationFact& f,Integer version){w.field("kind",f.kind);w.field("consent_fact_id",f.consent_fact_id);w.object("source",[&](auto& out){return emit_legacy_control_decision_source_fields(out,f.source,version);});return w.order_fields(f.field_order);}
Status emit_family_legacy_activation_fields(ProjectionWriter& w,const FamilyLegacyActivationFact& f,Integer version){if(version<1||version>4)return w.reject("UnsupportedFamilyLegacyVersion");w.field("kind",f.kind);w.field("consent_fact_id",f.consent_fact_id);w.field("death_fact_id",f.death_fact_id);w.field("tick",f.tick);return w.order_fields(f.field_order);}
Status emit_family_legacy_fields(ProjectionWriter& w,const FamilyLegacyState& state,Integer version){if(version<1||version>4)return w.reject("UnsupportedFamilyLegacyVersion");if(!ordered(state.field_order,{"consents","revocations","activations"}))return w.reject("BoundaryFieldOrderMismatch","family_legacy");auto table=[&](std::string_view name,const auto& rows,auto emit){return w.object(name,[&](auto& out){for(const auto& row:rows.rows()){auto valid=shape(row.second);if(!valid.ok)return out.reject(valid.error.original.code,valid.error.original.path);if(!reference(row.first))return out.reject("InvalidFamilyLegacyFact",row.first);out.object(row.first,[&](auto& at){return emit(at,row.second,version);});if(!out.status().ok)return out.status();}return out.status();});};table("consents",state.consents,emit_family_legacy_consent_fields);table("revocations",state.revocations,emit_family_legacy_revocation_fields);table("activations",state.activations,emit_family_legacy_activation_fields);return w.order_fields(state.field_order);}
Outcome<FamilyLegacyState> decode_family_legacy(const BoundaryValue& v){return decode_state(v,LegacyFormat::original1);}
BoundaryValue encode_family_legacy(const FamilyLegacyState& s){return encode_state(s,LegacyFormat::original1);}
Status visit_family_legacy_checkpoint(CodecVisitor& c,FamilyLegacyState& s){return visit_state(c,s,LegacyFormat::original1);}
Outcome<FamilyLegacyState> decode_family_legacy_v2(const BoundaryValue& v){return decode_state(v,LegacyFormat::biological2);}
BoundaryValue encode_family_legacy_v2(const FamilyLegacyState& s){return encode_state(s,LegacyFormat::biological2);}
Status visit_family_legacy_checkpoint_v2(CodecVisitor& c,FamilyLegacyState& s){return visit_state(c,s,LegacyFormat::biological2);}
Outcome<FamilyLegacyState> decode_family_legacy_v3(const BoundaryValue& v){return decode_state(v,LegacyFormat::collateral3);}
BoundaryValue encode_family_legacy_v3(const FamilyLegacyState& s){return encode_state(s,LegacyFormat::collateral3);}
Status visit_family_legacy_checkpoint_v3(CodecVisitor& c,FamilyLegacyState& s){return visit_state(c,s,LegacyFormat::collateral3);}
Outcome<FamilyLegacyState> decode_family_legacy_v4(const BoundaryValue& v){return decode_state(v,LegacyFormat::declared_age4);}
BoundaryValue encode_family_legacy_v4(const FamilyLegacyState& s){return encode_state(s,LegacyFormat::declared_age4);}
Status visit_family_legacy_checkpoint_v4(CodecVisitor& c,FamilyLegacyState& s){return visit_state(c,s,LegacyFormat::declared_age4);}

}
