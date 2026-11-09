// 统一观察页按固定扫描预算消费现有知情记录；查询不会登记报告、分配ID、推进世界或使用RNG。
#include "world_observation.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include <limits>
#include <map>

namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 运行身份按现有十六进制边界表达；epoch与revision均参与续页检查。
V identity_value(const WorldIdentity& identity){
    StableId hex;const char* digits="0123456789abcdef";for(const auto byte:identity.world_id){hex.push_back(digits[byte>>4]);hex.push_back(digits[byte&15]);}
    return boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{identity.execution_epoch}},{"revision",V{identity.revision}},{"tick",V{identity.tick}}});
}
// 游标同时固定页语义，不能把一地或一人的扫描位置复用于另一观察页。
V cursor_value(const WorldObservationCursor& cursor){return boundary::obj({{"identity",identity_value(cursor.identity)},{"observer_actor_id",V{cursor.observer_actor_id}},{"place_id",V{cursor.place_id}},{"offset",V{cursor.offset}}});}
// 当前版本只输出来源明确的实物、欠款及本人既有任务，不把所有事实都转成通用数据行。
bool current_task(const TaskRecord& task){return task.status=="active"||task.status=="accepted"||task.status=="in_progress"||task.status=="ready"||task.status=="settling";}
}

// 索引仅在候选创建/提交后重建，保存ID查找关系而不复制人物、位置、金额或知情真值。
WorldObservationIndex build_world_observation_index(const CommonStore& common,const SocialState& social,const TaskState& tasks){
    WorldObservationIndex index;index.revision=common.revision();index.tick=common.tick();
    for(const auto& id:common.actor_order()){const auto* actor=common.actor(id);if(!actor||!actor->alive)continue;auto at=common.resolved_location(id);if(at.ok)if(const auto* place=std::get_if<SettlementLocation>(&at.value))index.actors_by_place[place->place_id].push_back(id);}
    if(social.dynamic){
        std::map<std::pair<StableId,StableId>,const Report*> latest;
        for(const auto& row:social.dynamic->reports.rows()){
            const auto& report=row.second;if(report.visibility_scope!=ReportVisibility::local_public||!report.observation.supply||report.observation_tick>common.tick())continue;
            const auto& source=*report.observation.supply;const auto key=std::make_pair(source.site_id,source.source_key);const auto found=latest.find(key);
            if(found==latest.end()||found->second->observation_tick<report.observation_tick||(found->second->observation_tick==report.observation_tick&&found->second->observation.supply->source_revision<=source.source_revision))latest[key]=&report;
        }
        for(const auto& row:latest)index.public_reports_by_site[row.first.first].push_back(row.second->id);
    }
    for(const auto& row:tasks.tasks.rows())if(current_task(row.second)&&row.second.accepted_tick<=common.tick())index.tasks_by_actor[row.second.assignee].push_back(row.first);
    return index;
}

