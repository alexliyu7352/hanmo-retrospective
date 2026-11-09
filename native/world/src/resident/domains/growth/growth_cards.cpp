#include "resident/kernel/catalog_section_observation.hpp"
// 卡片来源账只引用真实初态和成长事实；没有任意授能力、加经验、发钱或换人的入口。
#include "growth_cards.hpp"
#include "growth_model.hpp"
#include <set>

namespace hanmo::resident {
namespace {
// 卡片类别是数据合同，当前尚未接入效果的分类不能借目录校验假成功。
bool supported(GrowthCardCategory category){return category==GrowthCardCategory::technique||category==GrowthCardCategory::heart;}
// 初始化与训练完成分别提供来源，首次取得后保持原记录，重复调用只返回无新增。
Status insert_acquisition(GrowthCardsState& state,const GrowthCardAcquisition& acquisition,bool& inserted){
    auto* cards=state.owned.find(acquisition.actor_id);
    if(!cards){state.owned.insert(acquisition.actor_id,{});cards=state.owned.find(acquisition.actor_id);}
    if(cards->find(acquisition.card_id)){inserted=false;return common_success();}
    cards->insert(acquisition.card_id,acquisition);inserted=true;return common_success();
}
// 当前能力仍来自原人物；不把存在卡片记录或图鉴当成学会本领。
std::optional<Integer> mastery(const CommonState& common,const StableId& actor,const StableId& skill){
    const auto* person=common.actors.find(actor);const auto* value=person?person->combat.learned.find(skill):nullptr;
    return value?std::optional<Integer>{*value}:std::nullopt;
}
// 训练来源必须真实完成且使此学生学到能力，练习不能替不存在的学习来源发卡。
const TrainingFact* learned_source(const GrowthState& growth,const StableId& source){
    const auto* value=growth.facts.find(source);const auto* fact=value?std::get_if<TrainingFact>(value):nullptr;
    if(!fact||fact->phase!="completed"||!fact->success||(fact->action_kind!="learn_skill"&&fact->action_kind!="teach_skill")||fact->training.mastery_after<0||fact->training.mastery_after>100)return nullptr;
    return fact;
}
}

// 七种分类与作者标签一一对应，不接受未知标签或忽略大小写。
StableId growth_card_category_name(GrowthCardCategory category){
    switch(category){case GrowthCardCategory::actor:return "actor";case GrowthCardCategory::skill:return "skill";case GrowthCardCategory::technique:return "technique";case GrowthCardCategory::heart:return "heart";case GrowthCardCategory::tactic:return "tactic";case GrowthCardCategory::title:return "title";case GrowthCardCategory::insight:return "insight";}
    return {};
}
Outcome<GrowthCardCategory> growth_card_category(const StableId& name){
    for(auto category:{GrowthCardCategory::actor,GrowthCardCategory::skill,GrowthCardCategory::technique,GrowthCardCategory::heart,GrowthCardCategory::tactic,GrowthCardCategory::title,GrowthCardCategory::insight})if(growth_card_category_name(category)==name)return {true,category,{}};
    return {false,{},common_failure("UnknownCardCategory",name).error};
}

// 当前目录只允许已实现的武功/心法能力引用，全部名字和来源必须来自同一有效目录。
Status validate_growth_card_catalog(const GrowthCardCatalog& catalog,const CommonCatalog& common){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 42, "GrowthCardCatalog", "validate_growth_card_catalog", &catalog);

