#include "resident/domains/growth/growth_model.hpp"
// canonical取得只消费原业务结果；查询和冷验证不分配ID，也不重发本领或资源。
#include "card_acquisition_contract.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 原证据比较覆盖完整字段，不能仅同卡ID就把不相干来源当成同一取得。
bool same_task(const TaskCompletionEvidence& a,const TaskCompletionEvidence& b){return std::tie(a.task_id,a.template_id,a.settlement_id,a.reported_tick)==std::tie(b.task_id,b.template_id,b.settlement_id,b.reported_tick);}
bool same_envoy(const GrowthEnvoyRoundTripEvidence& a,const GrowthEnvoyRoundTripEvidence& b){return a.world_id==b.world_id&&std::tie(a.actor_id,a.task_id,a.contract_id,a.acceptance_fact_id,a.mission_authority_fact_id,a.proposal_id,a.delivery_fact_id,a.presentation_fact_id,a.completion_fact_id,a.report_fact_id,a.scenario_id,a.accepted_tick,a.delivered_tick,a.completed_tick)==std::tie(b.actor_id,b.task_id,b.contract_id,b.acceptance_fact_id,b.mission_authority_fact_id,b.proposal_id,b.delivery_fact_id,b.presentation_fact_id,b.completion_fact_id,b.report_fact_id,b.scenario_id,b.accepted_tick,b.delivered_tick,b.completed_tick);}
bool same_camp(const GrowthCampTrainingExperience& a,const GrowthCampTrainingExperience& b){return a.world_id==b.world_id&&std::tie(a.fact_id,a.actor_id,a.garrison_id,a.place_id,a.unit_id,a.scenario_id,a.started_tick,a.completed_tick)==std::tie(b.fact_id,b.actor_id,b.garrison_id,b.place_id,b.unit_id,b.scenario_id,b.started_tick,b.completed_tick);}
struct VerifiedAcquisition {StableId actor,card;GrowthCardCategory category=GrowthCardCategory::technique;Integer definition_version=1,tick=0;std::optional<StableId> unlock,effect;};
using Verified=Outcome<VerifiedAcquisition>;
Verified fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
bool same_at(const ActorFoundationAt& a,const ActorFoundationAt& b){if(a.kind!=b.kind)return false;if(a.kind==ActorFoundationAt::Kind::legacy_tick)return a.legacy_tick==b.legacy_tick;auto n=compare_world_moments(a.exact_moment,b.exact_moment);return !a.legacy_tick&&!b.legacy_tick&&n.ok&&!n.value;}
Outcome<WorldMoment> source_now(const CardAcquisitionSources& s){if(s.actual_clock)return s.actual_clock->read_current_moment();if(s.growth_catalog.complete_study_sources)return {false,{},common_failure("CardAcquisitionActualClockRequired").error};return {true,WorldMoment{s.common.identity.tick,{}},{}};}
Outcome<ActorFoundationAt> basis_at(const CardAcquisitionSources& s,const CardAcquisitionBasis& basis,Integer tick){if(const auto* training=std::get_if<GrowthTrainingCardCompletion>(&basis))return read_growth_training_card_acquired_at(s,training->fact_id);ActorFoundationAt out;out.kind=ActorFoundationAt::Kind::legacy_tick;out.legacy_tick=tick;return {true,std::move(out),{}};}
// 旧真实owned引用必须回读已完成训练/明确初态和原本领；当前值不补出来源。
Verified owned_source(const CardAcquisitionSources& s,const ExistingGrowthCardAcquisitionSource& ref){
    const auto* owner=s.growth.cards?s.growth.cards->owned.find(ref.actor_id):nullptr;const auto* record=owner?owner->find(ref.card_id):nullptr;
    const auto* definition=s.growth_catalog.cards.definitions.find(ref.card_id);const auto* actor=s.common.actors.find(ref.actor_id);
    if(!s.growth_catalog.cards.enabled||!record||!definition||!actor||record->actor_id!=ref.actor_id||record->card_id!=ref.card_id||record->acquired_tick<0||record->acquired_tick>s.common.identity.tick)return fail("InvalidExistingCardAcquisition",ref.card_id);
    if(definition->category!=GrowthCardCategory::technique&&definition->category!=GrowthCardCategory::heart)return fail("UnsupportedCardEffect",ref.card_id);
    if(const auto* mastery=actor->combat.learned.find(definition->personal_skill_id);!mastery||*mastery<0||*mastery>100)return fail("CardAbilitySourceMismatch",ref.card_id);
    if(record->source_kind==GrowthCardSourceKind::scenario_initial){
        if(record->acquired_tick!=0||record->source_id!=s.common.metadata.scenario_id||std::none_of(s.growth_catalog.cards.initial_grants.begin(),s.growth_catalog.cards.initial_grants.end(),[&](const auto& row){return row.actor_id==ref.actor_id&&row.card_id==ref.card_id;}))return fail("InvalidInitialCardSource",ref.card_id);
    }else if(record->source_kind==GrowthCardSourceKind::training){
        const auto* value=s.growth.facts.find(record->source_id);const auto* training=value?std::get_if<TrainingFact>(value):nullptr;const auto* header=s.common.facts.find(record->source_id);
        if(!training||!header||header->domain!="cultivation"||header->kind!="training"||training->phase!="completed"||!training->success||(training->action_kind!="learn_skill"&&training->action_kind!="teach_skill")||training->training.student_id!=ref.actor_id||training->training.skill_id!=definition->personal_skill_id||training->completed_tick!=record->acquired_tick)return fail("InvalidCardLearningSource",ref.card_id);
    }else return fail("InvalidCardSourceKind",ref.card_id);
    return {true,{ref.actor_id,ref.card_id,definition->category,1,record->acquired_tick,{},definition->personal_skill_id},{}};
}
// 完整采购/使者证据必须与原S3 reader相同，不以两条任意任务字符串授新局资格。
Verified unlock_source(const CardAcquisitionSources& s,const GrowthActorUnlockEvidence& evidence,bool envoy){
    const auto* definition=s.actor_cards.definitions.find(evidence.card_id);
    if(!s.actor_cards.enabled||!definition||definition->id!=evidence.card_id||(s.actor_cards.version!=1&&s.actor_cards.version!=2)||evidence.target_actor_id!=definition->target_actor_id||evidence.world_id!=s.common.identity.world_id||evidence.scenario_id!=s.common.metadata.scenario_id||!s.common.actors.find(evidence.earner_actor_id)||evidence.acquired_tick<0||evidence.acquired_tick>s.common.identity.tick||bool(evidence.envoy)!=envoy||(envoy?!evidence.tasks.empty():evidence.tasks.size()!=2))return fail("InvalidActorUnlockAcquisition",evidence.card_id);
    if(envoy){if(s.actor_cards.version!=2||definition->condition!="completed_envoy_round_trip_v1"||!s.envoy||!s.envoy_read)return fail("ActorCardEnvoySourceRequired",evidence.card_id);auto original=read_verified_growth_envoy_round_trip(s.common,*s.envoy,*s.envoy_read,evidence.earner_actor_id);if(!original.ok)return {false,{},original.error};if(!original.value||!same_envoy(*original.value,*evidence.envoy)||evidence.acquired_tick!=original.value->completed_tick)return fail("ActorUnlockAcquisitionSourceMismatch",evidence.card_id);}
    else {if(definition->condition!="completed_procurement_twice_v1")return fail("ActorUnlockConditionMismatch",evidence.card_id);auto original=read_first_two_procurement_completion_evidence(s.tasks,s.task_catalog,evidence.earner_actor_id);if(!original.ok)return {false,{},original.error};if(!original.value[0]||!original.value[1]||!same_task(*original.value[0],evidence.tasks[0])||!same_task(*original.value[1],evidence.tasks[1])||evidence.acquired_tick!=original.value[1]->reported_tick)return fail("ActorUnlockAcquisitionSourceMismatch",evidence.card_id);}
    return {true,{evidence.earner_actor_id,evidence.card_id,GrowthCardCategory::actor,s.actor_cards.version,evidence.acquired_tick,evidence.target_actor_id,{}},{}};
}
// 见闻保实际参训完整源；本人后来控制切换不影响已发生的经历。
Verified camp_source(const CardAcquisitionSources& s,const GrowthCampTrainingExperience& evidence,const StableId& card){
    const auto* definition=s.insight_cards.definitions.find(card);if(!s.insight_cards.enabled||!definition||definition->condition!="completed_camp_training_v1")return fail("InvalidInsightCardAcquisition",card);
    auto actual=read_verified_growth_camp_training_experience(s.common,s.military,evidence.actor_id);if(!actual.ok)return {false,{},actual.error};if(!actual.value||!same_camp(*actual.value,evidence))return fail("InsightAcquisitionSourceMismatch",card);
    return {true,{evidence.actor_id,card,GrowthCardCategory::insight,1,evidence.completed_tick,{},{}},{}};
}
Verified legacy_source(const CardAcquisitionSources& s,const LegacyCardAcquisitionSource& source,const StableId& card){
    return std::visit([&](const auto& value)->Verified{using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,ExistingGrowthCardAcquisitionSource>)return owned_source(s,value);
        else if constexpr(std::is_same_v<T,ActorUnlockEvidenceV1>)return unlock_source(s,value.evidence,false);
        else if constexpr(std::is_same_v<T,ActorUnlockEvidenceV2>)return unlock_source(s,value.evidence,true);
        else return camp_source(s,value.evidence,card);
    },source);
}
// 新canonical初态和训练仍使用原真实本领writer；此处不复制或增加掌握值。
Verified basis_source(const CardAcquisitionSources& s,const CardAcquisitionBasis& basis,const StableId& card){
    return std::visit([&](const auto& value)->Verified{using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,ActorUnlockEvidenceV1>)return unlock_source(s,value.evidence,false);
        else if constexpr(std::is_same_v<T,ActorUnlockEvidenceV2>)return unlock_source(s,value.evidence,true);
        else if constexpr(std::is_same_v<T,CampTrainingExperienceV1>)return camp_source(s,value.evidence,card);
        else {
            const auto* definition=s.growth_catalog.cards.definitions.find(value.card_id);const auto* actor=s.common.actors.find(value.actor_id);
            if(!definition||!actor||!s.growth_catalog.cards.enabled||!actor->combat.learned.find(definition->personal_skill_id)||*actor->combat.learned.find(definition->personal_skill_id)<0||*actor->combat.learned.find(definition->personal_skill_id)>100)return fail("CardAbilitySourceMismatch",value.card_id);
            if(definition->category!=GrowthCardCategory::technique&&definition->category!=GrowthCardCategory::heart)return fail("UnsupportedCardEffect",value.card_id);
            Integer tick=0;
            if constexpr(std::is_same_v<T,ScenarioInitialCardGrant>){if(value.scenario_id!=s.common.metadata.scenario_id||std::none_of(s.growth_catalog.cards.initial_grants.begin(),s.growth_catalog.cards.initial_grants.end(),[&](const auto& g){return g.actor_id==value.actor_id&&g.card_id==value.card_id;}))return fail("InvalidInitialCardSource",value.card_id);}
            else {const auto* f=s.growth.facts.find(value.fact_id);const auto* t=f?std::get_if<TrainingFact>(f):nullptr;const auto* h=s.common.facts.find(value.fact_id);auto now=source_now(s);if(!now.ok)return {false,{},now.error};if(!t||!h||h->domain!="cultivation"||h->kind!="training"||t->phase!="completed"||!t->success||(t->action_kind!="learn_skill"&&t->action_kind!="teach_skill")||t->training.student_id!=value.actor_id||t->training.skill_id!=value.personal_skill_id||definition->personal_skill_id!=value.personal_skill_id||t->training.mastery_after<0||t->training.mastery_after>100||*actor->combat.learned.find(value.personal_skill_id)<t->training.mastery_after||t->completed_tick<0||t->completed_tick>now.value.half_day_tick)return fail("InvalidCardLearningSource",value.fact_id);tick=t->completed_tick;}
            return {true,{value.actor_id,value.card_id,definition->category,1,tick,{},definition->personal_skill_id},{}};
        }
    },basis);
}
// 表中一次键必须跨新旧authority唯一；无法用另一source重编号刷新取得。
std::optional<ProfileAcquisitionSource> existing(const CardAcquisitionState& state,const StableId& actor,const StableId& card){for(const auto& row:state.card_acquisitions.rows())if(row.second.actor_id==actor&&row.second.card_id==card)return ProfileAcquisitionSource{CardAcquisitionReceiptRefV2{row.first}};for(const auto& row:state.legacy_card_source_mappings.rows())if(row.second.actor_id==actor&&row.second.card_id==card)return ProfileAcquisitionSource{LegacyCardSourceMappingRefV1{row.first}};return {};}
Outcome<std::vector<std::pair<StableId,LegacyCardAcquisitionSource>>> actual_legacy(const CardAcquisitionSources& s,const StableId& actor){
    std::vector<std::pair<StableId,LegacyCardAcquisitionSource>> out;if(!s.common.actors.find(actor))return {false,{},common_failure("UnknownActor",actor).error};
    if(s.growth.cards)if(const auto* owned=s.growth.cards->owned.find(actor))for(const auto& row:owned->rows())out.push_back({row.first,ExistingGrowthCardAcquisitionSource{actor,row.first}});
    auto unlocks=read_growth_actor_unlock_evidence(s.common,s.shared,s.tasks,s.task_catalog,s.actor_cards,actor,s.envoy,s.envoy_read);if(!unlocks.ok)return {false,{},unlocks.error};for(const auto& value:unlocks.value)out.push_back({value.card_id,value.envoy?LegacyCardAcquisitionSource{ActorUnlockEvidenceV2{value}}:LegacyCardAcquisitionSource{ActorUnlockEvidenceV1{value}}});
    if(s.insight_cards.enabled){auto camp=read_verified_growth_camp_training_experience(s.common,s.military,actor);if(!camp.ok)return {false,{},camp.error};if(camp.value)for(const auto& row:s.insight_cards.definitions.rows())out.push_back({row.first,CampTrainingExperienceV1{*camp.value}});}
    std::set<StableId> cards;for(const auto& row:out)if(!cards.insert(row.first).second)return {false,{},common_failure("DuplicateCanonicalCardDefinition",row.first).error};return {true,std::move(out),{}};
}
}
// 先证明所有原源，再在同候选分配映射；任何后续技术失败由完整世界候选撤回。
Outcome<std::vector<LegacyCardSourceMappingRefV1>> bind_verified_legacy_card_acquisitions(CommonStore& common,CardAcquisitionState& state,const CardAcquisitionSources& sources,const StableId& actor){
    if(&sources.common!=&common.state())return {false,{},common_failure("CardAcquisitionCandidateMismatch").error};
    auto rows=actual_legacy(sources,actor);if(!rows.ok)return {false,{},rows.error};std::vector<LegacyCardSourceMappingRefV1> bound;
    // Preflight the real sources before allocating any mapping. The original
    // whole-world candidate, not a second deep state clone, owns rollback.
    for(const auto& row:rows.value){auto source=legacy_source(sources,row.second,row.first);if(!source.ok)return {false,{},source.error};if(source.value.actor!=actor||source.value.card!=row.first)return {false,{},common_failure("LegacyCardSourceIdentityMismatch",row.first).error};if(auto old=existing(state,actor,row.first)){auto read=read_card_acquisition_v2(sources,state,*old);if(!read.ok)return {false,{},read.error};}}
    for(const auto& row:rows.value){if(existing(state,actor,row.first))continue;
        auto id=common.next_id("card_source_mapping");if(!id.ok)return {false,{},id.error};LegacyCardSourceMapping mapping;mapping.mapping_id=id.value;mapping.actor_id=actor;mapping.card_id=row.first;mapping.legacy_source=row.second;mapping.canonical_acquisition_identity={sources.common.identity.world_id,actor,row.first};if(!state.legacy_card_source_mappings.insert(id.value,std::move(mapping)))return {false,{},common_failure("DuplicateCardSourceMapping",id.value).error};bound.push_back({id.value});
    }return {true,std::move(bound),{}};
}
// 不从旧整数source猜精确offset；新精确producer接入后应在其真实At adapter验证。
Outcome<ProfileAcquisitionSource> record_verified_card_acquisition(CommonStore& common,CardAcquisitionState& state,const CardAcquisitionSources& sources,const StableId& card,const CardAcquisitionBasis& basis,const ActorFoundationAt& at){
    if(&sources.common!=&common.state())return {false,{},common_failure("CardAcquisitionCandidateMismatch").error};
    if(card.empty())return {false,{},common_failure("CardAcquisitionDefinitionRequired").error};auto value=basis_source(sources,basis,card);if(!value.ok)return {false,{},value.error};if(value.value.card!=card)return {false,{},common_failure("CardAcquisitionDefinitionMismatch",card).error};if(auto old=existing(state,value.value.actor,card)){auto valid=read_card_acquisition_v2(sources,state,*old);return valid.ok?Outcome<ProfileAcquisitionSource>{true,*old,{}}:Outcome<ProfileAcquisitionSource>{false,{},valid.error};}
    auto original_at=basis_at(sources,basis,value.value.tick);if(!original_at.ok)return {false,{},original_at.error};if(!same_at(at,original_at.value))return {false,{},common_failure("AcquisitionMomentSourceUnavailable",card).error};
    if(sources.growth.cards)if(const auto* owner=sources.growth.cards->owned.find(value.value.actor))if(owner->find(card))return {false,{},common_failure("LegacyCardMappingRequired",card).error};
    auto id=common.next_id("card_acquisition");if(!id.ok)return {false,{},id.error};CardAcquisitionReceiptV2 receipt;receipt.receipt_id=id.value;receipt.actor_id=value.value.actor;receipt.card_id=card;receipt.category=value.value.category;receipt.definition_version=value.value.definition_version;receipt.acquired_at=at;receipt.mode=sources.mode;receipt.acquisition_basis=basis;receipt.applied_local_effect_ref=value.value.effect;
    if(!state.card_acquisitions.insert(id.value,std::move(receipt)))return {false,{},common_failure("DuplicateCardAcquisition",id.value).error};return {true,ProfileAcquisitionSource{CardAcquisitionReceiptRefV2{id.value}}, {}};
}
// 每次读取都沿canonical表反读原来源；source不存在或同ID换payload精确拒绝。
Outcome<CardAcquisitionReadV2>
read_card_acquisition_v2(const CardAcquisitionSources &sources,
                         const CardAcquisitionState &state,
                         const ProfileAcquisitionSource &source) {
  CardAcquisitionReadV2 out;
  out.source = source;
  out.mode = sources.mode;
  Verified value;
  if (const auto *ref = std::get_if<CardAcquisitionReceiptRefV2>(&source)) {
    const auto *r = state.card_acquisitions.find(ref->receipt_id);
    if (!r || r->receipt_id != ref->receipt_id)
      return {false,
              {},
              common_failure("UnknownCardAcquisition", ref->receipt_id).error};
    value = basis_source(sources, r->acquisition_basis, r->card_id);
    if (!value.ok)
      return {false, {}, value.error};
    auto original_at =
        basis_at(sources, r->acquisition_basis, value.value.tick);
    if (!original_at.ok)
      return {false, {}, original_at.error};
    if (r->actor_id != value.value.actor || r->card_id != value.value.card ||
        r->category != value.value.category ||
        r->definition_version != value.value.definition_version ||
        r->mode != sources.mode ||
        !same_at(r->acquired_at, original_at.value) ||
        r->applied_local_effect_ref != value.value.effect)
      return {false,
              {},
              common_failure("CardAcquisitionReceiptMismatch", ref->receipt_id)
                  .error};
    out.acquired_at = r->acquired_at;
  } else {
    const auto &mapping_ref = std::get<LegacyCardSourceMappingRefV1>(source);
    const auto *r =
        state.legacy_card_source_mappings.find(mapping_ref.mapping_id);
    if (!r || r->mapping_id != mapping_ref.mapping_id)
      return {false,
              {},
              common_failure("UnknownCardSourceMapping", mapping_ref.mapping_id)
                  .error};
    value = legacy_source(sources, r->legacy_source, r->card_id);
    if (!value.ok)
      return {false, {}, value.error};
    if (r->actor_id != value.value.actor || r->card_id != value.value.card ||
        r->canonical_acquisition_identity.world_id !=
            sources.common.identity.world_id ||
        r->canonical_acquisition_identity.actor_id != r->actor_id ||
        r->canonical_acquisition_identity.card_id != r->card_id)
      return {
          false,
          {},
          common_failure("CardSourceMappingMismatch", mapping_ref.mapping_id)
              .error};
    out.acquired_at.kind = ActorFoundationAt::Kind::legacy_tick;
    out.acquired_at.legacy_tick = value.value.tick;
  }
  out.canonical_identity = {sources.common.identity.world_id, value.value.actor,
                            value.value.card};
  out.category = value.value.category;
  out.definition_version = value.value.definition_version;
  out.actor_unlock_target = value.value.unlock;
  out.applied_local_effect_ref = value.value.effect;
  return {true, std::move(out), {}};
}
namespace {
// canonical记录必须使用Common真实分配序号；不接受外部伪ID或未来序号。
bool allocated_card_id(const StableId& id,const StableId& prefix,Integer next){const auto start=prefix+":";if(id.rfind(start,0)!=0)return false;const auto text=id.substr(start.size());Integer n=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&n>0&&n<next&&text==std::to_string(n);}
}
// 完整覆盖由原producer反向枚举；删掉映射与outbox不能藏掉实际取得。
Status validate_card_acquisitions(const CardAcquisitionSources& sources,const CardAcquisitionState& state){
    if(sources.mode!=CharacterCreationMode::standard&&sources.mode!=CharacterCreationMode::free)return common_failure("InvalidCardAcquisitionMode");
    // Cold coverage scans original owners directly. It does not materialize a
    // second vector of every acquired card or a deep string-key set.
    const auto has_key=[&](const StableId& actor,const StableId& card){for(const auto& row:state.card_acquisitions.rows())if(row.second.actor_id==actor&&row.second.card_id==card)return true;for(const auto& row:state.legacy_card_source_mappings.rows())if(row.second.actor_id==actor&&row.second.card_id==card)return true;return false;};
    auto actor_catalog=validate_growth_actor_card_catalog(sources.actor_cards,sources.shared);if(!actor_catalog.ok)return actor_catalog;
    auto insight_catalog=validate_growth_insight_card_catalog(sources.insight_cards,sources.shared);if(!insight_catalog.ok)return insight_catalog;
    // One canonical key cannot stand in for two selected definitions.
    for(const auto& row:sources.actor_cards.definitions.rows())if(sources.growth_catalog.cards.definitions.find(row.first)||sources.insight_cards.definitions.find(row.first))return common_failure("DuplicateCanonicalCardDefinition",row.first);
    for(const auto& row:sources.insight_cards.definitions.rows())if(sources.growth_catalog.cards.definitions.find(row.first))return common_failure("DuplicateCanonicalCardDefinition",row.first);
    auto check=[&](const ProfileAcquisitionSource& source)->Status{auto read=read_card_acquisition_v2(sources,state,source);if(!read.ok)return {false,{},read.error};const auto& actor=read.value.canonical_identity.actor_id;const auto& card=read.value.canonical_identity.card_id;std::size_t count=0;for(const auto& row:state.card_acquisitions.rows())if(row.second.actor_id==actor&&row.second.card_id==card)++count;for(const auto& row:state.legacy_card_source_mappings.rows())if(row.second.actor_id==actor&&row.second.card_id==card)++count;if(count!=1)return common_failure("DuplicateCanonicalCardAcquisition");return common_success();};
    for(const auto& row:state.card_acquisitions.rows()){if(!allocated_card_id(row.first,"card_acquisition",sources.common.next_instance_id))return common_failure("InvalidCardAcquisitionIdentity",row.first);if(sources.growth.cards)if(const auto* old=sources.growth.cards->owned.find(row.second.actor_id))if(old->find(row.second.card_id))return common_failure("DuplicateLegacyAndCanonicalAuthority",row.first);auto valid=check(CardAcquisitionReceiptRefV2{row.first});if(!valid.ok)return valid;}
    for(const auto& row:state.legacy_card_source_mappings.rows()){if(!allocated_card_id(row.first,"card_source_mapping",sources.common.next_instance_id))return common_failure("InvalidCardSourceMappingIdentity",row.first);auto valid=check(LegacyCardSourceMappingRefV1{row.first});if(!valid.ok)return valid;}
    for(const auto& actor:sources.common.actors.rows()){
        if(sources.growth.cards)if(const auto* owned=sources.growth.cards->owned.find(actor.first))for(const auto& row:owned->rows())if(!has_key(actor.first,row.first))return common_failure("MissingCanonicalCardAcquisition",actor.first+"/"+row.first);
        if(sources.actor_cards.enabled){auto tasks=read_first_two_procurement_completion_evidence(sources.tasks,sources.task_catalog,actor.first);if(!tasks.ok)return {false,{},tasks.error};bool envoy_acquired=false;if(std::any_of(sources.actor_cards.definitions.rows().begin(),sources.actor_cards.definitions.rows().end(),[](const auto& row){return row.second.condition=="completed_envoy_round_trip_v1";})){if(!sources.envoy||!sources.envoy_read)return common_failure("ActorCardEnvoySourceRequired");auto source=read_verified_growth_envoy_round_trip(sources.common,*sources.envoy,*sources.envoy_read,actor.first);if(!source.ok)return {false,{},source.error};envoy_acquired=bool(source.value);}for(const auto& row:sources.actor_cards.definitions.rows())if((row.second.condition=="completed_procurement_twice_v1"?bool(tasks.value[0]&&tasks.value[1]):envoy_acquired)&&!has_key(actor.first,row.first))return common_failure("MissingCanonicalCardAcquisition",actor.first+"/"+row.first);}
        if(sources.insight_cards.enabled){auto camp=read_verified_growth_camp_training_experience(sources.common,sources.military,actor.first);if(!camp.ok)return {false,{},camp.error};if(camp.value)for(const auto& row:sources.insight_cards.definitions.rows())if(!has_key(actor.first,row.first))return common_failure("MissingCanonicalCardAcquisition",actor.first+"/"+row.first);}
    }
    if(sources.growth_catalog.complete_study_sources&&sources.growth_catalog.cards.enabled){
        // A current learned ability and every successful actual learning result
        // must have one of the two canonical authorities, including NPC cards.
        for(const auto& actor:sources.common.actors.rows())for(const auto& definition:sources.growth_catalog.cards.definitions.rows())if(actor.second.combat.learned.find(definition.second.personal_skill_id)&&!has_key(actor.first,definition.first))return common_failure("MissingCanonicalCardAcquisition",actor.first+"/"+definition.first);
        for(const auto& fact:sources.growth.facts.rows()){const auto* training=std::get_if<TrainingFact>(&fact.second);if(!training||training->phase!="completed"||!training->success||(training->action_kind!="learn_skill"&&training->action_kind!="teach_skill"))continue;
            auto completed=read_growth_training_card_acquired_at(sources,fact.first);if(!completed.ok)return {false,{},completed.error};
            for(const auto& definition:sources.growth_catalog.cards.definitions.rows())if(definition.second.personal_skill_id==training->training.skill_id){auto acquired=existing(state,training->training.student_id,definition.first);if(!acquired)return common_failure("MissingCanonicalCardAcquisition",definition.first);auto original=read_card_acquisition_v2(sources,state,*acquired);if(!original.ok)return {false,{},original.error};const auto& at=original.value.acquired_at;const auto m=at.kind==ActorFoundationAt::Kind::legacy_tick?WorldMoment{at.legacy_tick,{}}:at.exact_moment;const auto n=completed.value.kind==ActorFoundationAt::Kind::legacy_tick?WorldMoment{completed.value.legacy_tick,{}}:completed.value.exact_moment;auto order=compare_world_moments(m,n);if(!order.ok)return {false,{},order.error};if(order.value>0)return common_failure("CardFirstSourceMismatch",definition.first);
                if(!order.value)if(const auto* receipt=std::get_if<CardAcquisitionReceiptRefV2>(&*acquired)){const auto* row=state.card_acquisitions.find(receipt->receipt_id);if(const auto* cause=row?std::get_if<GrowthTrainingCardCompletion>(&row->acquisition_basis):nullptr){const GrowthTerminalPublication* first=nullptr;const GrowthTerminalPublication* current=nullptr;for(const auto& publication:sources.growth.terminal_publications.rows()){if(publication.second.action_id==cause->fact_id)first=&publication.second;if(publication.second.action_id==fact.first)current=&publication.second;}const auto& ids=sources.common.facts.order();if(!first||!current||std::find(ids.begin(),ids.end(),first->publication_id)>std::find(ids.begin(),ids.end(),current->publication_id))return common_failure("CardFirstSourceMismatch",definition.first);}}
            }
        }
    }
    return common_success();
}
// 公开页只读当前受控人物，稳定world/revision/epoch由原边界同源封装。
Outcome<CardAcquisitionPage> read_controlled_card_acquisitions(const CardAcquisitionSources& sources,const CardAcquisitionState& state,const StableId& actor,Integer offset,Integer limit){
    if(actor!=sources.common.metadata.player_actor_id)return {false,{},common_failure("NotPlayerActor",actor).error};if(!sources.common.actors.find(actor))return {false,{},common_failure("UnknownActor",actor).error};if(offset<0||limit<1||limit>256)return {false,{},common_failure("InvalidCardAcquisitionPage").error};
    std::vector<ProfileAcquisitionSource> refs;for(const auto& row:state.card_acquisitions.rows())if(row.second.actor_id==actor)refs.push_back(CardAcquisitionReceiptRefV2{row.first});for(const auto& row:state.legacy_card_source_mappings.rows())if(row.second.actor_id==actor)refs.push_back(LegacyCardSourceMappingRefV1{row.first});
    CardAcquisitionPage out;out.source_identity=sources.common.identity;out.actor_id=actor;out.offset=offset;out.limit=limit;out.total=Integer(refs.size());if(offset>out.total)return {false,{},common_failure("InvalidCardAcquisitionPage").error};for(Integer i=offset;i<out.total&&i-offset<limit;++i){auto read=read_card_acquisition_v2(sources,state,refs[std::size_t(i)]);if(!read.ok)return {false,{},read.error};out.rows.push_back(std::move(read.value));}return {true,std::move(out),{}};
}
}

