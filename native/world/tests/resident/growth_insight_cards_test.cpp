#include "resident/domains/growth/growth_insight_cards.hpp"
#include <iostream>
using namespace hanmo::resident;
using namespace boundary;
namespace {
using V=BoundaryValue;int checks=0,failures=0;
void check(bool value,const char* label){++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
V definition(){return obj({{"id",V{"insight:camp"}},{"category",V{"insight"}},{"name_key",V{"name"}},{"description_key",V{"description"}},
    {"hint_key",V{"hint"}},{"icon_id",V{""}},{"prerequisites",obj({{"kind",V{"current_player_participation"}}})},
    {"acquire_condition",obj({{"kind",V{"completed_camp_training_v1"}}})},{"effect_kind",V{"record_insight"}},
    {"effect_params",obj({{"kind",V{"camp_training"}}})},{"repeat_policy",V{"once_per_actor_world"}}});}
const V& first(const V& page){return array(field(page,"cards","page"),"page/cards").front();}
}
int main(){
    CommonState common;common.metadata.player_actor_id="player";common.metadata.scenario_id="camp-life";
    common.identity.world_id[0]=1;common.identity.tick=10;common.identity.revision=4;common.identity.execution_epoch=7;
    ActorRecord actor;actor.id="player";common.actors.insert(actor.id,actor);
    CommonCatalog shared;shared.texts.insert("name","营中一日");shared.texts.insert("description","曾在军营实际完成训练。");shared.texts.insert("hint","合法到营并完成一次训练。");
    V definitions=obj({{"insight:camp",definition()}});V config=obj({{"profile",V{"completed_camp_training_insights_v1"}},{"card_ids",V{V::Array{V{"insight:camp"}}}}});
    auto catalog=decode_growth_insight_card_catalog(definitions,config,shared);
    check(catalog.ok&&catalog.value.enabled,"explicit selected VI04 author definition accepted");if(!catalog.ok)return 1;
    auto bad=config;*bad.find("profile")=V{"learned_ability_cards_v1"};check(!decode_growth_insight_card_catalog(definitions,bad,shared).ok,"old ability profile cannot implicitly enable insights");
    check(!decode_growth_insight_card_catalog(V{V::Dict{}},config,shared).ok,"missing selected table entry rejected");
    bad=definitions;*bad.find("insight:camp")->find("effect_kind")=V{"personal_ability"};check(!decode_growth_insight_card_catalog(bad,config,shared).ok,"unsupported ability effect cannot execute through insight card");
    MilitaryState military;auto view=growth_insight_cards_view(common,shared,military,catalog.value,"player",{});
    check(view.ok&&!get_bool(first(view.value),"discovered","row")&&!get_bool(first(view.value),"experienced_this_world","row"),"new untrained world has no experience");
    auto remembered=growth_insight_cards_view(common,shared,military,catalog.value,"player",{"insight:camp"});
    check(remembered.ok&&get_bool(first(remembered.value),"discovered","row")&&!get_bool(first(remembered.value),"acquired_this_world","row")&&first(remembered.value).find("experience")->as<std::monostate>(),"cross-world discovery reveals only card text, never local experience");
    MilitaryTrainingFact fact;fact.id="training:1";fact.actor_id="instructor";fact.phase="completed";fact.success=true;fact.start_tick=2;fact.completed_tick=6;fact.lessons.resize(4);
    fact.terms.participants={"instructor","player"};fact.terms.garrison_id="camp";fact.terms.place_id="camp-yard";fact.terms.unit_id="unit";military.training_facts.push_back(fact);
    auto acquired=growth_insight_cards_view(common,shared,military,catalog.value,"player",{});check(acquired.ok,"actual participant source available to finite page");
    if(acquired.ok){const auto& row=first(acquired.value);const auto& source=field(row,"experience","row");
        check(get_bool(row,"experienced_this_world","row")&&get_int(row,"acquired_tick","row")==6&&get_string(source,"actor_id","source")=="player"&&get_string(source,"fact_id","source")=="training:1","local experience is bound to same person and original completed fact");
        check(!row.find("learned")&&!row.find("personal_skill_id")&&!source.find("office_id")&&!source.find("execution_epoch"),"insight does not claim learned ability, office or permanent execution epoch");
        auto again=growth_insight_cards_view(common,shared,military,catalog.value,"player",{});check(again.ok&&compare_boundary(again.value,acquired.value).exact(),"repeated viewing leaves exact page and first source unchanged");}
    check(!growth_insight_cards_view(common,shared,military,catalog.value,"instructor",{}).ok,"other actors remain outside own insight page");
    auto end=growth_insight_cards_view(common,shared,military,catalog.value,"player",{},1,1);check(end.ok&&array(field(end.value,"cards","page"),"page/cards").empty()&&!growth_insight_cards_view(common,shared,military,catalog.value,"player",{},2,1).ok,"bounded paging accepts end and rejects overflow");
    auto disabled=catalog.value;disabled.enabled=false;check(!growth_insight_cards_view(common,shared,military,disabled,"player",{}).ok,"passive definition does not expose unregistered module");
    std::cout<<"GROWTH_INSIGHT_CARDS "<<checks<<" checks "<<failures<<" failures; COMPONENT_ONLY\n";return failures?1:0;
}
