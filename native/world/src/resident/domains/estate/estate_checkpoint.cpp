// 庄园typed检查点完整覆盖原生产/管理事实与命令参数，不把领域JSON压成黑箱。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "estate_model.hpp"
#include "../../storage/common_checkpoint.hpp"
namespace hanmo::resident {
namespace {
using namespace common_checkpoint;
Status facilities(CodecVisitor& c,EstateFacilities& f){c.field("farm",f.farm);c.field("granary",f.granary);c.field("palisade",f.palisade);return strings(c,"field_order",f.field_order);}
Status quantity(CodecVisitor& c,ItemQuantity& q){c.field("item_id",q.item_id);c.field("quantity",q.quantity);return strings(c,"field_order",q.field_order);}
Status command(CodecVisitor& c,EstateCommand& cmd){Integer kind=static_cast<Integer>(cmd.index());c.field("kind",kind);if(kind<0||kind>5)return c.reject("InvalidEstateCommand");if(c.reading()){switch(kind){case 0:cmd=EstateFund{};break;case 1:cmd=EstateAppoint{};break;case 2:cmd=EstateAssign{};break;case 3:cmd=EstateTransfer{};break;case 4:cmd=EstateBuild{};break;case 5:cmd=EstateDetachment{};break;}}return std::visit([&](auto& v){using T=std::decay_t<decltype(v)>;c.field("estate_id",v.estate_id);if constexpr(std::is_same_v<T,EstateFund>)c.field("quantity",v.quantity);else if constexpr(std::is_same_v<T,EstateAppoint>)c.field("manager_actor_id",v.manager_actor_id);else if constexpr(std::is_same_v<T,EstateAssign>){c.field("resident_actor_id",v.resident_actor_id);c.field("duty",v.duty);}else if constexpr(std::is_same_v<T,EstateBuild>)c.field("facility",v.facility);else if constexpr(std::is_same_v<T,EstateDetachment>){strings(c,"actor_ids",v.actor_ids);c.field("source_id",v.source_id);c.field("returning",v.returning);}else{c.field("item_id",v.item_id);c.field("quantity",v.quantity);c.field("source_id",v.source_id);c.field("deposit",v.deposit);c.field("source_id_present",v.source_id_present);}return strings(c,"field_order",v.field_order);},cmd);}
Status cash(CodecVisitor& c,EstateCashTerms& t){c.field("from_wallet_id",t.from_wallet_id);c.field("to_wallet_id",t.to_wallet_id);c.field("quantity",t.quantity);return strings(c,"field_order",t.field_order);}
Status transfer(CodecVisitor& c,EstateTransferTerms& t){c.field("from_inventory_id",t.from_inventory_id);c.field("to_inventory_id",t.to_inventory_id);c.field("item_id",t.item_id);c.field("quantity",t.quantity);c.sequence("owners",t.owners,[](CodecVisitor& v,Owner& o){return v.object("owner",[&](CodecVisitor& x){return owner(x,o);});});c.object("new_owner",[&](CodecVisitor& v){return owner(v,t.new_owner);});c.field("source_id",t.source_id);return strings(c,"field_order",t.field_order);}
Status record(CodecVisitor& c,EstateRecord& e){
c.field("id",e.id);
c.field("owner_actor_id",e.owner_actor_id);
c.field("manager_actor_id",e.manager_actor_id);
c.field("last_tick",e.last_tick);
c.field("last_production_id",e.last_production_id);
strings(c,"resident_actor_ids",e.resident_actor_ids);table(c,"duties",e.duties,[](CodecVisitor& v,StableId& duty){return v.field("duty",duty);});table(c,"detached",e.detached,[](CodecVisitor& v,StableId& source){return v.field("source",source);});c.object("facilities",[&](CodecVisitor& v){return facilities(v,e.facilities);});return strings(c,"field_order",e.field_order);}
Status production(CodecVisitor& c,EstateProductionFact& f){
c.field("estate_id",f.estate_id);
c.field("tick",f.tick);
c.field("owner_actor_id",f.owner_actor_id);
c.field("manager_actor_id",f.manager_actor_id);
c.field("farm_level",f.farm_level);
c.field("palisade_level",f.palisade_level);
c.field("lost_output",f.lost_output);
c.field("defense",f.defense);
strings(c,"workers",f.workers);
strings(c,"guards",f.guards);
strings(c,"fed",f.fed);
strings(c,"unfed",f.unfed);
strings(c,"reasons",f.reasons);
c.object("consumed",[&](CodecVisitor& v){return quantity(v,f.consumed);});c.object("produced",[&](CodecVisitor& v){return quantity(v,f.produced);});return strings(c,"field_order",f.field_order);}
// 检查点明确遍历建设字段，不通过JSON缓存规避领域校验。
Status terms(CodecVisitor& c,EstateBuildTerms& t){c.field("estate_id",t.estate_id);c.field("facility",t.facility);c.field("from_level",t.from_level);c.field("to_level",t.to_level);c.field("duration",t.duration);c.field("money_cost",t.money_cost);c.field("material_item_id",t.material_item_id);c.field("material_quantity",t.material_quantity);c.field("payer_wallet_id",t.payer_wallet_id);c.field("payee_wallet_id",t.payee_wallet_id);c.field("inventory_id",t.inventory_id);c.field("owner_actor_id",t.owner_actor_id);c.field("place_id",t.place_id);return strings(c,"field_order",t.field_order);}
Status construction(CodecVisitor& c,EstateConstructionFact& f){c.field("kind",f.kind);c.field("phase",f.phase);c.field("actor_id",f.actor_id);c.field("start_tick",f.start_tick);c.field("completed_tick",f.completed_tick);c.field("success",f.success);c.object("terms",[&](CodecVisitor& v){return terms(v,f.terms);});return strings(c,"field_order",f.field_order);}
Status muster(CodecVisitor& c,EstateMusterSource& f){c.field("kind",f.kind);c.field("estate_id",f.estate_id);return strings(c,"field_order",f.field_order);}
Status public_source(CodecVisitor& c,PublicEstateTransferSource& f){c.field("kind",f.kind);c.field("estate_id",f.estate_id);c.field("actor_id",f.actor_id);c.field("item_id",f.item_id);c.field("quantity",f.quantity);c.object("source_owner",[&](CodecVisitor& v){return owner(v,f.source_owner);});return strings(c,"field_order",f.field_order);}
Status management(CodecVisitor& c,EstateManagementFact& f){c.field("kind",f.kind);c.field("estate_id",f.estate_id);c.field("actor_id",f.actor_id);c.field("tick",f.tick);c.object("command",[&](CodecVisitor& v){return command(v,f.command);});c.optional("cash",f.cash,cash);c.optional("transfer",f.transfer,transfer);return strings(c,"field_order",f.field_order);}
}
Status visit_estate_checkpoint(CodecVisitor& c,EstateState& s){common_checkpoint::table(c,"estates",s.estates,record);common_checkpoint::table(c,"facts",s.facts,[](CodecVisitor& v,EstateFact& f){Integer kind=static_cast<Integer>(f.index());v.field("kind",kind);if(kind<0||kind>4)return v.reject("UnknownEstateFact");if(v.reading()){if(kind==0)f=EstateProductionFact{};else if(kind==1)f=EstateManagementFact{};else if(kind==2)f=EstateConstructionFact{};else if(kind==3)f=EstateMusterSource{};else f=PublicEstateTransferSource{};}return std::visit([&](auto& value){using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,EstateProductionFact>)return production(v,value);else if constexpr(std::is_same_v<T,EstateManagementFact>)return management(v,value);else if constexpr(std::is_same_v<T,EstateConstructionFact>)return construction(v,value);else if constexpr(std::is_same_v<T,EstateMusterSource>)return muster(v,value);else return public_source(v,value);},f);});common_checkpoint::table(c,"actions",s.actions,[](CodecVisitor& v,EstateBuildAction& a){v.object("terms",[&](CodecVisitor& x){return terms(x,a.terms);});return strings(v,"field_order",a.field_order);});return c.status();}
} // 检查点重建不跳过资产容积、钱粮守恒与事实因果校验。
