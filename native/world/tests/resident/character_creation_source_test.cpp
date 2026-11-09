// 仅检查新身份与初财的事务边界；不构造旧 ABI 的 WorldBundle。
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <fstream>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}}
int main(int argc,char** argv){try{
    auto shared=std::make_shared<CommonCatalog>();shared->places.insert("home",{"home","settlement",{}});shared->items.insert("grain",{"grain",1,1,false,false});
    CommonState seed;seed.metadata.scenario_id="scenario.creation";seed.metadata.player_actor_id="prototype";
    FamilyCatalog catalog;catalog.founder_selection=FamilyFounderSelection::created_character;catalog.founder_actor_id="prototype";catalog.prototypes.insert("prototype",{});catalog.character_creation.emplace();
    auto& config=*catalog.character_creation;config.prototype_actor_id="prototype";config.starting_place_id="home";config.allowed_modes={CharacterCreationMode::standard,CharacterCreationMode::free};config.initial_money=100;config.initial_goods.push_back({"grain",2});
    CharacterCreationAppearance appearance;appearance.id="first";appearance.attire_style="plain";appearance.features.present=true;
    for(auto field:{&AppearanceFeatures::face_shape,&AppearanceFeatures::eyes,&AppearanceFeatures::brows,&AppearanceFeatures::nose,&AppearanceFeatures::mouth,&AppearanceFeatures::hair,&AppearanceFeatures::beard,&AppearanceFeatures::skin_palette,&AppearanceFeatures::build})appearance.features.*field="authored";
    config.appearance_choices.insert(appearance.id,appearance);
    CharacterCreationDraft draft;draft.display_name="初客";draft.age_years=23;draft.attributes={60,60,60,60,60};draft.skills.infantry=300;draft.skills.commerce=300;
    CommonStore common(seed,shared);FamilyState family;InteractionState interaction;SocialState social;
    const auto sequence=common.state().next_instance_id;
    check(inspect_character_creation_budget(draft,false).ok&&common.state().next_instance_id==sequence&&common.state().actors.empty()&&common.state().wallets.empty(),"preview creates no actor or assets");
    auto bad=draft;bad.attributes.charisma=61;check(!initialize_character_creation(common,family,interaction,social,catalog,*shared,bad,"first").ok&&common.state().next_instance_id==sequence&&common.state().wallets.empty(),"over budget rejects before grants");
    bad=draft;bad.skills.extended.emplace();check(!validate_character_creation_draft(bad,false).ok,"client cannot turn on extended skills");
    auto made=initialize_character_creation(common,family,interaction,social,catalog,*shared,draft,"first");check(made.ok,"authorised creation succeeds");
    check(common.state().actors.size()==1&&common.wallet("personal:"+made.value)->balance==100&&common.inventory("bag:"+made.value)->lots.size()==1&&common.inventory("bag:"+made.value)->lots.front().quantity==2,"one person receives exact authored initial assets");
    check(common.state().metadata.player_actor_id==made.value&&family.founder_actor_id==made.value&&common.state().actors.find(made.value)->birth_tick==-23*720,"new player has actual generated identity and birthday");
    const auto after=common.state().next_instance_id;auto again=initialize_character_creation(common,family,interaction,social,catalog,*shared,draft,"first");
    check(again.ok&&again.value==made.value&&common.state().next_instance_id==after&&common.wallet("personal:"+made.value)->balance==100,"same source cannot grant a second identity or wallet");
    bad=draft;bad.display_name="别名";check(!initialize_character_creation(common,family,interaction,social,catalog,*shared,bad,"first").ok&&common.state().actors.size()==1,"changed replay conflicts");
    const auto source_id=family.facts.rows().front().first;const auto& source=std::get<CharacterCreationFact>(family.facts.rows().front().second);
    auto decoded=decode_character_creation_fact(encode_character_creation_fact(source,false),false);check(same_character_creation_draft(decoded.draft,draft,false)&&decoded.actor_id==made.value,"JSON retains exact original draft");
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);auto cold=source;check(visit_character_creation_fact(writer,cold,false).ok&&writer.finish().ok,"typed creation source writes");
    CharacterCreationFact restored;const auto& input=bytes;CodecVisitor reader(input);check(visit_character_creation_fact(reader,restored,false).ok&&reader.finish().ok&&same_character_creation_draft(restored.draft,draft,false),"typed source recovers draft");
    FamilyState forged=family;std::get<CharacterCreationFact>(*forged.facts.find(source_id)).draft.attributes.charisma=61;check(!validate_character_creation_source(common,forged,catalog,*shared).ok,"bad cold creation budget is rejected");
    auto old=catalog;old.character_creation.reset();check(!validate_character_creation_source(common,family,old,*shared).ok,"old profile cannot accept new source");
    CommonStore failed(seed,shared);failed.state().facts.insert("orphan",{"orphan","family_character_creation","family",{}});FamilyState empty;InteractionState empty_interaction;SocialState empty_social;
    auto rejected=initialize_character_creation(failed,empty,empty_interaction,empty_social,catalog,*shared,draft,"first");
    check(!rejected.ok&&failed.state().next_instance_id==sequence&&failed.state().actors.empty()&&failed.state().wallets.empty()&&failed.state().inventories.empty()&&failed.state().facts.size()==1&&failed.state().metadata.player_actor_id=="prototype"&&empty.facts.empty()&&empty_interaction.knowledge.empty()&&empty_social.memories.empty(),"late failure rolls back IDs player resources and neighbouring domains");
    auto exhausted_seed=seed;exhausted_seed.next_instance_id=std::numeric_limits<Integer>::max()-1;CommonStore exhausted(exhausted_seed,shared);FamilyState no_family;InteractionState no_interaction;SocialState no_social;
    const auto before_exhausted=encode_common_state(exhausted.state());
    auto exhausted_result=initialize_character_creation(exhausted,no_family,no_interaction,no_social,catalog,*shared,draft,"first");
    check(!exhausted_result.ok&&compare_boundary(before_exhausted,encode_common_state(exhausted.state())).exact()&&no_family.facts.empty()&&no_interaction.knowledge.empty()&&no_social.memories.empty(),"second ID exhaustion restores complete Common and adjacent ledgers");
    for(bool household:{false,true}){CodecVisitor::Buffer family_bytes;CodecVisitor fw(family_bytes);auto written=family;check(visit_family_checkpoint(fw,written,household,true,false).ok&&fw.finish().ok,"Family3/4 explicit source encodes");const auto& input_family=family_bytes;CodecVisitor fr(input_family);FamilyState recovered;check(visit_family_checkpoint(fr,recovered,household,true,false).ok&&fr.finish().ok&&validate_character_creation_source(common,recovered,catalog,*shared).ok,"Family3/4 preserves source and identity");CodecVisitor wrong(input_family);FamilyState legacy;check(!visit_family_checkpoint(wrong,legacy,household,false,false).ok,"Family1/2 cannot accept new source tag");}
    auto ordered_source=source;ordered_source.field_order={"kind","actor_id","prototype_actor_id","scenario_id","appearance_choice_id","created_tick","draft"};CodecVisitor::Buffer malformed_order;CodecVisitor order_writer(malformed_order);check(visit_character_creation_fact(order_writer,ordered_source,false).ok&&order_writer.finish().ok,"valid complete source field order writes");
    const StableId key="kind";auto found_key=std::find_end(malformed_order.begin(),malformed_order.end(),key.begin(),key.end());check(found_key!=malformed_order.end(),"locate final serialized order key without changing field headers");std::copy_n("nope",4,found_key);if(argc==2){std::ofstream raw(argv[1],std::ios::binary);raw.write(reinterpret_cast<const char*>(malformed_order.data()),static_cast<std::streamsize>(malformed_order.size()));if(!raw)throw std::runtime_error("cannot preserve malformed source bytes");}const auto& malformed_input=malformed_order;CodecVisitor bad_order_reader(malformed_input);CharacterCreationFact bad_order;check(!visit_character_creation_fact(bad_order_reader,bad_order,false).ok,"typed source rejects unknown saved field order before export");
    CommonStore free_world(seed,shared);FamilyState free_family;InteractionState free_interaction;SocialState free_social;draft.mode=CharacterCreationMode::free;draft.attributes.charisma=100;draft.skills.infantry=1200;
    check(initialize_character_creation(free_world,free_family,free_interaction,free_social,catalog,*shared,draft,"first").ok,"explicit free creation uses same authored grants");
    free_world.state().metadata.player_actor_id="later_successor";const auto mode=family_character_creation_mode(free_world,free_family,catalog,*shared);check(mode.ok&&mode.value==CharacterCreationMode::free,"succession does not launder free-world origin");
    std::cout<<"CHARACTER_CREATION_SOURCE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"CHARACTER_CREATION_SOURCE "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
