// 独立婉拒域门：核真实现场和本人来源、可再询问、资产不变及严格冷格式，不冒充完整世界注册。
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/tasks/task_offer_response.hpp"
#include "resident/domains/tasks/task_lifecycle_view.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
// 在已有共同Actor/钱包/岗位上验证记录动作；没有用另一本任务账模拟承诺。
int main(){
    auto catalog=std::make_shared<CommonCatalog>();catalog->places.insert("office",{"office","town",{}});
    RoleDefinition role;role.id="clerk";role.place_id="office";role.initial_actor="giver";role.services={"accept_offer","report_offer"};catalog->roles.insert(role.id,role);
    CommonState initial;initial.identity.tick=10;initial.identity.revision=3;initial.next_instance_id=9;
    for(auto id:{"player","giver","other"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.hp=100;actor.location=SettlementLocation{"town","office"};actor.wallet_id="cash:"+actor.id;actor.inventory_id="bag:"+actor.id;initial.actors.insert(id,actor);initial.wallets.insert(actor.wallet_id,{actor.wallet_id,{Owner::Kind::actor,id,{}},0,{}});initial.inventories.insert(actor.inventory_id,{actor.inventory_id,{Owner::Kind::actor,id,{}},100,0,{}});}
    CommonStore common(initial,catalog);TaskCatalog definitions;TaskDefinition offer;offer.id="grain";offer.template_id="procurement";offer.accept_role="clerk";offer.report_role="clerk";offer.issuer={Owner::Kind::actor,"giver",{}};offer.budget=20;offer.funding_wallet="cash:giver";definitions.offers.insert(offer.id,offer);TaskState tasks;TaskOfferResponseState responses;
    InteractionCatalog interactions;interactions.common=catalog;interactions.discoveries.push_back({"grain",{{"actor","giver"}}});InteractionState state;state.knowledge.push_back({"player",{{"grain",4,{"actor","giver"}}}});
    InteractionSession meeting;meeting.actor_id="player";meeting.id="conversation";meeting.phase="choice";meeting.context={"office","giver","clerk"};state.interactions.push_back(meeting);
    InteractionAccess access;access.resolve_location=[&](const StableId& actor){return common.actor(actor)->location;};access.role=[](const auto&){return std::optional<RoleAssignmentView>{{"giver",true}};};access.battle_occupied=[](const auto&){return false;};
    TaskCommand command{"decline_offer","grain",meeting.context};
    // 预览和单纯结束会话都不新增响应；经费不足不应妨碍拒绝一份尚未承诺的差事。
    auto quote=preview_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",command);assert(quote.ok&&quote.value.duration==0&&responses.responses.empty());
    state.interactions.clear();auto absent=record_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",command,responses);assert(!absent.ok&&absent.error.original.code=="NoConversation"&&responses.responses.empty());state.interactions.push_back(meeting);
    assert(record_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",command,responses).ok);assert(responses.responses.size()==1);assert(responses.responses.front().response_tick==10&&responses.responses.front().discovered_tick==4);assert(task_condition_status(common,tasks,state,"player","grain")=="known");
    assert(common.tick()==10&&common.revision()==3&&common.state().next_instance_id==9&&tasks.tasks.empty()&&common.wallet("cash:player")->balance==0&&common.wallet("cash:giver")->balance==0&&common.inventory("bag:player")->lots.empty());
    assert(record_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",command,responses).ok&&responses.responses.size()==1);assert(validate_task_offer_responses(common,definitions,state,interactions,responses).ok);
    // 重新询问保持原来源；已有真实任务后不能用婉拒绕过取消清算。
    TaskRecord accepted;accepted.id="task";accepted.offer_id="grain";accepted.assignee="player";accepted.status="active";accepted.serial=1;tasks.tasks.insert(accepted.id,accepted);
    auto active=preview_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",command);assert(!active.ok&&active.error.original.code=="OfferAlreadyAccepted");tasks.tasks.erase("task");
    auto forged=command;forged.offer_ref=OfferRef{OfferRef::Kind::authored,"other"};auto mismatch=preview_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",forged);assert(!mismatch.ok&&mismatch.error.original.code=="OfferReferenceMismatch");
    forged=command;forged.context.provider_actor_id="other";auto outsider=preview_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",forged);assert(!outsider.ok&&outsider.error.original.code=="StaleTaskConversation");
    auto remembered=state.knowledge.front().offers.front();state.knowledge.front().offers.clear();auto unknown=preview_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",command);assert(!unknown.ok&&unknown.error.original.code=="SourceUnknown");state.knowledge.front().offers.push_back(remembered);
    common.state().actors.find("giver")->location=SettlementLocation{"town","elsewhere"};auto remote=preview_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",command);assert(!remote.ok&&remote.error.original.code=="PersonAbsent");assert(validate_task_offer_responses(common,definitions,state,interactions,responses).ok);common.state().actors.find("giver")->location=SettlementLocation{"town","office"};
    // JSON与二进制都保原响应；重读不重新执行命令，不发资源或改获知日期。
    auto encoded=encode_task_offer_response_state(responses);auto decoded=decode_task_offer_response_state(encoded);assert(decoded.ok&&validate_task_offer_responses(common,definitions,state,interactions,decoded.value).ok&&compare_boundary(encoded,encode_task_offer_response_state(decoded.value)).exact());
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);assert(visit_task_offer_response_checkpoint(writer,responses).ok&&writer.finish().ok);const auto saved=bytes;TaskOfferResponseState restored;CodecVisitor reader(saved);assert(visit_task_offer_response_checkpoint(reader,restored).ok&&reader.finish().ok&&validate_task_offer_responses(common,definitions,state,interactions,restored).ok);assert(compare_boundary(encoded,encode_task_offer_response_state(restored)).exact());
    auto invalid=encoded;*invalid.find("version")=BoundaryValue{Integer{2}};assert(!decode_task_offer_response_state(invalid).ok);invalid=encoded;invalid.as<BoundaryValue::Dict>()->emplace_back("bonus",BoundaryValue{Integer{100}});assert(!decode_task_offer_response_state(invalid).ok);
    auto duplicate=responses;duplicate.responses.push_back(duplicate.responses.front());assert(!validate_task_offer_responses(common,definitions,state,interactions,duplicate).ok);auto future=responses;future.responses.front().response_tick=11;assert(!validate_task_offer_responses(common,definitions,state,interactions,future).ok);
    auto changed_source=responses;changed_source.responses.front().source.id="other";assert(!validate_task_offer_responses(common,definitions,state,interactions,changed_source).ok);
    // 有来源响应时回访仍同时保留承接与这次暂不承接；钱不足的承接错误不阻止婉拒。
    TaskConversationPreview choices=[&](const TaskCommand& input)->Outcome<TaskQuote>{if(input.kind=="decline_offer")return preview_task_offer_decline(common,tasks,definitions,state,interactions,access,"player",input);return {false,{},common_failure("IssuerInsufficientFunds").error};};
    auto revisit=query_task_conversation(common,tasks,definitions,state,interactions,access,"player",0,choices,&restored);assert(revisit.ok);auto rows=revisit.value.find("rows")->as<BoundaryValue::Array>();assert(rows&&rows->size()==1&&rows->front().find("response_text"));bool can_decline=false,can_accept=false;
    for(const auto& choice:*rows->front().find("actions")->as<BoundaryValue::Array>()){if(*choice.find("command")->as<StableId>()=="decline_offer")can_decline=*choice.find("available")->as<bool>();if(*choice.find("command")->as<StableId>()=="accept_offer")can_accept=true;}assert(can_decline&&can_accept);
    auto legacy=query_task_conversation(common,tasks,definitions,state,interactions,access,"player",0,choices);assert(legacy.ok);for(const auto& row:*legacy.value.find("rows")->as<BoundaryValue::Array>()){assert(!row.find("response_text"));for(const auto& choice:*row.find("actions")->as<BoundaryValue::Array>())assert(*choice.find("command")->as<StableId>()!="decline_offer");}
    assert(common.wallet("cash:player")->balance==0&&common.state().next_instance_id==9&&tasks.tasks.empty());
    std::cout<<"task_offer_response: explicit local refusal, known-source authority, no commitment or payment, revisit eligibility, idempotence and strict JSON/binary roundtrip passed\n";
}
