#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
// 地方会话独立追加到新版本interaction域；旧域字节和原会话字段顺序不变。
#include "regional_life_dialogue.hpp"
#include "interaction_projection.hpp"
#include "resident/domains/tasks/task_projection.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
#include "interaction_codec.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// 合同结果逐字段保存实际引用；回执可空但不得被默认合成。
Status emit_regional_outcome(ProjectionWriter& w,const RegionalTaskOutcomeRef& r){w.field("task_id",r.task_id);w.field("contract_id",r.contract_id);w.field("task_template_id",r.task_template_id);w.field("delivery_fact_id",r.delivery_fact_id);w.field("transfer_fact_id",r.transfer_fact_id);w.field("settlement_id",r.settlement_id);w.field("receipt_id",r.receipt_id);w.field("action_id",r.action_id);return w.field("state_digest",r.state_digest);}
RegionalTaskOutcomeRef decode_outcome(const V& v){
    using namespace boundary;fields(v,{"task_id","contract_id","task_template_id","delivery_fact_id","transfer_fact_id","settlement_id","receipt_id","action_id","state_digest"},{},"task_outcome");RegionalTaskOutcomeRef r;
    for(const auto& entry:{std::pair<const char*,StableId*>{"task_id",&r.task_id},{"contract_id",&r.contract_id},{"task_template_id",&r.task_template_id},{"delivery_fact_id",&r.delivery_fact_id},{"transfer_fact_id",&r.transfer_fact_id},{"settlement_id",&r.settlement_id},{"receipt_id",&r.receipt_id},{"action_id",&r.action_id},{"state_digest",&r.state_digest}})*entry.second=get_string(v,entry.first,"task_outcome");return r;
}
// 新来源只在interaction4内部写这组字段，原source的检查点顺序逐字保持。
Status visit_outcome(CodecVisitor& c,RegionalTaskOutcomeRef& r){
    for(const auto& entry:{std::pair<const char*,StableId*>{"task_id",&r.task_id},{"contract_id",&r.contract_id},{"task_template_id",&r.task_template_id},{"delivery_fact_id",&r.delivery_fact_id},{"transfer_fact_id",&r.transfer_fact_id},{"settlement_id",&r.settlement_id},{"receipt_id",&r.receipt_id},{"action_id",&r.action_id},{"state_digest",&r.state_digest}})c.field(entry.first,*entry.second);return c.status();
}
// 来源只含固定事实/报告或原OfferRef，不接受可执行任意字段。
Status emit_regional_source(ProjectionWriter& w,const RegionalSourceRef& s){w.text("kind",s.kind==RegionalSourceKind::life_result?"life_result":s.kind==RegionalSourceKind::runtime_offer?"runtime_offer":"task_outcome");w.field("fact_id",s.fact_id);w.field("report_id",s.report_id);w.field("need_id",s.need_id);w.field("need_revision",s.need_revision);if(s.offer_ref)w.object("offer_ref",[&](auto& o){o.text("kind",s.offer_ref->kind==OfferRef::Kind::runtime?"runtime":"authored");return o.field("id",s.offer_ref->id);});if(s.task_outcome)w.object("task_outcome",[&](auto& o){return emit_regional_outcome(o,*s.task_outcome);});return w.status();}
RegionalSourceRef decode_source(const V& value){
    using namespace boundary;fields(value,{"kind","fact_id","report_id","need_id","need_revision"},{"offer_ref","task_outcome"},"regional_source");RegionalSourceRef s;
    const auto kind=get_string(value,"kind","regional_source");if(kind=="life_result")s.kind=RegionalSourceKind::life_result;else if(kind=="runtime_offer")s.kind=RegionalSourceKind::runtime_offer;else if(kind=="task_outcome")s.kind=RegionalSourceKind::task_outcome;else fail("InvalidRegionalSource","kind");
    s.fact_id=get_string(value,"fact_id","source");s.report_id=get_string(value,"report_id","source");s.need_id=get_string(value,"need_id","source");s.need_revision=get_int(value,"need_revision","source");
    if(const auto* ref=value.find("offer_ref")){fields(*ref,{"kind","id"},{},"offer_ref");const auto tag=get_string(*ref,"kind","offer_ref");if(tag!="runtime"&&(tag!="authored"||s.kind!=RegionalSourceKind::task_outcome))fail("InvalidRegionalSource","offer_ref");s.offer_ref=OfferRef{tag=="runtime"?OfferRef::Kind::runtime:OfferRef::Kind::authored,get_string(*ref,"id","offer_ref")};}
    if(const auto* outcome=value.find("task_outcome"))s.task_outcome=decode_outcome(*outcome);return s;
}
Status emit_regional_binding(ProjectionWriter& w,const RegionalOpportunityBinding& b){w.field("binding_id",b.binding_id);w.field("topic_id",b.topic_id);w.field("dialogue_id",b.dialogue_id);w.field("observer_actor_id",b.observer_actor_id);w.field("provider_actor_id",b.provider_actor_id);w.field("role_id",b.role_id);w.field("place_id",b.place_id);w.object("source",[&](auto& o){return emit_regional_source(o,b.source);});if(b.task_command)w.object("task_command",[&](auto& o){return emit_task_command_fields(o,*b.task_command);});return w.status();}
RegionalOpportunityBinding decode_binding(const V& value){
    using namespace boundary;fields(value,{"binding_id","topic_id","dialogue_id","observer_actor_id","provider_actor_id","role_id","place_id","source"},{"task_command"},"regional_binding");RegionalOpportunityBinding b;
    b.binding_id=get_string(value,"binding_id","binding");b.topic_id=get_string(value,"topic_id","binding");b.dialogue_id=get_string(value,"dialogue_id","binding");b.observer_actor_id=get_string(value,"observer_actor_id","binding");b.provider_actor_id=get_string(value,"provider_actor_id","binding");b.role_id=get_string(value,"role_id","binding");b.place_id=get_string(value,"place_id","binding");b.source=decode_source(field(value,"source","binding"));
    if(const auto* task=value.find("task_command")){fields(*task,{"context","offer_ref","need_revision","quantity"},{"offer_id","kind"},"regional_task");auto decoded=decode_task_command("accept_offer",*task);if(!decoded.ok)fail(decoded.error.original.code,decoded.error.original.path);b.task_command=checkpoint_make_shared<const TaskCommand>(std::move(decoded.value));}return b;
}
// 检查点逐字段保存原typed引用，禁用profile不调用此函数。
Status visit_source(CodecVisitor& c,RegionalSourceRef& s){
    Integer kind=static_cast<Integer>(s.kind);c.field("kind",kind);if(kind<0||kind>2)return c.reject("InvalidRegionalSource");if(c.reading())s.kind=static_cast<RegionalSourceKind>(kind);
    c.field("fact_id",s.fact_id);c.field("report_id",s.report_id);c.field("need_id",s.need_id);c.field("need_revision",s.need_revision);
    c.optional("offer_ref",s.offer_ref,[&](auto& v,auto& ref){Integer tag=static_cast<Integer>(ref.kind);v.field("kind",tag);if(tag!=1&&(tag!=0||s.kind!=RegionalSourceKind::task_outcome))return v.reject("InvalidRegionalSource");if(v.reading())ref.kind=static_cast<OfferRef::Kind>(tag);return v.field("id",ref.id);});
    if(s.kind==RegionalSourceKind::task_outcome){if(c.reading())s.task_outcome=RegionalTaskOutcomeRef{};if(!s.task_outcome)return c.reject("MissingRegionalOutcome");c.object("task_outcome",[&](auto& v){return visit_outcome(v,*s.task_outcome);});}else if(c.reading())s.task_outcome.reset();return c.status();
}
Status visit_binding(CodecVisitor& c,RegionalOpportunityBinding& b){
    c.field("binding_id",b.binding_id);c.field("topic_id",b.topic_id);c.field("dialogue_id",b.dialogue_id);c.field("observer_actor_id",b.observer_actor_id);c.field("provider_actor_id",b.provider_actor_id);c.field("role_id",b.role_id);c.field("place_id",b.place_id);c.object("source",[&](auto& v){return visit_source(v,b.source);});
    bool has_task=bool(b.task_command);c.field("has_task",has_task);
    if(has_task){TaskCommand decoded_task;TaskCommand& task=c.reading()?decoded_task:const_cast<TaskCommand&>(*b.task_command);c.object("task",[&](auto& v){v.field("kind",task.kind);v.field("offer_id",task.offer_id);v.object("context",[&](auto& vc){return interaction_codec::visit_context(vc,task.context);});v.optional("source_kind",task.source_kind,[](auto& vc,auto& x){return vc.field("value",x);});v.optional("need_revision",task.need_revision,[](auto& vc,auto& x){return vc.field("value",x);});v.optional("quantity",task.quantity,[](auto& vc,auto& x){return vc.field("value",x);});v.optional("offer_ref",task.offer_ref,[](auto& vc,auto& x){Integer tag=static_cast<Integer>(x.kind);vc.field("kind",tag);if(tag!=1)return vc.reject("InvalidRegionalTaskCommand");if(vc.reading())x.kind=OfferRef::Kind::runtime;return vc.field("id",x.id);});domain_codec::visit_order(v,task.field_order);return v.status();});if(c.reading())b.task_command=checkpoint_make_shared<const TaskCommand>(std::move(task));}else if(c.reading())b.task_command.reset();return c.status();
}
struct SavedBinding {StableId actor_id,session_id;RegionalOpportunityBinding binding;bool committed=false;};
}
// 导出只增加一个显式root，既有会话的JSON形状完全沿旧编码器。
Status emit_regional_interaction_fields(ProjectionWriter& out,const InteractionState& state){
    if(!state.regional)return out.status();out.field("version",state.regional->version);
    auto selected=[](const auto& s){return bool(s.regional_binding);};dynamic_codec::FilteredRows bindings{state.interactions,selected};
    out.sequence("bindings",bindings,[](auto& w,const auto& s){return w.object("",[&](auto& o){o.field("actor_id",s.actor_id);o.field("session_id",s.id);o.object("binding",[&](auto& b){return emit_regional_binding(b,*s.regional_binding);});return o.field("action_committed",s.regional_action_committed);});});
    out.sequence("exposures",state.regional->exposures,[](auto& w,const auto& e){return w.object("",[&](auto& o){o.field("observer_actor_id",e.observer_actor_id);o.field("binding_id",e.binding_id);o.field("episode",e.episode);o.field("last_tick",e.last_tick);return o.object("source",[&](auto& q){return emit_regional_source(q,e.source);});});});return out.status();
}
BoundaryValue encode_regional_interaction(const InteractionState& state){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_regional_interaction_fields(out,state);});if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
Status decode_regional_interaction(const BoundaryValue& root,InteractionState& state){
    const auto* value=root.find("regional_life");if(!value)return common_success();
    try{
        using namespace boundary;fields(*value,{"version","bindings","exposures"},{},"regional_life");RegionalInteractionState regional;regional.version=get_int(*value,"version","regional_life");if(regional.version!=1&&regional.version!=2)fail("UnsupportedRegionalLifeState","regional_life");
        std::set<StableId> actors;
        for(const auto& row:array(field(*value,"bindings","regional_life"),"bindings")){fields(row,{"actor_id","session_id","binding","action_committed"},{},"binding");const auto actor=get_string(row,"actor_id","binding"),id=get_string(row,"session_id","binding");if(!actors.insert(actor).second)fail("DuplicateRegionalBinding",actor);auto found=std::find_if(state.interactions.begin(),state.interactions.end(),[&](const auto& s){return s.actor_id==actor&&s.id==id;});if(found==state.interactions.end())fail("MissingRegionalSession",id);found->regional_binding=decode_binding(field(row,"binding","session"));found->regional_action_committed=get_bool(row,"action_committed","session");}
        for(const auto& row:array(field(*value,"exposures","regional_life"),"exposures")){fields(row,{"observer_actor_id","binding_id","episode","last_tick","source"},{},"exposure");regional.exposures.push_back({get_string(row,"observer_actor_id","exposure"),get_string(row,"binding_id","exposure"),get_string(row,"episode","exposure"),get_int(row,"last_tick","exposure"),decode_source(field(row,"source","exposure"))});}
        state.regional=std::move(regional);return common_success();
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
// 新interaction3在原字段后追加有限绑定；引用原session ID拒绝绑定到另一次会话。
Status visit_regional_interaction(CodecVisitor& c,InteractionState& state){
    if(c.reading())state.regional=RegionalInteractionState{};if(!state.regional)return c.reject("MissingRegionalLifeState");
    c.field("regional_version",state.regional->version);if(state.regional->version!=1&&state.regional->version!=2)return c.reject("UnsupportedRegionalLifeState");
    if(!c.reading()){auto selected=[](const auto& s){return bool(s.regional_binding);};dynamic_codec::FilteredRows bindings{state.interactions,selected};c.write_sequence("regional_bindings",bindings,[](auto& v,const auto& s){auto& row=const_cast<InteractionSession&>(s);v.field("actor_id",row.actor_id);v.field("session_id",row.id);v.object("binding",[&](auto& vc){return visit_binding(vc,*row.regional_binding);});v.field("action_committed",row.regional_action_committed);return v.status();});}
    std::vector<SavedBinding> bindings;if(c.reading())c.sequence("regional_bindings",bindings,[](auto& v,auto& row){v.field("actor_id",row.actor_id);v.field("session_id",row.session_id);v.object("binding",[&](auto& vc){return visit_binding(vc,row.binding);});v.field("action_committed",row.committed);return v.status();});
    if(c.reading()){std::set<StableId> actors;for(auto& row:bindings){if(!actors.insert(row.actor_id).second)return c.reject("DuplicateRegionalBinding");auto found=std::find_if(state.interactions.begin(),state.interactions.end(),[&](const auto& s){return s.actor_id==row.actor_id&&s.id==row.session_id;});if(found==state.interactions.end())return c.reject("MissingRegionalSession");found->regional_binding=std::move(row.binding);found->regional_action_committed=row.committed;}}
    c.sequence("regional_exposures",state.regional->exposures,[](auto& v,auto& e){v.field("observer_actor_id",e.observer_actor_id);v.field("binding_id",e.binding_id);v.field("episode",e.episode);v.field("last_tick",e.last_tick);v.object("source",[&](auto& vc){return visit_source(vc,e.source);});return v.status();});return c.status();
}
}
