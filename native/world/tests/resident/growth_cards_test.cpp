// 卡片来源、真实掌握与跨局发现的边界窄门；不模拟世界命令或冒充七类效果已经完成。
#include "resident/domains/growth/growth_cards.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// 每个业务失败保留原规则码，Release同样执行全部检查。
void check(bool value,const StableId& message){++checks;if(!value)throw std::runtime_error(message);}
// 无法构造正式结果时立即终止，不把缺失证明归一化成空卡片列表。
template<class T>T take(Outcome<T> value){if(!value.ok)throw std::runtime_error(value.error.original.code+":"+value.error.original.path);return std::move(value.value);}
// 最小真实人物和目录输入；卡片只是同一人物已学武功/心法的来源记录。
struct Fixture {
    CommonState common;CommonCatalog shared;GrowthState growth;GrowthCardsState cards;GrowthCardCatalog catalog;
    Fixture(){
        common.metadata.scenario_id="scenario.cards";common.metadata.player_actor_id="student";
        for(const auto* id:{"student","teacher"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;common.actors.insert(id,actor);}
        common.actors.find("student")->combat.learned.insert("ability.strike",30);
        shared.personal_skill_kinds.insert("ability.strike","strike");shared.personal_skill_kinds.insert("ability.heart","heart");
        catalog.enabled=true;
        for(const auto& pair:{std::pair<const char*,GrowthCardCategory>{"strike",GrowthCardCategory::technique},{"heart",GrowthCardCategory::heart}}){GrowthCardDefinition definition;definition.id=StableId{"card."}+pair.first;definition.category=pair.second;definition.personal_skill_id=StableId{"ability."}+pair.first;definition.name_key=definition.id+".name";definition.description_key=definition.id+".description";definition.hint_key=definition.id+".hint";shared.texts.insert(definition.name_key,"本领名称");shared.texts.insert(definition.description_key,"实际本领说明");shared.texts.insert(definition.hint_key,"向合资格教师求学");catalog.definitions.insert(definition.id,definition);}
        catalog.initial_grants.push_back({"student","card.strike",{}});
    }
    // 输入源代表已经通过成长结算的事实；本测试只核卡片消费者，不据此声称授课本身已运行。
    void completion(const StableId& id="action.learn",bool success=true){TrainingFact fact;fact.actor_id="student";fact.action_kind="learn_skill";fact.phase="completed";fact.start_tick=0;fact.completed_tick=1;fact.success=success;fact.training.skill_id="ability.heart";fact.training.student_id="student";fact.training.teacher_id="teacher";fact.training.mastery_before=-1;fact.training.mastery_after=0;growth.facts.insert(id,fact);if(success)common.actors.find("student")->combat.learned.set("ability.heart",0);}
    // 固定本人视图不接受借图鉴读取其他人的能力账。
    BoundaryValue view(const std::vector<StableId>& discovered={}){return take(growth_cards_view(common,cards,catalog,shared,"student",discovered));}
};
// 已有能力来源和图鉴发现清楚分离，打开/分页页面不替人物创建任何本领。
void discovery_and_acquisition(){
    Fixture f;check(validate_growth_card_catalog(f.catalog,f.shared).ok,"valid implemented card catalog");
    check(initialize_growth_cards(f.common,f.cards,f.catalog).ok,"explicit initial learned ability records source");
    auto view=f.view({"card.heart"});const auto& rows=boundary::array(boundary::field(view,"cards","view"),"cards");
    const auto& heart=rows[0];check(boundary::get_string(heart,"card_id","")=="card.heart"&&boundary::get_bool(heart,"discovered","")&&!boundary::get_bool(heart,"learned","")&&!boundary::get_bool(heart,"acquired_this_world",""),"global discovery does not grant current-world ability");
    check(!f.common.actors.find("student")->combat.learned.find("ability.heart")&&f.common.next_instance_id==1&&f.common.identity.tick==0,"queries preserve actual identity and time");
    f.completion();auto acquired=take(record_growth_training_cards(f.common,f.growth,f.cards,f.catalog,"action.learn"));check(acquired==std::vector<StableId>{"card.heart"},"real completed learning records one card for student");
    f.common.identity.tick=1;check(validate_growth_cards(f.common,f.growth,f.cards,f.catalog).ok,"published boundary validates sources");
    check(take(record_growth_training_cards(f.common,f.growth,f.cards,f.catalog,"action.learn")).empty(),"replay never duplicates acquisition");
    check(!f.cards.owned.find("teacher"),"teacher does not receive students card");
    auto* person=f.common.actors.find("student");person->combat.heart_id="ability.heart";view=f.view();check(boundary::get_bool(boundary::array(boundary::field(view,"cards",""),"")[0],"configured",""),"configured state reads actual ActorCombat");
    const auto other=growth_cards_view(f.common,f.cards,f.catalog,f.shared,"teacher",{});check(!other.ok&&other.error.original.code=="NotPlayerActor","private card page is self only");
}
// 未完成/失败/伪造来源和坏作者引用不能发卡，尚未接入的五类效果不能默认成功。
void rejections(){
    Fixture f;f.completion("action.failed",false);auto result=record_growth_training_cards(f.common,f.growth,f.cards,f.catalog,"action.failed");check(!result.ok&&f.cards.owned.empty(),"failed learning yields no acquisition");
    f.completion("action.pending");std::get<TrainingFact>(*f.growth.facts.find("action.pending")).phase="started";result=record_growth_training_cards(f.common,f.growth,f.cards,f.catalog,"action.pending");check(!result.ok&&f.cards.owned.empty(),"pending learning yields no acquisition");
    result=record_growth_training_cards(f.common,f.growth,f.cards,f.catalog,"missing");check(!result.ok&&f.cards.owned.empty(),"missing source is not a grant");
    Fixture bad;bad.common.actors.find("student")->combat.learned.erase("ability.strike");check(!initialize_growth_cards(bad.common,bad.cards,bad.catalog).ok&&bad.cards.owned.empty(),"initial card cannot create missing ability");
    Fixture missing;missing.catalog.initial_grants.clear();check(!initialize_growth_cards(missing.common,missing.cards,missing.catalog).ok&&missing.cards.owned.empty(),"initial learned ability needs its explicit card source");
    for(auto category:{GrowthCardCategory::actor,GrowthCardCategory::skill,GrowthCardCategory::tactic,GrowthCardCategory::title,GrowthCardCategory::insight}){auto catalog=f.catalog;catalog.definitions.find("card.heart")->category=category;auto status=validate_growth_card_catalog(catalog,f.shared);check(!status.ok&&status.error.kind==ErrorKind::unsupported_capability,"unimplemented category effect is explicitly unsupported");}
    auto catalog=f.catalog;catalog.definitions.find("card.heart")->personal_skill_id="ability.strike";check(!validate_growth_card_catalog(catalog,f.shared).ok,"category cannot alias wrong ability kind");
    check(!growth_card_category("Heart").ok&&take(growth_card_category("heart"))==GrowthCardCategory::heart,"categories are exact closed identifiers");
    check(!growth_cards_view(f.common,f.cards,f.catalog,f.shared,"student",{},0,65).ok,"card page is bounded");
}
// 真实来源的JSON/typed冷块逐字保留，删除整条取得记录和把首次来源改晚都不能通过。
void persistence_and_missing_sources(){
    Fixture f;check(initialize_growth_cards(f.common,f.cards,f.catalog).ok,"initialize cold fixture");f.completion();take(record_growth_training_cards(f.common,f.growth,f.cards,f.catalog,"action.learn"));f.common.identity.tick=1;
    const auto before=encode_growth_cards(f.cards);auto decoded=take(decode_growth_cards(before));check(compare_boundary(before,encode_growth_cards(decoded)).exact(),"JSON round trip preserves all card records");
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(visit_growth_cards_checkpoint(writer,decoded).ok&&writer.finish().ok,"typed card writer succeeds");GrowthCardsState restored;const auto& input=bytes;CodecVisitor reader(input);check(visit_growth_cards_checkpoint(reader,restored).ok&&reader.finish().ok,"typed card reader succeeds");check(compare_boundary(before,encode_growth_cards(restored)).exact()&&validate_growth_cards(f.common,f.growth,restored,f.catalog).ok,"cold source and field order remain exact");
    auto missing=restored;missing.owned.find("student")->erase("card.heart");auto valid=validate_growth_cards(f.common,f.growth,missing,f.catalog);check(!valid.ok&&valid.error.original.code=="MissingCardAcquisition","deleting completed learning card is rejected");
    missing=restored;missing.owned.find("student")->erase("card.strike");valid=validate_growth_cards(f.common,f.growth,missing,f.catalog);check(!valid.ok&&valid.error.original.code=="MissingInitialCardAcquisition","deleting initial card is rejected");
    missing=restored;missing.owned.find("student")->find("card.heart")->source_id="forged";check(!validate_growth_cards(f.common,f.growth,missing,f.catalog).ok,"forged source is rejected");
    auto later=std::get<TrainingFact>(*f.growth.facts.find("action.learn"));later.action_kind="teach_skill";later.actor_id="teacher";later.start_tick=1;later.completed_tick=3;later.training.mastery_before=0;later.training.mastery_after=10;f.growth.facts.insert("action.later",later);f.common.identity.tick=3;f.common.actors.find("student")->combat.learned.set("ability.heart",10);
    missing=restored;auto* record=missing.owned.find("student")->find("card.heart");record->source_id="action.later";record->acquired_tick=3;valid=validate_growth_cards(f.common,f.growth,missing,f.catalog);check(!valid.ok&&valid.error.original.code=="CardFirstSourceMismatch","later real source cannot replace first acquisition");
    auto malformed=before;*malformed.find("owned")->find("student")->find("card.heart")->find("source_kind")=BoundaryValue{"arbitrary_grant"};check(!decode_growth_cards(malformed).ok,"unknown cold source kind rejected");
    check(take(record_growth_training_cards(f.common,f.growth,f.cards,f.catalog,"action.later")).empty()&&f.cards.owned.find("student")->find("card.heart")->source_id=="action.learn","later training keeps original acquisition");
}
// 映射能力本身也必须有来源，不能伪造learned而同时省略取得账和所有教学事实。
void missing_ability_source(){
    Fixture fixture;check(initialize_growth_cards(fixture.common,fixture.cards,fixture.catalog).ok,"initialize source-integrity fixture");
    fixture.common.actors.find("student")->combat.learned.set("ability.heart",0);
    auto result=validate_growth_cards(fixture.common,fixture.growth,fixture.cards,fixture.catalog);check(!result.ok&&result.error.original.code=="MissingCardAcquisition","unexplained learned ability cannot bypass acquisition ledger");
    fixture.completion();take(record_growth_training_cards(fixture.common,fixture.growth,fixture.cards,fixture.catalog,"action.learn"));fixture.common.identity.tick=1;
    check(validate_growth_cards(fixture.common,fixture.growth,fixture.cards,fixture.catalog).ok,"actual completed source validates same learned ability");
}
// 正式作者叶拒绝未知效果、重复收益与私加脚本字段；目录只引用已存在的能力和文本。
void author_contract(){
    using namespace boundary;Fixture f;BoundaryValue::Dict definitions;
    for(const auto& row:f.catalog.definitions.rows()){const auto& definition=row.second;definitions.emplace_back(row.first,obj({{"id",{row.first}},{"category",{growth_card_category_name(definition.category)}},{"name_key",{definition.name_key}},{"description_key",{definition.description_key}},{"prerequisites",obj({{"kind",{StableId{"personal_ability_eligibility"}}}})},{"acquire_condition",obj({{"kind",{StableId{"initial_or_completed_training"}}}})},{"hint_key",{definition.hint_key}},{"icon_id",{StableId{}}},{"effect_kind",{StableId{"personal_ability"}}},{"effect_params",obj({{"personal_skill_id",{definition.personal_skill_id}}})},{"repeat_policy",{StableId{"once_per_actor"}}}}));}
    BoundaryValue data{std::move(definitions)};auto config=obj({{"profile",{StableId{"learned_ability_cards_v1"}}},{"initial_grants",BoundaryValue{BoundaryValue::Array{obj({{"actor_id",{StableId{"student"}}},{"card_id",{StableId{"card.strike"}}}})}}}});
    check(decode_growth_card_catalog(data,config,f.shared).ok,"implemented author card contract admitted");
    auto bad=data;*bad.find("card.heart")->find("effect_kind")=BoundaryValue{"mint_money"};auto result=decode_growth_card_catalog(bad,config,f.shared);check(!result.ok&&result.error.kind==ErrorKind::unsupported_capability,"unknown effect never defaults to success");
    bad=data;*bad.find("card.heart")->find("repeat_policy")=BoundaryValue{"stack"};check(!decode_growth_card_catalog(bad,config,f.shared).ok,"repeated effect stacking is unsupported");
    bad=data;*bad.find("card.heart")->find("prerequisites")->find("kind")=BoundaryValue{"always"};check(!decode_growth_card_catalog(bad,config,f.shared).ok,"author cannot bypass actual ability prerequisites");
    bad=data;*bad.find("card.heart")->find("acquire_condition")->find("kind")=BoundaryValue{"on_open_page"};check(!decode_growth_card_catalog(bad,config,f.shared).ok,"opening a page cannot become an acquisition source");
    bad=data;bad.find("card.heart")->find("effect_params")->as<BoundaryValue::Dict>()->emplace_back("script",BoundaryValue{"untrusted"});check(!decode_growth_card_catalog(bad,config,f.shared).ok,"effect parameters cannot carry arbitrary script");
}
}
// 本门只验证卡片消费者；真实学习自动登记、持久codec和UI仍由后继闭环接入。
int main(int argc,char** argv){try{if(argc==2&&StableId{argv[1]}=="--source-integrity"){missing_ability_source();std::cout<<"GROWTH_CARDS_SOURCE "<<checks<<" checks 0 failures\n";return 0;}if(argc==2&&StableId{argv[1]}=="--author-contract"){author_contract();std::cout<<"GROWTH_CARDS_AUTHOR "<<checks<<" checks 0 failures\n";return 0;}discovery_and_acquisition();rejections();persistence_and_missing_sources();author_contract();std::cout<<"GROWTH_CARDS "<<checks<<" checks 0 failures\n";return 0;}catch(const std::exception& error){std::cerr<<"GROWTH_CARDS FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