    if(!catalog.enabled)return catalog.definitions.empty()&&catalog.initial_grants.empty()?common_success():common_failure("CardsCapabilityRequired");
    if(catalog.definitions.empty()||catalog.definitions.size()>4096)return common_failure("InvalidCardCatalogSize");
    std::set<StableId> abilities,grants;
    for(const auto& row:catalog.definitions.rows()){
        const auto& definition=row.second;
        if(row.first.empty()||definition.id!=row.first)return common_failure("InvalidCardIdentity",row.first);
        if(!supported(definition.category))return common_failure("UnsupportedCardEffect",row.first,ErrorKind::unsupported_capability);
        const auto* kind=common.personal_skill_kinds.find(definition.personal_skill_id);
        if(!kind||((*kind=="heart")!=(definition.category==GrowthCardCategory::heart)))return common_failure("CardAbilityCategoryMismatch",row.first);
        if(!abilities.insert(definition.personal_skill_id).second)return common_failure("DuplicateCardAbility",row.first);
        for(const auto* key:{&definition.name_key,&definition.description_key,&definition.hint_key})if(key->empty()||!common.texts.find(*key))return common_failure("MissingCardText",row.first);
    }
    if(catalog.initial_grants.size()>16384)return common_failure("InitialCardGrantLimit");
    for(const auto& grant:catalog.initial_grants){
        if(grant.actor_id.empty()||!catalog.definitions.find(grant.card_id)||!grants.insert(grant.actor_id+"\n"+grant.card_id).second)return common_failure("InvalidInitialCardGrant",grant.card_id);
    }
    return common_success();
}

// 原局初态已掌握的能力可以具有作者明确来源，重复初始化不增加卡片或重抽任何能力。
Status initialize_growth_cards(const CommonState& common,GrowthCardsState& state,const GrowthCardCatalog& catalog){
    if(!catalog.enabled)return state.owned.empty()?common_success():common_failure("CardsCapabilityRequired");
    if(common.identity.tick!=0||common.metadata.scenario_id.empty())return common_failure("InitialCardBoundaryRequired");
    auto candidate=state;
    for(const auto& grant:catalog.initial_grants){
        const auto* definition=catalog.definitions.find(grant.card_id);if(!definition||!supported(definition->category))return common_failure("InvalidInitialCardGrant",grant.card_id);
        const auto learned=mastery(common,grant.actor_id,definition->personal_skill_id);
        if(!learned||*learned<0||*learned>100)return common_failure("InitialCardAbilityRequired",grant.card_id);
        bool inserted=false;auto result=insert_acquisition(candidate,{grant.card_id,grant.actor_id,common.metadata.scenario_id,GrowthCardSourceKind::scenario_initial,0,{}},inserted);if(!result.ok)return result;
    }
    // 新profile不能把本人已学能力藏成未知卡；作者必须为每项对应的初态能力明确来源。
    for(const auto& actor:common.actors.rows())for(const auto& row:catalog.definitions.rows())if(mastery(common,actor.first,row.second.personal_skill_id)){
        const auto* owned=candidate.owned.find(actor.first);
        if(!owned||!owned->find(row.first))return common_failure("MissingInitialCardGrant",row.first);
    }
    state=std::move(candidate);return common_success();
}

// 新局显式启用后一次建立本域账；已有账必须由来源校验验证，不覆盖或重放初始授权。
Status initialize_growth_card_state(const CommonState& common,GrowthState& state,const GrowthCatalog& catalog){
    if(!catalog.cards.enabled)return state.cards?common_failure("CardsCapabilityRequired"):common_success();
    if(state.cards)return common_failure("GrowthCardsAlreadyInitialized");
    GrowthCardsState candidate;auto result=initialize_growth_cards(common,candidate,catalog.cards);
    if(!result.ok)return result;state.cards=std::move(candidate);return common_success();
}

