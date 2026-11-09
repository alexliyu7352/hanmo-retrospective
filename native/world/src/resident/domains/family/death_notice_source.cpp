// 亲历、信件发出与收信知情分别存证；本叶不写事实、不消耗物品或替NPC作决定。
#include "death_notice_source.hpp"
#include <algorithm>
#include <tuple>
#include <limits>
namespace hanmo::resident { namespace {
// 同tick也必须原因先于记录，不能按可排序的事实ID推测事件先后。
bool before(const CommonStore& c,const StableId& a,const StableId& b){const auto& ids=c.state().facts.order();auto x=std::find(ids.begin(),ids.end(),a),y=std::find(ids.begin(),ids.end(),b);return x!=ids.end()&&y!=ids.end()&&x<y;}
// 来源真值由具名死亡和亲历读共同核对；没有观察者证明就没有发信知情权。
Status truth(const CommonStore& c,const FamilyDeathState& life,const FamilyDeathOriginRead& death_read,const FamilyDeathWitnessRead& witness_read,const WitnessedDeathNoticeSource& s,Integer ceiling){
    if(s.version!="witnessed_death_v1"||s.sender_actor_id.empty()||s.recipient_actor_id.empty()||s.sender_actor_id==s.recipient_actor_id||s.sender_actor_id==s.decedent_actor_id||s.recipient_actor_id==s.decedent_actor_id)return common_failure("InvalidDeathNoticeParties");
    if(s.world_id!=c.state().identity.world_id||s.scenario_id!=c.state().metadata.scenario_id)return common_failure("DeathNoticeWorldMismatch");
    if(s.observed_tick<0||s.issued_tick<s.observed_tick||s.issued_tick>ceiling)return common_failure("InvalidDeathNoticeTime");
    if(!c.actor(s.sender_actor_id)||!c.actor(s.recipient_actor_id))return common_failure("UnknownDeathNoticeActor");
    const auto* death=life.deaths.find(s.death_fact_id);const auto* header=c.state().facts.find(s.death_fact_id);
    if(!death||!header||header->domain!="family_life"||header->kind!="family_death")return common_failure("MissingDeathNoticeDeathFact",s.death_fact_id);
    auto valid=validate_family_death_record(c,*death,death_read);if(!valid.ok)return valid;
    if(death->actor_id!=s.decedent_actor_id||death->cause_fact_id!=s.casualty_fact_id||death->battle_id!=s.battle_id||death->death_tick!=s.observed_tick||death->recorded_tick>s.issued_tick)return common_failure("DeathNoticeDeathMismatch");
    if(!before(c,s.casualty_fact_id,s.death_fact_id))return common_failure("DeathNoticeCauseOrder");
    if(!witness_read)return common_failure("DeathNoticeWitnessReadRequired");auto actual=witness_read(s.sender_actor_id,s.decedent_actor_id,s.casualty_fact_id);if(!actual.ok)return {false,{},actual.error};if(!actual.value)return common_failure("DeathNoticeNotWitnessed");const auto& w=*actual.value;
    if(w.observer_actor_id!=s.sender_actor_id||w.dead_actor_id!=s.decedent_actor_id||w.casualty_fact_id!=s.casualty_fact_id||w.battle_id!=s.battle_id||w.unit_id!=s.unit_id||w.unit_id.empty()||w.observed_tick!=s.observed_tick)return common_failure("DeathNoticeWitnessMismatch");
    for(const auto& id:{s.sender_actor_id,s.recipient_actor_id}){const auto* actor=c.state().actors.find(id);if((actor->birth_tick&&*actor->birth_tick>s.issued_tick)||(actor->created_tick&&*actor->created_tick>s.issued_tick))return common_failure("DeathNoticeActorNotYetPresent",id);}
    return common_success();
}
// 当前仍生存是新发信的条件；历史复验不把当前alive倒填过去。
Status living_now(const CommonStore& c,const StableId& sender,const StableId& recipient){for(const auto& id:{sender,recipient}){const auto* a=c.actor(id);if(!a||!a->alive)return common_failure("DeathNoticeLivingPartiesRequired",id);}return common_success();}
}
// 相等只覆盖冻结语义，JSON字段展示顺序不构成另一份知情证明。
bool witnessed_death_notice_equal(const WitnessedDeathNoticeSource& a,const WitnessedDeathNoticeSource& b){return std::tie(a.version,a.scenario_id,a.death_fact_id,a.decedent_actor_id,a.sender_actor_id,a.recipient_actor_id,a.casualty_fact_id,a.battle_id,a.unit_id,a.world_id,a.observed_tick,a.issued_tick)==std::tie(b.version,b.scenario_id,b.death_fact_id,b.decedent_actor_id,b.sender_actor_id,b.recipient_actor_id,b.casualty_fact_id,b.battle_id,b.unit_id,b.world_id,b.observed_tick,b.issued_tick);}
// 任务受领只消费原已登记终点；半日结束时刻须由真实阶段输入逐项证明。
Outcome<WitnessedDeathNoticeSource> read_witnessed_death_notice(const CommonStore& c,const FamilyDeathState& life,const FamilyDeathOriginRead& death_read,const FamilyDeathWitnessRead& witness_read,const StableId& sender,const StableId& recipient,const StableId& dead,Integer accept_tick,const DynamicStepInput* phase){
    Integer ceiling=c.tick();if(phase){if(phase->from_tick!=c.tick()||phase->from_tick==std::numeric_limits<Integer>::max()||phase->to_tick!=phase->from_tick+1)return {false,{},common_failure("InvalidDeathNoticePhase").error};ceiling=phase->to_tick;}if(accept_tick!=ceiling)return {false,{},common_failure("InvalidDeathNoticeAcceptanceTick").error};
    StableId fact;for(const auto& row:life.deaths.rows())if(row.second.actor_id==dead){if(!fact.empty())return {false,{},common_failure("DuplicateFamilyDeathActor",dead).error};fact=row.first;}if(fact.empty())return {false,{},common_failure("MissingDeathNoticeDeathFact",dead).error};
    auto quote=preview_witnessed_death_notice(c,life,death_read,witness_read,fact,sender,recipient);if(!quote.ok)return {false,{},quote.error};quote.value.source.issued_tick=accept_tick;auto valid=truth(c,life,death_read,witness_read,quote.value.source,ceiling);if(!valid.ok)return {false,{},valid.error};return {true,std::move(quote.value.source),{}};
}
// 来源是给原有委托受领的只读候选，不分配notice ID、Task ID或实物信件。
Outcome<WitnessedDeathNoticeQuote> preview_witnessed_death_notice(const CommonStore& c,const FamilyDeathState& life,const FamilyDeathOriginRead& death_read,const FamilyDeathWitnessRead& witness_read,const StableId& fact,const StableId& sender,const StableId& recipient){
    auto living=living_now(c,sender,recipient);if(!living.ok)return {false,{},living.error};const auto* death=life.deaths.find(fact);if(!death)return {false,{},common_failure("MissingDeathNoticeDeathFact",fact).error};if(!witness_read)return {false,{},common_failure("DeathNoticeWitnessReadRequired").error};auto witnessed=witness_read(sender,death->actor_id,death->cause_fact_id);if(!witnessed.ok)return {false,{},witnessed.error};if(!witnessed.value)return {false,{},common_failure("DeathNoticeNotWitnessed").error};
    WitnessedDeathNoticeQuote q;q.identity=c.state().identity;auto& s=q.source;s.scenario_id=c.state().metadata.scenario_id;s.world_id=q.identity.world_id;s.death_fact_id=fact;s.decedent_actor_id=death->actor_id;s.sender_actor_id=sender;s.recipient_actor_id=recipient;s.casualty_fact_id=death->cause_fact_id;s.battle_id=death->battle_id;s.unit_id=witnessed.value->unit_id;s.observed_tick=death->death_tick;s.issued_tick=c.tick();auto valid=truth(c,life,death_read,witness_read,s,c.tick());if(!valid.ok)return {false,{},valid.error};return {true,std::move(q),{}};
}
// 正式受领写入前须仍是同一个世界、执行代次和修订，过期报价不可变成新信件。
Status revalidate_witnessed_death_notice(const CommonStore& c,const FamilyDeathState& life,const FamilyDeathOriginRead& death_read,const FamilyDeathWitnessRead& witness_read,const WitnessedDeathNoticeQuote& q){if(std::tie(q.identity.world_id,q.identity.execution_epoch,q.identity.revision,q.identity.tick)!=std::tie(c.state().identity.world_id,c.state().identity.execution_epoch,c.state().identity.revision,c.state().identity.tick))return common_failure("StaleDeathNoticeSource");auto fresh=preview_witnessed_death_notice(c,life,death_read,witness_read,q.source.death_fact_id,q.source.sender_actor_id,q.source.recipient_actor_id);if(!fresh.ok)return {false,{},fresh.error};return witnessed_death_notice_equal(q.source,fresh.value.source)?common_success():common_failure("DeathNoticeSourceChanged");}
// 只有原信件绑定事实之后才可冷验；接收者是否真正收到由S3另核交付收据。
Status validate_witnessed_death_notice(const CommonStore& c,const FamilyDeathState& life,const FamilyDeathOriginRead& death_read,const FamilyDeathWitnessRead& witness_read,const WitnessedDeathNoticeSource& s,const StableId& boundary,const DynamicStepInput* phase){Integer ceiling=c.tick();if(phase){if(phase->from_tick!=c.tick()||phase->from_tick==std::numeric_limits<Integer>::max()||phase->to_tick!=phase->from_tick+1)return common_failure("InvalidDeathNoticePhase");ceiling=phase->to_tick;}auto valid=truth(c,life,death_read,witness_read,s,ceiling);if(!valid.ok)return valid;if(boundary.empty()||!c.state().facts.find(boundary)||!before(c,s.death_fact_id,boundary))return common_failure("DeathNoticeBindingOrder",boundary);
    for(const auto& id:{s.sender_actor_id,s.recipient_actor_id}){const auto* a=c.actor(id);const auto* death=family_death_for_actor(life,id);if(death){if(death->death_tick<s.issued_tick||(death->death_tick==s.issued_tick&&!before(c,boundary,death->cause_fact_id)))return common_failure("DeathNoticePartyAlreadyDead",id);}else if(!a->alive)return common_failure("DeathNoticePartyHistoryUnavailable",id);}
    return common_success();
}
}
