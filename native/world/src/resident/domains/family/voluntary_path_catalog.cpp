#include "resident/kernel/catalog_section_observation.hpp"
#include "voluntary_path_catalog.hpp"
#include <set>
namespace hanmo::resident {
namespace {
bool reference(const StableId& id){return !id.empty()&&id.size()<=256;}
Status shape(const VoluntaryPathDefinition& d){
    for(const auto* id:{&d.id,&d.template_id,&d.accept_node_id,&d.withdraw_node_id})
        if(!reference(*id))return common_failure("InvalidVoluntaryPathDefinition",d.id);
    if(d.terms_version!=1||d.accept_node_id==d.withdraw_node_id)
        return common_failure("DistinctVoluntaryDeclarationsRequired",d.id);
    if(!voluntary_relation_kind_name(d.relation_kind))return common_failure("InvalidVoluntaryRelationKind",d.id);
    if(d.relation_kind==VoluntaryRelationKind::accepted_disciple){
        if(!reference(d.disciple_skill_id))return common_failure("VoluntaryDiscipleSkillRequired",d.id);
    }else if(!d.disciple_skill_id.empty())return common_failure("UnexpectedVoluntaryDiscipleSkill",d.id);
    auto personal=decode_succession_personal_selection(encode_succession_personal_selection(d.personal));
    return personal.ok?common_success():Status{false,{},personal.error};
}
const DialogueDefinition* dialogue(const InteractionCatalog& c,const StableId& id){
    const DialogueDefinition* out=nullptr;
    for(const auto& d:c.dialogues)if(d.id==id){if(out)return nullptr;out=&d;}return out;
}
const DialogueNode* node(const DialogueDefinition& d,const StableId& id){
    const DialogueNode* out=nullptr;
    for(const auto& n:d.nodes)if(n.id==id){if(out)return nullptr;out=&n;}return out;
}
bool selector(const KnownPersonalDialogueSelector& s){
    return reference(s.source_id)&&(s.kind=="installed_actor"||s.kind=="born_prototype");
}
}
const char* voluntary_relation_kind_name(VoluntaryRelationKind kind){switch(kind){
case VoluntaryRelationKind::adoption:return "adoption";
case VoluntaryRelationKind::biological_child:return "biological_child";
case VoluntaryRelationKind::accepted_disciple:return "accepted_disciple";
case VoluntaryRelationKind::recognized_collateral:return "recognized_collateral";
}return nullptr;}
Outcome<VoluntaryRelationKind> decode_voluntary_relation_kind(const StableId& name){
    for(auto kind:{VoluntaryRelationKind::adoption,VoluntaryRelationKind::biological_child,
                  VoluntaryRelationKind::accepted_disciple,VoluntaryRelationKind::recognized_collateral})
        if(name==voluntary_relation_kind_name(kind))return {true,kind,{}};
    return {false,{},common_failure("InvalidVoluntaryRelationKind",name).error};
}
Outcome<VoluntaryPathCatalog> decode_voluntary_path_catalog(const BoundaryValue& value){try{
    auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};
    using namespace boundary;
    fields(value,{"version","profile","requests"},{},"family_voluntary_paths");
    if(get_int(value,"version","")!=1||get_string(value,"profile","")!="living_accepted_family_paths_v1")
        unsupported("UnsupportedVoluntaryPathProfile","");
    const auto& rows=array(field(value,"requests",""),"requests");
    if(rows.empty()||rows.size()>4096)fail("InvalidVoluntaryPathCount","");
    VoluntaryPathCatalog out;out.enabled=true;
    for(const auto& row:rows){
        fields(row,{"id","template_id","accept_node_id","withdraw_node_id","relation_kind",
                    "disciple_skill_id","terms_version","consent_kind","scope","personal"},{},"voluntary_path/request");
        if(get_string(row,"consent_kind","")!="successor_accepts_living_voluntary_succession_v1"||
           get_string(row,"scope","")!="player_control_and_selected_personal_assets")
            fail("InvalidVoluntaryPathScope","");
        VoluntaryPathDefinition d;d.id=get_string(row,"id","");d.template_id=get_string(row,"template_id","");
        d.accept_node_id=get_string(row,"accept_node_id","");d.withdraw_node_id=get_string(row,"withdraw_node_id","");
        d.disciple_skill_id=get_string(row,"disciple_skill_id","");d.terms_version=get_int(row,"terms_version","");
        auto kind=decode_voluntary_relation_kind(get_string(row,"relation_kind",""));if(!kind.ok)return {false,{},kind.error};
        d.relation_kind=kind.value;auto personal=decode_succession_personal_selection(field(row,"personal",""));
        if(!personal.ok)return {false,{},personal.error};d.personal=std::move(personal.value);
        auto valid=shape(d);if(!valid.ok)return {false,{},valid.error};
        const StableId id=d.id;if(!out.definitions.insert(id,std::move(d)))fail("DuplicateVoluntaryPath",id);
    }return {true,std::move(out),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<SuccessionConsentCatalog> decode_living_succession_catalog(const BoundaryValue& config,const BoundaryValue& definitions){try{
    auto unique=validate_succession_json_unique(config);if(!unique.ok)return {false,{},unique.error};unique=validate_succession_json_unique(definitions);if(!unique.ok)return {false,{},unique.error};
    using namespace boundary;fields(config,{"profile","request_ids"},{},"voluntary_succession");
    if(get_string(config,"profile","")!="living_accepted_family_paths_v2")unsupported("UnsupportedVoluntaryPathSuccessionProfile","");
    const auto ids=strings(field(config,"request_ids",""),"request_ids");if(ids.size()>4096)fail("InvalidSuccessionConsentCatalog","");
    if(ids.empty())return {true,SuccessionConsentCatalog{}, {}};
    // 旧声明逐项仍用原唯一decoder，不能因新profile改签者、条款或扩大货权。
    auto original=obj({{"profile",BoundaryValue{StableId{"authored_voluntary_succession_v1"}}},{"request_ids",string_array(ids)}});
    return decode_succession_consent_catalog(original,definitions);
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_voluntary_path_catalog(const VoluntaryPathCatalog& catalog,const SuccessionConsentCatalog& original,
    const InteractionCatalog& interactions,const VoluntaryPathTemplateRead& templates,const VoluntaryOtherDeclarationRead& other){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 40, "VoluntaryPathCatalog", "validate_voluntary_path_catalog", &catalog);

    if(!catalog.enabled)return catalog.definitions.empty()?common_success():common_failure("VoluntaryPathProfileRequired");
    if(!interactions.common||!templates||!other)return common_failure("VoluntaryPersonalTemplateRequired");
    if(catalog.definitions.empty()||catalog.definitions.size()>4096)return common_failure("InvalidVoluntaryPathCount");
    std::set<std::pair<StableId,StableId>> declarations;
    for(const auto& row:catalog.definitions.rows()){
        const auto& d=row.second;auto valid=shape(d);if(!valid.ok)return valid;
        if(row.first!=d.id||original.definitions.find(d.id))return common_failure("VoluntaryPathRequestCollision",row.first);
        auto t=templates(d.template_id);if(!t.ok)return {false,{},t.error};
        if(t.value.id!=d.template_id||!reference(t.value.dialogue_id)||!selector(t.value.listener)||!selector(t.value.provider))
            return common_failure("VoluntaryTemplateIdentityMismatch",d.template_id);
        if(t.value.listener.kind=="installed_actor"&&t.value.provider.kind=="installed_actor"&&
           t.value.listener.source_id==t.value.provider.source_id)return common_failure("VoluntaryDistinctPartiesRequired",d.id);
        const auto* talk=dialogue(interactions,t.value.dialogue_id);
        if(!talk||!talk->context_role.empty())return common_failure("VoluntaryPrivateDialogueRequired",d.template_id);
        for(const auto* id:{&d.accept_node_id,&d.withdraw_node_id}){
            const auto* say=node(*talk,*id);
            if(!say||say->kind!="say"||say->speaker!="$provider")return common_failure("VoluntaryProviderDeclarationRequired",*id);
            if(!declarations.emplace(talk->id,*id).second)return common_failure("DuplicateVoluntaryDeclaration",*id);
            auto used=other(talk->id,*id);if(!used.ok)return {false,{},used.error};
            if(used.value)return common_failure("VoluntaryOtherDeclarationCollision",*id);
        }
        // 旧生前许可的同一句声明也不能被新政策悄然复用。
        for(const auto& old:original.definitions.rows())if(old.second.dialogue_id==talk->id&&
            (old.second.node_id==d.accept_node_id||old.second.node_id==d.withdraw_node_id))
            return common_failure("VoluntaryOriginalDeclarationCollision",d.id);
    }return common_success();
}
}