// 同一成长结算只给实际学生登记首次取得；老师、旁观者和未结算动作都不会得到卡片。
Outcome<std::vector<StableId>> record_growth_training_cards(const CommonState& common,const GrowthState& growth,GrowthCardsState& state,const GrowthCardCatalog& catalog,const StableId& source){
    if(!catalog.enabled)return {true,{}, {}};
    const auto* fact=learned_source(growth,source);
    if(!fact||fact->completed_tick<0||fact->completed_tick>2147483647||common.identity.tick<0||common.identity.tick>2147483647||fact->completed_tick>common.identity.tick+1)return {false,{},common_failure("CardLearningSourceRequired",source).error};
    auto candidate=state;std::vector<StableId> acquired;
    for(const auto& row:catalog.definitions.rows()){
        const auto& definition=row.second;if(definition.personal_skill_id!=fact->training.skill_id)continue;
        if(!supported(definition.category))return {false,{},common_failure("UnsupportedCardEffect",row.first,ErrorKind::unsupported_capability).error};
        const auto current=mastery(common,fact->training.student_id,definition.personal_skill_id);
        if(!current||*current<fact->training.mastery_after||*current>100)return {false,{},common_failure("CardAbilitySourceMismatch",source).error};
        bool inserted=false;auto status=insert_acquisition(candidate,{row.first,fact->training.student_id,source,GrowthCardSourceKind::training,fact->completed_tick,{}},inserted);if(!status.ok)return {false,{},status.error};
        if(inserted)acquired.push_back(row.first);
    }
    state=std::move(candidate);return {true,std::move(acquired),{}};
}

// 冷恢复只验证来源，不因校验补登记、改人物或重新授予奖励。
Status validate_retained_legacy_growth_card_rows(const CommonState& common,const GrowthState& growth,const GrowthCardsState& state,const GrowthCardCatalog& catalog){
    if(!catalog.enabled)return state.owned.empty()?common_success():common_failure("CardsCapabilityRequired");
    for(const auto& actor_row:state.owned.rows()){
        if(!common.actors.find(actor_row.first)||actor_row.second.empty())return common_failure("InvalidCardOwner",actor_row.first);
        for(const auto& row:actor_row.second.rows()){
            const auto& record=row.second;const auto* definition=catalog.definitions.find(row.first);
            if(!definition||!supported(definition->category)||record.card_id!=row.first||record.actor_id!=actor_row.first||record.acquired_tick<0||record.acquired_tick>common.identity.tick)return common_failure("InvalidCardAcquisition",row.first);
            const auto learned=mastery(common,record.actor_id,definition->personal_skill_id);
            if(!learned||*learned<0||*learned>100)return common_failure("CardAbilitySourceMismatch",row.first);
            if(record.source_kind==GrowthCardSourceKind::scenario_initial){
                if(record.acquired_tick!=0||record.source_id!=common.metadata.scenario_id||std::none_of(catalog.initial_grants.begin(),catalog.initial_grants.end(),[&](const auto& grant){return grant.actor_id==record.actor_id&&grant.card_id==record.card_id;}))return common_failure("InvalidInitialCardSource",row.first);
            }else if(record.source_kind==GrowthCardSourceKind::training){
                const auto* fact=learned_source(growth,record.source_id);
                if(!fact||fact->training.student_id!=record.actor_id||fact->training.skill_id!=definition->personal_skill_id||fact->completed_tick!=record.acquired_tick)return common_failure("InvalidCardLearningSource",row.first);
            }else return common_failure("InvalidCardSourceKind",row.first);
        }
    }
    // 来源存在却删掉整条卡片记录也必须拒绝，不能只验证残留条目的内部字段。
    for(const auto& grant:catalog.initial_grants){
        const auto* owner=state.owned.find(grant.actor_id);const auto* record=owner?owner->find(grant.card_id):nullptr;
        if(!record||record->source_kind!=GrowthCardSourceKind::scenario_initial)return common_failure("MissingInitialCardAcquisition",grant.card_id);
    }
    return common_success();
}
Status validate_growth_cards(const CommonState& common,const GrowthState& growth,const GrowthCardsState& state,const GrowthCardCatalog& catalog){
    auto retained=validate_retained_legacy_growth_card_rows(common,growth,state,catalog);if(!retained.ok)return retained;
    std::unordered_map<StableId,StableId> cards_by_skill;
    for(const auto& row:catalog.definitions.rows())cards_by_skill.emplace(row.second.personal_skill_id,row.first);
    // 当前已学映射能力也必须有来源，省略整条教学事实和卡账不能绕过此门。
    for(const auto& actor:common.actors.rows())for(const auto& ability:actor.second.combat.learned.rows()){
        const auto card=cards_by_skill.find(ability.first);if(card==cards_by_skill.end())continue;
        const auto* owned=state.owned.find(actor.first);
        if(!owned||!owned->find(card->second))return common_failure("MissingCardAcquisition",actor.first+"/"+card->second);
    }
    for(const auto& row:growth.facts.rows()){
        const auto* fact=learned_source(growth,row.first);if(!fact)continue;
        const auto target=cards_by_skill.find(fact->training.skill_id);if(target==cards_by_skill.end())continue;
        const auto* owner=state.owned.find(fact->training.student_id);const auto* record=owner?owner->find(target->second):nullptr;
        if(!record)return common_failure("MissingCardAcquisition",row.first);
        if(record->acquired_tick>fact->completed_tick)return common_failure("CardFirstSourceMismatch",record->card_id);
    }
    return common_success();
}

