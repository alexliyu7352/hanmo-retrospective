#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "interaction_projection.hpp"
#include "resident/storage/common_projection.hpp"
// 会面/对白/事件的完整冷边界与typed检查点；任何未知动态字段拒绝，不保存任意JSON状态。
#include "hanmo/resident/checkpoint_memory.hpp"
#include "interaction_codec.hpp"
#include "interaction_context_codec_fields.hpp"
#include "dialogue_command_codec_fields.hpp"
#include "resident/domains/tasks/task_offer_revisit.hpp"
#include "regional_life_dialogue.hpp"
#include "resident/views/scene_presentation.hpp"
#include "resident/checkpoint/completion_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include <algorithm>
#include <type_traits>
namespace hanmo::resident {
Status emit_interaction_context_fields(ProjectionWriter& out,const InteractionContext& c){
    if(c.kind)out.field("kind",*c.kind);if(c.encounter_id)out.field("encounter_id",*c.encounter_id);out.field("place_id",c.place_id);
    if(!c.kind){out.field("role_id",c.role_id);out.field("provider_actor_id",c.provider_actor_id);}else{out.field("provider_actor_id",c.provider_actor_id);out.field("role_id",c.role_id);}
    return out.order_fields(c.field_order);
}
namespace interaction_codec {
using namespace domain_codec;
namespace {
InteractionContext decode_original_context(const BoundaryValue& v,std::string_view path){
    boundary::fields(v,{"place_id","provider_actor_id","role_id"},{"kind","encounter_id"},path);
    if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<InteractionContext>();
    else if(auto* budget=ScopedCheckpointBoundaryDecode::current())budget->default_values<InteractionContext>();
    InteractionContext c;
    // All owning text and original key order use the sole boundary helpers.
    // Optional values are move-installed only after their source copy is charged.
    c.field_order=order(v);
    c.place_id=boundary::get_string(v,"place_id",path);
    c.provider_actor_id=boundary::get_string(v,"provider_actor_id",path);
    c.role_id=boundary::get_string(v,"role_id",path);
    if(auto x=v.find("kind"))c.kind=boundary::string(*x,path);
    if(auto x=v.find("encounter_id"))c.encounter_id=boundary::string(*x,path);
    return c;
}
}
InteractionContext decode_context(const BoundaryValue& v,const StableId& path){return decode_original_context(v,path);}
Outcome<InteractionContext> decode_context(const BoundaryValue& v,ScopedCheckpointJsonDecode& budget,std::string_view path){
    static_assert(std::is_nothrow_move_constructible_v<InteractionContext>);
    if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch",path);
    try{return {true,decode_original_context(v,path),{}};}
    catch(boundary::DecodeFailure& failure){return {false,{},std::move(failure.error)};}
}
BoundaryValue encode_context(const InteractionContext& c){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_interaction_context_fields(out,c);});if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
Status visit_context(CodecVisitor& c,InteractionContext& v){return interaction_context_codec::fields(c,v);}
// 回访只在当前会面分支/选择可用；会面前筛选不可借旧会话读取人物记忆。
Condition condition(const V& v,const StableId& path,bool conversation=false){auto op=boundary::get_string(v,"op",path);Condition c;if(op=="flag"){boundary::fields(v,{"op","flag","equals"},{},path);c.kind=Condition::Kind::flag;c.flag=boundary::get_string(v,"flag",path);c.equals=boundary::get_bool(v,"equals",path);}else if(op=="party_member"){boundary::fields(v,{"op","actor_id","equals"},{},path);c.kind=Condition::Kind::party_member;c.actor_id=boundary::get_string(v,"actor_id",path);c.equals=boundary::get_bool(v,"equals",path);}else if(op=="actor_at"){boundary::fields(v,{"op","actor_id","place_id"},{},path);c.kind=Condition::Kind::actor_at;c.actor_id=boundary::get_string(v,"actor_id",path);c.place_id=boundary::get_string(v,"place_id",path);}else if(op=="task_state"){boundary::fields(v,{"op","offer_id","state"},{},path);c.kind=Condition::Kind::task_state;c.offer_id=boundary::get_string(v,"offer_id",path);c.state=boundary::get_string(v,"state",path);}else if(op=="offer_declined_before"){if(!conversation)boundary::fail("OfferRevisitRequiresConversation",path);auto decoded=decode_task_offer_revisit_condition(v);if(!decoded.ok)boundary::fail(decoded.error.original.code,path);c.kind=Condition::Kind::offer_declined_before;c.offer_id=decoded.value.offer_id;c.equals=decoded.value.equals;}else if(op=="treaty_personal_passage"){if(conversation)boundary::fail("TreatyPassageRequiresAdmission",path);boundary::fields(v,{"op","place_id","terms_id"},{},path);c.kind=Condition::Kind::treaty_personal_passage;c.place_id=boundary::get_string(v,"place_id",path);c.offer_id=boundary::get_string(v,"terms_id",path);}else if(op=="relation_min"){boundary::fields(v,{"op","from_actor","to_actor","trust"},{},path);c.kind=Condition::Kind::relation_min;c.from_actor=boundary::get_string(v,"from_actor",path);c.to_actor=boundary::get_string(v,"to_actor",path);c.trust=boundary::get_int(v,"trust",path);}else if(op=="all"||op=="any"){boundary::fields(v,{"op","terms"},{},path);c.kind=op=="all"?Condition::Kind::all:Condition::Kind::any;for(auto& term:boundary::array(boundary::field(v,"terms",path),path))c.terms.push_back(condition(term,path+"/terms",conversation));}else boundary::fail("UnsupportedCondition",path);return c;}
std::vector<Condition> conditions(const V& v,const StableId& path,bool conversation=false){std::vector<Condition> r;for(auto& term:boundary::array(v,path))r.push_back(condition(term,path,conversation));return r;}
DialogueCommand command(const V& v,const StableId& path){boundary::fields(v,{"kind"},{"offer_id","shop_id","service_id","target_actor_id","activity","skill_id","place_id","context"},path);DialogueCommand c;c.field_order=order(v);c.kind=boundary::get_string(v,"kind",path);
#define READ(name) if(auto x=v.find(#name))c.name=boundary::string(*x,path+"/"+#name)
    READ(offer_id);READ(shop_id);READ(service_id);READ(target_actor_id);READ(activity);READ(skill_id);READ(place_id);
#undef READ
    if(auto x=v.find("context"))c.context=decode_context(*x,path+"/context");return c;
}
V encode_command(const DialogueCommand& c){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_dialogue_command_fields(out,c);});if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
Status visit_command(CodecVisitor& c,DialogueCommand& p){return dialogue_command_codec::fields(c,p);}
// 持久位置检查点以明确标签存储每种真实字段；没有浮点回写或不透明location字典。
Status visit_location(CodecVisitor& c,Location& l){Integer tag=l.index();c.field("kind",tag);if(tag<0||tag>4)return c.reject("InvalidLocation");if(c.reading()){switch(tag){case 0:l=SettlementLocation{};break;case 1:l=WildernessLocation{};break;case 2:l=FollowingLocation{};break;case 3:l=InArmyLocation{};break;default:l=OnRouteLocation{};}}
    if(auto x=std::get_if<SettlementLocation>(&l)){c.field("settlement_id",x->settlement_id);c.field("place_id",x->place_id);}else if(auto x=std::get_if<WildernessLocation>(&l)){c.field("map_id",x->map_id);c.field("origin",x->origin);c.field("x",x->point.x);c.field("y",x->point.y);}else if(auto x=std::get_if<FollowingLocation>(&l))c.field("party_id",x->party_id);else if(auto x=std::get_if<InArmyLocation>(&l))c.field("army_id",x->army_id);else{auto& route=std::get<OnRouteLocation>(l);c.field("edge_id",route.edge_id);c.field("origin",route.origin);c.field("progress",route.progress);}return c.status();
}
Status visit_outcome(CodecVisitor& c,DialogueOutcome& o){
    c.object("completion",[&](auto& visitor){return visit_action_completion(visitor,o.completion);});c.field("present",o.present);c.field("ok",o.ok);c.optional("success",o.success,[](auto& codec,auto& value){return codec.field("value",value);});c.field("code",o.code);visit_order(c,o.field_order);return c.status();}
