// 新生前声明是独立本人SAY，其他关系或死后许可不可默许换控。
#include "resident/domains/family/voluntary_path_catalog.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {Integer checks=0;void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}}
int main(){try{
    auto json=parse_boundary_json(R"({"version":1,"profile":"living_accepted_family_paths_v1","requests":[{"id":"living","template_id":"private","accept_node_id":"accept_living","withdraw_node_id":"withdraw_living","relation_kind":"biological_child","disciple_skill_id":"","terms_version":1,"consent_kind":"successor_accepts_living_voluntary_succession_v1","scope":"player_control_and_selected_personal_assets","personal":{"money":0,"goods":[]}}]})");
    check(json.ok,"explicit policy JSON");auto decoded=decode_voluntary_path_catalog(json.value);
    check(decoded.ok,"living policy decodes");const auto catalog=decoded.value;
    check(catalog.definitions.find("living")!=nullptr,"independent insertion key survives move");
    InteractionCatalog interactions;interactions.common=std::make_shared<CommonCatalog>();
    DialogueDefinition dialogue;dialogue.id="private_dialogue";
    for(const auto* id:{"accept_living","withdraw_living"}){DialogueNode node;node.id=id;node.kind="say";node.speaker="$provider";dialogue.nodes.push_back(node);}
    interactions.dialogues.push_back(dialogue);SuccessionConsentCatalog original;
    KnownPersonalDialogueTemplate t{"private","private_dialogue",{"installed_actor","parent"},{"born_prototype","child"}};
    VoluntaryPathTemplateRead templates=[&](const StableId&)->Outcome<KnownPersonalDialogueTemplate>{return {true,t,{}};};
    VoluntaryOtherDeclarationRead other=[](const StableId&,const StableId&)->Outcome<bool>{return {true,false,{}};};
    auto valid=[&](const VoluntaryPathCatalog& c){return validate_voluntary_path_catalog(c,original,interactions,templates,other).ok;};
    check(valid(catalog),"real provider declarations can state living choice");
    for(const auto* speaker:{"$player","someone"}){interactions.dialogues[0].nodes[0].speaker=speaker;check(!valid(catalog),"only provider can accept own control");}
    interactions.dialogues[0].nodes[0].speaker="$provider";
    interactions.dialogues[0].nodes[0].kind="choice";check(!valid(catalog),"player choice is not successor acceptance");interactions.dialogues[0].nodes[0].kind="say";
    auto bad=catalog;bad.definitions.find("living")->withdraw_node_id="accept_living";check(!valid(bad),"accept and withdraw must be distinct");
    bad=catalog;bad.definitions.find("living")->relation_kind=VoluntaryRelationKind::accepted_disciple;
    check(!valid(bad),"disciple requires explicit original skill");bad.definitions.find("living")->disciple_skill_id="rites";check(valid(bad),"specific disciple policy accepted");
    bad=catalog;bad.definitions.find("living")->disciple_skill_id="rites";check(!valid(bad),"bio cannot smuggle disciple interpretation");
    other=[](const StableId&,const StableId&)->Outcome<bool>{return {true,true,{}};};check(!valid(catalog),"death or other acceptance sentence cannot be reused");
    other=[](const StableId&,const StableId&)->Outcome<bool>{return {false,{},common_failure("UnknownDeclarationCatalog").error};};check(!valid(catalog),"unknown other catalog is not empty");
    other=[](const StableId&,const StableId&)->Outcome<bool>{return {true,false,{}};};
    check(!validate_voluntary_path_catalog(catalog,original,interactions,templates,{}).ok,"complete selected declaration reader required");
    original.definitions.insert("living",{});check(!valid(catalog),"old request ID cannot be shadowed");original.definitions={};
    SuccessionConsentDefinition old;old.id="old";old.dialogue_id="private_dialogue";old.node_id="accept_living";
    original.definitions.insert("old",old);check(!valid(catalog),"old source node cannot be reinterpreted");original.definitions={};
    t.id="wrong";check(!valid(catalog),"same exact selected template required");t.id="private";
    interactions.dialogues[0].context_role="role";check(!valid(catalog),"new policy uses real private template");interactions.dialogues[0].context_role.clear();
    auto changed=json.value;auto& row=changed.find("requests")->as<BoundaryValue::Array>()->front();
    *row.find("consent_kind")=BoundaryValue{"successor_accepts_death_control"};check(!decode_voluntary_path_catalog(changed).ok,"death consent is not living consent");
    changed=json.value;changed.as<BoundaryValue::Dict>()->push_back({"version",BoundaryValue{Integer{1}}});check(!decode_voluntary_path_catalog(changed).ok,"duplicate JSON field rejected");
    for(auto kind:{VoluntaryRelationKind::adoption,VoluntaryRelationKind::biological_child,VoluntaryRelationKind::accepted_disciple,VoluntaryRelationKind::recognized_collateral})
        check(decode_voluntary_relation_kind(voluntary_relation_kind_name(kind)).value==kind,"four exact relation tags");
    check(!decode_voluntary_relation_kind("same_surname").ok,"same surname never authorizes a path");
    using namespace boundary;const auto empty_definitions=obj({});auto profile=obj({{"profile",BoundaryValue{"living_accepted_family_paths_v2"}},{"request_ids",BoundaryValue{BoundaryValue::Array{}}}});
    auto no_old=decode_living_succession_catalog(profile,empty_definitions);check(no_old.ok&&!no_old.value.enabled&&no_old.value.definitions.empty(),"new explicit profile may have no original role requests");
    check(!decode_succession_consent_catalog(profile,empty_definitions).ok,"original decoder rejects new profile");
    auto wrong=profile;*wrong.find("profile")=BoundaryValue{"authored_voluntary_succession_v1"};check(!decode_living_succession_catalog(wrong,empty_definitions).ok,"new decoder does not silently choose old semantics");
    wrong=profile;wrong.as<BoundaryValue::Dict>()->emplace_back("unknown",BoundaryValue{true});check(!decode_living_succession_catalog(wrong,empty_definitions).ok,"new catalog unknown keys rejected");
    wrong=profile;*wrong.find("request_ids")=string_array({"missing"});check(!decode_living_succession_catalog(wrong,empty_definitions).ok,"selected old declaration still required from original catalog");
    std::cout<<"VOLUNTARY_PATH_CATALOG "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