// 有界目录页按稳定卡ID排序，只呈现本人的实际掌握；跨局发现永远不是使用权限。
Outcome<BoundaryValue> growth_cards_view(const CommonState& common,const GrowthCardsState& state,const GrowthCardCatalog& catalog,const CommonCatalog& shared,const StableId& actor_id,const std::vector<StableId>& discovered,Integer offset,Integer limit){
    using namespace boundary;
    const auto* actor=common.actors.find(actor_id);if(!actor)return {false,{},common_failure("UnknownActor",actor_id).error};
    if(actor_id!=common.metadata.player_actor_id)return {false,{},common_failure("NotPlayerActor",actor_id).error};
    if(!catalog.enabled)return {false,{},common_failure("CardsUnavailable",{},ErrorKind::unsupported_capability).error};
    if(offset<0||limit<1||limit>64||discovered.size()>4096)return {false,{},common_failure("InvalidCardPage").error};
    auto ids=catalog.definitions.order();std::sort(ids.begin(),ids.end());
    if(offset>Integer(ids.size()))return {false,{},common_failure("InvalidCardPage").error};
    const auto* owned=state.owned.find(actor_id);const std::set<StableId> known(discovered.begin(),discovered.end());BoundaryValue::Array rows;
    const auto end=std::min<Integer>(Integer(ids.size()),offset+limit);
    for(Integer index=offset;index<end;++index){const auto& id=ids[index];const auto& definition=*catalog.definitions.find(id);const auto* acquisition=owned?owned->find(id):nullptr;
        const auto actual=mastery(common,actor_id,definition.personal_skill_id);const bool learned=actual.has_value()&&*actual>=0&&*actual<=100;const bool seen=acquisition||known.count(id);
        if(actual&&(*actual<0||*actual>100))return {false,{},common_failure("InvalidCardMastery",id).error};
        const auto* name=shared.texts.find(definition.name_key);const auto* description=shared.texts.find(definition.description_key);const auto* hint=shared.texts.find(definition.hint_key);
        rows.push_back(obj({{"card_id",{id}},{"category",{growth_card_category_name(definition.category)}},{"name",{seen&&name?*name:StableId{"尚未发现"}}},{"description",{seen&&description?*description:StableId{}}},{"hint",{hint?*hint:StableId{}}},{"discovered",{seen}},{"acquired_this_world",{acquisition!=nullptr}},{"learned",{learned}},{"personal_skill_id",{learned?definition.personal_skill_id:StableId{}}},{"mastery",actual?BoundaryValue{*actual}:BoundaryValue{std::monostate{}}},{"configured",{learned&&definition.category==GrowthCardCategory::heart&&actor->combat.heart_id==definition.personal_skill_id}},{"acquired_tick",acquisition?BoundaryValue{acquisition->acquired_tick}:BoundaryValue{std::monostate{}}},{"source_id",{acquisition?acquisition->source_id:StableId{}}}}));
    }
    return {true,obj({{"actor_id",{actor_id}},{"offset",{offset}},{"limit",{limit}},{"total",{Integer(ids.size())}},{"cards",BoundaryValue{std::move(rows)}},{"next_offset",end<Integer(ids.size())?BoundaryValue{end}:BoundaryValue{std::monostate{}}}}),{}};
}
}