V encode_outcome(const DialogueOutcome& o){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_dialogue_outcome_fields(out,o);});if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
}
Status visit_condition_checkpoint(CodecVisitor& c,Condition& v){Integer kind=static_cast<Integer>(v.kind);c.field("kind",kind);if(kind<0||kind>8||(kind==8&&!(c.schema_features&1024u)))return c.reject("UnsupportedCondition");if(c.reading())v.kind=static_cast<Condition::Kind>(kind);c.field("flag",v.flag);c.field("actor_id",v.actor_id);c.field("place_id",v.place_id);c.field("offer_id",v.offer_id);c.field("state",v.state);c.field("from_actor",v.from_actor);c.field("to_actor",v.to_actor);c.field("equals",v.equals);c.field("trust",v.trust);c.sequence("terms",v.terms,visit_condition_checkpoint);return c.status();}
Outcome<InteractionCatalog> decode_interaction_catalog(const BoundaryValue& root,std::shared_ptr<const CommonCatalog> common){using namespace interaction_codec;try{
    InteractionCatalog c;c.common=std::move(common);for(auto& row:boundary::object(boundary::field(root,"offers","data"),"offers"))c.offer_ids.push_back(row.first);auto required=boundary::strings(boundary::field(root,"required_capabilities","data"),"capabilities");c.road_encounters=std::find(required.begin(),required.end(),"road_encounters")!=required.end();c.npc_society=std::find(required.begin(),required.end(),"npc_society")!=required.end();for(auto& row:boundary::object(boundary::field(root,"flags","data"),"flags"))c.flags.push_back(row.first);for(auto& row:boundary::object(boundary::field(root,"places","data"),"places")){c.place_ids.push_back(row.first);c.admissions.push_back({row.first,conditions(boundary::field(row.second,"admission",row.first),row.first+"/admission")});}for(auto& row:boundary::object(boundary::field(root,"settlements","data"),"settlements"))c.settlement_ids.push_back(row.first);for(auto& row:boundary::object(boundary::field(root,"world_maps","data"),"world_maps"))c.map_ids.push_back(row.first);
    for(auto& row:boundary::object(boundary::field(root,"actors","data"),"actors")){ActorDialogueDefinition d;d.actor_id=row.first;if(auto x=row.second.find("dialogue_id");x&&!x->as<std::monostate>())d.dialogue_id=boundary::string(*x,row.first);if(auto x=row.second.find("road_dialogue_id");x&&!x->as<std::monostate>())d.road_dialogue_id=boundary::string(*x,row.first);c.actor_dialogues.push_back(d);}
    for(auto& row:boundary::object(boundary::field(root,"dialogues","data"),"dialogues")){auto& v=row.second;DialogueDefinition d;d.id=boundary::get_string(v,"id",row.first);d.context_role=boundary::get_string(v,"context_role",row.first);d.entry=boundary::get_string(v,"entry",row.first);for(auto& n:boundary::array(boundary::field(v,"nodes",row.first),row.first)){DialogueNode node;node.id=boundary::get_string(n,"id",row.first);node.kind=boundary::get_string(n,"kind",row.first);
#define NFIELD(name) if(auto x=n.find(#name))node.name=boundary::string(*x,row.first)
        NFIELD(next);NFIELD(speaker);NFIELD(text_key);NFIELD(if_true);NFIELD(if_false);NFIELD(on_success);NFIELD(on_failure);
#undef NFIELD
        if(auto x=n.find("condition"))node.condition=condition(*x,row.first,node.kind=="branch");if(auto x=n.find("command"))node.command=command(*x,row.first);if(auto x=n.find("options"))for(auto& option:boundary::array(*x,row.first)){DialogueOption o;o.id=boundary::get_string(option,"id",row.first);o.next=boundary::get_string(option,"next",row.first);o.text_key=boundary::get_string(option,"text_key",row.first);o.conditions=conditions(boundary::field(option,"conditions",row.first),row.first,node.kind=="choice");node.options.push_back(o);}d.nodes.push_back(node);}c.dialogues.push_back(d);}
    for(auto& row:boundary::object(boundary::field(root,"events","data"),"events")){auto& v=row.second;EventDefinition d;d.id=boundary::get_string(v,"id",row.first);d.place_id=boundary::get_string(v,"place_id",row.first);d.trigger=boundary::get_string(v,"trigger",row.first);d.on_unavailable=boundary::get_string(v,"on_unavailable",row.first);d.dialogue_id=boundary::get_string(v,"dialogue_id",row.first);d.once_per_actor=boundary::get_bool(v,"once_per_actor",row.first);d.cooldown_ticks=boundary::get_int(v,"cooldown_ticks",row.first);d.conditions=conditions(boundary::field(v,"conditions",row.first),row.first);c.events.push_back(d);}
    for(auto& row:boundary::object(boundary::field(root,"routes","data"),"routes")){auto& v=row.second;EncounterRouteDefinition d;d.id=row.first;d.origin=boundary::get_string(v,"origin",row.first);d.destination=boundary::get_string(v,"destination",row.first);d.duration_ticks=boundary::get_int(v,"duration_ticks",row.first);auto& settlements=boundary::field(root,"settlements","data");A points;points.push_back(boundary::field(boundary::field(settlements,d.origin,"settlements"),"map_xy","settlement"));if(auto shape=v.find("shape"))for(auto& point:boundary::array(*shape,row.first))points.push_back(point);points.push_back(boundary::field(boundary::field(settlements,d.destination,"settlements"),"map_xy","settlement"));if(d.origin>d.destination)std::reverse(points.begin(),points.end());auto rendered=render_boundary_json(V{std::move(points)});if(!rendered.ok)return {false,{},rendered.error};d.line=rendered.value;c.routes.push_back(d);}
    for(auto& row:boundary::object(boundary::field(root,"props","data"),"props")){
        PropDefinition prop;prop.id=row.first;prop.place_id=boundary::get_string(row.second,"place_id",row.first);
        if(auto v=row.second.find("name_key"))prop.name_key=boundary::string(*v,row.first);
        if(auto v=row.second.find("kind"))prop.kind=boundary::string(*v,row.first);
        prop.offer_ids=boundary::strings(boundary::field(row.second,"offer_ids",row.first),row.first);
        c.props.push_back(std::move(prop));
    }
    for(auto& row:boundary::object(boundary::field(root,"offers","data"),"offers")){
        OfferDiscoveryDefinition offer;offer.offer_id=row.first;
        for(auto& v:boundary::array(boundary::field(row.second,"discovery_sources",row.first),row.first))offer.sources.push_back({boundary::get_string(v,"kind",row.first),boundary::get_string(v,"id",row.first),order(v)});
        c.discoveries.push_back(std::move(offer));
    }
    for(auto& row:boundary::object(boundary::field(root,"roles","data"),"roles")){
        CompanionRoleDefinition role;role.role_id=row.first;if(auto v=row.second.find("companion_required_trust"))role.required_trust=boundary::integer(*v,row.first);c.companions.push_back(role);
    }
    auto scene=decode_scene_catalog(root);if(!scene.ok)return {false,{},scene.error};c.scene=checkpoint_make_shared<const SceneCatalog>(std::move(scene.value));
    return {true,std::move(c),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
static Outcome<InteractionState> decode_interaction_base_seed(const BoundaryValue& root){using namespace interaction_codec;try{
    InteractionState s;
    for(auto& row:boundary::object(boundary::field(root,"flags","state"),"flags"))s.flags.push_back({row.first,boundary::boolean(row.second,"flags/"+row.first)});
    for(auto& row:boundary::object(boundary::field(root,"knowledge","state"),"knowledge")){ActorKnowledge k;k.actor_id=row.first;for(auto& offer:boundary::object(row.second,"knowledge/"+row.first)){boundary::fields(offer.second,{"source","tick"},{},"knowledge");KnowledgeRecord r;r.field_order=order(offer.second);r.offer_id=offer.first;r.discovered_tick=boundary::get_int(offer.second,"tick","knowledge");auto& source=boundary::field(offer.second,"source","knowledge");boundary::fields(source,{"kind","id"},{},"knowledge/source");r.source={boundary::get_string(source,"kind","source"),boundary::get_string(source,"id","source")};r.source.field_order=order(source);k.offers.push_back(r);}s.knowledge.push_back(k);}
    for(auto& row:boundary::object(boundary::field(root,"events","state"),"events")){auto& v=row.second;boundary::fields(v,{"event_id","actor_id","place_id","status","last_tick"},{},"events/"+row.first);s.events.push_back({row.first,boundary::get_string(v,"event_id","event"),boundary::get_string(v,"actor_id","event"),boundary::get_string(v,"place_id","event"),boundary::get_string(v,"status","event"),boundary::get_int(v,"last_tick","event")});s.events.back().field_order=order(v);}
    for(auto& row:boundary::object(boundary::field(root,"interactions","state"),"interactions")){auto& v=row.second;boundary::fields(v,{"id","dialogue_id","context","node_id","phase","pending_command","pending_action_id","last_outcome","trade_completed"},{},"interactions/"+row.first);InteractionSession se;se.field_order=order(v);se.actor_id=row.first;se.id=boundary::get_string(v,"id","session");se.dialogue_id=boundary::get_string(v,"dialogue_id","session");se.context=decode_context(boundary::field(v,"context","session"),"session/context");se.node_id=boundary::get_string(v,"node_id","session");se.phase=boundary::get_string(v,"phase","session");se.pending_action_id=boundary::get_string(v,"pending_action_id","session");se.trade_completed=boundary::get_bool(v,"trade_completed","session");auto& pending=boundary::field(v,"pending_command","session");if(!boundary::object(pending,"session/pending_command").empty())se.pending_command=command(pending,"pending");auto& outcome=boundary::field(v,"last_outcome","session");bool complete=false;for(auto& field:boundary::object(outcome,"outcome"))if(field.first!="ok"&&field.first!="success"&&field.first!="code")complete=true;
    if(complete){auto decoded=decode_action_completion(outcome);if(!decoded.ok)return {false,{},decoded.error};se.last_outcome.completion=decoded.value;}
    else{boundary::fields(outcome,{}, {"ok","success","code"},"outcome");se.last_outcome.field_order=order(outcome);se.last_outcome.present=!boundary::object(outcome,"outcome").empty();if(auto x=outcome.find("ok"))se.last_outcome.ok=boundary::boolean(*x,"outcome/ok");if(auto x=outcome.find("success"))se.last_outcome.success=boundary::boolean(*x,"outcome/success");if(auto x=outcome.find("code"))se.last_outcome.code=boundary::string(*x,"outcome/code");}s.interactions.push_back(se);}
    for(auto& row:boundary::object(boundary::field(root,"encounters","state"),"encounters")){auto& v=row.second;boundary::fields(v,{"id","leaders","participants","anchors","action_ids","resume_paused","created_tick","resolved_tick","status","result_id","talked"},{},"encounters/"+row.first);EncounterRecord e;e.field_order=order(v);e.id=boundary::get_string(v,"id","encounter");if(e.id!=row.first)boundary::fail("InvalidEncounter",row.first);auto leaders=boundary::strings(boundary::field(v,"leaders","encounter"),"leaders");if(leaders.size()!=2)boundary::fail("InvalidEncounterMembers",row.first);e.leaders={leaders[0],leaders[1]};e.participants=boundary::strings(boundary::field(v,"participants","encounter"),"participants");auto& anchors=boundary::field(v,"anchors","encounter");auto& actions=boundary::field(v,"action_ids","encounter");auto& resume=boundary::field(v,"resume_paused","encounter");if(boundary::object(anchors,"anchors").size()!=2||boundary::object(actions,"actions").size()!=2||boundary::object(resume,"resume").size()!=2)boundary::fail("InvalidEncounterShape",row.first);for(std::size_t i=0;i<2;++i){e.anchor_field_orders[i]=order(boundary::field(anchors,e.leaders[i],"anchors"));e.anchors[i]=boundary::decode_location(boundary::field(anchors,e.leaders[i],"anchors"),"anchors/"+e.leaders[i]);e.action_ids[i]=boundary::get_string(actions,e.leaders[i],"actions");e.resume_paused[i]=boundary::get_bool(resume,e.leaders[i],"resume");}e.anchors_order=order(anchors);e.action_ids_order=order(actions);e.resume_paused_order=order(resume);e.created_tick=boundary::get_int(v,"created_tick","encounter");e.resolved_tick=boundary::get_int(v,"resolved_tick","encounter");e.status=boundary::get_string(v,"status","encounter");e.result_id=boundary::get_string(v,"result_id","encounter");e.talked=boundary::get_bool(v,"talked","encounter");s.encounters.push_back(e);}
    auto regional=decode_regional_interaction(root,s);if(!regional.ok)return {false,{},regional.error};
    return {true,std::move(s),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_dialogue_command_fields(ProjectionWriter& out,const DialogueCommand& c){out.field("kind",c.kind);
#define ADD(name) if(!c.name.empty()||interaction_codec::had(c.field_order,#name))out.field(#name,c.name)
    ADD(offer_id);ADD(shop_id);ADD(service_id);ADD(target_actor_id);ADD(activity);ADD(skill_id);ADD(place_id);
#undef ADD
    if(c.context)out.object("context",[&](auto& w){return emit_interaction_context_fields(w,*c.context);});return out.order_fields(c.field_order);
}
Status emit_dialogue_outcome_fields(ProjectionWriter& out,const DialogueOutcome& o){if(o.completion)return emit_action_completion_fields(out,*o.completion);if(o.present){if(o.field_order.empty()||interaction_codec::had(o.field_order,"ok"))out.field("ok",o.ok);if(o.success)out.field("success",*o.success);if(!o.code.empty()||interaction_codec::had(o.field_order,"code"))out.field("code",o.code);}return out.order_fields(o.field_order);}
Status emit_interaction_session_fields(ProjectionWriter& out,const InteractionSession& se){out.field("id",se.id);out.field("dialogue_id",se.dialogue_id);out.object("context",[&](auto& w){return emit_interaction_context_fields(w,se.context);});out.field("node_id",se.node_id);out.field("phase",se.phase);out.object("pending_command",[&](auto& w){return se.pending_command?emit_dialogue_command_fields(w,*se.pending_command):w.status();});out.field("pending_action_id",se.pending_action_id);out.object("last_outcome",[&](auto& w){return emit_dialogue_outcome_fields(w,se.last_outcome);});out.field("trade_completed",se.trade_completed);return out.order_fields(se.field_order);}
Status emit_interaction_encounter_fields(ProjectionWriter& out,const EncounterRecord& e){out.field("id",e.id);out.sequence("leaders",e.leaders,[](auto& w,const auto& id){return w.field("",id);});out.sequence("participants",e.participants,[](auto& w,const auto& id){return w.field("",id);});out.object("anchors",[&](auto& w){for(std::size_t i=0;i<2;++i)emit_common_location(w,e.leaders[i],e.anchors[i],e.anchor_field_orders[i]);return w.order_fields(e.anchors_order);});out.object("action_ids",[&](auto& w){for(std::size_t i=0;i<2;++i)w.field(e.leaders[i],e.action_ids[i]);return w.order_fields(e.action_ids_order);});out.object("resume_paused",[&](auto& w){for(std::size_t i=0;i<2;++i)w.field(e.leaders[i],e.resume_paused[i]);return w.order_fields(e.resume_paused_order);});out.field("created_tick",e.created_tick);out.field("resolved_tick",e.resolved_tick);out.field("status",e.status);out.field("result_id",e.result_id);out.field("talked",e.talked);return out.order_fields(e.field_order);}
static Status emit_interaction_base_state_fields(ProjectionWriter& out,const InteractionState& s){
    out.object("flags",[&](auto& w){for(const auto& f:s.flags)w.field(f.id,f.value);return w.status();});
    out.object("knowledge",[&](auto& w){for(const auto& k:s.knowledge){w.object(k.actor_id,[&](auto& a){for(const auto& r:k.offers){a.object(r.offer_id,[&](auto& v){v.object("source",[&](auto& q){q.field("kind",r.source.kind);q.field("id",r.source.id);return q.order_fields(r.source.field_order);});v.field("tick",r.discovered_tick);return v.order_fields(r.field_order);});if(!a.status().ok)break;}return a.status();});if(!w.status().ok)break;}return w.status();});
    out.object("events",[&](auto& w){for(const auto& e:s.events){w.object(e.key,[&](auto& v){v.field("event_id",e.event_id);v.field("actor_id",e.actor_id);v.field("place_id",e.place_id);v.field("status",e.status);v.field("last_tick",e.last_tick);return v.order_fields(e.field_order);});if(!w.status().ok)break;}return w.status();});
    out.object("interactions",[&](auto& w){for(const auto& se:s.interactions){w.object(se.actor_id,[&](auto& v){return emit_interaction_session_fields(v,se);});if(!w.status().ok)break;}return w.status();});
    out.object("encounters",[&](auto& w){for(const auto& e:s.encounters){w.object(e.id,[&](auto& v){return emit_interaction_encounter_fields(v,e);});if(!w.status().ok)break;}return w.status();});
    if(s.regional)out.object("regional_life",[&](auto& w){return emit_regional_interaction_fields(w,s);});return out.status();
}
Outcome<BoundaryValue> export_interaction_state(const InteractionState& s){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_interaction_state_fields(out,s);});if(status.ok)status=writer.finish();return {status.ok,std::move(value),status.error};}

// 错误参数保留领域原码；未知ID用空typed ID进入原preview顺序，不提前报通用codec错误。
Outcome<InteractionCommand> decode_interaction_command(const StableId& kind,const BoundaryValue& p){
    if(!p.as<BoundaryValue::Dict>())return {false,{},common_failure("InvalidCommand").error};
    auto text=[&](const char* key){auto x=p.find(key);return x&&x->as<StableId>()?*x->as<StableId>():StableId{};};
    auto context=[&]()->std::optional<InteractionContext>{auto value=p.find("context");if(!value)return {};try{return interaction_codec::decode_context(*value,"params/context");}catch(const boundary::DecodeFailure&){InteractionContext invalid;invalid.input_error="InvalidInteractionContext";return invalid;}};
    if(kind=="encounter_talk")return {true,EncounterTalk{text("encounter_id")},{}};
    if(kind=="encounter_leave")return {true,EncounterLeave{text("encounter_id")},{}};
    if(kind=="talk")return {true,Talk{text("role_id"),text("topic_id")},{}};
    if(kind=="talk_actor"){try{boundary::fields(p,{"actor_id","topic_id"},{},"params");return {true,TalkActor{text("actor_id"),text("topic_id")},{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
    if(kind=="ask_news"){try{boundary::fields(p,{}, {},"params");return {true,AskNews{},{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
    if(kind=="open_event")return {true,OpenEvent{text("event_id")},{}};
    if(kind=="dialogue_next")return {true,DialogueNext{},{}};
    if(kind=="dialogue_choose")return {true,DialogueChoose{text("option_id")},{}};
    if(kind=="end_interaction")return {true,EndInteraction{},{}};
    if(kind=="inspect")return {true,Inspect{text("prop_id")},{}};
    if(kind=="discover_offer"){DiscoverOffer c;c.offer_id=text("offer_id");c.offer_id_valid=p.find("offer_id")&&p.find("offer_id")->as<StableId>();c.prop_id=text("prop_id");c.has_prop_id=p.find("prop_id");c.context=context();return {true,c,{}};}
    if(kind=="join_party"){JoinParty c;c.actor_id=text("actor_id");c.context=context();if(p.find("return_receipt_id"))c.return_receipt_id=text("return_receipt_id");return {true,c,{}};}
    if(kind=="dismiss_companion")return {true,DismissCompanion{text("actor_id")},{}};
    if(kind=="dialogue_confirm")return {true,DialogueConfirm{},{}};
    if(kind=="dialogue_cancel")return {true,DialogueCancel{},{}};
    if(kind=="close_trade")return {true,CloseTrade{},{}};
    if(kind=="open_trade")return {true,OpenTrade{text("shop_id"),context()},{}};
    return {false,{},common_failure("UnsupportedAction").error};
}

static Status visit_interaction_base_checkpoint(CodecVisitor& c,InteractionState& s,bool regional){using namespace interaction_codec;c.sequence("flags",s.flags,[](auto& codec,auto& f){codec.field("id",f.id);codec.field("value",f.value);return codec.status();});
    c.sequence("knowledge",s.knowledge,[](auto& codec,auto& k){codec.field("actor_id",k.actor_id);codec.sequence("offers",k.offers,[](auto& v,auto& r){v.field("offer_id",r.offer_id);v.field("tick",r.discovered_tick);v.field("source_kind",r.source.kind);v.field("source_id",r.source.id);v.object("source_order",[&](auto& vc){return visit_order(vc,r.source.field_order);});visit_order(v,r.field_order);return v.status();});return codec.status();});
    c.sequence("events",s.events,[](auto& codec,auto& e){codec.field("key",e.key);codec.field("event_id",e.event_id);codec.field("actor_id",e.actor_id);codec.field("place_id",e.place_id);codec.field("status",e.status);codec.field("last_tick",e.last_tick);visit_order(codec,e.field_order);return codec.status();});
    c.sequence("interactions",s.interactions,[](auto& codec,auto& se){codec.field("actor_id",se.actor_id);codec.field("id",se.id);codec.field("dialogue_id",se.dialogue_id);codec.object("context",[&](auto& v){return visit_context(v,se.context);});codec.field("node_id",se.node_id);codec.field("phase",se.phase);codec.field("pending_action_id",se.pending_action_id);codec.optional("pending_command",se.pending_command,visit_command);codec.object("last_outcome",[&](auto& v){return visit_outcome(v,se.last_outcome);});codec.field("trade_completed",se.trade_completed);visit_order(codec,se.field_order);return codec.status();});
    c.sequence("encounters",s.encounters,[](auto& codec,auto& e){codec.field("id",e.id);for(std::size_t i=0;i<2;++i){codec.field("leader"+std::to_string(i),e.leaders[i]);codec.object("anchor"+std::to_string(i),[&](auto& v){return visit_location(v,e.anchors[i]);});codec.field("action_id"+std::to_string(i),e.action_ids[i]);codec.field("resume_paused"+std::to_string(i),e.resume_paused[i]);codec.object("anchor_order"+std::to_string(i),[&](auto& v){return visit_order(v,e.anchor_field_orders[i]);});}codec.sequence("participants",e.participants,[](auto& v,auto& p){return v.field("id",p);});codec.field("created_tick",e.created_tick);codec.field("resolved_tick",e.resolved_tick);codec.field("status",e.status);codec.field("result_id",e.result_id);codec.field("talked",e.talked);visit_order(codec,e.field_order);codec.object("anchors_order",[&](auto& v){return visit_order(v,e.anchors_order);});codec.object("actions_order",[&](auto& v){return visit_order(v,e.action_ids_order);});codec.object("resume_order",[&](auto& v){return visit_order(v,e.resume_paused_order);});return codec.status();});if(regional)return visit_regional_interaction(c,s);return c.status();
}
namespace {
Status interaction_base_shape(const InteractionState&s,Integer version){
 Integer base=version;if(interaction_response_version(version)){auto selected=interaction_response_base_version(version);if(!selected.ok)return {false,{},selected.error};base=selected.value;}
 if(base<2||base>4)return common_failure("InteractionPhysicalVersionUnsupported",{},ErrorKind::unsupported_capability);
 if((base==2&&s.regional)||(base!=2&&(!s.regional||s.regional->version!=base-2)))return common_failure("InteractionOriginalBaseVersionMismatch");
 if(interaction_response_version(version)){if(!s.response_sources||s.response_sources->physical_version!=version)return common_failure("InteractionResponseSelectionMismatch");}
 else if(s.response_sources)return common_failure("InteractionResponseExplicitVersionRequired",{},ErrorKind::unsupported_capability);
 return common_success();
}
bool response_root_present(const BoundaryValue&root){return root.find("session_boundaries")||root.find("response_sources")||root.find("selected_response_tails")||root.find("life_sources")||root.find("action_confirmation_sources");}
}
Outcome<InteractionState> decode_interaction_seed(const BoundaryValue&root){if(response_root_present(root))return {false,{},common_failure("InteractionResponseExplicitVersionRequired",{},ErrorKind::unsupported_capability).error};return decode_interaction_base_seed(root);}
Outcome<InteractionState> decode_interaction_selected_seed(const BoundaryValue&root,Integer version){
 const bool complete=interaction_response_version(version);if(!complete&&(version<2||version>4))return {false,{},common_failure("InteractionPhysicalVersionUnsupported",{},ErrorKind::unsupported_capability).error};
 if(!complete&&response_root_present(root))return {false,{},common_failure("InteractionResponseExplicitVersionRequired",{},ErrorKind::unsupported_capability).error};
 auto state=decode_interaction_base_seed(root);if(!state.ok)return state;if(complete){auto source=decode_interaction_response_roots(root,version);if(!source.ok)return {false,{},source.error};state.value.response_sources=std::move(source.value);}
 auto valid=interaction_base_shape(state.value,version);return valid.ok?Outcome<InteractionState>{true,std::move(state.value),{}}:Outcome<InteractionState>{false,{},valid.error};
}
Status emit_interaction_state_fields(ProjectionWriter&out,const InteractionState&s){if(s.response_sources)return common_failure("InteractionResponseExplicitVersionRequired",{},ErrorKind::unsupported_capability);return emit_interaction_base_state_fields(out,s);}
Status emit_interaction_selected_state_fields(ProjectionWriter&out,const InteractionState&s,Integer version){auto valid=interaction_base_shape(s,version);if(!valid.ok)return valid;valid=emit_interaction_base_state_fields(out,s);if(valid.ok&&s.response_sources)valid=emit_interaction_response_roots(out,*s.response_sources,version);return valid;}
Outcome<BoundaryValue> export_interaction_selected_state(const InteractionState&s,Integer version){BoundaryValue value;BoundaryProjectionWriter writer(value);auto valid=writer.object("",[&](auto&out){return emit_interaction_selected_state_fields(out,s,version);});if(valid.ok)valid=writer.finish();return {valid.ok,std::move(value),valid.error};}
Status visit_interaction_checkpoint(CodecVisitor&c,InteractionState&s,bool regional){if(!c.reading()&&s.response_sources)return c.reject("InteractionResponseExplicitVersionRequired");return visit_interaction_base_checkpoint(c,s,regional);}
Status visit_interaction_selected_checkpoint(CodecVisitor&c,InteractionState&s,Integer version){
 const bool complete=interaction_response_version(version);Integer base=version;if(complete){auto selected=interaction_response_base_version(version);if(!selected.ok)return {false,{},selected.error};base=selected.value;}if(base<2||base>4)return c.reject("InteractionPhysicalVersionUnsupported");
 if(!c.reading()){auto valid=interaction_base_shape(s,version);if(!valid.ok)return valid;valid=visit_interaction_base_checkpoint(c,s,base!=2);if(valid.ok&&complete)valid=visit_interaction_response_roots(c,*s.response_sources,version);return valid;}
 InteractionState candidate;auto valid=visit_interaction_base_checkpoint(c,candidate,base!=2);if(valid.ok&&complete){candidate.response_sources.emplace();valid=visit_interaction_response_roots(c,*candidate.response_sources,version);}if(valid.ok)valid=interaction_base_shape(candidate,version);if(valid.ok){s.flags=std::move(candidate.flags);s.knowledge=std::move(candidate.knowledge);s.events=std::move(candidate.events);s.interactions=std::move(candidate.interactions);s.encounters=std::move(candidate.encounters);s.regional=std::move(candidate.regional);s.response_sources=std::move(candidate.response_sources);}return valid;
}

}