Outcome<WorldObservationPage> query_world_observation(const CommonStore& common,const SocialState& social,const TaskState& tasks,const InteractionState&,const WorldObservationIndex& index,const StableId& observer,const StableId& place,const std::optional<WorldObservationCursor>& cursor){
    const auto fail=[](const StableId& code)->Outcome<WorldObservationPage>{return {false,{},common_failure(code).error};};
    if(observer.empty()||observer!=common.state().metadata.player_actor_id)return fail("WorldObservationObserverRequired");
    const auto* actor=common.actor(observer);auto location=common.resolved_location(observer);const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!actor||!actor->alive||!here||here->place_id!=place)return fail("WorldPlaceNotObservable");
    if(index.revision!=common.revision()||index.tick!=common.tick())return fail("StaleWorldObservationIndex");
    const auto identity=common.state().identity;Integer offset=0;
    if(cursor){const auto& source=cursor->identity;if(source.world_id!=identity.world_id||source.execution_epoch!=identity.execution_epoch||source.revision!=identity.revision||source.tick!=identity.tick||cursor->observer_actor_id!=observer||cursor->place_id!=place)return fail("StaleWorldObservationCursor");offset=cursor->offset;}
    const std::vector<StableId> empty;
    const auto ids=[&](const auto& map,const StableId& key)->const std::vector<StableId>&{auto found=map.find(key);return found==map.end()?empty:found->second;};
    const auto& people=ids(index.actors_by_place,place);const auto& local_reports=ids(index.public_reports_by_site,place);const auto& town_reports=place==here->settlement_id?empty:ids(index.public_reports_by_site,here->settlement_id);const auto& commitments=ids(index.tasks_by_actor,observer);
    const auto* knowledge=social.dynamic?social.dynamic->known_reports.find("0:"+observer):nullptr;const auto& known_ids=knowledge?knowledge->order():empty;
    const std::vector<const std::vector<StableId>*> sections={&people,&known_ids,&local_reports,&town_reports,&commitments};Integer total=0;
    for(const auto* section:sections){if(section->size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max()-total))return fail("WorldObservationLimit");total+=static_cast<Integer>(section->size());}
    if(offset<0||offset>total)return fail("InvalidWorldObservationCursor");
    WorldObservationPage page;page.identity=identity;page.observer_actor_id=observer;page.place_id=place;page.cursor=offset;
    while(offset<total&&page.scanned<world_observation_scan_limit&&page.rows.size()<static_cast<std::size_t>(world_observation_page_limit)){
        Integer at=offset++;++page.scanned;std::size_t section=0;while(section<sections.size()&&at>=static_cast<Integer>(sections[section]->size())){at-=static_cast<Integer>(sections[section]->size());++section;}
        const auto& id=(*sections[section])[static_cast<std::size_t>(at)];
        if(section==0){const auto* person=common.actor(id);if(!person)return fail("InvalidWorldObservationIndex");page.rows.emplace_back(ObservedActorSummary{id,place,common.tick(),person->available,!person->action_id.empty()});continue;}
        if(section==4){const auto* task=tasks.tasks.find(id);if(!task||task->assignee!=observer)return fail("InvalidWorldObservationIndex");page.rows.emplace_back(ObservedCommitmentSummary{{task->id,"task",task->status,common.state().facts.find(task->id)?task->id:StableId{},task->deadline_tick}});continue;}
        const auto* report=social.dynamic?social.dynamic->reports.find(id):nullptr;if(!report)return fail("InvalidWorldObservationIndex");
        const auto* learned=knowledge?knowledge->find(id):nullptr;if(section>1&&learned)continue;
        if(report->observation_tick>common.tick()||(learned&&learned->learned_tick>common.tick()))continue;
        if(section>1&&(report->visibility_scope!=ReportVisibility::local_public||!report->observation.supply))return fail("InvalidWorldObservationIndex");
        const auto learned_tick=learned?learned->learned_tick:common.tick();
        if(report->observation.supply){const auto& source=*report->observation.supply;KnownSupply supply{report->id,report->fact_id,source.organization_id,source.site_id,source.inventory_id,source.item_id,source.source_key,source.physical_amount,source.target_amount,source.source_revision,report->observation_tick,learned_tick,report->valid_until_tick};page.rows.emplace_back(ObservedSupplySummary{std::move(supply),report->valid_until_tick&&common.tick()>*report->valid_until_tick});}
        else if(report->observation.obligation&&report->observation.obligation->beneficiary_id==observer){const auto& source=*report->observation.obligation;page.rows.emplace_back(ObservedArrearSummary{{report->id,report->fact_id,source.arrear_id,source.principal_id,observer,source.outstanding_amount,source.due_tick,report->observation_tick,learned_tick}});}
    }
    if(offset<total)page.next_cursor=WorldObservationCursor{identity,observer,place,offset};
    return {true,std::move(page),{}};
}

