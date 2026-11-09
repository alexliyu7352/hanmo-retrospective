// 原回应、本人获知源和当前当面人物三者同时相符才可成为作者回访条件。
#include "task_offer_revisit.hpp"
#include "task_model.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
namespace hanmo::resident {
Outcome<std::optional<TaskOfferRevisit>> read_task_offer_revisit(
    const CommonStore& common, const TaskCatalog& catalog, const InteractionState& interaction,
    const InteractionCatalog& interactions, const InteractionAccess& access,
    const TaskOfferResponseState& responses, const StableId& actor, const StableId& offer) {
    using Result=Outcome<std::optional<TaskOfferRevisit>>;
    const auto fail=[](const StableId& code)->Result {return {false,{},common_failure(code).error};};
    if(responses.version!=1&&responses.version!=2)return fail("UnsupportedTaskOfferResponseVersion");
    if((responses.version==2)!=responses.runtime.has_value())return fail("TaskOfferResponseVersionMismatch");
    const InteractionSession* meeting=nullptr;
    for(const auto& session:interaction.interactions)if(session.actor_id==actor){
        if(meeting)return fail("DuplicateInteraction");
        meeting=&session;
    }
    if(!meeting||(meeting->phase!="say"&&meeting->phase!="choice"&&meeting->phase!="stepping"))
        return fail("NoConversation");
    auto contact=validate_interaction(common,interaction,interactions,access,actor,meeting->context);
    if(!contact.ok)return {false,{},contact.error};
    // 不用隐藏offer、远方人物或另一NPC的记录解释“为什么未回想起”。
    const auto* original=task_offer_response(responses,actor,offer);
    if(!original||original->context.provider_actor_id!=contact.value.provider_actor_id)
        return {true,std::nullopt,{}};
    TaskOfferResponseState selected;selected.responses.push_back(*original);
    auto valid=validate_task_offer_responses(common,catalog,interaction,interactions,selected);
    if(!valid.ok)return {false,{},valid.error};
    TaskOfferRevisit recalled;
    recalled.source_identity=common.state().identity;recalled.actor_id=actor;
    recalled.provider_actor_id=contact.value.provider_actor_id;recalled.session_id=meeting->id;
    recalled.offer_id=offer;recalled.response_tick=original->response_tick;
    recalled.response_context=original->context;recalled.discovered_source=original->source;
    return {true,std::move(recalled),{}};
}
// 只编码已经证明可由当前当面人回想的来源；不会列举别人的委托或当前远方任务状态。
BoundaryValue encode_task_offer_revisit(const TaskOfferRevisit& recalled) {
    using V=BoundaryValue;StableId world;const char* digits="0123456789abcdef";
    for(auto byte:recalled.source_identity.world_id){world+=digits[byte>>4];world+=digits[byte&15];}
    return boundary::obj({
        {"source_identity",boundary::obj({{"world_id",V{world}},
            {"execution_epoch",V{recalled.source_identity.execution_epoch}},
            {"revision",V{recalled.source_identity.revision}},{"tick",V{recalled.source_identity.tick}}})},
        {"actor_id",V{recalled.actor_id}},{"provider_actor_id",V{recalled.provider_actor_id}},
        {"session_id",V{recalled.session_id}},{"offer_id",V{recalled.offer_id}},
        {"response",V{StableId{"declined_for_now"}}},{"response_tick",V{recalled.response_tick}},
        {"response_context",interaction_codec::encode_context(recalled.response_context)},
        {"discovered_source",boundary::obj({{"kind",V{recalled.discovered_source.kind}},
            {"id",V{recalled.discovered_source.id}}})}});
}
// 新作者条件明确采用过去式，和Task当前active/ready/success状态独立。
Outcome<TaskOfferRevisitCondition> decode_task_offer_revisit_condition(const BoundaryValue& value) {
    try {
        using namespace boundary;
        fields(value,{"op","offer_id","equals"},{},"offer_declined_before");
        if(get_string(value,"op","offer_declined_before")!="offer_declined_before")
            fail("UnsupportedCondition","offer_declined_before/op");
        TaskOfferRevisitCondition out{get_string(value,"offer_id","offer_declined_before"),
            get_bool(value,"equals","offer_declined_before")};
        if(out.offer_id.empty())fail("InvalidOfferReference","offer_declined_before/offer_id");
        return {true,std::move(out),{}};
    } catch(const boundary::DecodeFailure& error) {return {false,{},error.error};}
}
BoundaryValue encode_task_offer_revisit_condition(const TaskOfferRevisitCondition& condition) {
    using V=BoundaryValue;
    return boundary::obj({{"op",V{StableId{"offer_declined_before"}}},
        {"offer_id",V{condition.offer_id}},{"equals",V{condition.equals}}});
}
Outcome<bool> evaluate_task_offer_revisit_condition(
    const CommonStore& common, const TaskCatalog& catalog, const InteractionState& interaction,
    const InteractionCatalog& interactions, const InteractionAccess& access,
    const TaskOfferResponseState& responses, const StableId& actor,
    const TaskOfferRevisitCondition& condition) {
    if(condition.offer_id.empty())return {false,false,common_failure("InvalidOfferReference").error};
    auto source=read_task_offer_revisit(common,catalog,interaction,interactions,access,responses,actor,condition.offer_id);
    if(!source.ok)return {false,false,source.error};
    return {true,source.value.has_value()==condition.equals,{}};
}
}
