#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/checkpoint_economic_index_copy.hpp"
// 分页只读取当前候选内的正式经济记录；权限不依赖目录中的显示名称或界面选择。
#include "local_economy_view.hpp"
#include "production_history.hpp"
#include "../takeover/execution_configurations.hpp"
#include "../local_economy_codec_detail.hpp"
#include "../local_economy_internal.hpp"
#include <limits>
#include "resident/domains/organization/organization_model.hpp"
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 世界身份和公职身份共同绑定续页，撤权或换人不能沿旧页获得新资料。
bool same_authority(const std::optional<ActingAuthority>& a,const std::optional<ActingAuthority>& b){return a.has_value()==b.has_value()&&(!a||(a->organization_id==b->organization_id&&a->office_id==b->office_id&&a->authority_epoch==b->authority_epoch&&a->actor_id==b->actor_id));}
V identity_value(const WorldIdentity& id){StableId hex;const char* digits="0123456789abcdef";for(auto byte:id.world_id){hex.push_back(digits[byte>>4]);hex.push_back(digits[byte&15]);}return boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{id.execution_epoch}},{"revision",V{id.revision}},{"tick",V{id.tick}}});}
V authority_value(const std::optional<ActingAuthority>& authority){if(!authority)return {};const auto& a=*authority;return boundary::obj({{"organization_id",V{a.organization_id}},{"office_id",V{a.office_id}},{"authority_epoch",V{a.authority_epoch}},{"actor_id",V{a.actor_id}}});}
V cursor_value(const LocalEconomyCursor& c){return boundary::obj({{"identity",identity_value(c.identity)},{"observer_actor_id",V{c.observer_actor_id}},{"place_id",V{c.place_id}},{"authority",authority_value(c.authority)},{"offset",V{c.offset}}});}
}
// 候选边界只为当前本人生成真实Office的合法来源；全量扫描不发生在query中。
LocalEconomyIndex build_local_economy_index(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const OrganizationState& organization_state,const OrganizationRead& organizations,const EconomicTakeoverReadContext* takeover){
    LocalEconomyIndex index;index.identity=common.state().identity;index.observer_actor_id=index_memory::copy(index.text_memory,common.state().metadata.player_actor_id);
    const auto location=common.resolved_location(index.observer_actor_id);const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;const auto* actor=common.actor(index.observer_actor_id);
    if(!catalog.enabled||!actor||!actor->alive||!here)return index;
    if(takeover&&(&takeover->common!=&common||&takeover->economy!=&state||&takeover->catalog!=&catalog||takeover->phase))return index;index.place_id=index_memory::copy(index.text_memory,here->place_id);
    const auto local=[&](const StableId& site){return site==here->place_id||site==here->settlement_id;};
    for(const auto& row:organization_state.offices.rows()){
        const auto& office=row.second;if((!office.holder_actor_id||*office.holder_actor_id!=actor->id)&&(!office.deputy_actor_id||*office.deputy_actor_id!=actor->id))continue;
        CheckpointMemoryCharge authority_memory;auto authority=index_memory::authority(authority_memory,office.organization_id,office.id,office.epoch,actor->id);
        if(organizations.authorize(authority,"inspect_local_economy",{ScopeRef::Kind::site,here->place_id},0).ok){index.authorities.push_back(index_memory::copy(index.text_memory,authority));index_memory::authority_entry(index.text_memory,index.authority_sources,authority);}
    }
    const auto append=[&](LocalEconomySource::Kind kind,const StableId& id,bool personal,const auto& visible){
        if(personal)index.personal_sources.push_back(index_memory::source(index.text_memory,kind,id));
        for(const auto& authority:index.authorities)if(personal||visible(authority))index_memory::authority_entry(index.text_memory,index.authority_sources,authority).push_back(index_memory::source(index.text_memory,kind,id));
    };
    for(const auto& row:state.production_units.rows()){const auto& unit=row.second;if(!local(unit.site_id))continue;CheckpointMemoryCharge operating_memory;Owner operating=index_memory::copy(operating_memory,unit.owner);
        if(takeover){auto current=read_economic_production_execution(*takeover,unit.id);if(!current.ok)continue;if(current.value){if(current.value->status!=EconomicTakeoverExecutionStatus::approved||!current.value->executing_owner)continue;index_memory::replace(operating_memory,operating,*current.value->executing_owner);index_memory::insert(index.text_memory,index.production_execution_owners,unit.id,operating);const auto* prior=state.production_management?state.production_management->pending_by_unit.find(unit.id):nullptr;auto command=economic_configuration_command_owner(*takeover,unit.id,prior?*prior:StableId{});if(!command.ok)index_memory::insert(index.text_memory,index.production_configuration_blocks,unit.id,command.error.original.code);}}
        append(LocalEconomySource::Kind::production,row.first,operating.kind==Owner::Kind::actor&&operating.id==actor->id,[&](const ActingAuthority& a){return operating.kind==Owner::Kind::organization&&operating.id==a.organization_id&&organizations.authorize(a,"inspect_local_economy",{ScopeRef::Kind::site,unit.site_id},0).ok;});}
    for(const auto& row:state.cohorts.rows()){const auto& cohort=row.second;if(!local(cohort.site_id)||!cohort.affiliation_id)continue;append(LocalEconomySource::Kind::population,row.first,false,[&](const ActingAuthority& a){return *cohort.affiliation_id==a.organization_id&&organizations.authorize(a,"inspect_local_economy",{ScopeRef::Kind::site,cohort.site_id},0).ok;});}
    for(const auto& row:state.arrears.rows()){const auto* obligation=state.obligations.find(row.second.obligation_id);if(!obligation)continue;append(LocalEconomySource::Kind::arrear,row.first,obligation->beneficiary.kind==SubjectKind::actor&&obligation->beneficiary.id==actor->id,[&](const ActingAuthority& a){return obligation->principal.kind==SubjectKind::organization&&obligation->principal.id==a.organization_id&&organizations.authorize(a,"inspect_local_economy",{ScopeRef::Kind::account,obligation->source_wallet_or_inventory},0).ok;});}
    for(const auto& row:state.rations.rows()){const auto& ration=row.second;const auto* obligation=state.obligations.find(ration.obligation_id);if(!obligation)continue;append(LocalEconomySource::Kind::ration,row.first,ration.beneficiary.kind==SubjectKind::actor&&ration.beneficiary.id==actor->id,[&](const ActingAuthority& a){const auto* cohort=ration.beneficiary.kind==SubjectKind::cohort?state.cohorts.find(ration.beneficiary.id):nullptr;return obligation->principal.kind==SubjectKind::organization&&obligation->principal.id==a.organization_id&&cohort&&local(cohort->site_id)&&organizations.authorize(a,"inspect_local_economy",{ScopeRef::Kind::site,cohort->site_id},0).ok;});}
    index_production_history(common,state,catalog,index,takeover);
    return index;
}
LocalEconomyIndex build_local_economy_index(const CommonStore& c,const LocalEconomyState& s,const LocalEconomyCatalog& catalog,const OrganizationState& state,const OrganizationRead& organizations){return build_local_economy_index(c,s,catalog,state,organizations,nullptr);}
// 分页只走已合法派生的ID序列；隐藏记录不影响扫描计数、游标或空页数量。
Outcome<LocalEconomyPage> query_local_economy(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const OrganizationRead& organizations,const LocalEconomyIndex& index,const LocalEconomyQuery& query){
    const auto fail=[](const StableId& code)->Outcome<LocalEconomyPage>{return {false,{},common_failure(code).error};};
    if(!catalog.enabled)return fail("LocalEconomyUnavailable");
    if(query.observer_actor_id.empty()||query.observer_actor_id!=common.state().metadata.player_actor_id)return fail("LocalEconomyObserverRequired");
    const auto* actor=common.actor(query.observer_actor_id);const auto location=common.resolved_location(query.observer_actor_id);const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!actor||!actor->alive||!here||here->place_id!=query.place_id)return fail("LocalEconomyPlaceNotObservable");
    if(state.last_step_tick!=common.tick())return fail("LocalEconomyClockMismatch");
    const auto identity=common.state().identity;const auto same_identity=[&](const WorldIdentity& source){return source.world_id==identity.world_id&&source.execution_epoch==identity.execution_epoch&&source.revision==identity.revision&&source.tick==identity.tick;};
    if(!same_identity(index.identity)||index.observer_actor_id!=query.observer_actor_id||index.place_id!=query.place_id)return fail("StaleLocalEconomyIndex");
    const CheckpointVector<LocalEconomySource>* sources=&index.personal_sources;
    if(query.authority){const auto& a=*query.authority;if(a.actor_id!=query.observer_actor_id)return fail("LocalEconomyAuthorityActorMismatch");auto proof=organizations.authorize(a,"inspect_local_economy",{ScopeRef::Kind::site,query.place_id},0);if(!proof.ok)return {false,{},proof.error};auto found=index.authority_sources.find({a.organization_id,a.office_id,a.authority_epoch,a.actor_id});if(found==index.authority_sources.end())return fail("StaleLocalEconomyIndex");sources=&found->second;}
    Integer offset=0;if(query.cursor){const auto& cursor=*query.cursor;if(!same_identity(cursor.identity)||cursor.observer_actor_id!=query.observer_actor_id||cursor.place_id!=query.place_id||!same_authority(cursor.authority,query.authority))return fail("StaleLocalEconomyCursor");offset=cursor.offset;}
    if(sources->size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max()))return fail("LocalEconomyViewLimit");const auto total=static_cast<Integer>(sources->size());if(offset<0||offset>total)return fail("InvalidLocalEconomyCursor");
    LocalEconomyPage page;page.identity=identity;page.observer_actor_id=query.observer_actor_id;page.place_id=query.place_id;page.cursor=offset;page.authorities.assign(index.authorities.begin(),index.authorities.end());
    while(offset<total&&page.scanned<local_economy_scan_limit&&page.rows.size()<static_cast<std::size_t>(local_economy_page_limit)){
        const auto& source=(*sources)[static_cast<std::size_t>(offset++)];++page.scanned;
        if(source.kind==LocalEconomySource::Kind::production){const auto* unit=state.production_units.find(source.id);if(!unit)return fail("InvalidLocalEconomyIndex");
            LocalProductionView value;value.history_available=index.production_receipts_by_unit.count(unit->id)!=0;value.unit_id=unit->id;value.site_id=unit->site_id;value.recipe_id=unit->recipe_id;value.output_inventory_id=unit->output_inventory_id;value.last_receipt_id=unit->last_settlement_id;value.owner=unit->owner;const auto taken=index.production_execution_owners.find(unit->id);const Owner& operating=taken==index.production_execution_owners.end()?unit->owner:taken->second;if(taken!=index.production_execution_owners.end()){value.execution_owner=operating;const auto receipts=index.production_receipts_by_unit.find(unit->id);value.last_receipt_id=receipts!=index.production_receipts_by_unit.end()&&!receipts->second.empty()?receipts->second.front():StableId{};}const auto blocked=index.production_configuration_blocks.find(unit->id);if(blocked!=index.production_configuration_blocks.end())value.configuration_block_code=blocked->second;value.capacity=unit->capacity;value.next_due_tick=unit->next_due_tick;
            if(catalog.production_management&&catalog.production_management->plans.find(unit->id)){
                auto context=production_context(catalog,*unit,unit->next_due_tick);if(!context.ok)return {false,{},context.error};value.context=context.value;value.capacity=context.value.capacity;
                value.may_configure=value.configuration_block_code.empty()&&actor->available&&operating.kind==Owner::Kind::actor&&operating.id==actor->id;
                if(value.configuration_block_code.empty()&&query.authority&&operating.kind==Owner::Kind::organization&&operating.id==query.authority->organization_id){value.configuration_authority=query.authority;value.may_configure=actor->available&&organizations.authorize(*query.authority,"configure_local_production",{ScopeRef::Kind::site,unit->site_id},0).ok;}
                if(value.configuration_block_code.empty()&&state.production_management)if(const auto* pending=state.production_management->pending_by_unit.find(unit->id)){const auto* record=state.production_management->configurations.find(*pending);if(!record)return fail("InvalidPendingProductionConfiguration");value.pending_configuration=*record;}
            }
            if(unit->labor_allocation_ids.size()>256)return fail("LocalEconomyViewLimit");for(const auto& id:unit->labor_allocation_ids){const auto* labor=state.labor_allocations.find(id);if(!labor||labor->activity_or_production_id!=unit->id)return fail("InvalidLocalEconomyViewSource");if(labor->from_tick<=common.tick()&&common.tick()<=labor->through_tick&&!local_economy_detail::add(value.allocated_workers,labor->workers,value.allocated_workers))return fail("EconomyQuantityOverflow");if(value.context){const auto* authored=catalog.labor_allocations.find(id);if(authored)value.labor.push_back({id,labor->cohort_id,labor->workers,authored->workers});else value.may_configure=false;}}
            if(!value.last_receipt_id.empty()){const auto* receipt=state.production_receipts.find(value.last_receipt_id);if(!receipt||receipt->unit_id!=unit->id)return fail("InvalidLocalEconomyViewSource");value.last_result=*receipt;}page.rows.emplace_back(std::move(value));
        }else if(source.kind==LocalEconomySource::Kind::population){const auto* cohort=state.cohorts.find(source.id);if(!cohort)return fail("InvalidLocalEconomyIndex");page.rows.emplace_back(LocalPopulationView{cohort->id,cohort->site_id,cohort->household_group_id,cohort->status,cohort->headcount,cohort->labor_capacity});
        }else if(source.kind==LocalEconomySource::Kind::arrear){const auto* arrear=state.arrears.find(source.id);const auto* obligation=arrear?state.obligations.find(arrear->obligation_id):nullptr;if(!obligation)return fail("InvalidLocalEconomyIndex");const bool self=obligation->beneficiary.kind==SubjectKind::actor&&obligation->beneficiary.id==query.observer_actor_id;if(!self&&(!query.authority||!organizations.authorize(*query.authority,"inspect_local_economy",{ScopeRef::Kind::account,obligation->source_wallet_or_inventory},0).ok))return fail("StaleLocalEconomyIndex");LocalArrearView row{arrear->id,obligation->id,obligation->principal.id,obligation->beneficiary.id,obligation->kind,arrear->due_tick,arrear->original_due,arrear->paid_amount,arrear->original_due-arrear->paid_amount,obligation->active};row.beneficiary_kind=obligation->beneficiary.kind;const auto reference=index.cohort_wage_need_by_arrear.find(arrear->id);if(reference!=index.cohort_wage_need_by_arrear.end())row.wage_need=reference->second;page.rows.emplace_back(std::move(row));
        }else{const auto* ration=state.rations.find(source.id);if(!ration)return fail("InvalidLocalEconomyIndex");page.rows.emplace_back(LocalRationView{*ration});}
    }
    if(offset<total)page.next_cursor=LocalEconomyCursor{identity,query.observer_actor_id,query.place_id,query.authority,offset};return {true,std::move(page),{}};
}
// 冷边界只编码已通过领域可见性检查的结果；产出与应收都保留实际收据和账期。
BoundaryValue encode_local_economy_page(const LocalEconomyPage& page){
    V::Array authorities;for(const auto& a:page.authorities)authorities.push_back(authority_value(a));V::Array rows;for(const auto& row:page.rows)rows.push_back(std::visit([](const auto& v)->V{
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,LocalProductionView>){V::Array labor;for(const auto& a:v.labor)labor.push_back(boundary::obj({{"allocation_id",V{a.allocation_id}},{"cohort_id",V{a.cohort_id}},{"workers",V{a.workers}},{"maximum_workers",V{a.maximum_workers}}}));V out=boundary::obj({{"kind",V{"production"}},{"history_available",V{v.history_available}},{"unit_id",V{v.unit_id}},{"site_id",V{v.site_id}},{"recipe_id",V{v.recipe_id}},{"owner",boundary::encode_owner(v.owner)},{"output_inventory_id",V{v.output_inventory_id}},{"last_receipt_id",V{v.last_receipt_id}},{"capacity",V{v.capacity}},{"next_due_tick",V{v.next_due_tick}},{"allocated_workers",V{v.allocated_workers}},{"last_result",v.last_result?dynamic_economy_codec::encode(*v.last_result,false,false,true):V{}},{"context",v.context?dynamic_economy_codec::encode(*v.context):V{}},{"pending_configuration",v.pending_configuration?dynamic_economy_codec::encode(*v.pending_configuration):V{}},{"configuration_authority",authority_value(v.configuration_authority)},{"labor",V{std::move(labor)}},{"may_configure",V{v.may_configure}}});if(v.execution_owner)out.as<V::Dict>()->emplace_back("execution_owner",boundary::encode_owner(*v.execution_owner));if(!v.configuration_block_code.empty())out.as<V::Dict>()->emplace_back("configuration_block_code",V{v.configuration_block_code});return out;}
        else if constexpr(std::is_same_v<T,LocalPopulationView>)return boundary::obj({{"kind",V{"population"}},{"cohort_id",V{v.cohort_id}},{"site_id",V{v.site_id}},{"household_group_id",V{v.household_group_id}},{"status",dynamic_economy_codec::encode(v.status)},{"headcount",V{v.headcount}},{"labor_capacity",V{v.labor_capacity}}});
        else if constexpr(std::is_same_v<T,LocalArrearView>){V out=boundary::obj({{"kind",V{"arrear"}},{"arrear_id",V{v.arrear_id}},{"obligation_id",V{v.obligation_id}},{"principal_id",V{v.principal_id}},{"beneficiary_id",V{v.beneficiary_id}},{"obligation_kind",dynamic_economy_codec::encode(v.kind)},{"due_tick",V{v.due_tick}},{"original_due",V{v.original_due}},{"paid_amount",V{v.paid_amount}},{"outstanding",V{v.outstanding}},{"obligation_active",V{v.obligation_active}}});if(v.wage_need){out.as<V::Dict>()->emplace_back("beneficiary_kind",dynamic_economy_codec::encode(v.beneficiary_kind));const auto& need=*v.wage_need;out.as<V::Dict>()->emplace_back("wage_need",boundary::obj({{"need_id",V{need.need_id}},{"source_fact_id",V{need.source_fact_id}},{"status",V{need.status}},{"revision",V{need.revision}}}));}return out;}
        else return boundary::obj({{"kind",V{"ration"}},{"result",dynamic_economy_codec::encode(v.result)}});
    },row));return boundary::obj({{"identity",identity_value(page.identity)},{"observer_actor_id",V{page.observer_actor_id}},{"place_id",V{page.place_id}},{"cursor",V{page.cursor}},{"scanned",V{page.scanned}},{"rows",V{std::move(rows)}},{"authorities",V{std::move(authorities)}},{"next_cursor",page.next_cursor?cursor_value(*page.next_cursor):V{}}});
}
// 续页字段严格解析；身份不能通过缺省字段伪造为当前世界。
Outcome<LocalEconomyCursor> decode_local_economy_cursor(const BoundaryValue& value){
    try{
        boundary::fields(value,{"identity","observer_actor_id","place_id","authority","offset"},{},"/cursor");LocalEconomyCursor out;const auto& id=boundary::field(value,"identity","/cursor");boundary::fields(id,{"world_id","execution_epoch","revision","tick"},{},"/cursor/identity");const auto hex=boundary::get_string(id,"world_id","/cursor/identity");if(hex.size()!=out.identity.world_id.size()*2)boundary::fail("InvalidWorldId","/cursor/identity/world_id");
        const auto nibble=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;};for(std::size_t i=0;i<out.identity.world_id.size();++i){const auto a=nibble(hex[2*i]),b=nibble(hex[2*i+1]);if(a<0||b<0)boundary::fail("InvalidWorldId","/cursor/identity/world_id");out.identity.world_id[i]=static_cast<std::uint8_t>((a<<4)|b);}
        out.identity.execution_epoch=boundary::get_int(id,"execution_epoch","/cursor/identity");out.identity.revision=boundary::get_int(id,"revision","/cursor/identity");out.identity.tick=boundary::get_int(id,"tick","/cursor/identity");out.observer_actor_id=boundary::get_string(value,"observer_actor_id","/cursor");out.place_id=boundary::get_string(value,"place_id","/cursor");out.offset=boundary::get_int(value,"offset","/cursor");
        const auto& authority=boundary::field(value,"authority","/cursor");if(!std::holds_alternative<std::monostate>(authority.data)){boundary::fields(authority,{"organization_id","office_id","actor_id","authority_epoch"},{},"/cursor/authority");ActingAuthority a;a.organization_id=boundary::get_string(authority,"organization_id","/cursor/authority");a.office_id=boundary::get_string(authority,"office_id","/cursor/authority");a.actor_id=boundary::get_string(authority,"actor_id","/cursor/authority");a.authority_epoch=boundary::get_int(authority,"authority_epoch","/cursor/authority");out.authority=std::move(a);}
        if(out.identity.execution_epoch<0||out.identity.revision<0||out.identity.tick<0||out.offset<0||out.observer_actor_id.empty()||out.place_id.empty()||(out.authority&&(out.authority->authority_epoch<0||out.authority->organization_id.empty()||out.authority->office_id.empty()||out.authority->actor_id.empty())))boundary::fail("InvalidLocalEconomyCursor","/cursor");
        return {true,std::move(out),{}};
    }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
// 只接受地点与具名续页/公职身份；缺省可选项为空，未知字段不落回默认本人。
Outcome<LocalEconomyQuery> decode_local_economy_query(const BoundaryValue& value,const StableId& observer){
    try{
        boundary::fields(value,{"place_id"},{"cursor","authority"},"/local_economy");
        LocalEconomyQuery out;out.observer_actor_id=observer;out.place_id=boundary::get_string(value,"place_id","/local_economy");
        if(observer.empty()||out.place_id.empty())boundary::fail("LocalEconomyObserverRequired","/local_economy");
        if(const auto* cursor=value.find("cursor"))if(!std::holds_alternative<std::monostate>(cursor->data)){auto decoded=decode_local_economy_cursor(*cursor);if(!decoded.ok)return {false,{},decoded.error};out.cursor=std::move(decoded.value);}
        if(const auto* authority=value.find("authority"))dynamic_economy_codec::decode(*authority,out.authority,"/local_economy/authority");
        if(out.authority&&(out.authority->actor_id!=observer||out.authority->organization_id.empty()||out.authority->office_id.empty()||out.authority->authority_epoch<0))boundary::fail("LocalEconomyAuthorityActorMismatch","/local_economy/authority");
        return {true,std::move(out),{}};
    }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
}