BoundaryValue encode_world_observation(const WorldObservationPage& page){
    V::Array rows;
    for(const auto& row:page.rows)rows.push_back(std::visit([](const auto& entry)->V{
        using T=std::decay_t<decltype(entry)>;
        if constexpr(std::is_same_v<T,ObservedActorSummary>)return boundary::obj({{"kind",V{"actor"}},{"actor_id",V{entry.actor_id}},{"place_id",V{entry.place_id}},{"observed_tick",V{entry.observed_tick}},{"available",V{entry.available}},{"action_occupied",V{entry.occupied}}});
        else if constexpr(std::is_same_v<T,ObservedSupplySummary>){const auto& source=entry.source;V::Dict fields={{"kind",V{"supply"}},{"report_id",V{source.report_id}},{"fact_id",V{source.fact_id}},{"organization_id",V{source.organization_id}},{"site_id",V{source.site_id}},{"item_id",V{source.item_id}},{"source_key",V{source.source_key}},{"quantity",V{source.quantity}},{"target_amount",V{source.target_amount}},{"source_revision",V{source.source_revision}},{"observed_tick",V{source.observation_tick}},{"learned_tick",V{source.learned_tick}},{"stale",V{entry.stale}}};if(source.valid_until_tick)fields.emplace_back("valid_until_tick",V{*source.valid_until_tick});return V{std::move(fields)};}
        else if constexpr(std::is_same_v<T,ObservedArrearSummary>){const auto& source=entry.source;return boundary::obj({{"kind",V{"arrear"}},{"report_id",V{source.report_id}},{"fact_id",V{source.fact_id}},{"arrear_id",V{source.arrear_id}},{"organization_id",V{source.organization_id}},{"outstanding",V{source.outstanding}},{"due_tick",V{source.due_tick}},{"observed_tick",V{source.observation_tick}},{"learned_tick",V{source.learned_tick}}});}
        else {const auto& source=entry.source;return boundary::obj({{"kind",V{"commitment"}},{"id",V{source.id}},{"status",V{source.status}},{"source_fact_id",V{source.source_fact_id}},{"due_tick",V{source.due_tick}}});}
    },row));
    V::Dict result={{"version",V{Integer(1)}},{"identity",identity_value(page.identity)},{"observer_actor_id",V{page.observer_actor_id}},{"place_id",V{page.place_id}},{"cursor",V{page.cursor}},{"rows",V{std::move(rows)}}};
    if(page.next_cursor)result.emplace_back("next_cursor",cursor_value(*page.next_cursor));return V{std::move(result)};
}

Outcome<WorldObservationCursor> decode_world_observation_cursor(const BoundaryValue& input){try{
    using namespace boundary;fields(input,{"identity","observer_actor_id","place_id","offset"},{},"cursor");const auto& identity=field(input,"identity","cursor");fields(identity,{"world_id","execution_epoch","revision","tick"},{},"cursor/identity");
    WorldObservationCursor result;const auto hex=get_string(identity,"world_id","identity");if(hex.size()!=result.identity.world_id.size()*2)fail("InvalidWorldObservationIdentity","cursor");
    const auto nibble=[](char ch)->Integer{if(ch>='0'&&ch<='9')return ch-'0';if(ch>='a'&&ch<='f')return ch-'a'+10;return -1;};
    for(std::size_t n=0;n<result.identity.world_id.size();++n){const auto high=nibble(hex[n*2]),low=nibble(hex[n*2+1]);if(high<0||low<0)fail("InvalidWorldObservationIdentity","cursor");result.identity.world_id[n]=static_cast<std::uint8_t>(high*16+low);}
    result.identity.execution_epoch=get_int(identity,"execution_epoch","identity");result.identity.revision=get_int(identity,"revision","identity");result.identity.tick=get_int(identity,"tick","identity");result.observer_actor_id=get_string(input,"observer_actor_id","cursor");result.place_id=get_string(input,"place_id","cursor");result.offset=get_int(input,"offset","cursor");
    if(result.identity.execution_epoch<0||result.identity.revision<0||result.identity.tick<0||result.observer_actor_id.empty()||result.place_id.empty()||result.offset<0)fail("InvalidWorldObservationCursor","cursor");return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
} // 未获知的历史与远地人物不进入观察者分页序列；旧游标也不能续写当前页面。
