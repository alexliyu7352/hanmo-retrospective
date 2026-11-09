// 生育页只显示本人实际会面、原承诺和亲历出生；别人的照护/死亡真值不自动成为本人知识。
#include "family_birth_commands.hpp"
#include "resident/views/common_presentation.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
using V=BoundaryValue;
// 每类页至多16条，完整世界账不交给UI。
V::Array slice(V::Array rows,Integer page){V::Array out;for(Integer i=page*16;i<std::min<Integer>(rows.size(),page*16+16);++i)out.push_back(std::move(rows[static_cast<std::size_t>(i)]));return out;}
V identity(const CommonStore& c){StableId world;const char* h="0123456789abcdef";for(auto byte:c.state().identity.world_id){world+=h[byte>>4];world+=h[byte&15];}const auto& i=c.state().identity;return boundary::obj({{"world_id",V{world}},{"execution_epoch",V{i.execution_epoch}},{"revision",V{i.revision}},{"tick",V{i.tick}}});}
// 名称投影不夹带远方位置、存活或私钱包。
V person(const CommonStore& c,const CommonCatalog& common,const StableId& actor){auto named=CommonPresentation(c.state(),common).display_name(actor);return boundary::obj({{"actor_id",V{actor}},{"name",V{named.ok?named.value:actor}}});}
V terms(const BirthCareTerms& t){return boundary::obj({{"caregiver_actor_id",V{t.caregiver_actor_id}},{"payer_actor_id",V{t.payer_actor_id}},{"place_id",V{t.place_id}},{"food_item_id",V{t.food_item_id}},{"birth_duration_ticks",V{t.birth_duration_ticks}},{"care_period_ticks",V{t.care_period_ticks}},{"food_per_period",V{t.food_per_period}},{"prepaid_care_periods",V{t.prepaid_care_periods}}});}
}
Outcome<BoundaryValue> query_family_birth_page(const CommonStore& c,const CommonCatalog& common,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,const FamilyBirthState& state,const BirthArrangementAccess& access,Integer page){
    using namespace boundary;if(!catalog.enabled)return {false,{},common_failure("BirthProfileRequired").error};if(page<0||page>4095)return {false,{},common_failure("InvalidBirthPage").error};
    auto history=validate_birth_arrangements(c,interactions,family,catalog,state,access);if(!history.ok)return {false,{},history.error};const auto actor=c.state().metadata.player_actor_id;V::Array current,proposals,arrangements,care;
    auto option=[&](const FamilyBirthCommand& command){auto q=preview_family_birth_command(c,meeting,interactions,contact,family,catalog,state,access,actor,command);return obj({{"kind",V{command.kind}},{"decision",V{command.decision}},{"action_id",V{command.action_id}},{"available",V{q.ok}},{"code",V{q.ok?StableId{}:q.error.original.code}}});};
    for(const auto& row:catalog.arrangements.rows()){
        if(row.second.listener_actor_id!=actor)continue;auto seen=preview_birth_arrangement_source(c,meeting,interactions,contact,family,catalog,actor,row.first,access.people);if(!seen.ok)continue;
        FamilyBirthCommand command;command.kind="family_birth_receive";command.arrangement_id=row.first;
        current.push_back(obj({{"arrangement_id",V{row.first}},{"other",person(c,common,row.second.speaker_actor_id)},{"terms",terms(row.second.terms)},{"option",option(command)}}));
    }
    for(const auto& row:state.proposals.rows()){
        const auto& source=row.second.source;if(source.listener_actor_id!=actor&&source.speaker_actor_id!=actor)continue;StableId response;for(const auto& reply:state.replies.rows())if(reply.second.proposal_id==row.first)response=reply.second.decision;
        V::Array options;if(response.empty())for(const auto& decision:source.listener_actor_id==actor?std::vector<StableId>{"accepted","refused"}:std::vector<StableId>{"withdrawn"}){FamilyBirthCommand command;command.kind="family_birth_reply";command.proposal_id=row.first;command.decision=decision;options.push_back(option(command));}
        proposals.push_back(obj({{"proposal_id",V{row.first}},{"other",person(c,common,source.listener_actor_id==actor?source.speaker_actor_id:source.listener_actor_id)},{"received_tick",V{source.tick}},{"terms",terms(source.terms)},{"response",V{response}},{"options",V{std::move(options)}}}));
    }
    for(const auto& row:state.replies.rows()){
        const auto& reply=row.second;if(reply.decision!="accepted"||!reply.confirmation)continue;const auto& source=*reply.confirmation;if(source.listener_actor_id!=actor&&source.speaker_actor_id!=actor)continue;
        const auto birth=std::find_if(state.births.rows().begin(),state.births.rows().end(),[&](const auto& x){return x.second.accepted_id==row.first;});const bool born=birth!=state.births.rows().end(),closed=history.value.closed_acceptances.count(row.first);
        V::Array options;FamilyBirthCommand command;
        if(!closed&&!born){command.kind="family_birth_withdraw";command.accepted_id=row.first;options.push_back(option(command));}
        if(!closed&&born&&source.terms.caregiver_actor_id==actor){if(const auto* pending=history.value.pending_care_actions.find(row.first)){command.kind="family_child_care_cancel";command.action_id=*pending;}else{command.kind="family_child_care";command.accepted_id=row.first;}options.push_back(option(command));}
        // 这里只给本人已参与的出生事实。资助终结不透露远方死亡原因，也不承诺一生照护已完成。
        V::Dict shown{{"accepted_id",V{row.first}},{"accepted_tick",V{reply.tick}},{"due_tick",V{reply.due_tick}},{"terms",terms(source.terms)},{"status",V{closed?"finite_arrangement_closed":born?"child_born":"awaiting_original_conditions"}},{"options",V{std::move(options)}}};
        if(born){shown.emplace_back("child",person(c,common,birth->second.actor_id));shown.emplace_back("birth_tick",V{birth->second.tick});}
        if(source.terms.caregiver_actor_id==actor){const auto* count=history.value.completed_care_periods.find(row.first);shown.emplace_back("personally_completed_periods",V{count?*count:Integer{0}});}
        arrangements.emplace_back(std::move(shown));
    }
    for(const auto& row:state.care.rows())if(row.second.caregiver_actor_id==actor){const auto& receipt=row.second;care.push_back(obj({{"fact_id",V{row.first}},{"action_id",V{receipt.action_id}},{"accepted_id",V{receipt.accepted_id}},{"child",person(c,common,receipt.child_actor_id)},{"started_tick",V{receipt.started_tick}},{"finished_tick",V{receipt.tick}},{"period_index",V{receipt.period_index}},{"food_consumed",V{receipt.food_consumed}},{"outcome",V{receipt.outcome}},{"explanation",V{receipt.outcome=="completed"?"本人完成原照护，实际消耗约定的一期私粮。":"这次照护没有完成，也未消耗口粮；具体处境须另核本人所知来源。"}}}));}
    const Integer current_count=current.size(),proposal_count=proposals.size(),arrangement_count=arrangements.size(),care_count=care.size();
    return {true,obj({{"actor_id",V{actor}},{"source_identity",identity(c)},{"page",V{page}},{"statement_count",V{current_count}},{"proposal_count",V{proposal_count}},{"arrangement_count",V{arrangement_count}},{"care_count",V{care_count}},{"current_statements",V{slice(std::move(current),page)}},{"proposals",V{slice(std::move(proposals),page)}},{"arrangements",V{slice(std::move(arrangements),page)}},{"care",V{slice(std::move(care),page)}},{"explanation",V{"双方独立决定生育，先明确有限照护与本人私粮。接受不会立即生成儿童，备粮不等于完成照护。"}}}),{}};
}
}
