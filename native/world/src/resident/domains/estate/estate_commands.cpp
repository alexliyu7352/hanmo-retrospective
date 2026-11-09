#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include "resident/checkpoint/projection_writer.hpp"
// 瞬时经营和建设报价只读取同候选；公私转移、借调来源与建设成本均沿原规则复核。
#include "estate_model.hpp"
#include "estate_detachment_release.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "../commerce/private_shop_staff_exclusion.hpp"
#include <optional>
#include <set>
namespace hanmo::resident {
namespace {
// 管理会话可由本人继续；建设开始只额外允许已经装入的本行动占用。
bool ready_to_manage(const CommonStore& c,const ActivityRead& a,const StableId& id,const StableId& place,const StableId& speaker,const StableId& own_action={},bool allow_injury=false){
    auto* person=c.state().actors.find(id);if(!person||!person->alive||!person->available||(!allow_injury&&person->injury>0)||(!person->action_id.empty()&&person->action_id!=own_action))return false;
    auto resolved=c.resolved_location(id);auto* at=resolved.ok?std::get_if<SettlementLocation>(&resolved.value):nullptr;
    return at&&at->place_id==place&&!a.in_open_battle(id)&&!a.in_interaction(id,speaker);
}
// 报价比较只忽略冷边界字段顺序，所有冻结业务字段都必须一致。
bool same_quote(const EstateQuote& a,const EstateQuote& b){
    if(a.duration!=b.duration||a.participants!=b.participants||a.actor_ids!=b.actor_ids||a.terms.has_value()!=b.terms.has_value()||a.cash.has_value()!=b.cash.has_value()||a.transfer.has_value()!=b.transfer.has_value())return false;
    if(a.terms&&!same_estate_build_terms(*a.terms,*b.terms))return false;
    if(a.cash){auto& x=*a.cash;auto& y=*b.cash;if(x.from_wallet_id!=y.from_wallet_id||x.to_wallet_id!=y.to_wallet_id||x.quantity!=y.quantity)return false;}
    if(a.transfer){auto& x=*a.transfer;auto& y=*b.transfer;if(x.from_inventory_id!=y.from_inventory_id||x.to_inventory_id!=y.to_inventory_id||x.item_id!=y.item_id||x.quantity!=y.quantity||x.source_id!=y.source_id||!owner_equal(x.new_owner,y.new_owner)||x.owners.size()!=y.owners.size())return false;for(std::size_t i=0;i<x.owners.size();++i)if(!owner_equal(x.owners[i],y.owners[i]))return false;}
    return true;
}
}
namespace {
bool original_lost_estate_retainer_matches(const StableId* detached,std::string_view source){
 return detached&&std::string_view(detached->data(),detached->size())==source;
}
}
Status validate_original_lost_estate_retainer_release(const OriginalEstateDetachmentReleaseView& v){
 bool valid;
 if(!v.detached_before||original_lost_estate_retainer_matches(v.detached_before,v.source_id))
  valid=v.detached_after==nullptr;
 else
  valid=v.detached_after&&*v.detached_after==*v.detached_before;
 return valid?common_success():common_failure("OriginalEstateDetachmentReleaseMismatch");
}
Status release_original_lost_estate_retainer_in_candidate(
 EstateState& state,const StableId& estate_id,const StableId& actor,
 const StableId& source,OriginalEstateDetachmentReleaseSink* sink){
 auto* estate=state.estates.find(estate_id);
 if(!estate)return common_failure("UnknownEstate",estate_id);
 const auto* detached=estate->detached.find(actor);
 auto release=[&](){
  if(original_lost_estate_retainer_matches(detached,std::string_view(source.data(),source.size())))estate->detached.erase(actor);
 };
 if(!sink){
  release();
  return common_success();
 }


 auto memory=CheckpointMemoryCharge(0);
 std::optional<StableId> before;
 if(detached)before.emplace(*detached);
 release();
 const auto* after=estate->detached.find(actor);
 const OriginalEstateDetachmentReleaseView observed{estate_id,actor,source,before?&*before:nullptr,after};
 auto valid=validate_original_lost_estate_retainer_release(observed);
 if(!valid.ok)return valid;
 return sink->append(observed);
}
// 命令名来自同一typed参数，不接受另一个可分叉的kind字段。
StableId estate_command_kind(const EstateCommand& c){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,EstateFund>)return "estate_fund";else if constexpr(std::is_same_v<T,EstateAppoint>)return "estate_appoint";else if constexpr(std::is_same_v<T,EstateAssign>)return "estate_assign";else if constexpr(std::is_same_v<T,EstateBuild>)return "estate_build";else if constexpr(std::is_same_v<T,EstateDetachment>)return v.returning?"estate_return":"estate_muster";else return v.deposit?"estate_deposit":"estate_withdraw";},c);}
// 来源必须在共同事实头登记；军旅载荷只通过当前候选的有限读取适配器访问。
bool estate_muster_source(const CommonStore& c,const EstateState& s,const StableId& source,const StableId& estate,const EstateAccess* access){
    if(source.empty())return false;auto* header=c.state().facts.find(source);if(!header)return false;
    if(header->kind=="estate_muster"){auto* fact=s.facts.find(source);auto* muster=fact?std::get_if<EstateMusterSource>(fact):nullptr;return muster&&muster->estate_id==estate;}
    if(header->kind=="military_muster"&&access&&access->military_muster_estate){auto value=access->military_muster_estate(source);return value&&*value==estate;}return false;
}
// 公物授权精确匹配收货人、物品、数量和机构；一次性消费由历史转移事实约束。
Status validate_estate_transfer_source(const CommonStore& c,const EstateState& s,const CommonCatalog& common,const StableId& estate,const StableId& actor,const EstateTransferTerms& terms){
    auto* header=c.state().facts.find(terms.source_id);auto* payload=s.facts.find(terms.source_id);auto* source=payload?std::get_if<PublicEstateTransferSource>(payload):nullptr;
    if(!header||header->kind!="public_estate_transfer"||!source||source->estate_id!=estate||source->actor_id!=actor||source->item_id!=terms.item_id||source->quantity!=terms.quantity||source->source_owner.kind!=Owner::Kind::organization||std::find(common.organization_ids.begin(),common.organization_ids.end(),source->source_owner.id)==common.organization_ids.end()||terms.owners.size()!=1||!owner_equal(terms.owners.front(),source->source_owner))return common_failure("InvalidEstateTransferSource");
    return common_success();
}
// 保留Godot错误优先级；建造此处只报价，不创建行动或扣取成本。
Outcome<EstateQuote> preview_estate(const CommonStore& c,const EstateState& s,const EstateCatalog& catalog,const CommonCatalog& common,const ActivityRead& activity,const StableId& actor,const EstateCommand& command,const EstateAccess* access,const StableId& own_action){
    auto id=std::visit([](const auto& cmd){return cmd.estate_id;},command);auto* e=s.estates.find(id);auto* d=catalog.estates.find(id);if(!e||!d)return {false,{},common_failure("UnknownEstate").error};
    if(actor!=e->owner_actor_id&&actor!=e->manager_actor_id)return {false,{},common_failure("EstateAuthorityRequired").error};if(!ready_to_manage(c,activity,actor,d->place_id,actor,own_action))return {false,{},common_failure("EstateActorUnavailable").error};EstateQuote quote;quote.command=command;
    auto status=std::visit([&](const auto& cmd)->Status{using T=std::decay_t<decltype(cmd)>;
        if constexpr(std::is_same_v<T,EstateFund>){if(actor!=e->owner_actor_id||cmd.quantity<1||cmd.quantity>100000)return common_failure("EstateFundingAuthority");auto from=c.actor(actor)->wallet_id;auto to=d->wallet_id;auto spend=c.spendable(from);if(!spend.ok)return {false,{},spend.error};if(from==to||spend.value<cmd.quantity)return common_failure("InsufficientFunds");auto* dest=c.wallet(to);if(!dest)return common_failure("UnknownWallet",to);if(dest->balance>2147483647-cmd.quantity)return common_failure("MoneyOverflow");quote.cash=EstateCashTerms{from,to,cmd.quantity};}
        else if constexpr(std::is_same_v<T,EstateAppoint>){if(!cmd.manager_actor_id.empty()){auto shop=guard_private_shop_worker_commitment(c,access?access->private_shop_staff:nullptr,cmd.manager_actor_id);if(!shop.ok)return shop;}if(!cmd.manager_actor_id.empty()&&(std::find(e->resident_actor_ids.begin(),e->resident_actor_ids.end(),cmd.manager_actor_id)==e->resident_actor_ids.end()||e->detached.find(cmd.manager_actor_id)||!ready_to_manage(c,activity,cmd.manager_actor_id,d->place_id,actor)))return common_failure("EstateManagerUnavailable");}
        else if constexpr(std::is_same_v<T,EstateAssign>){if(cmd.duty!="idle"){auto shop=guard_private_shop_worker_commitment(c,access?access->private_shop_staff:nullptr,cmd.resident_actor_id);if(!shop.ok)return shop;}if(std::find(e->resident_actor_ids.begin(),e->resident_actor_ids.end(),cmd.resident_actor_id)==e->resident_actor_ids.end()||(cmd.duty!="idle"&&cmd.duty!="labor"&&cmd.duty!="guard"))return common_failure("InvalidEstateDuty");if(e->detached.find(cmd.resident_actor_id)||!ready_to_manage(c,activity,cmd.resident_actor_id,d->place_id,actor))return common_failure("EstateResidentUnavailable");if(cmd.resident_actor_id==e->manager_actor_id&&cmd.duty!="idle")return common_failure("EstateManagerHasDuty");}
        else if constexpr(std::is_same_v<T,EstateBuild>){
            if(cmd.facility!="farm"&&cmd.facility!="granary"&&cmd.facility!="palisade")return common_failure("UnknownEstateFacility");
            for(auto& row:s.actions.rows())if(row.first!=own_action&&row.second.terms.estate_id==id&&row.second.terms.facility==cmd.facility)return common_failure("EstateFacilityBusy");
            auto terms=estate_build_terms(*d,*e,cmd.facility);if(terms.to_level>d->construction.max_level)return common_failure("EstateFacilityMaxLevel");
            if(cmd.facility=="granary"&&d->base_capacity+(e->facilities.granary+1)*d->granary_capacity_per_level>2147483647)return common_failure("EstateCapacityOverflow");
            auto* payer=c.wallet(terms.payer_wallet_id);auto* payee=c.wallet(terms.payee_wallet_id);Owner owner{Owner::Kind::actor,e->owner_actor_id,{}};
            if(!payer||!payee||!owner_equal(payer->owner,owner)||payer->id==payee->id)return common_failure("EstateConstructionPayment");
            auto spend=c.spendable(payer->id);if(!spend.ok)return {false,{},spend.error};if(spend.value<terms.money_cost)return common_failure("InsufficientFunds");if(payee->balance>2147483647-terms.money_cost)return common_failure("MoneyOverflow");
            auto count=c.count(terms.inventory_id,terms.material_item_id,{owner});if(!count.ok)return {false,{},count.error};if(count.value<terms.material_quantity)return common_failure("InsufficientEstateMaterials");
            quote.duration=terms.duration;quote.terms=terms;quote.participants={actor};
        }else if constexpr(std::is_same_v<T,EstateDetachment>){
            if(cmd.actor_ids.empty()||cmd.actor_ids.size()>e->resident_actor_ids.size()||!estate_muster_source(c,s,cmd.source_id,id,access))return common_failure("InvalidEstateMuster");std::set<StableId> seen;
            for(auto& person:cmd.actor_ids){if(!seen.insert(person).second||std::find(e->resident_actor_ids.begin(),e->resident_actor_ids.end(),person)==e->resident_actor_ids.end()||!ready_to_manage(c,activity,person,d->place_id,actor,{},cmd.returning))return common_failure("EstateResidentUnavailable");
                if(cmd.returning){if(!access||!access->army_id)return common_failure("UnboundEstateArmyAccess",person,ErrorKind::unsupported_capability);if(!access->army_id(person).empty())return common_failure("ArmyNotReturned");}
                auto* source=e->detached.find(person);if(!cmd.returning&&source)return common_failure("EstateAlreadyDetached");if(cmd.returning&&(!source||*source!=cmd.source_id))return common_failure("EstateDetachmentMismatch");}
            quote.actor_ids=cmd.actor_ids;
        }else{
            if(!common.items.find(cmd.item_id)||cmd.quantity<1||cmd.quantity>10000)return common_failure("InvalidEstateTransfer");auto bag=c.actor(actor)->inventory_id;Owner owner{Owner::Kind::actor,e->owner_actor_id,{}};std::vector<Owner> owners{owner};
            if(cmd.deposit){Owner personal{Owner::Kind::actor,actor,{}};if(!owner_equal(owner,personal))owners.push_back(personal);if(!cmd.source_id.empty()){
                auto* payload=s.facts.find(cmd.source_id);auto* source=payload?std::get_if<PublicEstateTransferSource>(payload):nullptr;if(!source)return common_failure("InvalidEstateTransferSource");owners={source->source_owner};
                EstateTransferTerms probe;probe.source_id=cmd.source_id;probe.item_id=cmd.item_id;probe.quantity=cmd.quantity;probe.owners=owners;auto valid=validate_estate_transfer_source(c,s,common,id,actor,probe);if(!valid.ok)return valid;
                for(auto& row:s.facts.rows())if(auto* fact=std::get_if<EstateManagementFact>(&row.second))if(fact->kind=="estate_transfer"&&fact->transfer&&fact->transfer->source_id==cmd.source_id)return common_failure("EstateTransferSourceUsed");
            }}else if(!cmd.source_id.empty())return common_failure("InvalidEstateTransferSource");
            auto from=cmd.deposit?bag:d->inventory_id;auto to=cmd.deposit?d->inventory_id:bag;if(from==to)return common_failure("SameContainer");auto count=c.count(from,cmd.item_id,owners);if(!count.ok)return {false,{},count.error};if(count.value<cmd.quantity)return common_failure("InsufficientOwnedEstateGoods");auto weight=c.inventory_weight(to);if(!weight.ok)return {false,{},weight.error};if(weight.value+cmd.quantity*common.items.find(cmd.item_id)->weight>c.inventory(to)->capacity)return common_failure("EstateStorageFull");quote.transfer=EstateTransferTerms{from,to,cmd.item_id,cmd.quantity,owners,owner,cmd.source_id};
        }return common_success();
    },command);if(!status.ok)return {false,{},status.error};return {true,std::move(quote),{}};
}
// 预览军旅借调以临时来源头验证全部庄园规则，不分配正式ID，也不复制共同资产。
Outcome<std::vector<StableId>> preview_estate_muster(const CommonStore& c,const EstateState& s,const EstateCatalog& catalog,const CommonCatalog& common,const ActivityRead& activity,const StableId& actor,const StableId& estate,const std::vector<StableId>& people){
    auto* e=s.estates.find(estate);auto* d=catalog.estates.find(estate);if(!e||!d)return {false,{},common_failure("UnknownEstate").error};
    // 复用普通报价的权限和人物检查；临时来源只在本只读调用的条件中视为有效。
    auto authority=preview_estate(c,s,catalog,common,activity,actor,EstateAppoint{estate,e->manager_actor_id});
    if(!authority.ok&&authority.error.original.code!="EstateManagerUnavailable")return {false,{},authority.error};
    if(people.empty()||people.size()>e->resident_actor_ids.size())return {false,{},common_failure("InvalidEstateMuster").error};std::set<StableId> seen;
    for(auto& person:people){if(!seen.insert(person).second||std::find(e->resident_actor_ids.begin(),e->resident_actor_ids.end(),person)==e->resident_actor_ids.end()||!ready_to_manage(c,activity,person,d->place_id,actor))return {false,{},common_failure("EstateResidentUnavailable").error};if(e->detached.find(person))return {false,{},common_failure("EstateAlreadyDetached").error};}
    return {true,people,{}};
}
// 同一事务复核报价后才搬钱货或改变真人借调，不允许瞬时升级设施。
Outcome<EstateReceipt> apply_estate(CommonStore& c,EstateState& s,const EstateCatalog& catalog,const CommonCatalog& common,const ActivityRead& activity,const StableId& actor,const EstateQuote& quote,const EstateAccess* access){
    if(std::holds_alternative<EstateBuild>(quote.command))return {false,{},common_failure("EstateBuildNeedsAction").error};auto current=preview_estate(c,s,catalog,common,activity,actor,quote.command,access);if(!current.ok)return {false,{},current.error};if(!same_quote(current.value,quote))return {false,{},common_failure("StaleEstateQuote").error};
    auto id=std::visit([](const auto& cmd){return cmd.estate_id;},quote.command);auto* estate=s.estates.find(id);EstateManagementFact fact;fact.kind="estate_management";fact.estate_id=id;fact.actor_id=actor;fact.tick=c.tick();fact.command=quote.command;EstateReceipt receipt;receipt.estate_id=id;
    auto status=std::visit([&](const auto& cmd)->Status{using T=std::decay_t<decltype(cmd)>;
        if constexpr(std::is_same_v<T,EstateFund>){auto cash=*current.value.cash;ResourceRecipe recipe;auto result=resource_recipe::append_money_transfer(recipe,cash.from_wallet_id,cash.to_wallet_id,cash.quantity,resource_recipe::MoneyTransferPolicy::spendable);if(!result.ok)return result;result=c.apply_resources(recipe);if(!result.ok)return result;fact.kind="estate_funding";fact.cash=cash;}
        else if constexpr(std::is_same_v<T,EstateAppoint>){estate->manager_actor_id=cmd.manager_actor_id;if(!cmd.manager_actor_id.empty())estate->duties.set(cmd.manager_actor_id,"idle");}
        else if constexpr(std::is_same_v<T,EstateAssign>)estate->duties.set(cmd.resident_actor_id,cmd.duty);
        else if constexpr(std::is_same_v<T,EstateDetachment>){for(auto& person:current.value.actor_ids){if(cmd.returning)estate->detached.erase(person);else estate->detached.set(person,cmd.source_id);}fact.kind="estate_detachment";receipt.actor_ids=current.value.actor_ids;}
        else if constexpr(std::is_same_v<T,EstateTransfer>){auto t=*current.value.transfer;auto result=c.apply_resources({{TransferLots{t.from_inventory_id,t.to_inventory_id,t.item_id,t.quantity,t.owners,{},t.new_owner}},{}});if(!result.ok)return result;fact.kind="estate_transfer";fact.transfer=t;}
        else return common_failure("EstateBuildNeedsAction");return common_success();
    },quote.command);if(!status.ok)return {false,{},status.error};auto next=c.next_id("estate");if(!next.ok)return {false,{},next.error};auto added=c.append_fact({next.value,fact.kind,"estate"});if(!added.ok)return {false,{},added.error};s.facts.insert(next.value,std::move(fact));receipt.fact_id=next.value;return {true,std::move(receipt),{}};
}
// 冷命令边界拒绝未知键和错误类型；原参数字段次序仍保留。
Outcome<EstateCommand> decode_estate_command(const StableId& kind,const BoundaryValue& params){try{using namespace boundary;
 if(kind!="estate_fund"&&kind!="estate_appoint"&&kind!="estate_assign"&&kind!="estate_withdraw"&&kind!="estate_deposit"&&kind!="estate_build"&&kind!="estate_muster"&&kind!="estate_return")return {false,{},common_failure("UnsupportedNativeCapability",kind,ErrorKind::unsupported_capability).error};
 get_string_ref(params,"estate_id","params");ActorFoundationJsonVisitor archive(params);auto checked=[](Status status){if(!status.ok)throw DecodeFailure{status.error};};checked(archive.defer_duplicate_field_check_to_finish());
 auto read=[&](std::string_view name,auto& value){using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,StableId>)get_string_ref(params,name,"params");else if constexpr(std::is_same_v<T,Integer>)get_int(params,name,"params");checked(archive.field(name,value));};
 auto finish=[&](auto&& value)->Outcome<EstateCommand>{value.field_order=order(params);checked(archive.finish());return {true,std::forward<decltype(value)>(value),{}};};
 if(kind=="estate_fund"){fields(params,{"estate_id","quantity"},{},"params");checked(archive.prepare_default_values<EstateFund>());EstateFund value;read("estate_id",value.estate_id);read("quantity",value.quantity);return finish(std::move(value));}
 if(kind=="estate_appoint"){fields(params,{"estate_id","manager_actor_id"},{},"params");checked(archive.prepare_default_values<EstateAppoint>());EstateAppoint value;read("estate_id",value.estate_id);read("manager_actor_id",value.manager_actor_id);return finish(std::move(value));}
 if(kind=="estate_assign"){fields(params,{"estate_id","resident_actor_id","duty"},{},"params");checked(archive.prepare_default_values<EstateAssign>());EstateAssign value;read("estate_id",value.estate_id);read("resident_actor_id",value.resident_actor_id);read("duty",value.duty);return finish(std::move(value));}
 if(kind=="estate_build"){fields(params,{"estate_id","facility"},{},"params");checked(archive.prepare_default_values<EstateBuild>());EstateBuild value;read("estate_id",value.estate_id);read("facility",value.facility);return finish(std::move(value));}
 if(kind=="estate_muster"||kind=="estate_return"){
  fields(params,{"estate_id","actor_ids","source_id"},{},"params");checked(archive.prepare_default_values<EstateDetachment>());EstateDetachment value;read("estate_id",value.estate_id);
  const auto& rows=array(field(params,"actor_ids","params"),"params/actor_ids");std::size_t index=0;checked(archive.sequence("actor_ids",value.actor_ids,[&](auto& child,auto& actor){auto n=index++;char digits[3*sizeof(n)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+n%10);n/=10;}while(n);string_ref(rows[index-1],{"params/actor_ids/",std::string_view(start,std::size_t(end-start))});return child.field("",actor);},true));
  read("source_id",value.source_id);value.returning=kind=="estate_return";return finish(std::move(value));
 }
 fields(params,{"estate_id","item_id","quantity"},{"source_id"},"params");checked(archive.prepare_default_values<EstateTransfer>());EstateTransfer value;read("estate_id",value.estate_id);read("item_id",value.item_id);read("quantity",value.quantity);value.deposit=kind=="estate_deposit";if(params.find("source_id")){read("source_id",value.source_id);value.source_id_present=true;}return finish(std::move(value));
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Outcome<EstateCommand> decode_estate_command(const StableId& kind,const BoundaryValue& params,ScopedCheckpointJsonDecode& budget){if(&budget!=ScopedCheckpointJsonDecode::current())budget.fail("CheckpointCandidateMemoryJobMismatch","estate_command");return decode_estate_command(kind,params);}
// 序列化只转换命令自己的显式字段，不携带可写世界快照。
Status emit_estate_command_fields(ProjectionWriter&w,const EstateCommand&command){return std::visit([&](const auto&cmd)->Status{using T=std::decay_t<decltype(cmd)>;w.field("estate_id",cmd.estate_id);
 if constexpr(std::is_same_v<T,EstateFund>)w.field("quantity",cmd.quantity);
 else if constexpr(std::is_same_v<T,EstateAppoint>)w.field("manager_actor_id",cmd.manager_actor_id);
 else if constexpr(std::is_same_v<T,EstateAssign>){w.field("resident_actor_id",cmd.resident_actor_id);w.field("duty",cmd.duty);}
 else if constexpr(std::is_same_v<T,EstateBuild>)w.field("facility",cmd.facility);
 else if constexpr(std::is_same_v<T,EstateDetachment>){w.sequence("actor_ids",cmd.actor_ids,[](auto&out,const auto&id){return out.field("",id);});w.field("source_id",cmd.source_id);}
 else {w.field("item_id",cmd.item_id);w.field("quantity",cmd.quantity);if(cmd.source_id_present)w.field("source_id",cmd.source_id);}
 return w.order_fields(cmd.field_order);},command);}
BoundaryValue encode_estate_command(const EstateCommand&command){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=writer.object("",[&](auto&w){return emit_estate_command_fields(w,command);});if(status.ok)status=writer.finish();if(!status.ok)throw boundary::DecodeFailure{status.error};return out;}
} // 原始参数有无source_id均保全，领域不另行提交或推进世界时间。
