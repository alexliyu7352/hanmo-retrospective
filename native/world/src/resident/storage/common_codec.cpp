#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
// 共同字段逐项无损冷解码；有序边界只在调用期间借用，不能进入候选成为通用字典。
#include "common_codec.hpp"
#include "resident/domains/military/field_battle/field_state_codec.hpp"
#include "resident/domains/travel/transport_physical_fact_headers.hpp"
#include "common_location_scoped_codec.hpp"
#include "common_checkpoint.hpp"
#include "common_projection.hpp"
#include "resident/checkpoint/checkpoint_catalog_rows.hpp"
#include "resident/domains/tasks/work_execution/work_execution_contract.hpp"
#include "resident/domains/tasks/task_exact_execution.hpp"
#include "resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp"
#include <charconv>
#include <iterator>
#include <set>
namespace hanmo::resident {
namespace boundary {
namespace {
template<class T>void prepare_common_value(std::uint64_t count=1){
    if(auto* scope=ScopedCheckpointJsonDecode::current())scope->default_values<T>(count);
    else if(auto* boundary=ScopedCheckpointBoundaryDecode::current())boundary->default_values<T>(count);
}
}
// 旧profile保持action字符串；新profile使用显式kind/id，不以ID前缀猜种类。
ReservationRef decode_reservation_ref(const BoundaryValue& v,std::string_view p,bool domain_activity_enabled){if(v.as<StableId>())return ReservationRef{string(v,p)};const auto& kind=get_string_ref(v,"kind",p);auto id=get_string(v,"id",p);if(kind=="domain_activity"){if(!domain_activity_enabled)unsupported("UnsupportedReservationKind",p);fields(v,{"kind","id","domain_id","activity_kind"},{},p);auto domain=get_string(v,"domain_id",p),activity=get_string(v,"activity_kind",p);if(id.empty()||domain.empty()||activity.empty())fail("InvalidReservation",p);return {ReservationRef::Kind::domain_activity,std::move(id),std::move(domain),std::move(activity)};}fields(v,{"kind","id"},{},p);ReservationRef::Kind k;if(kind=="action")k=ReservationRef::Kind::action;else if(kind=="contract")k=ReservationRef::Kind::contract;else if(kind=="org_activity")k=ReservationRef::Kind::org_activity;else fail("InvalidReservationKind",p);if(id.empty()&&k!=ReservationRef::Kind::action)fail("InvalidReservation",p);return {k,std::move(id)};}
BoundaryValue encode_reservation_ref(const ReservationRef& r,bool typed,bool domain_activity_enabled){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_common_reservation(writer,"",r,typed,domain_activity_enabled);if(status.ok)status=writer.finish();if(!status.ok)throw DecodeFailure{status.error};return out;}
Owner decode_owner(const BoundaryValue& v,std::string_view p){fields(v,{"kind","id"},{},p);const auto& kind=get_string_ref(v,"kind",p);Owner o;o.id=get_string(v,"id",p);if(kind=="actor")o.kind=Owner::Kind::actor;else if(kind=="organization")o.kind=Owner::Kind::organization;else if(kind=="task")o.kind=Owner::Kind::task;else if(kind=="household")o.kind=Owner::Kind::household;else fail("InvalidOwner",p);o.field_order=order(v);return o;}
Location decode_location(const BoundaryValue& v,std::string_view p,bool carrier_location_enabled){const auto& kind=get_string_ref(v,"kind",p);if(kind=="Settlement"){fields(v,{"kind","settlement_id","place_id"},{},p);return SettlementLocation{get_string(v,"settlement_id",p),get_string(v,"place_id",p)};}if(kind=="Wilderness"){fields(v,{"kind","map_id","origin","point"},{},p);auto& q=array(field(v,"point",p),checkpoint_decode_boundary_join({p,"/point"}));if(q.size()!=2)fail("InvalidWildernessPoint",p);return WildernessLocation{get_string(v,"map_id",p),get_string(v,"origin",p),{integer(q[0],p),integer(q[1],p)}};}if(kind=="Following"){fields(v,{"kind","party_id"},{},p);return FollowingLocation{get_string(v,"party_id",p)};}if(kind=="InArmy"){fields(v,{"kind","army_id"},{},p);return InArmyLocation{get_string(v,"army_id",p)};}if(kind=="OnRoute"){fields(v,{"kind","edge_id","origin","progress"},{},p);return OnRouteLocation{get_string(v,"edge_id",p),get_string(v,"origin",p),get_int(v,"progress",p)};}if(kind=="Carried"){if(!carrier_location_enabled)unsupported("CarrierLocationProfileRequired",p);fields(v,{"kind","carrier_kind","carrier_id"},{},p);const auto& k=get_string_ref(v,"carrier_kind",p);auto id=get_string(v,"carrier_id",p);CarriedLocation::CarrierKind carrier;if(k=="transport_unit")carrier=CarriedLocation::CarrierKind::transport_unit;else if(k=="vessel")carrier=CarriedLocation::CarrierKind::vessel;else if(k=="fleet")carrier=CarriedLocation::CarrierKind::fleet;else fail("InvalidCarrierKind",p);if(id.empty())fail("InvalidCarriedLocation",p);return CarriedLocation{carrier,std::move(id)};}fail("InvalidLocationKind",p);return {};}
// 显式同 scope 仅补外来字段接口；字符串与路径仍由原 decoder 分配前收费。
Location decode_location(const BoundaryValue &value, std::string_view path,
                         ScopedCheckpointJsonDecode &scope,
                         bool carrier_location_enabled) {
  if (ScopedCheckpointJsonDecode::current() != &scope)
    scope.fail("CheckpointJsonContextMismatch", path);
  return decode_location(value, path, carrier_location_enabled);
}
BoundaryValue encode_owner(const Owner &o) {
  BoundaryValue out;
  BoundaryProjectionWriter writer(out);
  auto status = emit_common_owner(writer, "", o);
  if (status.ok)
    status = writer.finish();
  if (!status.ok)
    throw DecodeFailure{status.error};
  return out;
}
BoundaryValue encode_location(const Location& location,bool carrier_location_enabled){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_common_location(writer,"",location,{},carrier_location_enabled);if(status.ok)status=writer.finish();if(!status.ok)throw DecodeFailure{status.error};return out;}
LotView decode_lot(const BoundaryValue& input,std::string_view path,const CommonResourceCodecContext& context){
    std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>> allowed={"id","item_id","quantity","owner","reserved_by"};if(context.item_instance_refs_enabled)allowed.insert("item_instance_id");if(context.lot_property_refs_enabled)allowed.insert("lot_properties_ref");
    for(const auto& key:allowed)field(input,key,path);for(const auto& row:object(input,path))if(!allowed.count(row.first))fail("CommonExtensionCodecMismatch",checkpoint_decode_boundary_join({path,"/",row.first}));
    prepare_common_value<LotView>();LotView lot;lot.id=get_string(input,"id",path);lot.item_id=get_string(input,"item_id",path);lot.quantity=get_int(input,"quantity",path);lot.owner=decode_owner(field(input,"owner",path),checkpoint_decode_boundary_join({path,"/owner"}));lot.reserved_by=decode_reservation_ref(field(input,"reserved_by",path),checkpoint_decode_boundary_join({path,"/reserved_by"}),context.domain_activity_reservation_refs_enabled);lot.field_order=order(input);
    if(context.item_instance_refs_enabled){const auto& value=field(input,"item_instance_id",path);if(!std::holds_alternative<std::monostate>(value.data)){lot.item_instance_id=string(value,checkpoint_decode_boundary_join({path,"/item_instance_id"}));if(lot.item_instance_id->empty())fail("InvalidItemInstanceReference",path);}}
    if(context.lot_property_refs_enabled){const auto& value=field(input,"lot_properties_ref",path);if(!std::holds_alternative<std::monostate>(value.data)){fields(value,{"properties_id"},{},checkpoint_decode_boundary_join({path,"/lot_properties_ref"}));auto id=get_string(value,"properties_id",checkpoint_decode_boundary_join({path,"/lot_properties_ref"}));if(id.empty())fail("InvalidLotPropertiesReference",path);lot.lot_properties_ref=LotPropertiesRef{std::move(id)};}}
    if(lot.item_instance_id&&lot.lot_properties_ref)fail("LotReferenceConflict",path);return lot;
}
BoundaryValue encode_lot(const LotView& lot,bool typed,const CommonResourceCodecContext& context){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_common_lot(writer,"",lot,typed,context);if(status.ok)status=writer.finish();if(!status.ok)throw DecodeFailure{status.error};return out;}

WalletView decode_wallet_frame(const BoundaryValue& v,std::string_view path,bool typed,const CommonResourceCodecContext& resources){fields(v,{"id","owner","balance","reserved"},{},path);prepare_common_value<WalletView>();WalletView w;w.id=get_string(v,"id",path);w.owner=decode_owner(field(v,"owner",path),path);w.balance=get_int(v,"balance",path);const auto& held=field(v,"reserved",path);if(held.as<BoundaryValue::Dict>()){const auto& reservations=object(held,path);if(auto* scope=ScopedCheckpointJsonDecode::current())scope->collection("reserved",reservations.size());checkpoint_decode_boundary_array(reservations.size(),sizeof(Reservation));prepare_common_value<Reservation>(reservations.size());w.reserved.reserve(reservations.size());for(auto& r:reservations)w.reserved.push_back({checkpoint_decode_boundary_string(r.first),integer(r.second,path)});}else{if(!typed)fail("InvalidResidentObject",checkpoint_decode_boundary_join({path,"/reserved"}));const auto& reservations=array(held,path);if(auto* scope=ScopedCheckpointJsonDecode::current())scope->collection("reserved",reservations.size());checkpoint_decode_boundary_array(reservations.size(),sizeof(Reservation));prepare_common_value<Reservation>(reservations.size());w.reserved.reserve(reservations.size());for(const auto& r:reservations){fields(r,{"reference","amount"},{},path);w.reserved.push_back({decode_reservation_ref(field(r,"reference",path),path,resources.domain_activity_reservation_refs_enabled),get_int(r,"amount",path)});}}w.field_order=order(v);return w;}
InventoryView decode_inventory_frame(const BoundaryValue& v,std::string_view path,const CommonResourceCodecContext& resources){fields(v,{"id","owner","capacity","revision","lots"},{},path);prepare_common_value<InventoryView>();InventoryView inv;inv.id=get_string(v,"id",path);inv.owner=decode_owner(field(v,"owner",path),path);inv.capacity=get_int(v,"capacity",path);inv.revision=get_int(v,"revision",path);const auto& lots=array(field(v,"lots",path),path);if(auto* scope=ScopedCheckpointJsonDecode::current())scope->collection("lots",lots.size());checkpoint_decode_boundary_array(lots.size(),sizeof(LotView));inv.lots.reserve(lots.size());for(auto& l:lots){inv.lots.push_back(decode_lot(l,path,resources));}inv.field_order=order(v);return inv;}

} // 有限位置标签不通过字符串字段兜底保存未实现位置。
Outcome<WalletView> decode_common_wallet_frame(const BoundaryValue& value,std::string_view path,bool typed,const CommonResourceCodecContext& resources,ScopedCheckpointJsonDecode& budget){
    if(&budget!=ScopedCheckpointJsonDecode::current())return {false,{},common_failure("CheckpointForeignDecodeContextMismatch",{},ErrorKind::internal).error};
    try{return {true,boundary::decode_wallet_frame(value,path,typed,resources),{}};}
    catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}
}
Outcome<InventoryView> decode_common_inventory_frame(const BoundaryValue& value,std::string_view path,const CommonResourceCodecContext& resources,ScopedCheckpointJsonDecode& budget){
    if(&budget!=ScopedCheckpointJsonDecode::current())return {false,{},common_failure("CheckpointForeignDecodeContextMismatch",{},ErrorKind::internal).error};
    try{return {true,boundary::decode_inventory_frame(value,path,resources),{}};}
    catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}
}