namespace hanmo::resident {
Outcome<ActorFoundationAt> read_growth_training_card_acquired_at(const CardAcquisitionSources& s,const StableId& id){
 const auto fail=[&](const char* code)->Outcome<ActorFoundationAt>{return {false,{},common_failure(code,id).error};};
 const auto* raw=s.growth.facts.find(id);const auto* fact=raw?std::get_if<TrainingFact>(raw):nullptr;const auto* header=s.common.facts.find(id);
 if(!fact||!header||header->domain!="cultivation"||header->kind!="training"||fact->phase!="completed"||!fact->success||(fact->action_kind!="learn_skill"&&fact->action_kind!="teach_skill"))return fail("CardActualCompletedTrainingRequired");
 ActorFoundationAt at;if(!s.growth_catalog.complete_study_sources){at.kind=ActorFoundationAt::Kind::legacy_tick;at.legacy_tick=fact->completed_tick;return {true,std::move(at),{}};}
 auto now=source_now(s);if(!now.ok)return {false,{},now.error};const GrowthTerminalPublication* publication=nullptr;for(const auto& row:s.growth.terminal_publications.rows())if(row.second.action_id==id){if(publication||row.first!=row.second.publication_id)return fail("CardTrainingTerminalPublicationRequired");publication=&row.second;}
 if(!publication||publication->original_result_ref.kind!=LifeFixedOutcomeRef::Kind::training||publication->original_result_ref.id!=id||publication->terminal_source_schema!=1||publication->terminal_source_ref.domain!="growth"||publication->terminal_source_ref.kind!="growth_fixed_service_boundary")return fail("CardTrainingTerminalPublicationRequired");
 const auto* published=s.common.facts.find(publication->publication_id);const auto& order=s.common.facts.order();if(!published||published->domain!="growth"||published->kind!="growth_result_terminal"||std::find(order.begin(),order.end(),id)>=std::find(order.begin(),order.end(),publication->publication_id))return fail("CardTrainingTerminalHeaderMismatch");
 const auto* terminal=s.growth.fixed_service_boundaries.find(publication->terminal_source_ref.id);if(!terminal||terminal->boundary_id!=publication->terminal_source_ref.id||terminal->kind!=LifeFixedBoundaryKind::completed||!terminal->original_outcome_ref||terminal->original_outcome_ref->kind!=LifeFixedOutcomeRef::Kind::training||terminal->original_outcome_ref->id!=id)return fail("CardTrainingTerminalSourceMismatch");
 const auto* application=std::get_if<LifeOriginalTerminalApplication>(&terminal->cause);if(!application||application->disposition!=LifeOriginalTerminalApplication::Disposition::completed)return fail("CardTrainingTerminalSourceMismatch");
 auto same=compare_world_moments(terminal->occurred_at,publication->occurred_at);if(!same.ok)return {false,{},same.error};auto future=compare_world_moments(terminal->occurred_at,now.value);if(!future.ok)return {false,{},future.error};if(same.value||future.value>0||fact->completed_tick!=terminal->occurred_at.half_day_tick||s.common.actions.find(id)||s.growth.actions.find(id))return fail("CardTrainingTerminalSourceMismatch");
 const auto* cursor=terminal;std::size_t count=0;while(cursor){if(++count>s.growth.fixed_service_boundaries.size()||cursor->source_schema!=1||cursor->action_ref!=id||cursor->action_kind!=fact->action_kind||cursor->owner_ref.kind!=0||cursor->owner_ref.action_id!=id||cursor->owner_ref.fact_kind!=GrowthFixedFactKind::training)return fail("CardTrainingBoundaryChainMismatch");if(cursor->boundary_ordinal==0){if(cursor->kind!=LifeFixedBoundaryKind::started||cursor->previous_boundary_ref||cursor->occurred_at.half_day_tick!=fact->start_tick)return fail("CardTrainingBoundaryChainMismatch");break;}if(!cursor->previous_boundary_ref)return fail("CardTrainingBoundaryChainMismatch");const auto* previous=s.growth.fixed_service_boundaries.find(*cursor->previous_boundary_ref);if(!previous||previous->boundary_id!=*cursor->previous_boundary_ref||previous->boundary_ordinal!=cursor->boundary_ordinal-1)return fail("CardTrainingBoundaryChainMismatch");auto chronology=compare_world_moments(previous->occurred_at,cursor->occurred_at);if(!chronology.ok)return {false,{},chronology.error};if(chronology.value>0)return fail("CardTrainingBoundaryChainMismatch");cursor=previous;}
 at.kind=ActorFoundationAt::Kind::exact_moment;at.exact_moment=terminal->occurred_at;return {true,std::move(at),{}};
}
}
