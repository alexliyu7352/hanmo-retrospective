// 精确附段真实生产codec回归；只证明冷字节边界，不冒充Growth或K2完整接入。
#include "resident/kernel/world_timeline_codec.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
using namespace hanmo::resident;
// 测试独有故障注入以真实分配失败触达生产catch，不给生产codec添加模拟入口。
static long allocation_failure_after=-1;
// 间接调用避免优化器把测试替换new的合法malloc/free实现误报为不同分配器。
static void (*volatile allocation_release)(void*)=std::free;
void* operator new(std::size_t size){if(allocation_failure_after==0){allocation_failure_after=-1;throw std::bad_alloc();}if(allocation_failure_after>0)--allocation_failure_after;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{allocation_release(p);}
void operator delete[](void* p)noexcept{allocation_release(p);}
void operator delete(void* p,std::size_t)noexcept{allocation_release(p);}
void operator delete[](void* p,std::size_t)noexcept{allocation_release(p);}
namespace {
// 断言不受NDEBUG影响，错误始终令本小目标失败。
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
// 测试数值仍通过同一生产精确叶构造。
CanonicalFraction fraction(const char* n,const char* d){auto x=normalize_fraction(n,d);check(x.ok,"fixture fraction");return x.value;}
// 只变更测试拥有的JSON节点，不接触真实世界对象。
BoundaryValue& field(BoundaryValue& v,const char* key){auto* x=v.find(key);check(x!=nullptr,"fixture field");return *x;}
// 原目录身份完整进入LockedRule输入，所有字段都由生产codec访问。
CatalogIdentity catalog(){return {"content","1","digest","rules","schema",StableId{"pack"},{{"growth","36"},{"action_elapsed","2"}},"artifact"};}
// 每种有限联合均有真实typed样本；本测试不宣称这些样本是已应用的游戏来源。
ActionElapsedStore sample(){
    ActionElapsedStore s;
    ActionElapsedState a; a.action_ref="action";a.execution_source_ref="life";a.duration_definition_ref="rest";a.started_at={10,fraction("1","3")};a.required_units={fraction("128","1")};a.credited_units={fraction("32","1")};a.last_settled_at={10,fraction("7","12")};a.timing=ElapsedRunning{{11,fraction("1","3")}};a.owner_cost_state_ref="paid";s.entries.push_back(a);
    a.action_ref="paused";a.timing=ElapsedSuspended{"owner-suspend"};s.entries.push_back(a);
    a.action_ref="done";a.credited_units=a.required_units;a.last_settled_at={11,fraction("1","3")};a.timing=ElapsedTerminal{"life-result"};s.entries.push_back(a);
    ActionElapsedTerminalBinding b;b.action_ref="done";b.action_kind="service";b.actor_id="actor";b.participant_actor_ids={"actor","provider"};b.original_execution_source_ref={"growth","life_activity","done"};b.started_source_ref=MomentOwnerSource{{"growth","growth_fixed_service_boundary","start"},1};b.ended_source_ref=OriginalPeriodSource{{"growth","life_activity","done"},1,11};b.terminal_result_ref="life-result";s.terminal_bindings.push_back(b);
    const FixedServiceInputRef applied=FixedAppliedInput{{"growth","growth_fixed_service_boundary","start"},1};
    const FixedServiceInputRef locked=FixedLockedRuleInput{catalog(),"rest-H1"};
    for(Integer i=0;i<5;++i){FixedServiceTransition t;t.transition_id="transition-"+std::to_string(i);t.action_ref="action";t.transition_ordinal=i;if(i)t.previous_transition_ref="transition-"+std::to_string(i-1);t.kind=static_cast<FixedTransitionKind>(i);t.occurred_at={10,fraction("1","3")};t.execution_source_ref={"growth","life_activity","action"};t.execution_source_schema=1;t.input_basis_refs={applied,locked};if(i)t.previous_input_basis_refs={applied,locked};
        if(i==0)t.cause=FixedOwnerAdopted{{"growth","growth_fixed_service_boundary","start"},1};
        else if(i==1||i==2)t.cause=FixedOwnerStateChanged{{"growth","growth_fixed_service_boundary",i==1?"suspend":"resume"},1,"rule"};
        else if(i==3)t.cause=FixedEligibilityChanged{{"condition","condition_receipt","changed"},2,"rule","life"};
        else {t.cause=FixedOwnerTerminated{{"growth","growth_fixed_service_boundary","end"},1};t.terminal_disposition=FixedTerminalDisposition::cancelled;t.input_basis_refs.clear();}
        s.fixed_service_transitions.push_back(t);
    }
    return s;
}
// 规范JSON与逐字段二进制互转必须逐值和字段序完全相同。
void roundtrip(){
    WorldTimelineState t{{"scale",1,128},fraction("1","3"),"period",StableId{"open"},std::nullopt,PeriodBoundaryState::open};
    auto tj=encode_world_timeline2(t);check(tj.ok,"timeline encode");auto td=decode_world_timeline2(tj.value);check(td.ok,"timeline decode");auto tb=encode_world_timeline2_checkpoint(t);check(tb.ok,"timeline binary encode");auto tr=decode_world_timeline2_checkpoint(tb.value);check(tr.ok,"timeline binary decode");auto tj2=encode_world_timeline2(tr.value);check(tj2.ok&&compare_boundary(tj.value,tj2.value).exact(),"timeline symmetric");
    const auto original=sample();auto json=encode_action_elapsed2(original);check(json.ok,"elapsed encode");check(field(field(std::get<BoundaryValue::Array>(field(json.value,"entries").data).front(),"payload"),"required_units").find("value")!=nullptr,"WorkUnits value retained");auto text=render_boundary_json(json.value);check(text.ok,"render");auto parsed=parse_boundary_json(text.value);check(parsed.ok,"parse");auto decoded=decode_action_elapsed2(parsed.value);check(decoded.ok,"elapsed decode");auto bytes=encode_action_elapsed2_checkpoint(decoded.value);check(bytes.ok,"elapsed binary encode");auto restored=decode_action_elapsed2_checkpoint(bytes.value);check(restored.ok,"elapsed binary decode");auto again=encode_action_elapsed2(restored.value);check(again.ok&&compare_boundary(json.value,again.value).exact(),"all fields symmetric");
    const auto before=bytes.value;auto repeated=encode_action_elapsed2_checkpoint(restored.value);check(repeated.ok&&before==repeated.value,"save has no new event");check(restored.value.fixed_service_transitions.size()==original.fixed_service_transitions.size(),"load has no new event");
    for(int disposition=0;disposition<4;++disposition){auto v=original;v.fixed_service_transitions.back().terminal_disposition=static_cast<FixedTerminalDisposition>(disposition);auto b=encode_action_elapsed2_checkpoint(v);check(b.ok&&decode_action_elapsed2_checkpoint(b.value).ok,"finite disposition");}
}
// 破坏有限tag、规范分数、必需根及重复键时绝不猜默认值。
void json_rejections(){
    auto original=encode_action_elapsed2(sample());check(original.ok,"fixture encode");
    auto reject=[&](BoundaryValue v){check(!decode_action_elapsed2(v).ok,"corrupt elapsed rejected");};
    auto v=original.value;std::get<BoundaryValue::Dict>(v.data).pop_back();reject(v);
    v=original.value;std::get<BoundaryValue::Dict>(v.data).emplace_back("extra",Integer{0});reject(v);
    v=original.value;std::get<BoundaryValue::Dict>(v.data).push_back(std::get<BoundaryValue::Dict>(v.data).front());reject(v);
    v=original.value;field(v,"version")=BoundaryValue{Integer{1}};reject(v);
    v=original.value;std::swap(std::get<BoundaryValue::Dict>(v.data)[0],std::get<BoundaryValue::Dict>(v.data)[1]);reject(v);
    v=original.value;std::get<BoundaryValue::Array>(field(v,"gate_results").data).push_back(BoundaryValue{});reject(v);
    v=original.value;auto& rows=std::get<BoundaryValue::Array>(field(v,"entries").data);rows.push_back(rows.front());reject(v);
    for(const Integer bad:{Integer{-1},Integer{1},Integer{99}}){v=original.value;field(std::get<BoundaryValue::Array>(field(v,"entries").data).front(),"timing_kind")=BoundaryValue{bad};reject(v);}
    v=original.value;auto& payload=field(std::get<BoundaryValue::Array>(field(v,"entries").data).front(),"payload");field(payload,"action_ref")="different";reject(v);
    for(const char* bad:{"01","+1"," 1","1.0","1e0",""}){v=original.value;auto& p=field(std::get<BoundaryValue::Array>(field(v,"entries").data).front(),"payload");field(field(field(p,"credited_units"),"value"),"numerator")=bad;reject(v);}
    v=original.value;auto& p=field(std::get<BoundaryValue::Array>(field(v,"entries").data).front(),"payload");field(field(field(p,"credited_units"),"value"),"numerator")="2";field(field(field(p,"credited_units"),"value"),"denominator")="4";reject(v);
    v=original.value;auto& p2=field(std::get<BoundaryValue::Array>(field(v,"entries").data).front(),"payload");field(field(p2,"timing"),"tag")=BoundaryValue{Integer{9}};reject(v);
    for(const char* name:{"cause","execution_source_ref"}){v=original.value;auto& t=std::get<BoundaryValue::Array>(field(v,"fixed_service_transitions").data).front();std::get<BoundaryValue::Dict>(field(t,name).data).emplace_back("extra",Integer{0});reject(v);}
    v=original.value;auto& t=std::get<BoundaryValue::Array>(field(v,"fixed_service_transitions").data).front();field(field(t,"cause"),"tag")=BoundaryValue{Integer{4}};reject(v);
    v=original.value;auto& t2=std::get<BoundaryValue::Array>(field(v,"fixed_service_transitions").data).front();field(t2,"source_schema")=BoundaryValue{Integer{2}};reject(v);
    v=original.value;auto& t3=std::get<BoundaryValue::Array>(field(v,"fixed_service_transitions").data).front();field(std::get<BoundaryValue::Array>(field(t3,"input_basis_refs").data).front(),"tag")=BoundaryValue{Integer{2}};reject(v);
    v=original.value;auto& t4=std::get<BoundaryValue::Array>(field(v,"fixed_service_transitions").data).front();auto& inputs=std::get<BoundaryValue::Array>(field(t4,"input_basis_refs").data);inputs.push_back(inputs.front());reject(v);
    v=original.value;auto& b=std::get<BoundaryValue::Array>(field(v,"terminal_bindings").data).front();field(field(b,"started_source_ref"),"tag")=BoundaryValue{Integer{2}};reject(v);
    WorldTimelineState state{{"scale",1,128},{},"period",{}, {},PeriodBoundaryState::ready};auto j=encode_world_timeline2(state);check(j.ok,"ready encode");field(field(j.value,"state"),"boundary_state")=BoundaryValue{Integer{2}};check(!decode_world_timeline2(j.value).ok,"unknown boundary rejected");
}
// 原有界visitor拒所有截断、尾随、错字段及未知tag，不在恢复时生成替代数据。
void binary_rejections(){auto b=encode_action_elapsed2_checkpoint(sample());check(b.ok,"binary fixture");for(std::size_t n=0;n<b.value.size();++n){std::vector<std::uint8_t> cut(b.value.begin(),b.value.begin()+n);check(!decode_action_elapsed2_checkpoint(cut).ok,"truncation");}auto tail=b.value;tail.push_back(0);check(!decode_action_elapsed2_checkpoint(tail).ok,"trailing");auto corrupt=b.value;corrupt[0]=255;check(!decode_action_elapsed2_checkpoint(corrupt).ok,"unknown binary type");
    const std::string key="timing_kind";auto it=std::search(corrupt.begin(),corrupt.end(),key.begin(),key.end());check(it!=corrupt.end(),"binary finite tag located");corrupt=b.value;it=std::search(corrupt.begin(),corrupt.end(),key.begin(),key.end());*(it+key.size())=99;check(!decode_action_elapsed2_checkpoint(corrupt).ok,"binary unknown timing kind");}
// 两公开visitor在原binary对象内可组合；失败守住调用者已有值且不需要新世界事件。
void embedded_visitors(){WorldTimelineState t{{"scale",1,128},fraction("9007199254740993","9007199254740995"),"period",StableId{"open"},{},PeriodBoundaryState::open};auto s=sample();s.entries.front().credited_units={fraction("12345678901234567890123456789012345678901","98765432109876543210987654321098765432109")};CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(writer.object("world_timeline",[&](auto& c){return visit_world_timeline2(c,t);}).ok,"embedded timeline write");check(writer.object("action_elapsed",[&](auto& c){return visit_action_elapsed2(c,s);}).ok&&writer.finish().ok,"embedded elapsed write");WorldTimelineState restored;ActionElapsedStore rs;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));check(reader.object("world_timeline",[&](auto& c){return visit_world_timeline2(c,restored);}).ok,"embedded timeline read");check(reader.object("action_elapsed",[&](auto& c){return visit_action_elapsed2(c,rs);}).ok&&reader.finish().ok,"embedded elapsed read");auto a=encode_action_elapsed2(s),b=encode_action_elapsed2(rs);check(a.ok&&b.ok&&compare_boundary(a.value,b.value).exact(),"large rational exact");auto one=encode_world_timeline2_checkpoint(t);check(one.ok,"embedded failure fixture");one.value.pop_back();CodecVisitor broken(static_cast<const CodecVisitor::Buffer&>(one.value));auto prior=encode_world_timeline2(restored);check(!visit_world_timeline2(broken,restored).ok,"failed visitor");auto after=encode_world_timeline2(restored);check(prior.ok&&after.ok&&compare_boundary(prior.value,after.value).exact(),"failed visitor keeps target");}
// 若任何分配失败，公开JSON/binary入口均返回internal，不能抛出或误当业务拒绝。
void allocation_failures(){const auto s=sample();auto j=encode_action_elapsed2(s);auto b=encode_action_elapsed2_checkpoint(s);check(j.ok&&b.ok,"allocation fixtures");for(long fail=0;fail<48;++fail){allocation_failure_after=fail;auto encoded=encode_action_elapsed2(s);allocation_failure_after=-1;check(encoded.ok||encoded.error.kind==ErrorKind::internal,"JSON encode OOM internal");allocation_failure_after=fail;auto decoded=decode_action_elapsed2(j.value);allocation_failure_after=-1;check(decoded.ok||decoded.error.kind==ErrorKind::internal,"JSON decode OOM internal");allocation_failure_after=fail;auto eb=encode_action_elapsed2_checkpoint(s);allocation_failure_after=-1;check(eb.ok||eb.error.kind==ErrorKind::internal,"binary encode OOM internal");allocation_failure_after=fail;auto db=decode_action_elapsed2_checkpoint(b.value);allocation_failure_after=-1;check(db.ok||db.error.kind==ErrorKind::internal,"binary decode OOM internal");}}
}
int main(){try{roundtrip();json_rejections();binary_rejections();embedded_visitors();allocation_failures();std::cout<<"world timeline codec checks passed\n";return 0;}catch(const std::exception& e){allocation_failure_after=-1;std::cerr<<e.what()<<'\n';return 1;}}
