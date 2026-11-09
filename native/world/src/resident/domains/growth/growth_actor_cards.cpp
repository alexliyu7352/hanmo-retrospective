#include "resident/kernel/catalog_section_observation.hpp"
// 人物卡只读原采购或实际交涉往返来源；任务、外交、钱货与人物身份各守原账。
#include "growth_actor_cards.hpp"
#include <algorithm>
namespace hanmo::resident {
Status validate_growth_actor_card_catalog(const GrowthActorCardCatalog& cards,const CommonCatalog& shared){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 41, "GrowthActorCardCatalog", "validate_growth_actor_card_catalog", &cards);

    if(cards.version!=1&&cards.version!=2)return common_failure("UnsupportedActorCardVersion");
    if(!cards.enabled)return cards.definitions.empty()&&cards.version==1?common_success():common_failure("ActorCardsCapabilityRequired");
    if(cards.definitions.empty()||cards.definitions.size()>4096)return common_failure("InvalidActorCardCatalogSize");
    for(const auto& entry:cards.definitions.rows()){
        const auto& card=entry.second;
        if(entry.first.empty()||entry.first.size()>256||card.id!=entry.first)return common_failure("InvalidActorCardIdentity",entry.first);
        if(card.condition!="completed_procurement_twice_v1"&&(cards.version!=2||card.condition!="completed_envoy_round_trip_v1"))return common_failure("UnsupportedActorCardCondition",entry.first,ErrorKind::unsupported_capability);
        if(card.target_actor_id.empty()||std::find(shared.actor_ids.begin(),shared.actor_ids.end(),card.target_actor_id)==shared.actor_ids.end())return common_failure("MissingActorCardTarget",entry.first);
        for(const auto& prior:cards.definitions.rows()){if(&prior==&entry)break;if(prior.second.target_actor_id==card.target_actor_id)return common_failure("DuplicateActorCardTarget",entry.first);}
        for(const auto* key:{&card.name_key,&card.description_key,&card.hint_key})if(key->empty()||!shared.texts.find(*key))return common_failure("MissingActorCardText",entry.first);
    }
    return common_success();
}

