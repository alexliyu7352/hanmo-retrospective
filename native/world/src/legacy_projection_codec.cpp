// 旧记录只读解码与已声明引用闭合；不移植全kind规则、世界推进或位置解析。
#include "legacy_projection_codec.hpp"
#include "typed_wire.hpp"
#include "inventory_core.hpp"
#include <limits>

namespace hanmo::detail {
namespace {
constexpr std::int64_t maximum=2147483647;
using Keys=std::set<std::string>;
using Partition=std::pair<Keys,Keys>;
using Partitions=std::map<std::pair<std::string,std::string>,Partition>;

// 必需字段先报缺失路径，任何类型读取都复用同一严格TypedValue检查。
const Value& field(const Dict& row,const std::string& key,const std::string& path) {
    const auto found=row.find(key);
    if(found==row.end()) fail("MissingProjectionField",at(path,key));
    return found->second;
}
// 字符串保持原字节，不把数字或StringName转换为实体ID。
std::string text(const Dict& row,const std::string& key,const std::string& path) {
    return *require_value<std::string>(field(row,key,path),at(path,key));
}
// 每个记录域内部唯一；不同域允许同一原ID。
void unique(const std::string& id,Keys& seen,const std::string& code,const std::string& path) {
    if(id.empty() || !seen.insert(id).second) fail(code,path);
}
// 强引用仅查指定域，不按字符串前缀猜对象，也不补造目标。
void reference(const Keys& known,const std::string& id,const std::string& path) {
    if(!known.count(id)) fail("DanglingProjectionReference",path);
}
// 已知键仅移到typed字段，缺失可选键不补；分区保留原键集用于coverage复验。
Dict extras(const Dict& row,std::initializer_list<const char*> required,std::initializer_list<const char*> optional,
            const std::string& kind,const std::string& id,const std::string& path,Partitions& partitions) {
    Dict result=row; Keys known,other;
    for(const auto* key:required) { (void)field(row,key,path); known.insert(key); result.erase(key); }
    for(const auto* key:optional) if(row.count(key)) { known.insert(key); result.erase(key); }
    for(const auto& entry:result) other.insert(entry.first);
    partitions.emplace(std::make_pair(kind,id),std::make_pair(std::move(known),std::move(other)));
    return result;
}
// 外层数组行类型在对应下标处检查，不合并或排序原有语义数组。
template<class Callback> void each(const Dict& payload,const char* key,Callback callback) {
    const auto path=at("/payload",key);
    const auto& rows=*require_value<Array>(payload.at(key),path);
    for(std::size_t i=0;i<rows.size();++i) {
        const auto p=at(path,std::to_string(i)); callback(*require_value<Dict>(rows[i],p),p);
    }
}
// coverage字段名可为空String；它们不是引用ID，不能套用非空ID规则。
Keys field_names(const Value& input,const std::string& path) {
    Keys result;
    for(const auto& v:*require_value<Array>(input,path))
        if(!result.insert(*require_value<std::string>(v,path)).second) fail("InvalidProjectionCoverage",path);
    return result;
}
// 清单必须逐记录解释原键分区；deferred只是范围声明，不是安装失败。
void coverage(const Value& input,const Partitions& expected) {
    const std::string p="/payload/coverage"; const auto& root=*require_value<Dict>(input,p);
    fields(root,{"ownership","records"},{},p);
    for(const auto& v:*require_value<Array>(root.at("ownership"),at(p,"ownership"))) {
        const auto& row=*require_value<Dict>(v,at(p,"ownership"));
        fields(row,{"owner","container","scope","key","status"},{},at(p,"ownership"));
        for(const auto* key:{"owner","container","scope","key"}) (void)text(row,key,at(p,"ownership"));
        const auto status=text(row,"status",at(p,"ownership"));
        if(status!="typed" && status!="preserved_legacy" && status!="reference_only" && status!="deferred") fail("InvalidProjectionCoverage",at(p,"ownership"));
    }
    std::set<std::pair<std::string,std::string>> seen;
    const auto& rows=*require_value<Array>(root.at("records"),at(p,"records"));
    for(std::size_t i=0;i<rows.size();++i) {
        const auto path=at(at(p,"records"),std::to_string(i)); const auto& row=*require_value<Dict>(rows[i],path);
        fields(row,{"kind","id","known","extras"},{},path);
        const auto key=std::make_pair(text(row,"kind",path),text(row,"id",path));
        const auto found=expected.find(key);
        if(found==expected.end() || !seen.insert(key).second) fail("InvalidProjectionCoverage",path);
        const auto known=field_names(row.at("known"),at(path,"known")); const auto extra=field_names(row.at("extras"),at(path,"extras"));
        if(known!=found->second.first || extra!=found->second.second) fail("InvalidProjectionCoverage",path);
    }
    if(seen.size()!=expected.size()) fail("InvalidProjectionCoverage",at(p,"records"));
}
// 位置只核本联合与引用；目录未带几何数据，不能假称重验路线/地形可走性。
void location(const Value& input,const std::map<std::string,Keys>& refs,const std::string& path) {
    const auto& row=*require_value<Dict>(input,path); const auto kind=text(row,"kind",path);
    if(kind=="Settlement") {
        reference(refs.at("places"),text(row,"place_id",path),at(path,"place_id"));
        reference(refs.at("settlements"),text(row,"settlement_id",path),at(path,"settlement_id"));
    } else if(kind=="OnRoute") {
        reference(refs.at("routes"),text(row,"edge_id",path),at(path,"edge_id"));
        reference(refs.at("settlements"),text(row,"origin",path),at(path,"origin"));
        whole(field(row,"progress",path),0,std::numeric_limits<std::int64_t>::max(),at(path,"progress"));
        if(row.count("place_id")) fail("InvalidProjectionLocation",path);
    } else if(kind=="Wilderness") {
        fields(row,{"kind","map_id","point","origin"},{},path);
        reference(refs.at("world_maps"),text(row,"map_id",path),at(path,"map_id"));
        reference(refs.at("settlements"),text(row,"origin",path),at(path,"origin"));
        const auto& point=*require_value<Array>(row.at("point"),at(path,"point"));
        if(point.size()!=2) fail("InvalidProjectionLocation",at(path,"point"));
        for(const auto& coordinate:point) whole(coordinate,0,maximum,at(path,"point"));
    } else if(kind=="Following" || kind=="InArmy") {
        const char* key=kind=="Following"?"party_id":"army_id";
        fields(row,{"kind",key},{},path);
        reference(refs.at(kind=="Following"?"parties":"armies"),text(row,key,path),at(path,key));
    } else fail("InvalidProjectionLocation",at(path,"kind"));
}
// 完整actor_creation关系只沿明确选中的事实读取，不扫描任意扩展字段推导新边。
void actor_creation(const ActorState& actor,const std::map<std::string,const LegacyFact*>& facts,const std::string& path) {
    const auto found=facts.find(actor.id);
    if(found==facts.end()) fail("DanglingProjectionReference",at(path,"actor_creation"));
    const auto& fact=*found->second;
    if(fact.kind!="actor_creation" || !actor.prototype_id || !actor.display_name || !actor.birth_tick || !actor.created_tick)
        fail("InvalidActorCreationReference",path);
    const auto& e=fact.extras;
    if(text(e,"actor_id",path)!=actor.id || text(e,"prototype_id",path)!=*actor.prototype_id || text(e,"display_name",path)!=*actor.display_name ||
       *require_value<std::int64_t>(field(e,"birth_tick",path),at(path,"birth_tick"))!=*actor.birth_tick ||
       *require_value<std::int64_t>(field(e,"created_tick",path),at(path,"created_tick"))!=*actor.created_tick)
        fail("InvalidActorCreationReference",path);
    const auto source=text(e,"source",path); const auto creation=facts.find(source);
    if(creation==facts.end()) fail("DanglingProjectionReference",at(path,"source"));
    if(creation->second->kind!="family_birth" && creation->second->kind!="family_adoption") fail("InvalidActorCreationReference",at(path,"source"));
}
} // 本profile辅助不暴露通用字段读取入口。

// 整份wire、known/extras与必要引用全部成功才返回候选；失败结果不保留部分表。
Result<LegacyProjectionInput> decode_legacy_artifact(const std::string& bytes) {
    try {
        const auto envelope=parse(bytes,"/artifact");
        if(!envelope.is_object()) fail("InvalidProjectionValue","/artifact");
        fields(envelope,{"format","binding_kind","payload_sha256","payload_json"},{},"/artifact");
        if(envelope.at("format")!="HM-NATIVE-LEGACY-ARTIFACT/1" || envelope.at("binding_kind")!="artifact") fail("UnsupportedProjectionProfile","/artifact");
        const auto raw=json_string(envelope.at("payload_json"),"/artifact/payload_json");
        const auto digest=json_string(envelope.at("payload_sha256"),"/artifact/payload_sha256");
        if(!hex(digest,64) || digest!=sha256(raw)) fail("PayloadDigestMismatch","/artifact/payload_sha256");
        const auto typed=value(parse(raw,"/payload"),"/payload"); const auto& payload=*require_value<Dict>(typed,"/payload");
        fields(payload,{"source","provenance","actors","wallets","inventories","activities","offices","organizations","references","items","facts","coverage"},{},"/payload");
        provenance(payload);
        LegacyProjectionInput result; result.input_sha256=sha256(bytes); result.payload_sha256=digest;
        result.source=*require_value<Dict>(payload.at("source"),"/payload/source"); result.provenance=*require_value<Dict>(payload.at("provenance"),"/payload/provenance");
        const auto& refs=*require_value<Dict>(payload.at("references"),"/payload/references");
        fields(refs,{"tasks","parties","armies","catalog_actors","places","settlements","routes","world_maps","items","offices","garrisons","roles"},{},"/payload/references");
        for(const auto* key:legacy_reference_names) result.references.emplace(key,ids(refs.at(key),at("/payload/references",key)));
        const auto tick=*require_value<std::int64_t>(result.source.at("tick"),"/payload/source/tick");
        Partitions partitions; Keys actor_ids,wallet_ids,activity_ids,office_ids,organization_ids,item_ids,fact_ids;
        each(payload,"actors",[&](const Dict& row,const std::string& p) {
            ActorState a; a.id=text(row,"id",p); unique(a.id,actor_ids,"DuplicateActorId",at(p,"id"));
            a.extras=extras(row,{"id","wallet_id","inventory_id","action_id","alive","available","hp","fatigue","merit","location"},{"prototype_id","display_name","birth_tick","created_tick"},"actor",a.id,p,partitions);
            a.wallet_id=text(row,"wallet_id",p); a.inventory_id=text(row,"inventory_id",p); a.action_id=text(row,"action_id",p);
            a.alive=*require_value<bool>(row.at("alive"),at(p,"alive")); a.available=*require_value<bool>(row.at("available"),at(p,"available"));
            a.hp=whole(row.at("hp"),0,100,at(p,"hp")); a.fatigue=whole(row.at("fatigue"),0,100,at(p,"fatigue")); a.merit=whole(row.at("merit"),0,maximum,at(p,"merit"));
            if(row.count("prototype_id")) a.prototype_id=text(row,"prototype_id",p);
            if(row.count("display_name")) a.display_name=text(row,"display_name",p);
            const bool dynamic=!result.references.at("catalog_actors").count(a.id);
            if(row.count("created_tick")) a.created_tick=whole(row.at("created_tick"),0,dynamic?tick:0,at(p,"created_tick"));
            if(row.count("birth_tick")) a.birth_tick=whole(row.at("birth_tick"),-maximum,dynamic?a.created_tick.value_or(tick):0,at(p,"birth_tick"));
            reference(result.references.at("catalog_actors"),a.prototype_id.value_or(a.id),at(p,"prototype_id"));
            a.location=row.at("location"); location(a.location,result.references,at(p,"location")); result.actors.push_back(std::move(a));
        });
        each(payload,"activities",[&](const Dict& row,const std::string& p) {
            LegacyActivity a; a.id=text(row,"id",p); unique(a.id,activity_ids,"DuplicateActivityId",at(p,"id"));
            a.extras=extras(row,{"id","kind","actor_id","participants","start_tick","remaining_ticks","params"},{},"activity",a.id,p,partitions);
            a.kind=text(row,"kind",p); a.actor_id=text(row,"actor_id",p); reference(actor_ids,a.actor_id,at(p,"actor_id"));
            a.start_tick=whole(row.at("start_tick"),0,maximum,at(p,"start_tick")); a.remaining_ticks=whole(row.at("remaining_ticks"),1,maximum,at(p,"remaining_ticks"));
            Keys members;
            for(const auto& v:*require_value<Array>(row.at("participants"),at(p,"participants"))) {
                const auto id=*require_value<std::string>(v,at(p,"participants")); reference(actor_ids,id,at(p,"participants")); unique(id,members,"DuplicateParticipant",at(p,"participants")); a.participants.push_back(id);
            }
            if(!members.count(a.actor_id)) fail("InvalidProjectionOccupation",at(p,"actor_id"));
            a.params=*require_value<Dict>(row.at("params"),at(p,"params")); result.activities.push_back(std::move(a));
        });
        each(payload,"organizations",[&](const Dict& row,const std::string& p) {
            OrganizationDefinition d; d.id=text(row,"id",p); unique(d.id,organization_ids,"DuplicateOrganizationId",at(p,"id"));
            d.extras=extras(row,{"id","name_key","kind","leader_actor"},{},"organization",d.id,p,partitions);
            d.name_key=text(row,"name_key",p); d.kind=text(row,"kind",p);
            d.leader_actor=text(row,"leader_actor",p); reference(result.references.at("catalog_actors"),*d.leader_actor,at(p,"leader_actor"));
            result.organizations.push_back(std::move(d));
        });
        const std::map<std::string,Keys> owners{{"actor",actor_ids},{"organization",organization_ids},{"task",result.references.at("tasks")}};
        each(payload,"wallets",[&](const Dict& row,const std::string& p) {
            WalletState w; w.id=text(row,"id",p); unique(w.id,wallet_ids,"DuplicateWalletId",at(p,"id"));
            w.extras=extras(row,{"id","owner","balance","reserved"},{},"wallet",w.id,p,partitions); w.owner=row.at("owner"); owner(w.owner,at(p,"owner"),&owners);
            w.balance=whole(row.at("balance"),0,maximum,at(p,"balance")); std::int64_t total=0;
            for(const auto& [id,v]:*require_value<Dict>(row.at("reserved"),at(p,"reserved"))) {
                reference(activity_ids,id,at(at(p,"reserved"),id)); const auto amount=whole(v,0,maximum,at(at(p,"reserved"),id));
                if(total>std::numeric_limits<std::int64_t>::max()-amount) fail("ProjectionOverflow",at(p,"reserved"));
                total+=amount; if(total>w.balance) fail("OverReservedMoney",at(p,"reserved")); w.reserved.emplace(id,amount);
            }
            result.wallets.push_back(std::move(w));
        });
        std::map<std::string,std::int64_t> weights;
        each(payload,"items",[&](const Dict& row,const std::string& p) {
            ItemDefinition d; d.id=text(row,"id",p); unique(d.id,item_ids,"DuplicateItemId",at(p,"id")); reference(result.references.at("items"),d.id,at(p,"id"));
            d.extras=extras(row,{"id","weight"},{},"item",d.id,p,partitions); d.weight=whole(row.at("weight"),1,10000,at(p,"weight")); weights.emplace(d.id,d.weight); result.items.push_back(std::move(d));
        });
        if(item_ids!=result.references.at("items")) fail("MissingProjectionDefinition","/payload/items");
        result.inventories=decode_inventories(payload.at("inventories"),owners,activity_ids,weights,true);
        each(payload,"inventories",[&](const Dict& row,const std::string& p) {
            (void)extras(row,{"id","owner","capacity","revision","lots"},{},"inventory",text(row,"id",p),p,partitions);
            const auto& lots=*require_value<Array>(row.at("lots"),at(p,"lots"));
            for(std::size_t i=0;i<lots.size();++i) {
                const auto lp=at(at(p,"lots"),std::to_string(i)); const auto& lot=*require_value<Dict>(lots[i],lp);
                (void)extras(lot,{"id","item_id","quantity","owner","reserved_by"},{},"lot",text(lot,"id",lp),lp,partitions);
            }
        });
        each(payload,"facts",[&](const Dict& wrapper,const std::string& p) {
            fields(wrapper,{"id","record"},{},p); LegacyFact f; f.id=text(wrapper,"id",p); unique(f.id,fact_ids,"DuplicateFactId",at(p,"id"));
            const auto& row=*require_value<Dict>(wrapper.at("record"),at(p,"record")); f.kind=text(row,"kind",at(p,"record"));
            f.extras=extras(row,{"kind"},{},"fact",f.id,at(p,"record"),partitions); result.facts.push_back(std::move(f));
        });
        std::map<std::string,const LegacyFact*> facts; for(const auto& f:result.facts) facts.emplace(f.id,&f);
        Keys enabled_holders;
        each(payload,"offices",[&](const Dict& wrapper,const std::string& p) {
            fields(wrapper,{"state","definition"},{},p); const auto path=at(p,"state"); const auto& row=*require_value<Dict>(wrapper.at("state"),path);
            LegacyOffice o; o.id=text(row,"id",path); unique(o.id,office_ids,"DuplicateOfficeId",at(path,"id")); reference(result.references.at("offices"),o.id,at(path,"id"));
            o.extras=extras(row,{"id","holder_actor_id","enabled","appointment_id"},{},"office_state",o.id,path,partitions);
            o.holder_actor_id=text(row,"holder_actor_id",path); o.appointment_id=text(row,"appointment_id",path); o.enabled=*require_value<bool>(row.at("enabled"),at(path,"enabled"));
            o.definition=*require_value<Dict>(wrapper.at("definition"),at(p,"definition"));
            // definition整块沿头保存；coverage归extras，不另造第二份定义字段。
            Keys definition_keys; for(const auto& [key,v]:o.definition) { (void)v; definition_keys.insert(key); }
            partitions.emplace(std::make_pair("office_definition",o.id),std::make_pair(Keys{},definition_keys));
            const auto d=at(p,"definition"); if(text(o.definition,"id",d)!=o.id) fail("ProjectionDefinitionMismatch",d);
            reference(organization_ids,text(o.definition,"organization_id",d),at(d,"organization_id"));
            reference(result.references.at("garrisons"),text(o.definition,"garrison_id",d),at(d,"garrison_id"));
            reference(result.references.at("roles"),text(o.definition,"appointing_role_id",d),at(d,"appointing_role_id"));
            if(!o.holder_actor_id.empty()) reference(actor_ids,o.holder_actor_id,at(path,"holder_actor_id"));
            if(o.enabled) {
                if(o.holder_actor_id.empty() || !enabled_holders.insert(o.holder_actor_id).second) fail("InvalidOfficeAppointment",path);
                const auto found=facts.find(o.appointment_id); if(found==facts.end()) fail("DanglingProjectionReference",at(path,"appointment_id"));
                const auto& f=*found->second; const auto fp=at(path,"appointment_id");
                if(f.kind!="military_action" || text(f.extras,"command",fp)!="office_accept" || text(f.extras,"phase",fp)!="completed" ||
                   !*require_value<bool>(field(f.extras,"success",fp),at(fp,"success")) || text(f.extras,"actor_id",fp)!=o.holder_actor_id ||
                   text(*require_value<Dict>(field(f.extras,"terms",fp),at(fp,"terms")),"office_id",at(fp,"terms"))!=o.id) fail("InvalidOfficeAppointment",fp);
            } else if(o.holder_actor_id.empty()!=o.appointment_id.empty()) fail("InvalidOfficeHistory",path);
            result.offices.push_back(std::move(o));
        });
        if(office_ids!=result.references.at("offices")) fail("MissingProjectionDefinition","/payload/offices");
        std::map<std::string,const LegacyActivity*> activities; for(const auto& a:result.activities) activities.emplace(a.id,&a);
        std::map<std::string,const WalletState*> wallets; for(const auto& w:result.wallets) wallets.emplace(w.id,&w);
        std::map<std::string,const InventoryState*> inventories; for(const auto& i:result.inventories) inventories.emplace(i.id,&i);
        Keys used_wallets,used_inventories;
        for(const auto& a:result.actors) {
            const auto p=at("/payload/actors",a.id); reference(wallet_ids,a.wallet_id,at(p,"wallet_id"));
            if(!inventories.count(a.inventory_id)) fail("DanglingProjectionReference",at(p,"inventory_id"));
            const Value expected{Dict{{"kind",Value{std::string("actor")}},{"id",Value{a.id}}}};
            if(!(wallets.at(a.wallet_id)->owner==expected) || !(inventories.at(a.inventory_id)->owner==expected)) fail("ActorAssetOwnership",p);
            if(!used_wallets.insert(a.wallet_id).second || !used_inventories.insert(a.inventory_id).second) fail("SharedIdentityAssets",p);
            if(!a.action_id.empty()) {
                reference(activity_ids,a.action_id,at(p,"action_id")); const auto& members=activities.at(a.action_id)->participants;
                if(std::find(members.begin(),members.end(),a.id)==members.end()) fail("InvalidProjectionOccupation",at(p,"action_id"));
            }
            if(!result.references.at("catalog_actors").count(a.id)) actor_creation(a,facts,p);
        }
        std::map<std::string,const ActorState*> actors; for(const auto& a:result.actors) actors.emplace(a.id,&a);
        for(const auto& a:result.activities) for(const auto& id:a.participants)
            if(actors.at(id)->action_id!=a.id) fail("InvalidProjectionOccupation",at("/payload/activities",a.id));
        result.coverage=payload.at("coverage"); coverage(result.coverage,partitions);
        return {true,std::move(result),{}};
    } catch(const Failure& f) { return {false,{},f.error}; }
      catch(const std::bad_alloc&) { return {false,{}, {"ProjectionResourceLimit","/artifact"}}; }
}

// 查询只核自身完整形状和工件绑定；未知Actor由store在整批执行前检查。
Result<std::vector<ActorCommitmentsQuery>> decode_actor_queries(const std::string& bytes,const std::string& input_sha256) {
    try {
        const auto root=parse(bytes,"/queries"); if(!root.is_object()) fail("InvalidProjectionValue","/queries");
        fields(root,{"format","input_sha256","queries"},{},"/queries");
        if(root.at("format")!="HM-NATIVE-ACTOR-COMMITMENTS-QUERIES/1") fail("UnsupportedProjectionProfile","/queries/format");
        if(json_string(root.at("input_sha256"),"/queries/input_sha256")!=input_sha256) fail("SourceMismatch","/queries/input_sha256");
        if(!root.at("queries").is_array()) fail("InvalidProjectionValue","/queries/queries");
        std::vector<ActorCommitmentsQuery> result; Keys seen;
        for(std::size_t i=0;i<root.at("queries").size();++i) {
            const auto path=at("/queries/queries",std::to_string(i)); const auto& row=root.at("queries")[i];
            if(!row.is_object()) fail("InvalidProjectionValue",path);
            fields(row,{"query_id","actor_ids"},{},path);
            ActorCommitmentsQuery q; q.query_id=json_string(row.at("query_id"),at(path,"query_id")); unique(q.query_id,seen,"DuplicateQueryId",at(path,"query_id"));
            if(!row.at("actor_ids").is_array()) fail("InvalidProjectionValue",at(path,"actor_ids"));
            Keys actors;
            for(std::size_t j=0;j<row.at("actor_ids").size();++j) {
                const auto p=at(at(path,"actor_ids"),std::to_string(j)); auto id=json_string(row.at("actor_ids")[j],p);
                unique(id,actors,"DuplicateQueryActor",p); q.actor_ids.push_back(std::move(id));
            }
            result.push_back(std::move(q));
        }
        return {true,std::move(result),{}};
    } catch(const Failure& f) { return {false,{},f.error}; }
      catch(const std::bad_alloc&) { return {false,{}, {"ProjectionResourceLimit","/queries"}}; }
}
} // 新profile不扩大旧inventory wire的接受范围。