namespace {
using namespace boundary;
// 十进制随机状态严格拒绝前导零、溢出或非十进制字符。
Integer decimal(const BoundaryValue& v,const std::string& p){
    const auto& text=string_ref(v,{p});Integer value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())fail("InvalidRandomState",p);
    // Same signed integer and canonical decimal comparison as the original
    // reader; the existing writer also uses to_chars on a 32-byte stack range.
    char digits[32];const auto formatted=std::to_chars(digits,digits+sizeof(digits),value);
    if(formatted.ec!=std::errc{}||std::string_view(digits,static_cast<std::size_t>(formatted.ptr-digits))!=text)fail("InvalidRandomState",p);
    return value;
}
AppearanceProfile appearance(const BoundaryValue& v,const std::string& p){fields(v,{"format_version","generator_version","generation_seed","base_features","attire_style","source","parent_sources"},{},p);AppearanceProfile a;a.field_order=order(v);a.format_version=get_int(v,"format_version",p);a.generator_version=get_string(v,"generator_version",p);a.generation_seed=get_string(v,"generation_seed",p);a.attire_style=get_string(v,"attire_style",p);a.source=get_string(v,"source",p);if(!array(field(v,"parent_sources",p),p).empty()){if(a.source=="legacy")fail("InvalidLegacyAppearance",p);unsupported("AppearanceInheritanceCapabilityRequired",p);}auto& f=field(v,"base_features",p);if(!object(f,p).empty()){fields(f,{"face_shape","eyes","brows","nose","mouth","hair","beard","skin_palette","build"},{},p);a.base_features.present=true;a.base_features.field_order=order(f);
a.base_features.face_shape=get_string(f,"face_shape",p);
a.base_features.eyes=get_string(f,"eyes",p);
a.base_features.brows=get_string(f,"brows",p);
a.base_features.nose=get_string(f,"nose",p);
a.base_features.mouth=get_string(f,"mouth",p);
a.base_features.hair=get_string(f,"hair",p);
a.base_features.beard=get_string(f,"beard",p);
a.base_features.skin_palette=get_string(f,"skin_palette",p);
a.base_features.build=get_string(f,"build",p);
}return a;}
ActorRecord actor_record(const BoundaryValue& v,const std::string& p,bool extended_skills,bool carrier_location_enabled){fields(v,{"id","location","alive","available","hp","fatigue","merit","attributes","skills","wallet_id","inventory_id","action_id","appearance","injury","combat"},{"birth_tick","created_tick","prototype_id","display_name"},p);ActorRecord a;a.field_order=order(v);a.location_order=order(field(v,"location",p));a.attributes_order=order(field(v,"attributes",p));a.skills_order=order(field(v,"skills",p));a.combat_order=order(field(v,"combat",p));
a.id=get_string(v,"id",p);
a.wallet_id=get_string(v,"wallet_id",p);
a.inventory_id=get_string(v,"inventory_id",p);
a.action_id=get_string(v,"action_id",p);
a.alive=get_bool(v,"alive",p);
a.available=get_bool(v,"available",p);
a.hp=get_int(v,"hp",p);
a.fatigue=get_int(v,"fatigue",p);
a.merit=get_int(v,"merit",p);
a.injury=get_int(v,"injury",p);
if(auto* x=v.find("birth_tick"))a.birth_tick=integer(*x,p);
if(auto* x=v.find("created_tick"))a.created_tick=integer(*x,p);
if(auto* x=v.find("prototype_id"))a.prototype_id=string(*x,p);
if(auto* x=v.find("display_name"))a.display_name=string(*x,p);
a.location=decode_location(field(v,"location",p),checkpoint_decode_boundary_join({p,"/location"}),carrier_location_enabled);a.appearance=checkpoint_make_shared<const AppearanceProfile>(appearance(field(v,"appearance",p),checkpoint_decode_boundary_join({p,"/appearance"})));
{auto& f=field(v,"attributes",p);fields(f,{"administration","charisma","intellect","leadership","martial"},{},p);
a.attributes.administration=get_int(f,"administration",p);
a.attributes.charisma=get_int(f,"charisma",p);
a.attributes.intellect=get_int(f,"intellect",p);
a.attributes.leadership=get_int(f,"leadership",p);
a.attributes.martial=get_int(f,"martial",p);
}
{auto& f=field(v,"skills",p);
if(extended_skills)fields(f,{"archery","cavalry","commerce","debate","infantry","martial","negotiation","rites","tactics","transport","extended"},{},p);
else fields(f,{"archery","cavalry","commerce","debate","infantry","martial","negotiation","rites","tactics","transport"},{},p);
a.skills.archery=get_int(f,"archery",p);
a.skills.cavalry=get_int(f,"cavalry",p);
a.skills.commerce=get_int(f,"commerce",p);
a.skills.debate=get_int(f,"debate",p);
a.skills.infantry=get_int(f,"infantry",p);
a.skills.martial=get_int(f,"martial",p);
a.skills.negotiation=get_int(f,"negotiation",p);
a.skills.rites=get_int(f,"rites",p);
a.skills.tactics=get_int(f,"tactics",p);
a.skills.transport=get_int(f,"transport",p);
if(extended_skills){const auto& x=field(f,"extended",p);fields(x,{"naval","espionage","fortification","agriculture","medicine","forging"},{},checkpoint_decode_boundary_join({p,"/skills/extended"}));ActorExtendedSkills e;e.naval=get_int(x,"naval",p);e.espionage=get_int(x,"espionage",p);e.fortification=get_int(x,"fortification",p);e.agriculture=get_int(x,"agriculture",p);e.medicine=get_int(x,"medicine",p);e.forging=get_int(x,"forging",p);e.field_order=order(x);a.skills.extended=std::move(e);}
}
{auto& c=field(v,"combat",p);fields(c,{"weapon_id","learned","heart_id"},{"armor_id"},p);a.combat.weapon_id=get_string(c,"weapon_id",p);a.combat.heart_id=get_string(c,"heart_id",p);if(auto* x=c.find("armor_id"))a.combat.armor_id=string(*x,p);const auto& learned=object(field(c,"learned",p),p);CheckpointCatalogRows<Integer> rows(learned.size(),p);for(const auto& row:learned)rows.insert(row.first,integer(row.second,p));rows.publish(a.combat.learned);}return a;}
// 全树实例上界只读扫描覆盖领域尚未decode的引用，保原共同碰撞检查。
Integer max_instance_text(std::string_view text){
    Integer result=0;const auto separator=text.find(':');
    if(separator!=std::string_view::npos&&separator>0){
        static constexpr std::string_view prefixes[]={"lot","action","task","result","relation","settlement","interaction","party","encounter","battle","social","actor","estate","journey","family","voluntary_path","military","military_request","military_tactic","military_tactic_practice","military_recovery","military_attrition","military_rest","military_siege_engineering","military_siege_lifecycle","military_rout_entry","military_rout","military_release","army","campaign","contact","battle_round","siege","quote","affair","claim","contract","delivery","labor","local_due","local_payment","private_shop_rent_assessed","private_shop_rent_paid","private_shop_rent_resumed","local_production","local_ration","local_supply","local_supply_observation","message","need","opportunity","org_activity","organization","population_transfer","transfer","cargo_disposition","population_military","population_loss","siege_integration_action","siege_integration_proposal","siege_officer_decision","military_integration","population_integration","membership_offer","org_member","org_duty","membership_wage","membership_obligation","membership_terminal"};
        if(std::find(std::begin(prefixes),std::end(prefixes),text.substr(0,separator))!=std::end(prefixes)){
            Integer value=0;const auto parsed=std::from_chars(text.data()+separator+1,text.data()+text.size(),value);
            if(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size())result=value;
        }
    }
    return result;
}
Integer max_instance(const BoundaryValue& v){
    if(const auto* text=v.as<std::string>())return max_instance_text(*text);
    Integer result=0;
    if(const auto* rows=v.as<BoundaryValue::Array>())for(const auto& child:*rows)result=std::max(result,max_instance(child));
    else if(const auto* rows=v.as<BoundaryValue::Dict>())for(const auto& row:*rows){result=std::max(result,max_instance_text(row.first));result=std::max(result,max_instance(row.second));}
    return result;
}
} // 未知字段与畸形标量必须在独立seed门拒绝。
Outcome<CommonCatalog> decode_common_catalog(const BoundaryValue& data){try{using namespace boundary;CommonCatalog c;
{
 const auto& input=object(field(data,"items","data"),"items");CheckpointCatalogRows<ItemDefinition> rows(input.size(),"items");
 for(const auto& row:input){const auto& v=row.second;rows.insert(row.first,{get_string(v,"id",row.first),get_int(v,"weight",row.first),get_int(v,"base_price",row.first),v.find("weapon")!=nullptr,v.find("armor")!=nullptr});const auto* item=rows.find(row.first);if(item->weight<1||item->weight>10000||item->base_price<0||item->base_price>2147483647)fail("InvalidItemDefinition",row.first);}
 rows.publish(c.items);
}
{
 const auto& input=object(field(data,"places","data"),"places");CheckpointCatalogRows<PlaceDefinition> rows(input.size(),"places");
 for(const auto& row:input)rows.insert(row.first,{checkpoint_decode_boundary_string(row.first),get_string(row.second,"settlement_id",row.first),strings(field(row.second,"passages",row.first),row.first)});
 rows.publish(c.places);
}
{
 const auto& input=object(field(data,"roles","data"),"roles");CheckpointCatalogRows<RoleDefinition> rows(input.size(),"roles");
 for(const auto& row:input){const auto& v=row.second;RoleDefinition role;role.id=checkpoint_decode_boundary_string(row.first);role.initial_actor=get_string(v,"initial_actor",row.first);role.place_id=get_string(v,"place_id",row.first);if(!std::holds_alternative<std::monostate>(field(v,"organization_id",row.first).data))role.organization_id=get_string(v,"organization_id",row.first);role.services=strings(field(v,"services",row.first),row.first);if(const auto* value=v.find("dialogue_id"))role.dialogue_id=string(*value,row.first);if(const auto* value=v.find("companion_cost_per_tick"))role.companion_cost_per_tick=integer(*value,row.first);rows.insert(row.first,std::move(role));}
 rows.publish(c.roles);
}
{
 const auto& input=object(field(data,"organizations","data"),"organizations");checkpoint_decode_boundary_array(input.size(),sizeof(StableId));c.organization_ids.reserve(input.size());
 for(const auto& row:input)c.organization_ids.push_back(checkpoint_decode_boundary_string(row.first));
}
if(const auto* active=data.find("active_static_actor_ids"))c.actor_ids=strings(*active,"active_static_actor_ids");else{
 const auto& input=object(field(data,"actors","data"),"actors");checkpoint_decode_boundary_array(input.size(),sizeof(StableId));c.actor_ids.reserve(input.size());
 for(const auto& row:input)c.actor_ids.push_back(checkpoint_decode_boundary_string(row.first));
}
{
 const auto& input=object(field(data,"wallets","data"),"wallets");CheckpointCatalogRows<RequiredContainer> rows(input.size(),"wallets");
 for(const auto& row:input)rows.insert(row.first,{decode_owner(field(row.second,"owner",row.first),row.first),0});
 rows.publish(c.required_wallets);
}
{
 const auto& input=object(field(data,"inventories","data"),"inventories");CheckpointCatalogRows<RequiredContainer> rows(input.size(),"inventories");
 for(const auto& row:input)rows.insert(row.first,{decode_owner(field(row.second,"owner",row.first),row.first),get_int(row.second,"capacity",row.first)});
 rows.publish(c.required_inventories);
}
{
 const auto& input=object(field(data,"actors","data"),"actors");CheckpointCatalogRows<std::optional<Integer>> births(input.size(),"actors");CheckpointCatalogRows<ActorNameDefinition> names(input.size(),"actors");
 for(const auto& row:input){std::optional<Integer> birth;if(const auto* age=row.second.find("initial_age_years")){const auto years=integer(*age,row.first);if(years<0||years>120)fail("InvalidActorDefinition",row.first);birth=-years*720;}births.insert(row.first,birth);ActorNameDefinition name;if(const auto* key=row.second.find("name_key"))name.name_key=string(*key,checkpoint_decode_boundary_join({"actors/",row.first,"/name_key"}));if(const auto* profile=row.second.find("presentation_profile_id"))name.presentation_profile_id=string(*profile,checkpoint_decode_boundary_join({"actors/",row.first,"/presentation_profile_id"}));if(const auto* portrait=row.second.find("portrait_kind"))name.portrait_kind=string(*portrait,checkpoint_decode_boundary_join({"actors/",row.first,"/portrait_kind"}));names.insert(row.first,std::move(name));}
 births.publish(c.actor_initial_birth);names.publish(c.actor_names);
}
// Original text/name tables remain cold-only; no BoundaryValue is retained.
if(const auto* texts=data.find("texts")){
 const auto& input=object(*texts,"texts");CheckpointCatalogRows<std::string> rows(input.size(),"texts");
 for(const auto& row:input)rows.insert(row.first,string(row.second,checkpoint_decode_boundary_join({"texts/",row.first})));
 rows.publish(c.texts);
}
if(const auto* skills=data.find("personal_skills")){
 const auto& input=object(*skills,"personal_skills");CheckpointCatalogRows<StableId> rows(input.size(),"personal_skills");
 for(const auto& row:input)rows.insert(row.first,get_string(row.second,"kind",row.first));
 rows.publish(c.personal_skill_kinds);
}
return {true,std::move(c),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<CommonState> decode_common_seed(const BoundaryValue& source,const CommonCatalog& catalog,const CommonResultCodec* result_codec,CommonSeedValidation validation){try{using namespace boundary;CommonState s;s.root_field_order=order(source);if(auto* battles=source.find("battles"))s.battle_order=order(*battles);
s.metadata.ruleset_id=get_string(source,"ruleset_id","");
s.metadata.content_id=get_string(source,"content_id","");
s.metadata.content_version=get_string(source,"content_version","");
s.metadata.content_digest=get_string(source,"content_digest","");
s.metadata.engine_build=get_string(source,"engine_build","");
s.metadata.rng_backend=get_string(source,"rng_backend","");
s.metadata.scenario_id=get_string(source,"scenario_id","");
s.metadata.player_actor_id=get_string(source,"player_actor_id","");
s.identity.tick=get_int(source,"tick","");s.identity.revision=get_int(source,"revision","");s.next_instance_id=get_int(source,"next_instance_id","");
{
 const auto& input=object(field(source,"actors",""),"actors");CheckpointCatalogRows<ActorRecord> decoded(input.size(),"actors");
 for(auto& row:input)decoded.insert(row.first,actor_record(row.second,checkpoint_decode_boundary_join({"actors/",row.first}),catalog.extended_skills,catalog.carrier_location_enabled));
 decoded.publish(s.actors);
}
{
 const auto& input=object(field(source,"roles",""),"roles");CheckpointCatalogRows<RoleRecord> decoded(input.size(),"roles");
 for(auto& row:input){auto& v=row.second;fields(v,{"actor_id","enabled"},{},row.first);decoded.insert(row.first,{get_string(v,"actor_id",row.first),get_bool(v,"enabled",row.first),order(v)});}
 decoded.publish(s.roles);
}
{
 const auto& input=object(field(source,"wallets",""),"wallets");CheckpointCatalogRows<WalletView> decoded(input.size(),"wallets");
 for(const auto& row:input)decoded.insert(row.first,decode_wallet_frame(row.second,row.first,s.metadata.ruleset_id=="hanmo.native.local_society/1",{catalog.item_instances_enabled,catalog.lot_properties_enabled,catalog.domain_activity_reservations_enabled}));
 decoded.publish(s.wallets);
}
{
 const auto& input=object(field(source,"inventories",""),"inventories");CheckpointCatalogRows<InventoryView> decoded(input.size(),"inventories");
 for(const auto& row:input)decoded.insert(row.first,decode_inventory_frame(row.second,row.first,{catalog.item_instances_enabled,catalog.lot_properties_enabled,catalog.domain_activity_reservations_enabled}));
 decoded.publish(s.inventories);
}
{
 const auto& input=object(field(source,"actions",""),"actions");CheckpointCatalogRows<ActionCoreView> decoded(input.size(),"actions");CheckpointCatalogRows<std::vector<StableId>> orders(input.size(),"actions");
 for(auto& row:input){auto& v=row.second;fields(v,{"id","kind","actor_id","participants","remaining_ticks","start_tick","params"},{},row.first);ActionCoreView a;a.id=get_string(v,"id",row.first);a.kind=get_string(v,"kind",row.first);a.actor_id=get_string(v,"actor_id",row.first);a.participants=strings(field(v,"participants",row.first),row.first);a.remaining_ticks=get_int(v,"remaining_ticks",row.first);a.start_tick=get_int(v,"start_tick",row.first);object(field(v,"params",row.first),row.first);decoded.insert(row.first,std::move(a));orders.insert(row.first,order(v));}
 decoded.publish(s.actions);orders.publish(s.action_field_order);
}
{
 const auto& input=object(field(source,"parties",""),"parties");CheckpointCatalogRows<PartyRecord> decoded(input.size(),"parties");
 for(auto& row:input){auto& v=row.second;fields(v,{"id","leader_id","members","terms","created_tick"},{},row.first);PartyRecord a;a.id=get_string(v,"id",row.first);a.leader_id=get_string(v,"leader_id",row.first);a.members=strings(field(v,"members",row.first),row.first);a.created_tick=get_int(v,"created_tick",row.first);const auto& terms=object(field(v,"terms",row.first),row.first);CheckpointCatalogRows<CompanionTerms> decoded_terms(terms.size(),row.first);for(auto& t:terms){fields(t.second,{"role_id","cost_per_tick"},{},row.first);decoded_terms.insert(t.first,{get_string(t.second,"role_id",row.first),get_int(t.second,"cost_per_tick",row.first),order(t.second)});}decoded_terms.publish(a.terms);a.field_order=order(v);decoded.insert(row.first,std::move(a));}
 decoded.publish(s.parties);
}
// 生命终点仅exact family_death归独立域；其余family前缀和全部旧声明分类保持。
// 生前路径撤回只准精确具名头/domain；完整资格与取消结果仍交所属领域冷门。
{
 const auto& input=object(field(source,"facts",""),"facts");CheckpointCatalogRows<FactRecord> decoded(input.size(),"facts");
 for(auto& row:input){const auto& kind=get_string_ref(row.second,"kind",row.first);
// These finite original Military rows own the payload; Common stores headers only.
// Their actual selected original owner is checked by World restore.
const bool exact_military_header=kind=="campaign_accepted"||kind=="military_office_completion_applied"||kind=="military_formation_completion_checked"||kind=="military_training_completion_applied"||kind=="military_action_completion_applied";
const bool exact_action_header = kind == "military_action" && row.second.find("domain");
if (exact_action_header) {
  if (validation != CommonSeedValidation::defer_until_bundle)
    unsupported("MilitaryAuthorityWholeWorldSourcesRequired", row.first);
  fields(row.second, {"kind", "domain"}, {}, row.first);
  if (object(row.second, row.first).size() != 2 ||
      get_string_ref(row.second, "domain", row.first) != "military")
    fail("MilitaryAuthorityHeaderMismatch", row.first);
}
if(exact_military_header){
 if(validation!=CommonSeedValidation::defer_until_bundle)unsupported("MilitaryAuthorityWholeWorldSourcesRequired",row.first);
 fields(row.second,{"kind","domain"},{},row.first);
 if(object(row.second,row.first).size()!=2||get_string_ref(row.second,"domain",row.first)!="military")fail("MilitaryAuthorityHeaderMismatch",row.first);
}
// 目录与领域 emitter/反覆盖共用；完整来源仍由发布前同 World owner 校验。
const bool field_battle_header = owns_field_battle_fact_kind(kind);
if (field_battle_header) {
  if (validation != CommonSeedValidation::defer_until_bundle)
    unsupported("FieldBattleWholeWorldSourcesRequired", row.first);
  fields(row.second, {"kind", "domain"}, {}, row.first);
  if (object(row.second, row.first).size() != 2 ||
      get_string_ref(row.second, "domain", row.first) != "military")
    fail("FieldBattleHeaderMismatch", row.first);
}
// 原 typed 根独占的有限头集合；领域名不是豁免条件，坏域和多余正文立即拒绝。
const bool political_original_header = kind == "political_relations_initialized" ||
    kind == "political_battle_relation_changed" || kind == "political_alliance_breached" ||
    kind == "political_passage_decided";
const bool growth_terminal_header = kind == "growth_result_terminal";
const bool travel_owned_header = kind == "movement_boundary_contact" ||
    kind == "movement_boundary_continuation" ||
    kind == "local_move" ||
    kind == "group_contact_member_release" || kind == "group_member_loss_handoff";
const bool transport_physical_header = owns_transport_physical_fact_kind("travel", kind) ||
    owns_transport_physical_fact_kind("local_economy", kind);
if (political_original_header || growth_terminal_header || travel_owned_header ||
    transport_physical_header) {
  if (validation != CommonSeedValidation::defer_until_bundle)
    unsupported("OriginalTypedFactWholeWorldSourcesRequired", row.first);
  fields(row.second, {"kind", "domain"}, {}, row.first);
  const auto& actual_domain = get_string_ref(row.second,"domain",row.first);
  if (object(row.second,row.first).size()!=2 ||
      (political_original_header && actual_domain!="politics") ||
      (growth_terminal_header && actual_domain!="growth") ||
      (travel_owned_header && actual_domain!="travel") ||
      (transport_physical_header && !owns_transport_physical_fact_kind(actual_domain,kind)))
    fail("OriginalTypedFactHeaderMismatch", row.first);
}
// 占用正文在独立原段；这里只准同 World 冷恢复的精确头，不收额外业务键。
if (kind == "actor_social_occupancy_changed") {
  if (validation != CommonSeedValidation::defer_until_bundle)
    unsupported("SocialOccupancyWholeWorldSourcesRequired", row.first);
  fields(row.second, {"kind", "domain"}, {}, row.first);
  if (object(row.second, row.first).size() != 2 ||
      get_string_ref(row.second, "domain", row.first) != "interaction")
    fail("SocialOccupancySourceHeaderMissing", row.first);
}
const bool condition_application_header = owns_condition_application_fact_kind(kind);
if (condition_application_header) {
  if (validation != CommonSeedValidation::defer_until_bundle)
    unsupported("ConditionApplicationWholeWorldSourcesRequired", row.first);
  fields(row.second, {"kind", "domain"}, {}, row.first);
  if (object(row.second, row.first).size() != 2 ||
      get_string_ref(row.second, "domain", row.first) != "actor_foundation")
    fail("ConditionApplicationHeaderMismatch", row.first);
}
const bool original_social=kind=="relation_applied"||kind=="social_event_applied";
if(original_social){
 if(validation!=CommonSeedValidation::defer_until_bundle)unsupported("OriginalSocialWholeWorldSourcesRequired",row.first);
 fields(row.second,{"kind","domain"},{},row.first);
 if(object(row.second,row.first).size()!=2||get_string_ref(row.second,"domain",row.first)!="social")fail("OriginalSocialFactHeaderMismatch",row.first);
}
const bool work_expense=is_governance_work_original_fact_kind(kind),facility_condition=kind=="facility_condition_change";
if(work_expense||facility_condition){
 if(validation!=CommonSeedValidation::defer_until_bundle)unsupported("EconomicFacilityWholeWorldSourcesRequired",row.first);
 const auto& domain=get_string_ref(row.second,"domain",row.first);
 if(work_expense?domain!="local_economy":(domain!="local_economy"&&domain!="estate"&&domain!="commerce"))fail("EconomicFacilityFactHeaderMismatch",row.first);
 // Actual typed owners validate the complete original payload after World decode.
}
const bool task_exact=owns_task_exact_original_fact_kind(kind);
if(task_exact){
 if(validation!=CommonSeedValidation::defer_until_bundle)unsupported("TaskExecutionWholeWorldSourcesRequired",row.first);
 // The full payload and its exact field order are checked against the actual
 // selected original Task row after the World section has been decoded.
 if(get_string_ref(row.second,"domain",row.first)!="tasks")fail("TaskExecutionFactHeaderMismatch",row.first);
}
const bool task_work=owns_task_work_original_fact_kind(kind);
if(task_work){
 if(validation!=CommonSeedValidation::defer_until_bundle)unsupported("TaskWorkWholeWorldSourcesRequired",row.first);
 fields(row.second,{"kind","domain"},{},row.first);
 if(object(row.second,row.first).size()!=2||get_string_ref(row.second,"domain",row.first)!="tasks")fail("TaskWorkFactHeaderMismatch",row.first);
}
if(kind=="family_care_relief_accepted"||kind=="family_care_relief_started"||kind=="family_care_relief_provided"||kind=="family_care_relief_closed")fields(row.second,{"kind"},{"domain"},row.first);if(kind=="task_deadline_extended")fields(row.second,{"kind"},{"domain"},row.first);const bool political_succession=kind=="political_successor_nominated"||kind=="political_succession_accepted"||kind=="political_succession_declined"||kind=="political_succession_withdrawn"||kind=="political_succession_acceded";if(political_succession)fields(row.second,{"kind","domain"},{},row.first);const bool political_head_handover=kind=="political_head_handover_offered"||kind=="political_head_handover_accepted"||kind=="political_head_handover_declined"||kind=="political_head_handover_withdrawn";const bool political_term_handover=kind=="political_term_handover_offered"||kind=="political_term_handover_accepted"||kind=="political_term_handover_declined"||kind=="political_term_handover_withdrawn";const bool political_office_reception=kind=="political_office_reception_assumed";const bool political_office_resignation=kind=="political_office_resignation_requested"||kind=="political_office_resignation_answered"||kind=="political_office_resignation_withdrawn";const bool political_subordinate_appointment=kind=="political_subordinate_appointment_offered"||kind=="political_subordinate_appointment_accepted"||kind=="political_subordinate_appointment_declined"||kind=="political_subordinate_appointment_withdrawn"||kind=="political_subordinate_appointment_arrived";if(political_subordinate_appointment)fields(row.second,{"kind","domain"},{},row.first);if(political_office_resignation)fields(row.second,{"kind","domain"},{},row.first);const bool political_governance_agency=kind=="political_governance_agency_offered"||kind=="political_governance_agency_accepted"||kind=="political_governance_agency_declined"||kind=="political_governance_agency_revoked"||kind=="political_governance_agency_issued";if(political_governance_agency)fields(row.second,{"kind","domain"},{},row.first);if(political_office_reception)fields(row.second,{"kind","domain"},{},row.first);if(political_term_handover)fields(row.second,{"kind","domain"},{},row.first);const bool political_successor_round=kind=="political_successor_round_nominated"||kind=="political_successor_round_accepted"||kind=="political_successor_round_declined"||kind=="political_successor_round_withdrawn"||kind=="political_successor_round_acceded";if(political_successor_round)fields(row.second,{"kind","domain"},{},row.first);if(political_head_handover)fields(row.second,{"kind","domain"},{},row.first);const bool invitation_reply=kind=="political_governance_invitation_closed";if(invitation_reply)fields(row.second,{"kind"},{"domain"},row.first);const bool daily_rent=kind=="private_shop_rent_assessed"||kind=="private_shop_rent_paid"||kind=="private_shop_rent_resumed";if(daily_rent)fields(row.second,{"kind","domain"},{},row.first);const bool apprenticeship=kind=="family_apprenticeship_proposed"||kind=="family_apprenticeship_replied"||kind=="family_apprenticeship_exited";const bool visit_reply=kind=="visit_message_reply_posted"||kind=="visit_message_reply_collected";const bool inquiry=kind=="contact_inquiry_started"||kind=="contact_inquiry_finished";const bool visit_invitation=kind=="visit_appointment_invitation_offered"||kind=="visit_appointment_invitation_collected"||kind=="visit_appointment_invitation_accepted";const bool visit_meeting=kind=="visit_appointment_meeting_started"||kind=="visit_appointment_meeting_cancelled"||kind=="visit_appointment_meeting_finished";const bool shop_contact=kind=="task_shop_order_contact_started";const bool collateral_recognition=kind=="family_collateral_recognized";if(collateral_recognition)fields(row.second,{"kind","domain","source"},{},row.first);const bool known_dialogue=kind=="known_personal_dialogue_opened";const bool born_dialogue=kind=="born_personal_dialogue_opened";const bool appointment=kind=="personal_appointment_accepted"||kind=="personal_appointment_cancelled"||kind=="personal_appointment_started"||kind=="personal_appointment_finished";const bool liquidation=(kind=="private_shop_executor_appointed"||kind=="private_shop_executor_accepted"||kind=="private_shop_executor_revoked"||kind=="private_shop_estate_closed");const bool estate_order=kind=="task_shop_order_estate_cancelled";const bool tactic_learning=kind=="military_tactic_learned"||kind=="military_tactic_practice_completed";if(tactic_learning)fields(row.second,{"kind"},{"domain"},row.first);const bool personal_cancellation=kind=="task_personal_message_cancelled";const bool personal_acceptance=kind=="task_personal_acceptance_started"||kind=="task_personal_acceptance_completed"||kind=="task_personal_acceptance_interrupted";const bool personal_report=kind=="task_personal_report_started"||kind=="task_personal_report_completed"||kind=="task_personal_report_interrupted";const bool personal_message=kind=="task_personal_message_started"||kind=="task_personal_message_delivered"||kind=="task_personal_message_interrupted";const bool message_return=kind=="task_message_return_started"||kind=="task_message_return_collected"||kind=="task_message_return_interrupted"||kind=="task_message_return_report_started"||kind=="task_message_return_report_interrupted"||kind=="task_message_return_delivered";const bool voluntary_path_cancel=kind=="family_voluntary_path_cancelled";if(voluntary_path_cancel&&row.first.rfind("voluntary_path:",0)!=0)fail("InvalidFactDomain",row.first);std::string_view domain=original_social?"social":personal_acceptance?"task_personal_message_acceptances":political_subordinate_appointment?"political_subordinate_appointments":personal_cancellation?"task_personal_message_cancellations":personal_report?"task_personal_message_reports":political_office_resignation?"political_office_resignations":personal_message?"task_personal_message_deliveries":voluntary_path_cancel?"family_voluntary_paths":message_return?"task_message_returns":political_governance_agency?"political_governance_agencies":political_office_reception?"political_office_receptions":shop_contact?"task_shop_order_contacts":collateral_recognition?"family_collateral_legacy":political_term_handover?"political_term_handovers":political_successor_round?"political_successor_rounds":known_dialogue?"known_personal_dialogues":political_head_handover?"political_head_handovers":political_succession?"political_successions":visit_invitation?"visit_appointment_invitations":visit_meeting?"visit_appointment_meetings":tactic_learning?"military_tactics":daily_rent?"private_shop_daily_rent":invitation_reply?"political_governance_invitation_replies":born_dialogue?"born_personal_dialogues":kind=="political_governance_continued"?"political_governance_continuations":apprenticeship?"family_apprenticeship":visit_reply?"visit_message_replies":inquiry?"contact_inquiries":liquidation?"private_shop_liquidations":estate_order?"task_shop_order_estate_cancellations":(kind=="personal_recovery_credit"||kind=="personal_recovery_closed")?"personal_injury_recovery":appointment?"personal_appointments":kind=="political_shop_registry_accepted"?"political_shop_registry_successions":kind=="task_deadline_extended"?"task_extensions":(kind=="private_shop_opened"||kind=="private_shop_capital_added"||kind=="private_shop_markup_changed"||kind=="private_shop_stock_received"||kind=="private_shop_operating_started"||kind=="private_shop_operating_completed"||kind=="private_shop_operating_interrupted"||kind=="private_shop_staff_accepted"||kind=="private_shop_staff_ended"||kind=="private_shop_staff_served"||kind=="private_shop_order_issued"||kind=="private_shop_order_bound"||kind=="private_shop_order_settled"||kind=="private_shop_closed")?"private_shops":(kind=="political_shop_registry_initialized"||kind=="political_wanted_registered"||kind=="political_wanted_released"||kind=="political_shop_clearance_recorded"||kind=="political_shop_permit_issued"||kind=="political_shop_permit_revoked")?"political_shop_licensing":(kind=="task_shop_order_accepted"||kind=="task_shop_order_settled"||kind=="task_shop_order_interrupted")?"task_shop_orders":(kind=="growth_market_movement")?"growth_market_history":kind=="visit_reception_claimed"?"visit_reception_claims":(kind=="visit_message_posted"||kind=="visit_message_received"||kind=="visit_message_withdrawn")?"visit_messages":kind=="life_activity"?"growth":kind=="personal_battle"?"personal":(kind=="training"||kind=="skill_study")?"cultivation":kind=="long_rest"?"services":kind=="trade_bargain"?"economy":(kind=="economic_supply_policy_issued"||kind=="economic_supply_observed")?"economic_supply_policies":(kind=="estate_employment_offered"||kind=="estate_employment_decided"||kind=="estate_employment_service")?"estate_employment":(kind=="family_care_relief_accepted"||kind=="family_care_relief_started"||kind=="family_care_relief_provided"||kind=="family_care_relief_closed")?"family_care_relief":(kind=="family_birth_proposed"||kind=="family_birth_replied"||kind=="family_birth_accepted"||kind=="family_birth_closed"||kind=="family_birth_recorded"||kind=="family_child_care_started"||kind=="family_child_care"||kind=="family_child_matured")?"family_birth":(kind=="family_marriage_proposed"||kind=="family_marriage_replied"||kind=="family_marriage_dissolved")?"family_marriage":(kind=="family_bequest_disclosed"||kind=="family_bequest_accepted"||kind=="family_inheritance_settled")?"family_inheritance":(kind=="family_bequest_declaration"||kind=="family_bequest_revocation")?"family_bequest":(kind=="family_legacy_control_consent"||kind=="family_legacy_control_revocation"||kind=="family_legacy_control_activated")?"family_legacy":(kind=="tactic_lesson_started"||kind=="tactic_lesson_credit"||kind=="tactic_lesson_closed")?"growth_tactic_lessons":kind=="family_death"?"family_life":(kind=="actor_creation"||kind.rfind("family_",0)==0)?"family":(kind=="escort_arrival"||kind=="task_static_asset_transfer"||kind=="task_death_notice_bound"||kind=="task_message_handoff"||kind=="task_message_report"||kind=="task_static_payment"||kind=="task_contract_payment"||kind=="task_transfer"||kind=="task_delivery"||kind=="task_cargo_disposition"||kind=="task_personal_advance_paid"||kind=="task_personal_advance_repaid"||kind=="governance_task_accepted"||kind=="governance_work_completed"||kind=="governance_work_interrupted"||kind=="governance_task_completed"||kind=="governance_task_cancelled"||kind=="governance_task_failed"||kind=="envoy_task_accepted"||kind=="envoy_handover_interrupted"||kind=="envoy_task_delivered"||kind=="envoy_task_completed"||kind=="envoy_task_cancelled"||kind=="envoy_task_failed")?"tasks":kind=="terrain_journey"?"travel":(kind.rfind("estate_",0)==0||kind=="public_estate_transfer")?"estate":kind.rfind("military_",0)==0||kind.rfind("campaign_",0)==0||kind=="office_resignation"||kind=="siege_material"||kind=="siege_integration_proposal"||kind=="siege_integration_actor_action"||kind=="siege_officer_decision"?"military":"interaction";if(auto* declared=row.second.find("domain")){if(s.metadata.ruleset_id!=dynamic_ruleset_id)fail("UnexpectedFactDomain",row.first);const auto& named=string_ref(*declared,{row.first});if(voluntary_path_cancel!=(named=="family_voluntary_paths"))fail("InvalidFactDomain",row.first);if(personal_cancellation&&named!="task_personal_message_cancellations")fail("InvalidFactDomain",row.first);if(personal_acceptance&&named!="task_personal_message_acceptances")fail("InvalidFactDomain",row.first);if(personal_report&&named!="task_personal_message_reports")fail("InvalidFactDomain",row.first);if(personal_message&&named!="task_personal_message_deliveries")fail("InvalidFactDomain",row.first);if(message_return&&named!="task_message_returns")fail("InvalidFactDomain",row.first);if(political_head_handover!=(named=="political_head_handovers"))fail("InvalidFactDomain",row.first);if(political_term_handover!=(named=="political_term_handovers"))fail("InvalidFactDomain",row.first);if(political_office_reception!=(named=="political_office_receptions"))fail("InvalidFactDomain",row.first);if(political_office_resignation!=(named=="political_office_resignations"))fail("InvalidFactDomain",row.first);if(political_subordinate_appointment!=(named=="political_subordinate_appointments"))fail("InvalidFactDomain",row.first);if(political_governance_agency!=(named=="political_governance_agencies"))fail("InvalidFactDomain",row.first);if(political_successor_round!=(named=="political_successor_rounds"))fail("InvalidFactDomain",row.first);if(political_succession!=(named=="political_successions"))fail("InvalidFactDomain",row.first);if(daily_rent!=(named=="private_shop_daily_rent"))fail("InvalidFactDomain",row.first);if(invitation_reply!=(named=="political_governance_invitation_replies"))fail("InvalidFactDomain",row.first);if((kind=="political_governance_continued")!=(named=="political_governance_continuations"))fail("InvalidFactDomain",row.first);if(apprenticeship!=(named=="family_apprenticeship"))fail("InvalidFactDomain",row.first);if(liquidation!=(named=="private_shop_liquidations")||estate_order!=(named=="task_shop_order_estate_cancellations"))fail("InvalidFactDomain",row.first);if((kind=="personal_recovery_credit"||kind=="personal_recovery_closed")!=(named=="personal_injury_recovery"))fail("InvalidFactDomain",row.first);if(visit_reply&&named!="visit_message_replies")fail("InvalidFactDomain",row.first);if(inquiry&&named!="contact_inquiries")fail("InvalidFactDomain",row.first);if(visit_invitation&&named!="visit_appointment_invitations")fail("InvalidFactDomain",row.first);if(visit_meeting&&named!="visit_appointment_meetings")fail("InvalidFactDomain",row.first);if(shop_contact&&named!="task_shop_order_contacts")fail("InvalidFactDomain",row.first);if(known_dialogue&&named!="known_personal_dialogues")fail("InvalidFactDomain",row.first);if(born_dialogue&&named!="born_personal_dialogues")fail("InvalidFactDomain",row.first);if(appointment&&named!="personal_appointments")fail("InvalidFactDomain",row.first);if(kind=="political_shop_registry_accepted"&&named!="political_shop_registry_successions")fail("InvalidFactDomain",row.first);if((kind=="private_shop_opened"||kind=="private_shop_capital_added"||kind=="private_shop_markup_changed"||kind=="private_shop_stock_received"||kind=="private_shop_operating_started"||kind=="private_shop_operating_completed"||kind=="private_shop_operating_interrupted"||kind=="private_shop_staff_accepted"||kind=="private_shop_staff_ended"||kind=="private_shop_staff_served"||kind=="private_shop_order_issued"||kind=="private_shop_order_bound"||kind=="private_shop_order_settled"||kind=="private_shop_closed")&&named!="private_shops")fail("InvalidFactDomain",row.first);if((kind=="political_shop_registry_initialized"||kind=="political_wanted_registered"||kind=="political_wanted_released"||kind=="political_shop_clearance_recorded"||kind=="political_shop_permit_issued"||kind=="political_shop_permit_revoked")&&named!="political_shop_licensing")fail("InvalidFactDomain",row.first);if((kind=="task_shop_order_accepted"||kind=="task_shop_order_settled"||kind=="task_shop_order_interrupted")&&named!="task_shop_orders")fail("InvalidFactDomain",row.first);if((kind=="growth_market_movement")&&named!="growth_market_history")fail("InvalidFactDomain",row.first);if(kind=="task_deadline_extended"&&named!="task_extensions")fail("InvalidFactDomain",row.first);if(kind=="visit_reception_claimed"&&named!="visit_reception_claims")fail("InvalidFactDomain",row.first);if((kind=="family_care_relief_accepted"||kind=="family_care_relief_started"||kind=="family_care_relief_provided"||kind=="family_care_relief_closed")&&named!="family_care_relief")fail("InvalidFactDomain",row.first);if((kind=="visit_message_posted"||kind=="visit_message_received"||kind=="visit_message_withdrawn")&&named!="visit_messages")fail("InvalidFactDomain",row.first);if((kind=="family_birth_proposed"||kind=="family_birth_replied"||kind=="family_birth_accepted"||kind=="family_birth_closed"||kind=="family_birth_recorded"||kind=="family_child_care_started"||kind=="family_child_care"||kind=="family_child_matured")&&named!="family_birth")fail("InvalidFactDomain",row.first);if(named=="family_birth"&&!catalog.family_birth_identity_enabled)fail("BirthProfileRequired",row.first);if((kind=="economic_supply_policy_issued"||kind=="economic_supply_observed")&&named!="economic_supply_policies")fail("InvalidFactDomain",row.first);if((kind=="estate_employment_offered"||kind=="estate_employment_decided"||kind=="estate_employment_service")&&named!="estate_employment")fail("InvalidFactDomain",row.first);if((kind=="family_marriage_proposed"||kind=="family_marriage_replied"||kind=="family_marriage_dissolved")&&named!="family_marriage")fail("InvalidFactDomain",row.first);if((kind=="family_bequest_disclosed"||kind=="family_bequest_accepted"||kind=="family_inheritance_settled")&&named!="family_inheritance")fail("InvalidFactDomain",row.first);if((kind=="family_bequest_declaration"||kind=="family_bequest_revocation")&&named!="family_bequest")fail("InvalidFactDomain",row.first);if(kind=="family_death"&&named!="family_life")fail("InvalidFactDomain",row.first);if((kind=="family_legacy_control_consent"||kind=="family_legacy_control_revocation"||kind=="family_legacy_control_activated")&&named!="family_legacy")fail("InvalidFactDomain",row.first);if(collateral_recognition&&named!="family_collateral_legacy")fail("InvalidFactDomain",row.first);if(!(political_original_header&&named=="politics")&&!(growth_terminal_header&&named=="growth")&&!(travel_owned_header&&named=="travel")&&!(transport_physical_header&&owns_transport_physical_fact_kind(named,kind))&&!(named=="political_subordinate_appointments"&&political_subordinate_appointment)&&!(named=="task_personal_message_cancellations"&&personal_cancellation)&&!(named=="task_personal_message_acceptances"&&personal_acceptance)&&!(named=="task_personal_message_reports"&&personal_report)&&!(named=="political_office_resignations"&&political_office_resignation)&&!(named=="family_voluntary_paths"&&voluntary_path_cancel)&&!(named=="family_collateral_legacy"&&collateral_recognition)&&!(named=="political_office_receptions"&&political_office_reception)&&!(named=="political_governance_agencies"&&political_governance_agency)&&!(named=="political_term_handovers"&&political_term_handover)&&!(named=="political_successor_rounds"&&political_successor_round)&&!(named=="political_head_handovers"&&political_head_handover)&&!(named=="political_successions"&&political_succession)&&!(named=="private_shop_daily_rent"&&daily_rent)&&!(named=="political_governance_invitation_replies"&&invitation_reply)&&!(named=="political_governance_continuations"&&kind=="political_governance_continued")&&!(named=="family_apprenticeship"&&apprenticeship)&&!(named=="private_shop_liquidations"&&liquidation)&&!(named=="task_shop_order_estate_cancellations"&&estate_order)&&!(named=="task_extensions"&&kind=="task_deadline_extended")&&!(named=="political_shop_registry_successions"&&kind=="political_shop_registry_accepted")&&!(named=="visit_message_replies"&&visit_reply)&&!(named=="contact_inquiries"&&inquiry)&&!(named=="visit_appointment_invitations"&&visit_invitation)&&!(named=="visit_appointment_meetings"&&visit_meeting)&&!(named=="task_personal_message_deliveries"&&personal_message)&&!(named=="task_message_returns"&&message_return)&&!(named=="task_shop_order_contacts"&&shop_contact)&&!(named=="known_personal_dialogues"&&known_dialogue)&&!(named=="born_personal_dialogues"&&born_dialogue)&&!(named=="personal_appointments"&&appointment)&&!(named=="personal_injury_recovery"&&(kind=="personal_recovery_credit"||kind=="personal_recovery_closed"))&&!(named=="private_shops"&&(kind=="private_shop_opened"||kind=="private_shop_capital_added"||kind=="private_shop_markup_changed"||kind=="private_shop_stock_received"||kind=="private_shop_operating_started"||kind=="private_shop_operating_completed"||kind=="private_shop_operating_interrupted"||kind=="private_shop_staff_accepted"||kind=="private_shop_staff_ended"||kind=="private_shop_staff_served"||kind=="private_shop_order_issued"||kind=="private_shop_order_bound"||kind=="private_shop_order_settled"||kind=="private_shop_closed"))&&!(named=="political_shop_licensing"&&(kind=="political_shop_registry_initialized"||kind=="political_wanted_registered"||kind=="political_wanted_released"||kind=="political_shop_clearance_recorded"||kind=="political_shop_permit_issued"||kind=="political_shop_permit_revoked"))&&!(named=="task_shop_orders"&&(kind=="task_shop_order_accepted"||kind=="task_shop_order_settled"||kind=="task_shop_order_interrupted"))&&!(named=="growth_market_history"&&(kind=="growth_market_movement"))&&!(named=="visit_reception_claims"&&kind=="visit_reception_claimed")&&!(named=="family_care_relief"&&(kind=="family_care_relief_accepted"||kind=="family_care_relief_started"||kind=="family_care_relief_provided"||kind=="family_care_relief_closed"))&&!(named=="visit_messages"&&(kind=="visit_message_posted"||kind=="visit_message_received"||kind=="visit_message_withdrawn"))&&!(named=="family_birth"&&((kind=="family_birth_proposed"||kind=="family_birth_replied"||kind=="family_birth_accepted"||kind=="family_birth_closed"||kind=="family_birth_recorded"||kind=="family_child_care_started"||kind=="family_child_care"||kind=="family_child_matured")||kind=="actor_creation"))&&!(named=="economic_supply_policies"&&(kind=="economic_supply_policy_issued"||kind=="economic_supply_observed"))&&!(named=="estate_employment"&&(kind=="estate_employment_offered"||kind=="estate_employment_decided"||kind=="estate_employment_service"))&&!(named=="family_marriage"&&(kind=="family_marriage_proposed"||kind=="family_marriage_replied"||kind=="family_marriage_dissolved"))&&!(named=="family_inheritance"&&(kind=="family_bequest_disclosed"||kind=="family_bequest_accepted"||kind=="family_inheritance_settled"))&&!(named=="family_bequest"&&(kind=="family_bequest_declaration"||kind=="family_bequest_revocation"))&&!(named=="family_legacy"&&(kind=="family_legacy_control_consent"||kind=="family_legacy_control_revocation"||kind=="family_legacy_control_activated"))&&!(named=="growth_tactic_lessons"&&(kind=="tactic_lesson_started"||kind=="tactic_lesson_credit"||kind=="tactic_lesson_closed"))&&!(named=="family_life"&&kind=="family_death")&&!(named=="politics"&&(kind=="diplomatic_offer_proposed"||kind=="diplomatic_mission_authorized"||kind=="diplomatic_offer_presented"||kind=="diplomatic_offer_replied"||kind=="diplomatic_mission_communicated"||kind=="diplomatic_envoy_report_received"||kind=="political_fiscal_policy_changed"||kind=="political_field_authorized"||kind=="political_field_accepted"||kind=="political_field_declined"||kind=="political_field_withdrawn"||kind=="political_military_officer_authorized"||kind=="political_military_officer_accepted"||kind=="political_military_officer_declined"||kind=="political_military_officer_withdrawn"||kind=="treaty_passage_document_presented"||kind=="treaty_passage_signed"||kind=="political_occupation_fiscal_policy_changed"||kind=="governance_authority_issued"||kind=="governance_outcome_plan"||kind=="political_civil_registered"||kind=="political_civil_consequence"||kind=="political_public_responsibilities_pending"||kind=="political_public_responsibilities_assumed"||kind=="political_public_responsibility_decided"))&&!(named=="military_tactics"&&tactic_learning)&&!(named=="military_naval"&&(kind=="naval_form"||kind=="naval_disband"||kind=="naval_contact"||kind=="naval_battle"||kind=="naval_plan"||kind=="naval_round"||kind=="naval_capture"||kind=="naval_decline"||kind=="naval_continue"||kind=="naval_repair_start"||kind=="naval_repair_result"||kind=="naval_voyage"||kind=="naval_navigation_step"||kind=="naval_gunnery_preparation"))&&!(named=="interaction"&&kind=="actor_social_occupancy_changed")&&!(named=="actor_foundation"&&condition_application_header)&&!(named=="social"&&original_social)&&named!="local_economy"&&named!="organization"&&named!="population_military"&&!(named=="military"&&(field_battle_header||exact_military_header||exact_action_header||kind=="siege_integration_proposal"||kind=="siege_integration_actor_action"||kind=="siege_officer_decision"||kind=="military_integration"))&&!(named=="tasks"&&(task_exact||task_work||kind=="task_static_asset_transfer"||kind=="task_death_notice_bound"||kind=="task_message_handoff"||kind=="task_message_report"||kind=="task_static_payment"||kind=="task_contract_payment"||kind=="task_transfer"||kind=="task_delivery"||kind=="task_cargo_disposition"||kind=="task_personal_advance_paid"||kind=="task_personal_advance_repaid"||kind=="governance_task_accepted"||kind=="governance_work_completed"||kind=="governance_work_interrupted"||kind=="governance_task_completed"||kind=="governance_task_cancelled"||kind=="governance_task_failed"||kind=="envoy_task_accepted"||kind=="envoy_handover_interrupted"||kind=="envoy_task_delivered"||kind=="envoy_task_completed"||kind=="envoy_task_cancelled"||kind=="envoy_task_failed")))fail("InvalidFactDomain",row.first);domain=named;}decoded.insert(row.first,{checkpoint_decode_boundary_string(row.first),checkpoint_decode_boundary_string(kind),checkpoint_decode_boundary_text(domain),order(row.second)});}
 decoded.publish(s.facts);
}
{
 const auto& input=object(field(source,"rng",""),"rng");CheckpointCatalogRows<RngStream> decoded(input.size(),"rng");
 for(auto& row:input){fields(row.second,{"seed","state"},{},row.first);decoded.insert(row.first,{decimal(field(row.second,"seed",row.first),row.first),decimal(field(row.second,"state",row.first),row.first),order(row.second)});}
 decoded.publish(s.rng);
}
{
 const auto& input=object(field(source,"request_sequences",""),"request_sequences");CheckpointCatalogRows<Integer> decoded(input.size(),"request_sequences");
 for(auto& row:input)decoded.insert(row.first,integer(row.second,row.first));
 decoded.publish(s.request_sequences);
}
{
 const auto& input=object(field(source,"results",""),"results");CheckpointCatalogRows<std::shared_ptr<const CommitResult>> decoded(input.size(),"results");
 for(auto& row:input){if(!result_codec)return {false,{},common_failure("ResultCodecRequired","results",ErrorKind::unsupported_capability).error};auto result=result_codec->decode(row.second);if(!result.ok)return {false,{},result.error};if(!result.value)return {false,{},common_failure("InvalidCachedResult","results/"+row.first).error};decoded.insert(row.first,std::move(result.value));}
 decoded.publish(s.results);
}
const auto& history=array(field(source,"history",""),"history");checkpoint_decode_boundary_array(history.size(),sizeof(HistoryRecord));s.history.reserve(history.size());for(auto& v:history){fields(v,{"tick","kind","actor_id","result_id"},{},"history");s.history.push_back({get_int(v,"tick","history"),get_string(v,"kind","history"),get_string(v,"actor_id","history"),get_string(v,"result_id","history"),order(v)});}
if(max_instance(source)>=s.next_instance_id)return {false,{},common_failure("InstanceCounterCollision").error};
if(validation==CommonSeedValidation::standalone){CommonStore candidate(s,std::make_shared<const CommonCatalog>(catalog));auto status=candidate.validate_common();if(!status.ok)return {false,{},status.error};}return {true,std::move(s),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_common_wallet_frame_fields(ProjectionWriter& fields,const WalletView& wallet,bool typed,const CommonResourceCodecContext& resources){
    fields.field("id",wallet.id);emit_common_owner(fields,"owner",wallet.owner);fields.field("balance",wallet.balance);
    if(typed)fields.sequence("reserved",wallet.reserved,[&](auto& held,const auto& r){return held.object("",[&](auto& v){emit_common_reservation(v,"reference",r.action_id,true,resources.domain_activity_reservation_refs_enabled);return v.field("amount",r.amount);});});
    else fields.object("reserved",[&](auto& held){for(const auto& r:wallet.reserved){if(r.action_id.kind!=ReservationRef::Kind::action)return held.reject("UnsupportedReservationKind",r.action_id.id);held.field(r.action_id.id,r.amount);}return held.status();});if(!fields.status().ok)return fields.status();return fields.order_fields(wallet.field_order);
}
Status emit_common_inventory_frame_fields(ProjectionWriter& fields,const InventoryView& inventory,bool typed,const CommonResourceCodecContext& resources){
    fields.field("id",inventory.id);emit_common_owner(fields,"owner",inventory.owner);fields.field("capacity",inventory.capacity);fields.field("revision",inventory.revision);auto emitted=fields.sequence("lots",inventory.lots,[&](auto& item,const auto& lot){return emit_common_lot(item,"",lot,typed,resources);});if(!emitted.ok)return emitted;return fields.order_fields(inventory.field_order);
}
Status emit_common_wallet_frame(ProjectionWriter& out,std::string_view name,const WalletView& wallet,bool typed,const CommonResourceCodecContext& resources){
    return out.object(name,wallet.field_order,[&](auto& fields){return emit_common_wallet_frame_fields(fields,wallet,typed,resources);});
}
Status emit_common_inventory_frame(ProjectionWriter& out,std::string_view name,const InventoryView& inventory,bool typed,const CommonResourceCodecContext& resources){
    return out.object(name,inventory.field_order,[&](auto& fields){return emit_common_inventory_frame_fields(fields,inventory,typed,resources);});
}
// 只导出共同拥有字段；action.params、fact payload和results由外层调用各唯一owner补齐。
// 同一原字段发射供JSON和无树形状核验调用，领域payload仍由世界组合口补齐。
Status emit_common_state_fields(ProjectionWriter& w,const CommonState& s,const CommonResultCodec* result_codec,const CommonResourceCodecContext& resources,const CommonProjectionPayloads* payloads){
    auto strings=[](ProjectionWriter& out,std::string_view name,const auto& values){return out.sequence(name,values,[](auto& child,const auto& value){return child.field("",value);});};
    const bool typed=s.metadata.ruleset_id=="hanmo.native.local_society/1";
    w.field("ruleset_id",s.metadata.ruleset_id);w.field("content_id",s.metadata.content_id);w.field("content_version",s.metadata.content_version);w.field("content_digest",s.metadata.content_digest);
    w.field("engine_build",s.metadata.engine_build);w.field("rng_backend",s.metadata.rng_backend);w.field("scenario_id",s.metadata.scenario_id);w.field("player_actor_id",s.metadata.player_actor_id);
    w.field("tick",s.identity.tick);w.field("revision",s.identity.revision);w.field("next_instance_id",s.next_instance_id);
    w.object("actors",[&](auto& out){for(const auto& row:s.actors.rows()){
        const auto& a=row.second;out.object(row.first,a.field_order,[&](auto& actor){
            actor.field("id",a.id);emit_common_location(actor,"location",a.location,a.location_order,resources.carrier_location_enabled);
            actor.field("alive",a.alive);actor.field("available",a.available);actor.field("hp",a.hp);actor.field("fatigue",a.fatigue);actor.field("merit",a.merit);
            actor.object("attributes",a.attributes_order,[&](auto& fields){fields.field("administration",a.attributes.administration);fields.field("charisma",a.attributes.charisma);fields.field("intellect",a.attributes.intellect);fields.field("leadership",a.attributes.leadership);return fields.field("martial",a.attributes.martial);});
            actor.object("skills",a.skills_order,[&](auto& fields){
                fields.field("archery",a.skills.archery);fields.field("cavalry",a.skills.cavalry);fields.field("commerce",a.skills.commerce);fields.field("debate",a.skills.debate);fields.field("infantry",a.skills.infantry);fields.field("martial",a.skills.martial);fields.field("negotiation",a.skills.negotiation);fields.field("rites",a.skills.rites);fields.field("tactics",a.skills.tactics);fields.field("transport",a.skills.transport);
                if(a.skills.extended){const auto& e=*a.skills.extended;fields.object("extended",e.field_order,[&](auto& x){x.field("naval",e.naval);x.field("espionage",e.espionage);x.field("fortification",e.fortification);x.field("agriculture",e.agriculture);x.field("medicine",e.medicine);return x.field("forging",e.forging);});}return fields.status();
            });
            actor.field("wallet_id",a.wallet_id);actor.field("inventory_id",a.inventory_id);actor.field("action_id",a.action_id);
            if(a.appearance){const auto& ap=*a.appearance;actor.object("appearance",ap.field_order,[&](auto& appearance){
                appearance.field("format_version",ap.format_version);appearance.field("generator_version",ap.generator_version);appearance.field("generation_seed",ap.generation_seed);
                // absent base_features 原输出空object且不应用其未使用field_order。
                static const FieldOrder empty;appearance.object("base_features",ap.base_features.present?ap.base_features.field_order:empty,[&](auto& features){if(ap.base_features.present){const auto& b=ap.base_features;features.field("face_shape",b.face_shape);features.field("eyes",b.eyes);features.field("brows",b.brows);features.field("nose",b.nose);features.field("mouth",b.mouth);features.field("hair",b.hair);features.field("beard",b.beard);features.field("skin_palette",b.skin_palette);features.field("build",b.build);}return features.status();});
                appearance.field("attire_style",ap.attire_style);appearance.field("source",ap.source);return strings(appearance,"parent_sources",ap.parent_sources);
            });}
            actor.field("injury",a.injury);if(a.birth_tick)actor.field("birth_tick",*a.birth_tick);if(a.created_tick)actor.field("created_tick",*a.created_tick);if(a.prototype_id)actor.field("prototype_id",*a.prototype_id);if(a.display_name)actor.field("display_name",*a.display_name);
            actor.object("combat",a.combat_order,[&](auto& combat){combat.field("weapon_id",a.combat.weapon_id);combat.object("learned",[&](auto& learned){for(const auto& r:a.combat.learned.rows())learned.field(r.first,r.second);return learned.status();});combat.field("heart_id",a.combat.heart_id);if(a.combat.armor_id)combat.field("armor_id",*a.combat.armor_id);return combat.status();});return actor.status();
        });if(!out.status().ok)break;}return out.status();});
    w.object("parties",[&](auto& out){for(const auto& row:s.parties.rows()){const auto& p=row.second;out.object(row.first,p.field_order,[&](auto& party){party.field("id",p.id);party.field("leader_id",p.leader_id);strings(party,"members",p.members);party.object("terms",[&](auto& terms){for(const auto& r:p.terms.rows())terms.object(r.first,r.second.field_order,[&](auto& term){term.field("role_id",r.second.role_id);return term.field("cost_per_tick",r.second.cost_per_tick);});return terms.status();});return party.field("created_tick",p.created_tick);});if(!out.status().ok)break;}return out.status();});
    w.object("roles",[&](auto& out){for(const auto& row:s.roles.rows())out.object(row.first,row.second.field_order,[&](auto& role){role.field("actor_id",row.second.actor_id);return role.field("enabled",row.second.enabled);});return out.status();});
    w.object("wallets",[&](auto& out){for(const auto& row:s.wallets.rows()){auto status=emit_common_wallet_frame(out,row.first,row.second,typed,resources);if(!status.ok)return status;}return out.status();});
    w.object("inventories",[&](auto& out){for(const auto& row:s.inventories.rows()){auto status=emit_common_inventory_frame(out,row.first,row.second,typed,resources);if(!status.ok)return status;}return out.status();});
    w.object("actions",[&](auto& out){for(const auto& row:s.actions.rows()){
        const auto& action=row.second;
        auto status=out.object(row.first,[&](auto& fields){
            fields.field("id",action.id);fields.field("kind",action.kind);fields.field("actor_id",action.actor_id);
            strings(fields,"participants",action.participants);fields.field("remaining_ticks",action.remaining_ticks);fields.field("start_tick",action.start_tick);
            if(payloads){auto emitted=payloads->emit_action_fields(fields,action);if(!emitted.ok)return emitted;
                if(const auto* order=s.action_field_order.find(row.first))return fields.order_fields(*order);}
            return fields.status();
        });if(!status.ok)return status;
    }return out.status();});
    w.object("facts",[&](auto& out){for(const auto& row:s.facts.rows()){
        auto status=payloads?payloads->emit_fact_record(out,row.first,row.second):out.object(row.first,[&](auto& fields){return fields.field("kind",row.second.kind);});
        if(!status.ok)return status;
    }return out.status();});
    w.object("rng",[&](auto& out){for(const auto& row:s.rng.rows())out.object(row.first,row.second.field_order,[&](auto& fields){
        // 原十进制字符串在栈上形成，shape核验不为两个临时字符串分配堆空间。
        char seed[32],state[32];auto a=std::to_chars(seed,seed+sizeof(seed),row.second.seed);auto b=std::to_chars(state,state+sizeof(state),row.second.state);if(a.ec!=std::errc{}||b.ec!=std::errc{})return fields.reject("InvalidRandomState");fields.text("seed",{seed,static_cast<std::size_t>(a.ptr-seed)});return fields.text("state",{state,static_cast<std::size_t>(b.ptr-state)});
    });return out.status();});
    w.object("request_sequences",[&](auto& out){for(const auto& row:s.request_sequences.rows())out.field(row.first,row.second);return out.status();});
    w.sequence("history",s.history,[](auto& out,const auto& h){return out.object("",h.field_order,[&](auto& fields){fields.field("tick",h.tick);fields.field("kind",h.kind);fields.field("actor_id",h.actor_id);return fields.field("result_id",h.result_id);});});
    if(result_codec)w.object("results",[&](auto& out){for(const auto& row:s.results.rows()){if(!row.second)return out.reject("InvalidCachedResult",row.first);auto status=result_codec->emit(out,row.first,*row.second);if(!status.ok)return status;}return out.status();});
    return w.status();
}
// 两种sink消费完全同一字段路径；失败只销毁本地未发布输出。
Status emit_common_state(ProjectionWriter& writer,const StableId& name,const CommonState& state,const CommonResultCodec* results,const CommonResourceCodecContext& resources,const CommonProjectionPayloads* payloads){return writer.object(name,[&](auto& fields){return emit_common_state_fields(fields,state,results,resources,payloads);});}
BoundaryValue encode_common_state(const CommonState& state,const CommonResultCodec* results,const CommonResourceCodecContext& resources,const CommonProjectionPayloads* payloads){BoundaryValue output;BoundaryProjectionWriter writer(output);auto status=emit_common_state(writer,"",state,results,resources,payloads);if(status.ok)status=writer.finish();if(!status.ok)boundary::fail(status.error.original.code,status.error.original.path);return output;}
} // 完整边界由中央按全局事实/action序合成，不能把部分导出当完整世界。