// 成功与返报完整性由S3判断；本域只投影既定卡条件，不重结算或新增完成计数账。
static Outcome<GrowthActorCardsPage> project_cards(const CommonState& common,const CommonCatalog& shared,const TaskState& tasks,const TaskCatalog& task_catalog,const GrowthActorCardCatalog& cards,const StableId& actor,Integer offset,Integer limit,const EnvoyTaskState* envoy,const TaskEnvoyRead* envoy_read,bool require_player=true){
    if(!cards.enabled)return {false,{},common_failure("ActorCardsUnavailable",{},ErrorKind::unsupported_capability).error};
    const auto valid=validate_growth_actor_card_catalog(cards,shared);if(!valid.ok)return {false,{},valid.error};
    if(actor.empty()||(require_player&&actor!=common.metadata.player_actor_id))return {false,{},common_failure("NotPlayerActor",actor).error};
    if(!common.actors.find(actor))return {false,{},common_failure("UnknownActor",actor).error};
    if(common.identity.tick<0||common.identity.tick>2147483647||common.metadata.scenario_id.empty()||std::all_of(common.identity.world_id.begin(),common.identity.world_id.end(),[](auto value){return value==0;}))return {false,{},common_failure("InvalidActorCardWorld").error};
    const auto total=Integer(cards.definitions.size());
    if(offset<0||offset>total||limit<1||limit>64)return {false,{},common_failure("InvalidActorCardPage").error};
    const auto completed=task_completion_evidence_for_actor(tasks,task_catalog,actor);if(!completed.ok)return {false,{},completed.error};
    std::vector<TaskCompletionEvidence> procurement;
    for(const auto& source:completed.value){
        if(source.reported_tick<0||source.reported_tick>common.identity.tick)return {false,{},common_failure("ActorCardSourceTimeMismatch",source.task_id,ErrorKind::internal).error};
        if(source.template_id=="procurement")procurement.push_back(source);
    }
    std::sort(procurement.begin(),procurement.end(),[](const auto& a,const auto& b){return a.reported_tick!=b.reported_tick?a.reported_tick<b.reported_tick:a.task_id<b.task_id;});
    if(procurement.size()>2)procurement.resize(2);
    std::optional<GrowthEnvoyRoundTripEvidence> round_trip;
    if(std::any_of(cards.definitions.rows().begin(),cards.definitions.rows().end(),[](const auto& row){return row.second.condition=="completed_envoy_round_trip_v1";})){
        if(!envoy||!envoy_read)return {false,{},common_failure("ActorCardEnvoySourceRequired").error};
        auto source=read_verified_growth_envoy_round_trip(common,*envoy,*envoy_read,actor);if(!source.ok)return {false,{},source.error};round_trip=std::move(source.value);
    }
    GrowthActorCardsPage page;page.source_identity=common.identity;page.actor_id=actor;page.offset=offset;page.limit=limit;page.total=total;
    for(const auto& row:cards.definitions.rows())if(row.second.condition=="completed_envoy_round_trip_v1"?bool(round_trip):procurement.size()==2)++page.acquired_total;
    auto ids=cards.definitions.order();std::sort(ids.begin(),ids.end());
    for(Integer n=offset;n<std::min(total,offset+limit);++n){
        const auto& card=*cards.definitions.find(ids[std::size_t(n)]);GrowthActorCardRow row;row.card_id=card.id;row.target_actor_id=card.target_actor_id;row.completed_count=card.condition=="completed_envoy_round_trip_v1"?(round_trip?1:0):Integer(procurement.size());
        if(card.condition=="completed_envoy_round_trip_v1"&&round_trip){
            GrowthActorUnlockEvidence source;source.card_id=card.id;source.target_actor_id=card.target_actor_id;source.earner_actor_id=actor;source.world_id=common.identity.world_id;source.scenario_id=common.metadata.scenario_id;source.acquired_tick=round_trip->completed_tick;source.envoy=round_trip;row.acquired=std::move(source);
        }else if(card.condition=="completed_procurement_twice_v1"&&procurement.size()==2){
            GrowthActorUnlockEvidence source;source.card_id=card.id;source.target_actor_id=card.target_actor_id;source.earner_actor_id=actor;source.world_id=common.identity.world_id;source.scenario_id=common.metadata.scenario_id;source.acquired_tick=procurement.back().reported_tick;source.tasks=procurement;row.acquired=std::move(source);
        }
        page.cards.push_back(std::move(row));
    }
    return {true,std::move(page),{}};
}
Outcome<GrowthActorCardsPage> project_growth_actor_cards(const CommonState& c,const CommonCatalog& s,const TaskState& t,const TaskCatalog& tc,const GrowthActorCardCatalog& cards,const StableId& actor,Integer offset,Integer limit){return project_cards(c,s,t,tc,cards,actor,offset,limit,nullptr,nullptr);}
Outcome<GrowthActorCardsPage> project_growth_actor_cards_with_envoy(const CommonState& c,const CommonCatalog& s,const TaskState& t,const TaskCatalog& tc,const GrowthActorCardCatalog& cards,const EnvoyTaskState& envoy,const TaskEnvoyRead& read,const StableId& actor,Integer offset,Integer limit){return project_cards(c,s,t,tc,cards,actor,offset,limit,&envoy,&read);}
// 逐页复用同一真实资格规则，内部回执校验不会因后来接控而否认旧人的成果。
Outcome<std::vector<GrowthActorUnlockEvidence>> read_growth_actor_unlock_evidence(const CommonState& c,const CommonCatalog& s,const TaskState& t,const TaskCatalog& tc,const GrowthActorCardCatalog& cards,const StableId& actor,const EnvoyTaskState* envoy,const TaskEnvoyRead* read){
    if(!cards.enabled)return {true,{}, {}};
    std::vector<GrowthActorUnlockEvidence> out;
    for(Integer offset=0;offset<Integer(cards.definitions.size());offset+=64){auto page=project_cards(c,s,t,tc,cards,actor,offset,64,envoy,read,false);if(!page.ok)return {false,{},page.error};for(const auto& row:page.value.cards)if(row.acquired)out.push_back(*row.acquired);}
    return {true,std::move(out),{}};
}
// 展示文本来自同一目录；完成证据供专属跨局账保存，图鉴本身不能生成它们。
Outcome<BoundaryValue> encode_growth_actor_cards_page(const GrowthActorCardsPage& page,const GrowthActorCardCatalog& cards,const CommonCatalog& shared){
    using namespace boundary;using V=BoundaryValue;
    const auto valid=validate_growth_actor_card_catalog(cards,shared);if(!valid.ok)return {false,{},valid.error};
    if(!cards.enabled||page.actor_id.empty()||page.total!=Integer(cards.definitions.size())||page.offset<0||page.offset>page.total||page.limit<1||page.limit>64||Integer(page.cards.size())!=std::min(page.limit,page.total-page.offset))return {false,{},common_failure("InvalidActorCardProjection").error};
    auto hex=[](const WorldId& id){StableId value;const char* digits="0123456789abcdef";for(auto byte:id){value+=digits[byte>>4];value+=digits[byte&15];}return value;};
    auto ids=cards.definitions.order();std::sort(ids.begin(),ids.end());V::Array rows;
    for(std::size_t n=0;n<page.cards.size();++n){
        const auto& row=page.cards[n];const auto* definition=cards.definitions.find(row.card_id);
        const bool is_envoy=definition&&definition->condition=="completed_envoy_round_trip_v1";const Integer required=is_envoy?1:2;
        if(!definition||row.card_id!=ids[std::size_t(page.offset)+n]||row.target_actor_id!=definition->target_actor_id||row.completed_count<0||row.completed_count>required||row.acquired.has_value()!=(row.completed_count==required))return {false,{},common_failure("InvalidActorCardProjection",row.card_id).error};
        V acquired{std::monostate{}};
        if(row.acquired){
            const auto& source=*row.acquired;
            if(source.card_id!=row.card_id||source.target_actor_id!=row.target_actor_id||source.earner_actor_id!=page.actor_id||source.world_id!=page.source_identity.world_id||source.acquired_tick<0||source.acquired_tick>page.source_identity.tick||(is_envoy?(!source.tasks.empty()||!source.envoy):(source.tasks.size()!=2||bool(source.envoy))))return {false,{},common_failure("InvalidActorCardProjection",row.card_id).error};
            if(is_envoy){
                const auto& e=*source.envoy;
                if(e.actor_id!=source.earner_actor_id||e.world_id!=source.world_id||e.scenario_id!=source.scenario_id||e.completed_tick!=source.acquired_tick||e.accepted_tick<0||e.delivered_tick<e.accepted_tick||e.completed_tick<e.delivered_tick)return {false,{},common_failure("InvalidActorCardProjection",row.card_id).error};
                for(const auto* id:{&e.task_id,&e.contract_id,&e.acceptance_fact_id,&e.mission_authority_fact_id,&e.proposal_id,&e.delivery_fact_id,&e.presentation_fact_id,&e.completion_fact_id,&e.report_fact_id})if(id->empty()||id->size()>256)return {false,{},common_failure("InvalidActorCardProjection",row.card_id).error};
                auto evidence=obj({{"task_id",V{e.task_id}},{"contract_id",V{e.contract_id}},{"acceptance_fact_id",V{e.acceptance_fact_id}},{"mission_authority_fact_id",V{e.mission_authority_fact_id}},{"proposal_id",V{e.proposal_id}},{"delivery_fact_id",V{e.delivery_fact_id}},{"presentation_fact_id",V{e.presentation_fact_id}},{"completion_fact_id",V{e.completion_fact_id}},{"report_fact_id",V{e.report_fact_id}},{"accepted_tick",V{e.accepted_tick}},{"delivered_tick",V{e.delivered_tick}},{"completed_tick",V{e.completed_tick}}});
                acquired=obj({{"card_id",V{source.card_id}},{"target_actor_id",V{source.target_actor_id}},{"earner_actor_id",V{source.earner_actor_id}},{"world_id",V{hex(source.world_id)}},{"scenario_id",V{source.scenario_id}},{"acquired_tick",V{source.acquired_tick}},{"envoy",std::move(evidence)}});
            }else{
            V::Array tasks;
            for(const auto& task:source.tasks)tasks.push_back(obj({{"task_id",V{task.task_id}},{"template_id",V{task.template_id}},{"settlement_id",V{task.settlement_id}},{"reported_tick",V{task.reported_tick}}}));
            acquired=obj({{"card_id",V{source.card_id}},{"target_actor_id",V{source.target_actor_id}},{"earner_actor_id",V{source.earner_actor_id}},{"world_id",V{hex(source.world_id)}},{"scenario_id",V{source.scenario_id}},{"acquired_tick",V{source.acquired_tick}},{"tasks",V{std::move(tasks)}}});
            }
        }
        rows.push_back(obj({{"card_id",V{row.card_id}},{"category",V{StableId{"actor"}}},{"target_actor_id",V{row.target_actor_id}},{"name",V{*shared.texts.find(definition->name_key)}},{"description",V{*shared.texts.find(definition->description_key)}},{"hint",V{*shared.texts.find(definition->hint_key)}},{"completed_count",V{row.completed_count}},{"acquired",std::move(acquired)}}));
        if(cards.version==2){rows.back().as<V::Dict>()->emplace_back("condition_kind",V{definition->condition});rows.back().as<V::Dict>()->emplace_back("required_count",V{required});}
    }
    const auto& identity=page.source_identity;
    auto source=obj({{"world_id",V{hex(identity.world_id)}},{"execution_epoch",V{identity.execution_epoch}},{"revision",V{identity.revision}},{"tick",V{identity.tick}}});
    const auto next=page.offset+Integer(page.cards.size());
    auto output=obj({{"actor_id",V{page.actor_id}},{"source_identity",std::move(source)},{"offset",V{page.offset}},{"limit",V{page.limit}},{"total",V{page.total}},{"cards",V{std::move(rows)}},{"next_offset",next<page.total?V{next}:V{std::monostate{}}}});
    if(cards.version==2)output.as<V::Dict>()->emplace_back("source_version",V{Integer{2}});
    return {true,std::move(output),{}};
}
}
