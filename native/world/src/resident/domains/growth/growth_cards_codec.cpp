#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "growth_state_projection.hpp"
// 卡片目录与来源账使用闭合字段和typed冷边界；不把未知效果存成可执行JSON。
#include "growth_cards.hpp"
#include "resident/storage/common_checkpoint.hpp"

namespace hanmo::resident {
namespace {
using namespace boundary;
// 来源标签严格区分作者初态与已经结算的训练，不从自由字符串生成第三类授权。
GrowthCardSourceKind source_kind(const StableId& value,const StableId& path){
    if(value=="scenario_initial")return GrowthCardSourceKind::scenario_initial;
    if(value=="training")return GrowthCardSourceKind::training;
    fail("InvalidCardSourceKind",path);return GrowthCardSourceKind::training;
}
// 编码前检查枚举，损坏内存不能被默写成合法训练来源。
StableId source_name(GrowthCardSourceKind kind){
    if(kind==GrowthCardSourceKind::scenario_initial)return "scenario_initial";
    if(kind==GrowthCardSourceKind::training)return "training";
    fail("InvalidCardSourceKind","cards/source_kind");return {};
}
// 完整取得记录保留原字段顺序；能力和掌握字段刻意不属于卡片账。
GrowthCardAcquisition acquisition(const BoundaryValue& value,const StableId& path){
    fields(value,{"card_id","actor_id","source_kind","source_id","acquired_tick"},{},path);
    GrowthCardAcquisition record;record.card_id=get_string(value,"card_id",path);record.actor_id=get_string(value,"actor_id",path);record.source_kind=source_kind(get_string(value,"source_kind",path),path+"/source_kind");record.source_id=get_string(value,"source_id",path);record.acquired_tick=get_int(value,"acquired_tick",path);record.field_order=order(value);return record;
}
// 一条卡片记录的二进制遍历不访问世界、随机流或资源账户。
Status visit_acquisition(CodecVisitor& visitor,GrowthCardAcquisition& record){
    visitor.field("card_id",record.card_id);visitor.field("actor_id",record.actor_id);
    Integer kind=static_cast<Integer>(record.source_kind);visitor.field("source_kind",kind);if(kind<0||kind>1)return visitor.reject("InvalidCardSourceKind");
    if(visitor.reading())record.source_kind=static_cast<GrowthCardSourceKind>(kind);
    visitor.field("source_id",record.source_id);visitor.field("acquired_tick",record.acquired_tick);
    return common_checkpoint::strings(visitor,"field_order",record.field_order);
}
}

// 当前唯一效果引用已经存在的个人本领；其他五类的业务效果没有实现时明确Unsupported。
Outcome<GrowthCardCatalog> decode_growth_card_catalog(const BoundaryValue& definitions,const BoundaryValue& config,const CommonCatalog& common){try{
    using namespace boundary;
    fields(config,{"profile","initial_grants"},{},"cards");
    if(get_string(config,"profile","cards")!="learned_ability_cards_v1")unsupported("UnsupportedCardsProfile","cards/profile");
    GrowthCardCatalog catalog;catalog.enabled=true;
    for(const auto& row:object(definitions,"cards/definitions")){
        const auto path="cards/definitions/"+row.first;const auto& value=row.second;
        fields(value,{"id","category","name_key","description_key","prerequisites","acquire_condition","hint_key","icon_id","effect_kind","effect_params","repeat_policy"},{},path);
        // 资格委派给原个人本领，来源只承认明确初态和已完成教学，不执行作者表达式。
        const auto& prerequisites=field(value,"prerequisites",path);fields(prerequisites,{"kind"},{},path+"/prerequisites");
        if(get_string(prerequisites,"kind",path)!="personal_ability_eligibility")unsupported("UnsupportedCardPrerequisite",path+"/prerequisites");
        const auto& acquisition=field(value,"acquire_condition",path);fields(acquisition,{"kind"},{},path+"/acquire_condition");
        if(get_string(acquisition,"kind",path)!="initial_or_completed_training")unsupported("UnsupportedCardAcquisition",path+"/acquire_condition");
        if(get_string(value,"effect_kind",path)!="personal_ability")unsupported("UnsupportedCardEffect",path+"/effect_kind");
        if(get_string(value,"repeat_policy",path)!="once_per_actor")unsupported("UnsupportedCardRepeatPolicy",path+"/repeat_policy");
        const auto& params=field(value,"effect_params",path);fields(params,{"personal_skill_id"},{},path+"/effect_params");
        auto category=growth_card_category(get_string(value,"category",path));if(!category.ok)return {false,{},category.error};
        GrowthCardDefinition definition;definition.id=get_string(value,"id",path);definition.category=category.value;definition.name_key=get_string(value,"name_key",path);definition.description_key=get_string(value,"description_key",path);definition.hint_key=get_string(value,"hint_key",path);definition.icon_id=get_string(value,"icon_id",path);definition.personal_skill_id=get_string(params,"personal_skill_id",path);definition.field_order=order(value);
        if(!catalog.definitions.insert(row.first,std::move(definition)))fail("DuplicateCardDefinition",path);
    }
    for(const auto& value:array(field(config,"initial_grants","cards"),"cards/initial_grants")){
        fields(value,{"actor_id","card_id"},{},"cards/initial_grants");catalog.initial_grants.push_back({get_string(value,"actor_id","cards/initial_grants"),get_string(value,"card_id","cards/initial_grants"),order(value)});
    }
    auto valid=validate_growth_card_catalog(catalog,common);if(!valid.ok)return {false,{},valid.error};return {true,std::move(catalog),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}

// JSON读取只恢复明确的卡片来源字段，业务有效性由同一候选的validate_growth_cards核实。
Outcome<GrowthCardsState> decode_growth_cards(const BoundaryValue& value){try{
    using namespace boundary;fields(value,{"version","owned"},{},"cards");
    if(get_int(value,"version","cards")!=1)unsupported("UnsupportedCardsVersion","cards/version");
    GrowthCardsState state;state.field_order=order(value);
    for(const auto& actor:object(field(value,"owned","cards"),"cards/owned")){
        OrderedTable<GrowthCardAcquisition> cards;
        for(const auto& row:object(actor.second,"cards/owned/"+actor.first))if(!cards.insert(row.first,acquisition(row.second,"cards/owned/"+actor.first+"/"+row.first)))fail("DuplicateCardAcquisition",row.first);
        if(!state.owned.insert(actor.first,std::move(cards)))fail("DuplicateCardOwner",actor.first);
    }
    return {true,std::move(state),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}

// 编码保持人物、卡片与字段原序；没有重新获取、重新发奖或隐式迁移行为。
Status emit_growth_cards_fields(ProjectionWriter& out,const GrowthCardsState& state){out.field("version",Integer{1});out.object("owned",[&](auto& actors){for(const auto& actor:state.owned.rows())actors.object(actor.first,[&](auto& cards){for(const auto& row:actor.second.rows())cards.object(row.first,row.second.field_order,[&](auto& c){const auto& r=row.second;c.field("card_id",r.card_id);c.field("actor_id",r.actor_id);c.field("source_kind",source_name(r.source_kind));c.field("source_id",r.source_id);return c.field("acquired_tick",r.acquired_tick);});return cards.status();});return actors.status();});return out.order_fields(state.field_order);}
BoundaryValue encode_growth_cards(const GrowthCardsState& state){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=out.object("",[&](auto& w){return emit_growth_cards_fields(w,state);});if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}

// 新module独占此冷块；调用方未启用时不能借追加optional标记改变旧银行布局。
Status visit_growth_cards_checkpoint(CodecVisitor& visitor,GrowthCardsState& state){
    Integer version=1;visitor.field("version",version);if(version!=1)return visitor.reject("UnsupportedCardsVersion");
    common_checkpoint::table(visitor,"owned",state.owned,[](CodecVisitor& owner,auto& cards){return common_checkpoint::table(owner,"cards",cards,visit_acquisition);});
    return common_checkpoint::strings(visitor,"field_order",state.field_order);
}
}
