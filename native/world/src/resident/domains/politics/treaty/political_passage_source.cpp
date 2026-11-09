#include "political_passage_source.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {namespace {
using EntryKey=std::tuple<Integer,StableId,StableId,Integer,Integer,Integer,StableId,StableId,StableId,StableId>;
Outcome<EntryKey> entry_key(const MovementExecutionRef& execution,Integer ordinal,const PassageBoundaryRef& boundary){
    if(ordinal<1)return {false,{},common_failure("InvalidPoliticalEntryOrdinal").error};
    Integer type=0,segment=0;StableId first,second;
    if(const auto* p=std::get_if<PersonalMovementExecution>(&execution)){first=p->action_id;second=p->journey_id;segment=p->segment_ordinal;}
    else if(const auto* g=std::get_if<GroupMovementExecution>(&execution)){type=1;first=g->journey_id;second=g->journey_id;segment=g->segment_ordinal;}
    else return {false,{},common_failure("UnsupportedPoliticalEntryExecution",{},ErrorKind::unsupported_capability).error};
    if(first.empty()||second.empty()||segment<0||boundary.id.empty()||boundary.map_id.empty()||boundary.from_site_id.empty()||boundary.to_site_id.empty())return {false,{},common_failure("InvalidPoliticalEntrySource").error};
    return {true,{type,first,second,segment,ordinal,static_cast<Integer>(boundary.kind),boundary.id,boundary.map_id,boundary.from_site_id,boundary.to_site_id},{}};
}
SourceRef source_ref(const StableId& id){return {"politics","political_passage_decided",id};}
}
Outcome<const PoliticalPassageDecisionSource*> read_political_passage_decision(const CommonStore& common,const PoliticalPassageDecisionState& state,const SourceRef& source){
    world_stage_observation::observe_node({world_stage_observation::NodeMetric::api_attempt, "politics.decision.header_reader", &common.state(), nullptr, {"fact", source.domain, source.kind, source.id, 0}, {}, -1});
    const auto* stored=state.decisions.find(source.id);const auto* fact=common.state().facts.find(source.id);
    if(source.domain!="politics"||source.kind!="political_passage_decided"||!stored||stored->id!=source.id||!fact||fact->domain!=source.domain||fact->kind!=source.kind||stored->decision.read_stamp)return {false,{},common_failure("PoliticalPassageDecisionSourceMissing",source.id).error};
    world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "politics.decision.header_only", &common.state(), nullptr, {"fact", source.domain, source.kind, source.id, stored->decision.source_schema_version}, {}, -1, stored->occurred_at.half_day_tick, true});
    return {true,stored,{}};
}
Outcome<SourceRef> adopt_political_passage_decision_in_candidate(CommonStore& common,PoliticalPassageDecisionState& state,const PoliticalPassageContext& context,const PoliticalPassageEntryRead& entries,const PassageRequest& request,const TransportManifestRead& manifest){
    if(&context.common!=static_cast<const CommonRead*>(&common)||context.identity.world_id!=common.state().identity.world_id||context.identity.execution_epoch!=common.state().identity.execution_epoch||context.identity.revision!=common.revision()||context.identity.tick!=common.tick())return {false,{},common_failure("PoliticalPassageCandidateMismatch").error};
    if(request.purpose!=PassagePurpose::enter||!request.execution)return {false,{},common_failure("ActualPassageEntryRequired").error};
    auto actual=entries.read_actual_entry(request,manifest);if(!actual.ok)return {false,{},actual.error};
    if(actual.value.composition_revision!=manifest.composition_revision||actual.value.manifest_revision!=manifest.manifest_revision)return {false,{},common_failure("PoliticalEntryManifestMismatch").error};
    auto key=entry_key(*request.execution,actual.value.entry_ordinal,request.boundary);if(!key.ok)return {false,{},key.error};
    for(const auto& row:state.decisions.rows()){auto old_key=entry_key(row.second.execution,row.second.entry_ordinal,row.second.decision.boundary);if(!old_key.ok)return {false,{},old_key.error};if(old_key.value!=key.value)continue;
        auto source=read_political_passage_decision(common,state,source_ref(row.first));if(!source.ok)return {false,{},source.error};
        const auto& old=*source.value;
        auto same_at=compare_world_moments(old.occurred_at,actual.value.occurred_at);if(!same_at.ok)return {false,{},same_at.error};
        if(same_at.value!=0||old.manifest_revision!=actual.value.manifest_revision||old.composition_revision!=actual.value.composition_revision||old.decision.subject.kind!=request.subject.kind||old.decision.subject.id!=request.subject.id||old.decision.mode!=request.mode||old.decision.source_schema_version!=context.selected_source_schema||
          (context.selected_source_schema==2&&(!same_political_passage_purpose(old.decision.purpose_context,request.purpose_context)||old.decision.requested_window.from_tick!=request.window.from_tick||old.decision.requested_window.until_tick!=request.window.until_tick)))return {false,{},common_failure("PoliticalPassageReplayMismatch",old.id).error};
        return {true,source_ref(old.id),{}};
    }
    auto decision=evaluate_political_passage(context,request,manifest);if(!decision.ok)return {false,{},decision.error};
    if(decision.value.admission!=PassageAdmission::allowed)return {false,{},common_failure("PoliticalPassageNotAdmitted",request.boundary.id).error};
    auto id=common.next_id("political_passage_decision");if(!id.ok)return {false,{},id.error};
    PoliticalPassageDecisionSource source;source.id=id.value;source.execution=*request.execution;source.carrier=request.carrier;
    source.entry_ordinal=actual.value.entry_ordinal;source.composition_revision=actual.value.composition_revision;source.manifest_revision=actual.value.manifest_revision;source.occurred_at=actual.value.occurred_at;
    source.decision=std::move(decision.value);source.decision.read_stamp.reset();
    auto appended=common.append_fact({id.value,"political_passage_decided","politics"});if(!appended.ok)return {false,{},appended.error};
    state.decisions.insert(id.value,std::move(source));return {true,source_ref(id.value),{}};
}
Status validate_political_passage_sources(const CommonStore& common,const PoliticalPassageDecisionState& state,const PoliticalPassageEntryRead& entries,const PoliticalPassageHistoryRead& history){
    std::set<EntryKey> unique;
    for(const auto& row:state.decisions.rows()){
        world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "politics.decision.full_rows", &common.state(), nullptr, {"fact", "politics", "political_passage_decided", row.first, row.second.decision.source_schema_version}, {}, -1});
        auto read=read_political_passage_decision(common,state,source_ref(row.first));if(!read.ok)return {false,{},read.error};const auto& source=*read.value;
        auto key=entry_key(source.execution,source.entry_ordinal,source.decision.boundary);if(!key.ok)return {false,{},key.error};if(!unique.insert(key.value).second)return common_failure("DuplicatePoliticalPassageEntry",source.id);
        if(source.decision.identity.world_id!=common.state().identity.world_id||source.decision.identity.revision<0||source.decision.identity.revision>common.revision()||(source.decision.source_schema_version!=1&&source.decision.source_schema_version!=2)||source.decision.admission!=PassageAdmission::allowed||!valid_source_ref(source.decision.admission_source))return common_failure("InvalidPoliticalPassageDecision",source.id);
        if(source.decision.toll&&(source.decision.toll->amount<0||source.decision.toll->currency_id.empty()||source.decision.toll->payee_wallet_id.empty()||!valid_source_ref(source.decision.toll->tariff_source_ref)))return common_failure("InvalidPoliticalPassageToll",source.id);
        if(source.decision.source_schema_version==2){auto shape=validate_political_passage_authority_payload(source.decision);if(!shape.ok)return shape;auto time=validate_political_passage_decision_source_time(source);if(!time.ok)return time;}
        auto actual=entries.validate_historical_entry(source);if(!actual.ok)return actual;auto policy=history.validate_historical_decision(source);if(!policy.ok)return policy;world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "politics.decision.full_validated", &common.state(), nullptr, {"fact", "politics", "political_passage_decided", source.id, source.decision.source_schema_version}, {}, -1, source.occurred_at.half_day_tick, true});
    }
    return common_success();
}
// 时间来自原实际进入，不接受以头序或当前 clock 代替原 M。
Status validate_political_passage_decision_source_time(const PoliticalPassageDecisionSource& source){
    auto equal=encode_world_moment(source.occurred_at);
    if(!equal.ok)return {false,{},equal.error};
    if(source.occurred_at.half_day_tick<0||source.decision.entry_tick!=source.occurred_at.half_day_tick)
        return common_failure("PoliticalPassageSourceMomentMismatch",source.id);
    return common_success();
}
}
