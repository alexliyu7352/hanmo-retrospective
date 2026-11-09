// 新卡片模块只在显式profile写冷块；本门运行本域codec与初态，不冒充完整世界学习。
#include "resident/domains/growth/growth_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// Release下同样保留正式拒绝码，避免测试空结果假通过。
void check(bool valid,const char* reason){++checks;if(!valid)throw std::runtime_error(reason);}
// 源记录来自明确初态已掌握能力，测试不修改经验、分配人物或重复取得奖励。
void module_boundary(){
    CommonState common;common.metadata.scenario_id="cards";common.metadata.player_actor_id="student";
    ActorRecord person;person.id="student";person.combat.learned.insert("heart",20);common.actors.insert("student",person);
    GrowthCatalog catalog;catalog.cards.enabled=true;GrowthCardDefinition definition;definition.id="card.heart";definition.category=GrowthCardCategory::heart;definition.personal_skill_id="heart";catalog.cards.definitions.insert(definition.id,definition);catalog.cards.initial_grants.push_back({"student",definition.id,{}});
    GrowthState state;check(initialize_growth_card_state(common,state,catalog).ok&&state.cards.has_value(),"explicit profile initializes source ledger");
    const auto snapshot=encode_growth_cards(*state.cards);check(!initialize_growth_card_state(common,state,catalog).ok&&compare_boundary(snapshot,encode_growth_cards(*state.cards)).exact(),"initialization cannot overwrite existing source ledger");
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(visit_growth_checkpoint(writer,state,false,true).ok&&writer.finish().ok,"enabled module writes typed growth payload");
    GrowthState restored;const auto& input=bytes;CodecVisitor reader(input);check(visit_growth_checkpoint(reader,restored,false,true).ok&&reader.finish().ok,"enabled module reads typed growth payload");
    check(restored.cards&&compare_boundary(snapshot,encode_growth_cards(*restored.cards)).exact(),"source fields survive enclosing growth checkpoint");
    CodecVisitor::Buffer second;CodecVisitor rewriter(second);check(visit_growth_checkpoint(rewriter,restored,false,true).ok&&rewriter.finish().ok&&second==bytes,"new typed checkpoint is byte stable");
    GrowthState absent;CodecVisitor::Buffer rejected;CodecVisitor missing(rejected);auto status=visit_growth_checkpoint(missing,absent,false,true);check(!status.ok&&status.error.original.code=="MissingGrowthCards","enabled writer rejects absent block");
    CodecVisitor old_writer(rejected);status=visit_growth_checkpoint(old_writer,state);check(!status.ok&&status.error.original.code=="CardsCapabilityRequired","legacy writer rejects extension instead of dropping it");
    GrowthState old_state;CodecVisitor old_reader(input);status=visit_growth_checkpoint(old_reader,old_state);check(!status.ok||!old_reader.finish().ok,"new payload cannot be accepted as legacy payload");
    CodecVisitor::Buffer legacy;CodecVisitor legacy_writer(legacy);check(visit_growth_checkpoint(legacy_writer,absent).ok&&legacy_writer.finish().ok,"legacy empty state writes without card block");
    GrowthState accidental;const auto& old_input=legacy;CodecVisitor promoted(old_input);status=visit_growth_checkpoint(promoted,accidental,false,true);check(!status.ok||!promoted.finish().ok,"legacy payload cannot silently become card profile");
    GrowthCatalog legacy_catalog;check(!initialize_growth_card_state(common,state,legacy_catalog).ok,"legacy initialization rejects existing extension");
    check(common.identity.tick==0&&common.next_instance_id==1&&*common.actors.find("student")->combat.learned.find("heart")==20,"source operations do not change identity time or mastery");
}
}
// 本门不链接旧resident archive，源码全部以同一新GrowthState头编译。
int main(){try{module_boundary();std::cout<<"GROWTH_CARDS_MODULE "<<checks<<" checks 0 failures\n";return 0;}catch(const std::exception& error){std::cerr<<"GROWTH_CARDS_MODULE FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
