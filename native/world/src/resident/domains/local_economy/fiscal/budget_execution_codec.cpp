#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "budget_execution_codec.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {namespace {
using namespace exact_fields;
// Read-only adapter of the same finite fields below. It neither constructs a
// BoundaryValue subtree nor copies the retained bindings during projection.
class BudgetProjection {
 ProjectionWriter&w_;
public:explicit BudgetProjection(ProjectionWriter&w):w_(w){}
 bool reading()const{return false;}Status status()const{return w_.status();}
 Status reject(const StableId&code,const StableId&path={}){return w_.reject(code,path);}
 Status field(const StableId&name,StableId&v){return w_.field(name,v);}
 Status field(const StableId&name,Integer&v){return w_.field(name,v);}
 template<class Fn>Status object(const StableId&name,Fn fn){return w_.object(name,[&](auto&out){BudgetProjection child(out);return fn(child);});}
 template<class T,class Fn>Status sequence(const StableId&name,std::vector<T>&rows,Fn fn){return w_.sequence(name,rows,[&](auto&out,const T&row){return out.object("",[&](auto&item){BudgetProjection child(item);return fn(child,const_cast<T&>(row));});});}
};
template<class C>Status execution_ref(C& c,BudgetExecutionRef& v){required_ref(c,"budget_id",v.budget_id);c.field("budget_revision",v.budget_revision);c.field("policy_revision",v.policy_revision);return required_ref(c,"approval_fact_id",v.approval_fact_id);}
template<class C>Status unit(C& c,BudgetUnit& v){Integer kind=static_cast<Integer>(v.index());finite(c,"kind",kind,1);if(!c.status().ok)return c.status();if(c.reading()){if(kind==0)v=BudgetMoneyQian{};else v=BudgetItemQuantity{};}if(kind==1)return required_ref(c,"item_id",std::get<BudgetItemQuantity>(v).item_id);return c.status();}
template<class C>Status binding(C& c,BudgetExecutionBinding& v,Integer version){required_ref(c,"id",v.id);c.object("execution_ref",[&](auto& x){return execution_ref(x,v.execution_ref);});c.object("responsibility_ref",[&](auto& x){return source(x,v.responsibility_ref);});schema(c,"responsibility_source_schema",v.responsibility_source_schema);Integer purpose=static_cast<Integer>(v.purpose);finite(c,"purpose",purpose,version==1?5:6);if(!c.status().ok)return c.status();take(validate_budget_execution_purpose(static_cast<BudgetExecutionPurpose>(purpose),version));if(c.reading())v.purpose=static_cast<BudgetExecutionPurpose>(purpose);c.object("unit",[&](auto& x){return unit(x,v.unit);});c.object("period",[&](auto& x){x.field("period_from_tick",v.period.period_from_tick);return x.field("period_until_tick",v.period.period_until_tick);});required_ref(c,"payer_container_id",v.payer_container_id);return c.object("accepted_at",[&](auto& x){return moment(x,v.accepted_at);});}
Status selected_version(Integer version) {
 if(version==1||version==2)return common_success();
 return common_failure("BudgetExecutionVersionMismatch",
     "budget_execution/observed="+std::to_string(version)+"/expected=1|2",
     ErrorKind::unsupported_capability);
}
Status version_pair(Integer observed,Integer expected) {
 auto valid=selected_version(expected);if(!valid.ok)return valid;
 if(observed==expected)return common_success();
 return common_failure("BudgetExecutionVersionMismatch",
     "budget_execution/observed="+std::to_string(observed)+
         "/expected="+std::to_string(expected),ErrorKind::unsupported_capability);
}
template<class C>Status state(C& c,BudgetExecutionState& v,Integer version){
 take(selected_version(version));
 if(!c.reading())take(version_pair(v.schema_version,version));
 // /1 的旧 bindings 序列字节不变；只有明选 /2 才读写新增头。
 if(version==2){
  c.field("schema_version",v.schema_version);
  if(!c.status().ok)return c.status();
  take(version_pair(v.schema_version,2));
 }else take(version_pair(v.schema_version,1));
 return c.sequence("bindings",v.bindings,[&](auto& x,auto& b){return binding(x,b,version);});
}
}
Status emit_budget_execution(ProjectionWriter&w,const BudgetExecutionState&v,Integer version){
 return exact_fields::guarded<Done>([&]{
  take(selected_version(version));take(version_pair(v.schema_version,version));
  take(w.object("",[&](auto&out){BudgetProjection codec(out);return state(codec,const_cast<BudgetExecutionState&>(v),version);}));
  return Done{};
 });
}
Outcome<BoundaryValue> encode_budget_execution(const BudgetExecutionState&v,Integer version){
 return exact_fields::guarded<BoundaryValue>([&]{
  BoundaryValue out;BoundaryProjectionWriter writer(out);
  take(emit_budget_execution(writer,v,version));take(writer.finish());return out;
 });
}
Outcome<BudgetExecutionState> decode_budget_execution(const BoundaryValue&v,Integer version){
 auto valid=selected_version(version);if(!valid.ok)return {false,{},std::move(valid.error)};
 // 原 /1 JSON 没有 schema 头；显式 /2 不能把它自动升版，即使 bindings 为空。
 if(v.as<BoundaryValue::Dict>()){
  const auto* marker=v.find("schema_version");
  if(!marker&&version==2){auto mismatch=version_pair(1,2);return {false,{},std::move(mismatch.error)};}
  if(marker)if(const auto* observed=marker->as<Integer>()){
   valid=version_pair(*observed,version);if(!valid.ok)return {false,{},std::move(valid.error)};
  }
 }
 return exact_fields::decode_json<BudgetExecutionState>(v,[&](auto& c,auto& x){return state(c,x,version);});
}
Status visit_budget_execution(CodecVisitor&c,BudgetExecutionState&v,Integer version){
 auto valid=selected_version(version);if(!valid.ok)return valid;
 return exact_fields::visit_binary(c,v,[&](auto& x,auto& s){return state(x,s,version);});
}
// 旧调用签名只代表 /1，不能从 state 或 purpose 推断并暗升新格式。
Status emit_budget_execution(ProjectionWriter&w,const BudgetExecutionState&v){return emit_budget_execution(w,v,1);}
Outcome<BoundaryValue> encode_budget_execution(const BudgetExecutionState&v){return encode_budget_execution(v,1);}
Outcome<BudgetExecutionState> decode_budget_execution(const BoundaryValue&v){return decode_budget_execution(v,1);}
Status visit_budget_execution(CodecVisitor&c,BudgetExecutionState&v){return visit_budget_execution(c,v,1);}
}
