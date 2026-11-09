// 页只显示眼前经办人可办理的已知机会，或本人曾当面对他作出的回应；不读远方NPC状态。
#include "task_runtime_offer_view.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/affairs/issued_supply/affairs_issued_supply.hpp"
#include "resident/domains/director/director_model.hpp"
#include <set>
namespace hanmo::resident {
Outcome<BoundaryValue> query_runtime_task_offer_conversation(const CommonStore& common,const TaskState& tasks,const AffairsState& affairs,const SocialState& social,const InteractionState& interaction,const InteractionCatalog& catalog,const InteractionAccess& access,const KnownSubjectView& known,const RuntimeTaskOfferResponses& responses,const StableId& actor,Integer cursor,const TaskConversationPreview& preview,const EconomicSupplyPolicyRead* issued_supply){
    using V=BoundaryValue;using Result=Outcome<V>;const auto fail=[](const StableId& code)->Result{return {false,{},common_failure(code).error};};
    if(!preview||!catalog.common||!access.role)return fail("TaskConversationUnavailable");
    const InteractionSession* meeting=nullptr;for(const auto& row:interaction.interactions)if(row.actor_id==actor){if(meeting)return fail("DuplicateInteraction");meeting=&row;}
    if(!meeting)return fail("NoConversation");auto proof=preview_face_to_face_contact(common,interaction,catalog,access,actor,meeting->context.provider_actor_id);if(!proof.ok)return {false,{},proof.error};
    if(proof.value.session_actor_id!=actor||known.actor_id!=actor||known.tick!=common.tick()||known.revision!=common.revision()||known.place_id!=meeting->context.place_id)return fail("StaleTaskConversation");
    const auto context_for=[&](const OfferInstance& offer)->std::optional<InteractionContext>{const auto* role=catalog.common->roles.find(offer.definition.accept_role);auto assignment=access.role(offer.definition.accept_role);if(!role||role->place_id!=meeting->context.place_id||!assignment||assignment->actor_id!=meeting->context.provider_actor_id)return {};InteractionContext context;context.place_id=meeting->context.place_id;context.provider_actor_id=meeting->context.provider_actor_id;context.role_id=offer.definition.accept_role;return context;};
    std::vector<StableId> ids;std::set<StableId> seen;std::vector<OpportunityView> available;
    auto current_opportunities=query_opportunities_with_issued_supply(common,affairs,tasks,known,issued_supply);if(!current_opportunities.ok)return {false,{},current_opportunities.error};
    for(const auto& opportunity:current_opportunities.value){const auto* offer=tasks.offer_instances.find(opportunity.offer.id);if(!offer||!context_for(*offer))continue;const auto* accepted=task_for(tasks,actor,offer->id);if(accepted&&accepted->contract_id){const auto* contract=tasks.contracts.find(*accepted->contract_id);if(!contract)return fail("InvalidTaskContract");if(contract->status!=ContractStatus::settled)continue;}available.push_back(opportunity);if(seen.insert(offer->id).second)ids.push_back(offer->id);}
    for(const auto& row:responses.responses)if(row.actor_id==actor&&row.context.provider_actor_id==meeting->context.provider_actor_id&&seen.insert(row.offer_id).second)ids.push_back(row.offer_id);
    if(cursor<0||static_cast<std::size_t>(cursor)>ids.size())return fail("InvalidTaskConversationCursor");
    V::Array rows;Integer scanned=cursor;for(;static_cast<std::size_t>(scanned)<ids.size()&&rows.size()<16;++scanned){
        const auto& id=ids[static_cast<std::size_t>(scanned)];const auto* offer=tasks.offer_instances.find(id);if(!offer)return fail("InvalidRuntimeOfferResponse");
        const auto* response=runtime_task_offer_response(responses,actor,id);if(response&&response->context.provider_actor_id!=meeting->context.provider_actor_id)response=nullptr;
        if(response){RuntimeTaskOfferResponses selected;selected.responses.push_back(*response);auto valid=validate_runtime_task_offer_responses(common,tasks,affairs,social,catalog,selected);if(!valid.ok)return {false,{},valid.error};}
        const OpportunityView* current=nullptr;for(const auto& row:available)if(row.offer.id==id){current=&row;break;}
        V::Dict row{{"id",V{id}},{"display_name",V{offer->definition.display_name}},{"status",V{StableId{"known"}}},{"status_text",V{current?StableId{"眼前可询问的公开委托"}:StableId{"保留当时的回应"}}},{"currently_offered",V{bool(current)}},{"objective_text",V{StableId{"按当前公布的需粮份额办理"}}},{"source_text",V{response?StableId{"本人曾当面回应的原机会"}:StableId{"本处已知的真实供给报告"}}}};
        if(response){row.emplace_back("response_tick",V{response->response_tick});row.emplace_back("response_text",V{"你曾在第"+std::to_string(response->response_tick/2+1)+"日表示这次暂不承接。此记录不等于已经承诺，也不阻止按当前条件重新承接。"});}
        V::Array actions;if(current){
            auto context=context_for(*offer);if(!context)return fail("StaleTaskConversation");TaskCommand command;command.offer_id=id;command.offer_ref=OfferRef{OfferRef::Kind::runtime,id};command.need_revision=current->need_revision;command.context=*context;
            const auto add=[&](const StableId& text){auto quote=preview(command);V::Dict action{{"command",V{command.kind}},{"params",encode_task_command(command)},{"text",V{text}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}};if(quote.ok)action.emplace_back("quote",encode_task_quote(quote.value));actions.emplace_back(V{std::move(action)});};
            command.kind="accept_offer";command.quantity=current->available_amount;add("承接当前可领的"+std::to_string(current->available_amount)+"份");command.kind="decline_runtime_offer";command.quantity.reset();add("这次暂不承接");
            row.emplace_back("available_amount",V{current->available_amount});row.emplace_back("need_revision",V{current->need_revision});row.emplace_back("progress_text",V{StableId{"承接份额、经费和时限以本次原生报价为准；暂拒不占份额。"}});
        }else row.emplace_back("progress_text",V{StableId{"这次会面没有同源可受领的原机会；当时的回应仍然保留。"}});
        row.emplace_back("actions",V{std::move(actions)});rows.emplace_back(V{std::move(row)});
    }
    V::Dict result{{"ok",V{true}},{"actor_id",V{actor}},{"provider_actor_id",V{meeting->context.provider_actor_id}},{"session_id",V{meeting->id}},{"place_id",V{meeting->context.place_id}},{"revision",V{common.revision()}},{"cursor",V{cursor}},{"rows",V{std::move(rows)}}};if(static_cast<std::size_t>(scanned)<ids.size())result.emplace_back("next_cursor",V{scanned});return {true,V{std::move(result)},{}};
}
}
