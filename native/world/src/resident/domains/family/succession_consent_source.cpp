#include "resident/kernel/catalog_section_observation.hpp"
// 接续同意只沿真实会话、已有成年人物与原收养来源形成；不会从好感或同姓补造接受。
#include "succession_consent_source.hpp"
#include "family_model.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
constexpr Integer maximum_tick=2147483647,adult_ticks=18*720;
// 所有永久引用有明确有限长度，避免无界作者键进入来源账。
bool valid_id(const StableId& id){return !id.empty()&&id.size()<=256;}
// 原会话只引用当前目录中已选择的对白，不扫描未准入的内容包。
const DialogueDefinition* dialogue_by_id(const InteractionCatalog& catalog,const StableId& id){for(const auto& d:catalog.dialogues)if(d.id==id)return &d;return nullptr;}
// 节点ID不能以正文相同代替，重复文案不扩大本人接受范围。
const DialogueNode* node_by_id(const DialogueDefinition& dialogue,const StableId& id){for(const auto& node:dialogue.nodes)if(node.id==id)return &node;return nullptr;}
// 冷条款保当时成年人门槛，不以当前年龄掩盖早于成年时的非法接续。
Status adult_at(const CommonStore& common,const StableId& id,Integer tick){const auto* actor=common.state().actors.find(id);if(!actor)return common_failure("UnknownActor",id);if(tick<0||tick>common.tick()||tick>maximum_tick||!actor->birth_tick||*actor->birth_tick< -maximum_tick||*actor->birth_tick>tick)return common_failure("ActorAgeUnknown",id);return tick-*actor->birth_tick>=adult_ticks?common_success():common_failure("SuccessionRequiresAdult",id);}
// 数量比较忽略品类顺序但不容重复品类；原批次仍由私产报价精确绑定。
bool selection_matches(const SuccessionPersonalSelection& selected,const SuccessionPersonalTerms& terms){
    if(selected.money!=terms.money)return false;OrderedTable<Integer> totals;
    for(const auto& lot:terms.lots){auto* old=totals.find(lot.item_id);if(lot.quantity<=0||(old&&*old>std::numeric_limits<Integer>::max()-lot.quantity))return false;totals.set(lot.item_id,(old?*old:0)+lot.quantity);}
    if(totals.size()!=selected.goods.size())return false;std::set<StableId> seen;for(const auto& good:selected.goods){const auto* quantity=totals.find(good.item_id);if(!quantity||good.quantity<=0||*quantity!=good.quantity||!seen.insert(good.item_id).second)return false;}return true;
}
// 历史lot号须已真实分配，不接受未来实例、负值或不规范写法。
bool allocated_lot(const StableId& id,Integer next){if(id.rfind("lot:",0)!=0)return false;const auto suffix=id.substr(4);Integer value=0;const auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==suffix.data()+suffix.size()&&value>0&&value<next&&std::to_string(value)==suffix;}
// 场所/岗位/提供人是实际接触来源；路遇和错误上下文不提供家事办理许可。
bool context_equal(const InteractionContext& a,const InteractionContext& b){return std::tie(a.place_id,a.provider_actor_id,a.role_id,a.kind,a.encounter_id,a.input_error)==std::tie(b.place_id,b.provider_actor_id,b.role_id,b.kind,b.encounter_id,b.input_error);}
}
// 关系来源必须对应同一人原成功收养，不以children索引单独授权。
Outcome<StableId> succession_adoption_basis(const CommonStore& common,const FamilyState& family,const StableId& predecessor,const StableId& successor,Integer tick){
    const auto* id=family.children.find(successor);const auto* fact=id?family.facts.find(*id):nullptr;const auto* head=id?common.state().facts.find(*id):nullptr;
    if(!id||!fact||!head||head->domain!="family")return {false,{},common_failure("SuccessionRelationshipRequired",successor).error};
    bool valid=false;
    if(const auto* adopted=std::get_if<AdoptionFact>(fact))valid=adopted->kind=="family_adoption"&&head->kind==adopted->kind&&adopted->success&&adopted->guardian_actor_id==predecessor&&adopted->child_actor_id==successor&&adopted->completed_tick>=0&&adopted->completed_tick<=tick;
    else if(const auto* adopted=std::get_if<AdultAdoptionFact>(fact))valid=adopted->kind=="family_adult_adoption"&&head->kind==adopted->kind&&adopted->agreement.adopter_actor_id==predecessor&&adopted->agreement.adoptee_actor_id==successor&&adopted->agreement.accepted_tick>=0&&adopted->agreement.accepted_tick<=tick;
    return valid?Outcome<StableId>{true,*id,{}}:Outcome<StableId>{false,{},common_failure("SuccessionRelationshipMismatch",successor).error};
}
// 原目录逐项准入；本人say可用provider或确切候选ID，玩家choice/他人台词不能授权私产接续。
Status validate_succession_consent_catalog(const SuccessionConsentCatalog& catalog,const CommonCatalog& shared,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 39, "SuccessionConsentCatalog", "validate_succession_consent_catalog", &catalog);

    if(!catalog.enabled)return catalog.definitions.empty()?common_success():common_failure("SuccessionConsentCapabilityRequired");
    if(catalog.definitions.empty()||catalog.definitions.size()>4096)return common_failure("InvalidSuccessionConsentCatalog");std::set<std::tuple<StableId,StableId,StableId,StableId>> bindings;
    for(const auto& row:catalog.definitions.rows()){
        const auto& d=row.second;for(const auto* id:{&d.id,&d.predecessor_actor_id,&d.successor_actor_id,&d.dialogue_id,&d.node_id})if(!valid_id(*id))return common_failure("InvalidSuccessionConsentDefinition",row.first);
        if(d.id!=row.first||d.terms_version!=1||d.predecessor_actor_id==d.successor_actor_id)return common_failure("InvalidSuccessionConsentParties",row.first);
        for(const auto* id:{&d.predecessor_actor_id,&d.successor_actor_id})if(std::find(shared.actor_ids.begin(),shared.actor_ids.end(),*id)==shared.actor_ids.end())return common_failure("UnknownSuccessionConsentActor",*id);
        const auto* dialogue=dialogue_by_id(interaction,d.dialogue_id);if(!dialogue||dialogue->context_role.empty()||!shared.roles.find(dialogue->context_role))return common_failure("SuccessionOrdinaryDialogueRequired",row.first);
        const auto* node=node_by_id(*dialogue,d.node_id);if(!node||node->kind!="say"||(node->speaker!="$provider"&&node->speaker!=d.successor_actor_id))return common_failure("SuccessionCandidateSayRequired",row.first);
        if(!bindings.emplace(d.predecessor_actor_id,d.successor_actor_id,d.dialogue_id,d.node_id).second)return common_failure("DuplicateSuccessionConsentBinding",row.first);
        if(d.personal.money<0||d.personal.goods.size()>4096)return common_failure("InvalidSuccessionPersonalSelection",row.first);std::set<StableId> items;
        for(const auto& good:d.personal.goods)if(!valid_id(good.item_id)||good.quantity<=0||!shared.items.find(good.item_id)||!items.insert(good.item_id).second)return common_failure("InvalidSuccessionItemSelection",good.item_id);
    }
    return common_success();
}
// 只接受具名本人同意与私产分项范围；一般收养请求、赠与或全部家产配置明确拒绝。
Outcome<SuccessionConsentCatalog> decode_succession_consent_catalog(const BoundaryValue& config,const BoundaryValue& definitions){try{
    for(const auto* value:{&config,&definitions}){auto unique=validate_succession_json_unique(*value);if(!unique.ok)return {false,{},unique.error};}
    using namespace boundary;fields(config,{"profile","request_ids"},{},"succession_consent");if(get_string(config,"profile","")!="authored_voluntary_succession_v1")unsupported("UnsupportedSuccessionConsentProfile","succession_consent/profile");const auto selected=strings(field(config,"request_ids",""),"request_ids");if(selected.empty()||selected.size()>4096)fail("InvalidSuccessionConsentCatalog","request_ids");SuccessionConsentCatalog result;result.enabled=true;
    for(const auto& id:selected){const auto& v=field(definitions,id,"succession_consent_requests");fields(v,{"id","predecessor_actor_id","successor_actor_id","dialogue_id","node_id","terms_version","consent_kind","scope","personal"},{},id);if(get_string(v,"consent_kind","")!="successor_accepts_voluntary_succession_v1"||get_string(v,"scope","")!="player_control_and_selected_personal_assets")fail("InvalidSuccessionConsentScope",id);SuccessionConsentDefinition d;d.id=get_string(v,"id","");d.predecessor_actor_id=get_string(v,"predecessor_actor_id","");d.successor_actor_id=get_string(v,"successor_actor_id","");d.dialogue_id=get_string(v,"dialogue_id","");d.node_id=get_string(v,"node_id","");d.terms_version=get_int(v,"terms_version","");auto personal=decode_succession_personal_selection(field(v,"personal",""));if(!personal.ok)return {false,{},personal.error};d.personal=std::move(personal.value);if(d.id!=id||!result.definitions.insert(id,std::move(d)))fail("DuplicateSuccessionConsentDefinition",id);}
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 长期来源只核原接受的真实关系、成年日期及准确条款，不反向要求尚存原货批。
Status validate_succession_consent_source(const CommonStore& common,const FamilyState& family,const CommonCatalog& shared,const InteractionCatalog& interaction,const SuccessionConsentCatalog& catalog,const SuccessionConsentSource& a){
    if(!catalog.enabled)return common_failure("SuccessionConsentCapabilityRequired");auto valid=validate_succession_consent_catalog(catalog,shared,interaction);if(!valid.ok)return valid;valid=validate_succession_personal_terms(a.personal);if(!valid.ok)return valid;
    const auto* d=catalog.definitions.find(a.request_id);if(!d||std::tie(d->predecessor_actor_id,d->successor_actor_id,d->dialogue_id,d->node_id,d->terms_version)!=std::tie(a.personal.predecessor_actor_id,a.personal.successor_actor_id,a.dialogue_id,a.node_id,a.terms_version)||!selection_matches(d->personal,a.personal))return common_failure("SuccessionConsentBindingMismatch");
    if(a.world_id!=common.state().identity.world_id||std::all_of(a.world_id.begin(),a.world_id.end(),[](auto b){return b==0;})||a.scenario_id!=common.state().metadata.scenario_id||!valid_id(a.scenario_id))return common_failure("SuccessionConsentWorldMismatch");if(a.accepted_revision<0||a.accepted_revision>common.revision())return common_failure("SuccessionConsentSourceTimeMismatch");
    const auto* dialogue=dialogue_by_id(interaction,a.dialogue_id);const auto* node=node_by_id(*dialogue,a.node_id);const auto* role=shared.roles.find(dialogue->context_role);
    if(!valid_id(a.session_id)||a.node_kind!=node->kind||!face_to_face_contact_context_valid(a.context)||a.context.kind||a.context.provider_actor_id!=d->successor_actor_id||a.context.role_id!=dialogue->context_role||a.context.place_id!=role->place_id||!shared.places.find(a.context.place_id))return common_failure("InvalidSuccessionConsentSource");
    for(const auto* actor:{&d->predecessor_actor_id,&d->successor_actor_id}){valid=adult_at(common,*actor,a.accepted_tick);if(!valid.ok)return valid;}
    const auto* predecessor=common.state().actors.find(d->predecessor_actor_id);const auto* successor=common.state().actors.find(d->successor_actor_id);
    if(a.predecessor_birth_tick!=*predecessor->birth_tick||a.successor_birth_tick!=*successor->birth_tick)return common_failure("SuccessionConsentIdentityChanged");
    if(a.personal.source_wallet_id!=predecessor->wallet_id||a.personal.source_inventory_id!=predecessor->inventory_id||a.personal.destination_wallet_id!=successor->wallet_id||a.personal.destination_inventory_id!=successor->inventory_id)return common_failure("SuccessionPersonalContainerMismatch");
    auto basis=succession_adoption_basis(common,family,d->predecessor_actor_id,d->successor_actor_id,a.accepted_tick);if(!basis.ok)return {false,{},basis.error};if(basis.value!=a.relation_fact_id)return common_failure("SuccessionRelationshipMismatch");
    for(const auto& lot:a.personal.lots)if(!shared.items.find(lot.item_id)||!allocated_lot(lot.lot_id,common.state().next_instance_id))return common_failure("InvalidSuccessionPersonalLot",lot.lot_id);return common_success();
}
// 元数据字段序不参与业务相等，原来源与所有所选批次则逐项一致。
bool succession_consent_source_equal(const SuccessionConsentSource& a,const SuccessionConsentSource& b){return std::tie(a.request_id,a.relation_fact_id,a.dialogue_id,a.node_id,a.node_kind,a.session_id,a.scenario_id,a.world_id,a.terms_version,a.accepted_tick,a.accepted_revision,a.predecessor_birth_tick,a.successor_birth_tick)==std::tie(b.request_id,b.relation_fact_id,b.dialogue_id,b.node_id,b.node_kind,b.session_id,b.scenario_id,b.world_id,b.terms_version,b.accepted_tick,b.accepted_revision,b.predecessor_birth_tick,b.successor_birth_tick)&&context_equal(a.context,b.context)&&succession_personal_terms_equal(a.personal,b.personal);}
// 当前玩家选择接续时，继任者必须已在真实节点接受目录中的同一份分项条款。
Outcome<SuccessionConsentQuote> preview_succession_consent(const CommonStore& common,const FamilyState& family,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const SuccessionConsentCatalog& catalog,const StableId& predecessor,const StableId& successor,const StableId& request_id){
    if(!catalog.enabled||!interaction.common)return {false,{},common_failure("SuccessionConsentCapabilityRequired").error};auto valid=validate_succession_consent_catalog(catalog,*interaction.common,interaction);if(!valid.ok)return {false,{},valid.error};const auto* d=catalog.definitions.find(request_id);if(!d||d->predecessor_actor_id!=predecessor||d->successor_actor_id!=successor)return {false,{},common_failure("SuccessionConsentRequestMismatch",request_id).error};
    auto basis=succession_adoption_basis(common,family,predecessor,successor,common.tick());if(!basis.ok)return {false,{},basis.error};auto proof=preview_authored_node_source(common,state,interaction,access,predecessor,successor);if(!proof.ok)return {false,{},proof.error};if(proof.value.dialogue_id!=d->dialogue_id||proof.value.node_id!=d->node_id)return {false,{},common_failure("SuccessionConsentNotCurrent",request_id).error};auto personal=preview_succession_personal_assets(common,predecessor,successor,d->personal);if(!personal.ok)return {false,{},personal.error};
    SuccessionConsentSource a;a.request_id=request_id;a.relation_fact_id=basis.value;a.dialogue_id=proof.value.dialogue_id;a.node_id=proof.value.node_id;a.node_kind=proof.value.node_kind;a.session_id=proof.value.session_id;a.context=proof.value.context;a.world_id=common.state().identity.world_id;a.scenario_id=common.state().metadata.scenario_id;a.accepted_tick=common.tick();a.accepted_revision=common.revision();a.terms_version=d->terms_version;a.personal=personal.value.terms;
    for(const auto* id:{&predecessor,&successor}){valid=adult_at(common,*id,common.tick());if(!valid.ok)return {false,{},valid.error};}a.predecessor_birth_tick=*common.state().actors.find(predecessor)->birth_tick;a.successor_birth_tick=*common.state().actors.find(successor)->birth_tick;
    valid=validate_succession_consent_source(common,family,*interaction.common,interaction,catalog,a);if(!valid.ok)return {false,{},valid.error};return {true,{std::move(proof.value),std::move(personal.value),std::move(a)}, {}};
}
// 提交前重取同一节点与私产报价，防止旧回调、换人物或收养来源改变后沿用许可。
Status revalidate_succession_consent(const CommonStore& common,const FamilyState& family,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const SuccessionConsentCatalog& catalog,const SuccessionConsentQuote& quote){
    auto valid=revalidate_authored_node_source(common,state,interaction,access,quote.source);if(!valid.ok)return valid;auto assets=succession_personal_asset_recipe(common,quote.personal);if(!assets.ok)return {false,{},assets.error};if(!succession_personal_terms_equal(quote.personal.terms,quote.agreement.personal))return common_failure("SuccessionConsentAssetMismatch");auto fresh=preview_succession_consent(common,family,state,interaction,access,catalog,quote.agreement.personal.predecessor_actor_id,quote.agreement.personal.successor_actor_id,quote.agreement.request_id);if(!fresh.ok)return {false,{},fresh.error};const auto& a=quote.source;const auto& b=fresh.value.source;if(std::tie(a.actor_id,a.provider_actor_id,a.session_id,a.dialogue_id,a.node_id,a.node_kind)!=std::tie(b.actor_id,b.provider_actor_id,b.session_id,b.dialogue_id,b.node_id,b.node_kind)||!context_equal(a.context,b.context)||!succession_consent_source_equal(quote.agreement,fresh.value.agreement))return common_failure("StaleSuccessionConsent");return common_success();
}
}
