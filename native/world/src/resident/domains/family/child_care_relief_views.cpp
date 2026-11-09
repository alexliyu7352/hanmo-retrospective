// 仅本人原出生、实际当前会面、本人承诺和亲自照护记录；不公开远方死亡、别人的履行明细或库存。
#include "child_care_relief_commands.hpp"
#include "resident/views/common_presentation.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
using V=BoundaryValue;
V::Array page_rows(V::Array rows,Integer page){V::Array out;for(Integer i=page*16;i<std::min<Integer>(rows.size(),page*16+16);++i)out.push_back(std::move(rows[static_cast<std::size_t>(i)]));return out;}
V identity(const CommonStore& c){StableId world;const char* hex="0123456789abcdef";for(auto n:c.state().identity.world_id){world+=hex[n>>4];world+=hex[n&15];}const auto& i=c.state().identity;return boundary::obj({{"world_id",V{world}},{"execution_epoch",V{i.execution_epoch}},{"revision",V{i.revision}},{"tick",V{i.tick}}});}
V person(const CommonStore& c,const CommonCatalog& common,const StableId& actor){auto name=CommonPresentation(c.state(),common).display_name(actor);return boundary::obj({{"actor_id",V{actor}},{"name",V{name.ok?name.value:actor}}});}
V terms(const ChildCareReliefTerms& t){return boundary::obj({{"place_id",V{t.place_id}},{"food_item_id",V{t.food_item_id}},{"period_ticks",V{t.period_ticks}},{"food_per_period",V{t.food_per_period}},{"prepaid_periods",V{t.prepaid_periods}}});}
}
Outcome<BoundaryValue> query_child_care_relief_page(const CommonStore& c,const CommonCatalog& common,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const ChildCareReliefCatalog& catalog,const FamilyBirthState& births,const FamilyDeathState& life,const ChildCareReliefState& state,const ChildCareReliefAccess& access,Integer page){
    using namespace boundary;if(!catalog.enabled)return {false,{},common_failure("ChildCareReliefProfileRequired").error};if(page<0||page>4095)return {false,{},common_failure("InvalidChildCareReliefPage").error};auto history=validate_child_care_relief(c,interactions,catalog,births,life,state,access);if(!history.ok)return {false,{},history.error};const auto actor=c.state().metadata.player_actor_id;V::Array offers,agreements,care;
    auto option=[&](const ChildCareReliefCommand& command){auto quote=preview_child_care_relief_command(c,meeting,interactions,contact,catalog,births,life,state,access,actor,command);return obj({{"kind",V{command.kind}},{"action_id",V{command.action_id}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}});};
    for(const auto& row:catalog.offers.rows()){
        const auto& offer=row.second;auto node=preview_authored_node_source(c,meeting,interactions,contact,actor,offer.provider_actor_id);if(!node.ok||node.value.dialogue_id!=offer.dialogue_id||node.value.node_id!=offer.node_id)continue;
        for(const auto& child:births.births.rows()){
            if(std::find(child.second.biological_parent_ids.begin(),child.second.biological_parent_ids.end(),actor)==child.second.biological_parent_ids.end())continue;
            // 只呈现本人参加出生、今天真实在场且听到同意的孩子；资格未满足不泄露未见儿童信息。
            auto source=preview_child_care_relief_source(c,meeting,interactions,contact,catalog,births,life,actor,child.second.actor_id,row.first);if(!source.ok)continue;
            ChildCareReliefCommand command;command.kind="family_care_relief_accept";command.offer_id=row.first;command.child_actor_id=child.second.actor_id;
            offers.push_back(obj({{"offer_id",V{row.first}},{"child",person(c,common,child.second.actor_id)},{"provider",person(c,common,offer.provider_actor_id)},{"terms",terms(offer.terms)},{"option",option(command)}}));
        }
    }
    for(const auto& row:state.agreements.rows()){
        const auto& source=row.second.source;if(source.parent_actor_id!=actor&&source.provider_actor_id!=actor)continue;const bool closed=history.value.closed_agreements.count(row.first);bool personally_closed=false;for(const auto& close:state.closures.rows())if(close.second.accepted_id==row.first&&(close.second.actor_id==actor&&(close.second.reason=="parent_withdrew"||close.second.reason=="provider_withdrew")))personally_closed=true;const bool known_closed=personally_closed||(source.provider_actor_id==actor&&closed);V::Array options;
        if(!closed){ChildCareReliefCommand stop;stop.kind="family_care_relief_withdraw";stop.accepted_id=row.first;options.push_back(option(stop));}
        if(!closed&&source.provider_actor_id==actor){ChildCareReliefCommand command;if(const auto* pending=history.value.pending_by_agreement.find(row.first)){command.kind="family_care_relief_cancel";command.action_id=*pending;}else{command.kind="family_child_care_relief";command.accepted_id=row.first;}options.push_back(option(command));}
        V::Dict shown{{"accepted_id",V{row.first}},{"accepted_tick",V{source.tick}},{"child",person(c,common,source.child_actor_id)},{"provider",person(c,common,source.provider_actor_id)},{"parent",person(c,common,source.parent_actor_id)},{"terms",terms(source.terms)},{"status",V{known_closed?"finite_help_closed":"finite_help_recorded"}},{"options",V{std::move(options)}}};
        if(source.provider_actor_id==actor)shown.emplace_back("personally_completed_periods",V{*history.value.completed_periods.find(row.first)});agreements.emplace_back(std::move(shown));
    }
    for(const auto& row:state.care.rows())if(row.second.provider_actor_id==actor){const auto& done=row.second;care.push_back(obj({{"fact_id",V{row.first}},{"accepted_id",V{done.accepted_id}},{"action_id",V{done.action_id}},{"child",person(c,common,done.child_actor_id)},{"period_index",V{done.period_index}},{"started_tick",V{done.started_tick}},{"finished_tick",V{done.tick}},{"food_consumed",V{done.food_consumed}},{"outcome",V{done.outcome}},{"explanation",V{done.outcome=="completed"?"本人实际完成一期志愿照护，消耗自己约定的一份口粮。":"本次照护没有完成，也未消费口粮；原出生责任继续保留。"}}}));}
    const Integer offer_count=offers.size(),agreement_count=agreements.size(),care_count=care.size();return {true,obj({{"actor_id",V{actor}},{"source_identity",identity(c)},{"page",V{page}},{"offer_count",V{offer_count}},{"agreement_count",V{agreement_count}},{"care_count",V{care_count}},{"current_offers",V{page_rows(std::move(offers),page)}},{"agreements",V{page_rows(std::move(agreements),page)}},{"care",V{page_rows(std::move(care),page)}},{"explanation",V{"本人听到有限私粮援助后才能接受。新帮助不改变亲代、监护、控制或原出生私粮责任；预付不等于已照护。"}}}),{}};
}
}
