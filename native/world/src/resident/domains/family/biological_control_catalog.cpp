#include "resident/kernel/catalog_section_observation.hpp"
// 两种准入分开：原作者本人沿原私人会面，真正出生者只沿S3具名模板绑定到实际本人。
#include "biological_control_catalog.hpp"
#include "succession_personal_assets.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {namespace {
bool reference(const StableId& id){return !id.empty()&&id.size()<=256;}
const DialogueDefinition* dialogue(const InteractionCatalog& catalog,const StableId& id){for(const auto& row:catalog.dialogues)if(row.id==id)return &row;return nullptr;}
const DialogueNode* node(const DialogueDefinition& talk,const StableId& id){for(const auto& row:talk.nodes)if(row.id==id)return &row;return nullptr;}
// 目录结构不提前授亲代/生死/可玩资格，也不把别域请求ID覆盖成新的同意来源。
Status definition_shape(const BiologicalControlDefinition& d){for(const auto* id:{&d.id,&d.accept_node_id,&d.revoke_node_id})if(!reference(*id))return common_failure("InvalidBiologicalControlDefinition");if(d.terms_version!=1||d.accept_node_id==d.revoke_node_id)return common_failure("InvalidBiologicalControlTerms");if(d.meeting_kind==BiologicalControlMeetingKind::installed_candidate){if(!reference(d.predecessor_actor_id)||!reference(d.successor_actor_id)||d.predecessor_actor_id==d.successor_actor_id||!reference(d.dialogue_id)||!d.template_id.empty())return common_failure("InvalidBiologicalControlInstalledBinding");}else if(d.meeting_kind==BiologicalControlMeetingKind::born_personal_template){if(!reference(d.template_id)||!d.predecessor_actor_id.empty()||!d.successor_actor_id.empty()||!d.dialogue_id.empty())return common_failure("InvalidBiologicalControlTemplateBinding");}else return common_failure("UnsupportedBiologicalControlMeetingKind");return common_success();}
}
Outcome<BiologicalControlCatalog> decode_biological_control_catalog(const BoundaryValue& value){try{
    auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};using namespace boundary;
    fields(value,{"version","profile","requests"},{},"family_biological_legacy");if(get_int(value,"version","")!=1||get_string(value,"profile","")!="biological_posthumous_control_v1")unsupported("UnsupportedBiologicalControlProfile","");
    const auto& rows=array(field(value,"requests",""),"requests");if(rows.empty()||rows.size()>4096)fail("InvalidBiologicalControlRequestCount","");BiologicalControlCatalog out;out.enabled=true;
    for(const auto& row:rows){BiologicalControlDefinition d;const auto kind=get_string(row,"meeting_kind","");if(kind=="installed_candidate"){fields(row,{"id","meeting_kind","predecessor_actor_id","successor_actor_id","dialogue_id","accept_node_id","revoke_node_id","terms_version","scope"},{},"biological_control/request");d.predecessor_actor_id=get_string(row,"predecessor_actor_id","");d.successor_actor_id=get_string(row,"successor_actor_id","");d.dialogue_id=get_string(row,"dialogue_id","");}
        else if(kind=="born_personal_template"){fields(row,{"id","meeting_kind","template_id","accept_node_id","revoke_node_id","terms_version","scope"},{},"biological_control/request");d.meeting_kind=BiologicalControlMeetingKind::born_personal_template;d.template_id=get_string(row,"template_id","");}
        else unsupported("UnsupportedBiologicalControlMeetingKind",kind);
        d.id=get_string(row,"id","");d.accept_node_id=get_string(row,"accept_node_id","");d.revoke_node_id=get_string(row,"revoke_node_id","");d.terms_version=get_int(row,"terms_version","");if(get_string(row,"scope","")!="own_life_control_only")fail("InvalidBiologicalControlScope",d.id);auto valid=definition_shape(d);if(!valid.ok)return {false,{},valid.error};if(!out.definitions.insert(d.id,d))fail("DuplicateBiologicalControlRequest",d.id);
    }return {true,std::move(out),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_biological_control_catalog(const BiologicalControlCatalog& catalog,const LegacyControlConsentCatalog& original,const CommonCatalog& common,const InteractionCatalog& interactions,const BiologicalControlTemplateRead& templates){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 24, "BiologicalControlCatalog", "validate_biological_control_catalog", &catalog);

    if(!catalog.enabled)return catalog.definitions.empty()?common_success():common_failure("BiologicalControlProfileRequired");if(catalog.definitions.empty()||catalog.definitions.size()>4096)return common_failure("InvalidBiologicalControlRequestCount");
    std::set<std::tuple<StableId,StableId,StableId,StableId,StableId>> bindings;
    for(const auto& row:catalog.definitions.rows()){const auto& d=row.second;auto valid=definition_shape(d);if(!valid.ok)return valid;if(row.first!=d.id||original.definitions.find(d.id))return common_failure("BiologicalControlRequestCollision",row.first);
        StableId dialogue_id=d.dialogue_id,selector,other;
        if(d.meeting_kind==BiologicalControlMeetingKind::installed_candidate){for(const auto* actor:{&d.predecessor_actor_id,&d.successor_actor_id})if(std::find(common.actor_ids.begin(),common.actor_ids.end(),*actor)==common.actor_ids.end())return common_failure("BiologicalControlInstalledActorRequired",*actor);selector=d.predecessor_actor_id;other=d.successor_actor_id;}
        else{if(!templates)return common_failure("BiologicalControlTemplateSourceRequired",d.template_id);auto selected=templates(d.template_id);if(!selected.ok)return {false,{},selected.error};if(selected.value.template_id!=d.template_id||!reference(selected.value.prototype_id)||!reference(selected.value.dialogue_id))return common_failure("BiologicalControlTemplateIdentityMismatch",d.template_id);dialogue_id=selected.value.dialogue_id;selector=d.template_id;}
        const auto* talk=dialogue(interactions,dialogue_id);if(!talk)return common_failure("BiologicalControlDialogueRequired",dialogue_id);
        if(d.meeting_kind==BiologicalControlMeetingKind::installed_candidate){const auto* role=common.roles.find(talk->context_role);if(!role)return common_failure("BiologicalControlInstalledMeetingRequired",dialogue_id);}
        for(const auto* id:{&d.accept_node_id,&d.revoke_node_id}){const auto* speech=node(*talk,*id);const bool speaker=speech&&(speech->speaker=="$provider"||(d.meeting_kind==BiologicalControlMeetingKind::installed_candidate&&speech->speaker==d.successor_actor_id));if(!speech||speech->kind!="say"||!speaker)return common_failure("BiologicalControlCandidateSayRequired",*id);if(!bindings.emplace(d.meeting_kind==BiologicalControlMeetingKind::installed_candidate?"installed":"born",selector,other,dialogue_id,*id).second)return common_failure("DuplicateBiologicalControlNode",*id);}
    }return common_success();
}
}
