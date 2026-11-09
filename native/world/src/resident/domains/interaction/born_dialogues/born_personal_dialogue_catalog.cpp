#include "resident/kernel/catalog_section_observation.hpp"
// 首片模板只支持本人说话与明确选择；不借模板自动发命令、接岗位、执行任意脚本。
#include "born_personal_dialogue_catalog.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/family/birth_arrangement_source.hpp"
#include <set>
namespace hanmo::resident {namespace {
void unique(const BoundaryValue& v,const StableId& path){std::set<StableId> seen;for(const auto& row:boundary::object(v,path))if(!seen.insert(row.first).second)boundary::fail("DuplicateBornPersonalDialogueField",path+"/"+row.first);}
}
Status validate_born_personal_dialogue_catalog(const BornPersonalDialogueCatalog& catalog,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 50, "BornPersonalDialogueCatalog", "validate_born_personal_dialogue_catalog", &catalog);

    if(catalog.version!=1)return common_failure("UnsupportedBornPersonalDialogueVersion");
    if(!interaction.common||catalog.templates.empty()||catalog.templates.size()>256)return common_failure("InvalidBornPersonalDialogueCatalog");
    std::set<StableId> ids,dialogues;
    for(const auto& t:catalog.templates){
        if(t.id.empty()||!ids.insert(t.id).second||t.prototype_id.empty())return common_failure("InvalidBornPersonalDialogueTemplate",t.id);
        const DialogueDefinition* d=nullptr;for(const auto& row:interaction.dialogues)if(row.id==t.dialogue_id){if(d)return common_failure("AmbiguousBornPersonalDialogue",t.id);d=&row;}
        if(!d||!d->context_role.empty()||!dialogues.insert(d->id).second||d->nodes.empty()||d->nodes.size()>128)return common_failure("InvalidBornPersonalDialogue",t.id);
        std::map<StableId,const DialogueNode*> nodes;bool provider_say=false;
        for(const auto& n:d->nodes){
            if(n.id.empty()||!nodes.emplace(n.id,&n).second||n.command||n.condition||!n.if_true.empty()||!n.if_false.empty()||!n.on_success.empty()||!n.on_failure.empty())return common_failure("InvalidBornPersonalDialogueNode",t.id);
            if(n.kind=="say") {if((n.speaker!="$provider"&&n.speaker!="$player")||!n.options.empty()||n.next.empty())return common_failure("InvalidBornPersonalDialogueSay",t.id);provider_say|=n.speaker=="$provider";}
            else if(n.kind=="choice") {if(!n.speaker.empty()||!n.next.empty()||n.options.empty()||n.options.size()>16)return common_failure("InvalidBornPersonalDialogueChoice",t.id);std::set<StableId> options;for(const auto& o:n.options)if(o.id.empty()||!options.insert(o.id).second||o.next.empty()||!o.conditions.empty())return common_failure("InvalidBornPersonalDialogueChoice",t.id);}
            else if(n.kind!="end"||!n.speaker.empty()||!n.next.empty()||!n.options.empty())return common_failure("UnsupportedBornPersonalDialogueNode",t.id);
        }
        auto entry=nodes.find(d->entry);if(!provider_say||entry==nodes.end()||entry->second->kind!="say"||entry->second->speaker!="$provider")return common_failure("BornPersonalDialogueProviderEntryRequired",t.id);
        std::vector<StableId> pending{d->entry};std::set<StableId> reached;while(!pending.empty()){const auto id=pending.back();pending.pop_back();auto n=nodes.find(id);if(n==nodes.end())return common_failure("BornPersonalDialogueMissingTarget",t.id);if(!reached.insert(id).second)continue;if(!n->second->next.empty())pending.push_back(n->second->next);for(const auto& o:n->second->options)pending.push_back(o.next);}
        if(reached.size()!=nodes.size())return common_failure("BornPersonalDialogueUnreachableNode",t.id);
    }
    return common_success();
}
Status validate_born_personal_dialogue_birth_catalog(const BornPersonalDialogueCatalog& catalog,const FamilyCatalog& family,const BirthArrangementCatalog& birth){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 51, "BornPersonalDialogueCatalog", "validate_born_personal_dialogue_birth_catalog", &catalog);

    if(!family.enabled||!birth.enabled)return common_failure("BornPersonalDialogueBirthProfileRequired");
    for(const auto& t:catalog.templates){if(!family.prototypes.find(t.prototype_id))return common_failure("BornPersonalDialoguePrototypeRequired",t.id);bool declared=false;for(const auto& row:birth.arrangements.rows())if(row.second.terms.child_prototype_id==t.prototype_id)declared=true;if(!declared)return common_failure("BornPersonalDialogueBirthTemplateRequired",t.id);}
    return common_success();
}
Outcome<BornPersonalDialogueCatalog> decode_born_personal_dialogue_catalog(const BoundaryValue& value,const InteractionCatalog& interaction,const FamilyCatalog& family,const BirthArrangementCatalog& birth){try{
    using namespace boundary;unique(value,"born_personal_dialogues");fields(value,{"version","templates"},{},"born_personal_dialogues");BornPersonalDialogueCatalog out;out.version=get_int(value,"version","born_personal_dialogues");
    const auto& rows=array(field(value,"templates","born_personal_dialogues"),"templates");if(rows.size()>256)fail("BornPersonalDialogueTemplateLimit","templates");
    for(const auto& row:rows){unique(row,"template");fields(row,{"id","prototype_id","dialogue_id"},{},"template");out.templates.push_back({get_string(row,"id","template"),get_string(row,"prototype_id","template"),get_string(row,"dialogue_id","template")});}
    auto checked=validate_born_personal_dialogue_catalog(out,interaction);if(checked.ok)checked=validate_born_personal_dialogue_birth_catalog(out,family,birth);return checked.ok?Outcome<BornPersonalDialogueCatalog>{true,std::move(out),{}}:Outcome<BornPersonalDialogueCatalog>{false,{},checked.error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_born_personal_dialogue_catalog(const BornPersonalDialogueCatalog& catalog){using V=BoundaryValue;V::Array rows;for(const auto& t:catalog.templates)rows.push_back(boundary::obj({{"id",V{t.id}},{"prototype_id",V{t.prototype_id}},{"dialogue_id",V{t.dialogue_id}}}));return boundary::obj({{"version",V{catalog.version}},{"templates",V{std::move(rows)}}});}
}
