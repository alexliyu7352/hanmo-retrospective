#include "resident/kernel/catalog_section_observation.hpp"
// 首片只原provider/player SAY、无条件choice与end；两端选择绝不扫描/显示旁支关系。
#include "known_personal_dialogue_catalog.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/family/birth_arrangement_source.hpp"
#include <set>
namespace hanmo::resident {namespace {
void unique(const BoundaryValue& v,const StableId& path){std::set<StableId> seen;for(const auto& row:boundary::object(v,path))if(!seen.insert(row.first).second)boundary::fail("DuplicateKnownPersonalDialogueField",path+"/"+row.first);}
bool selector(const KnownPersonalDialogueSelector& s,const CommonCatalog& common,const FamilyCatalog& family,const BirthArrangementCatalog& birth){if(s.source_id.empty())return false;if(s.kind=="installed_actor")return std::find(common.actor_ids.begin(),common.actor_ids.end(),s.source_id)!=common.actor_ids.end();if(s.kind!="born_prototype"||!family.enabled||!birth.enabled||!family.prototypes.find(s.source_id)||std::find(common.actor_ids.begin(),common.actor_ids.end(),s.source_id)!=common.actor_ids.end())return false;for(const auto& row:birth.arrangements.rows())if(row.second.terms.child_prototype_id==s.source_id)return true;return false;}
}
const KnownPersonalDialogueTemplate* known_personal_dialogue_template(const KnownPersonalDialogueCatalog& c,const StableId& id){for(const auto& row:c.templates)if(row.id==id)return &row;return nullptr;}
Status validate_known_personal_dialogue_catalog(const KnownPersonalDialogueCatalog& c,const InteractionCatalog& i,const FamilyCatalog& family,const BirthArrangementCatalog& birth){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 57, "KnownPersonalDialogueCatalog", "validate_known_personal_dialogue_catalog", &c);

 if(c.version!=1)return common_failure("UnsupportedKnownPersonalDialogueVersion");if(!i.common||c.templates.empty()||c.templates.size()>256)return common_failure("InvalidKnownPersonalDialogueCatalog");std::set<StableId> ids,dialogues;
 for(const auto& t:c.templates){if(t.id.empty()||!ids.insert(t.id).second||!selector(t.listener,*i.common,family,birth)||!selector(t.provider,*i.common,family,birth)||(t.listener.kind=="installed_actor"&&t.provider.kind=="installed_actor"&&t.listener.source_id==t.provider.source_id))return common_failure("InvalidKnownPersonalDialogueTemplate",t.id);
  const DialogueDefinition* d=nullptr;for(const auto& row:i.dialogues)if(row.id==t.dialogue_id){if(d)return common_failure("AmbiguousKnownPersonalDialogue",t.id);d=&row;}
  if(!d||!d->context_role.empty()||!dialogues.insert(d->id).second||d->nodes.empty()||d->nodes.size()>128)return common_failure("InvalidKnownPersonalDialogue",t.id);
  std::map<StableId,const DialogueNode*> nodes;bool provider_say=false;
  for(const auto& n:d->nodes){if(n.id.empty()||!nodes.emplace(n.id,&n).second||n.command||n.condition||!n.if_true.empty()||!n.if_false.empty()||!n.on_success.empty()||!n.on_failure.empty())return common_failure("InvalidKnownPersonalDialogueNode",t.id);
   if(n.kind=="say"){if((n.speaker!="$provider"&&n.speaker!="$player")||!n.options.empty()||n.next.empty())return common_failure("InvalidKnownPersonalDialogueSay",t.id);provider_say|=n.speaker=="$provider";}
   else if(n.kind=="choice"){if(!n.speaker.empty()||!n.next.empty()||n.options.empty()||n.options.size()>16)return common_failure("InvalidKnownPersonalDialogueChoice",t.id);std::set<StableId> options;for(const auto& o:n.options)if(o.id.empty()||!options.insert(o.id).second||o.next.empty()||!o.conditions.empty())return common_failure("InvalidKnownPersonalDialogueChoice",t.id);}
   else if(n.kind!="end"||!n.speaker.empty()||!n.next.empty()||!n.options.empty())return common_failure("UnsupportedKnownPersonalDialogueNode",t.id);
  }
  auto entry=nodes.find(d->entry);if(!provider_say||entry==nodes.end()||entry->second->kind!="say"||entry->second->speaker!="$provider")return common_failure("KnownPersonalDialogueProviderEntryRequired",t.id);
  std::vector<StableId> pending{d->entry};std::set<StableId> reached;while(!pending.empty()){const auto id=pending.back();pending.pop_back();const auto n=nodes.find(id);if(n==nodes.end())return common_failure("KnownPersonalDialogueMissingTarget",t.id);if(!reached.insert(id).second)continue;if(!n->second->next.empty())pending.push_back(n->second->next);for(const auto& o:n->second->options)pending.push_back(o.next);}
  if(reached.size()!=nodes.size())return common_failure("KnownPersonalDialogueUnreachableNode",t.id);
 }
 return common_success();
}
Outcome<KnownPersonalDialogueCatalog> decode_known_personal_dialogue_catalog(const BoundaryValue& value,const InteractionCatalog& interaction,const FamilyCatalog& family,const BirthArrangementCatalog& birth){try{using namespace boundary;unique(value,"known_personal_dialogues");fields(value,{"version","templates"},{},"known_personal_dialogues");KnownPersonalDialogueCatalog out;out.version=get_int(value,"version","known_personal_dialogues");const auto& rows=array(field(value,"templates","known_personal_dialogues"),"templates");if(rows.size()>256)fail("KnownPersonalDialogueTemplateLimit","templates");auto endpoint=[](const BoundaryValue& v){unique(v,"selector");fields(v,{"kind","source_id"},{},"selector");return KnownPersonalDialogueSelector{get_string(v,"kind","selector"),get_string(v,"source_id","selector")};};for(const auto& row:rows){unique(row,"template");fields(row,{"id","dialogue_id","listener","provider"},{},"template");out.templates.push_back({get_string(row,"id","template"),get_string(row,"dialogue_id","template"),endpoint(field(row,"listener","template")),endpoint(field(row,"provider","template"))});}auto checked=validate_known_personal_dialogue_catalog(out,interaction,family,birth);return checked.ok?Outcome<KnownPersonalDialogueCatalog>{true,std::move(out),{}}:Outcome<KnownPersonalDialogueCatalog>{false,{},checked.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_known_personal_dialogue_catalog(const KnownPersonalDialogueCatalog& c){using V=BoundaryValue;V::Array rows;auto endpoint=[](const KnownPersonalDialogueSelector& s){return boundary::obj({{"kind",V{s.kind}},{"source_id",V{s.source_id}}});};for(const auto& t:c.templates)rows.push_back(boundary::obj({{"id",V{t.id}},{"dialogue_id",V{t.dialogue_id}},{"listener",endpoint(t.listener)},{"provider",endpoint(t.provider)}}));return boundary::obj({{"version",V{c.version}},{"templates",V{std::move(rows)}}});}
}
