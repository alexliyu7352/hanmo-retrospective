#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "family_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
// 接续私产条款的严格冷边界；这里只保存选择与原来源，不产生继任或赠与授权。
#include "succession_personal_assets.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
const std::vector<std::pair<const char*,StableId SuccessionPersonalTerms::*>> ids={
    {"predecessor_actor_id",&SuccessionPersonalTerms::predecessor_actor_id},{"successor_actor_id",&SuccessionPersonalTerms::successor_actor_id},
    {"source_wallet_id",&SuccessionPersonalTerms::source_wallet_id},{"destination_wallet_id",&SuccessionPersonalTerms::destination_wallet_id},
    {"source_inventory_id",&SuccessionPersonalTerms::source_inventory_id},{"destination_inventory_id",&SuccessionPersonalTerms::destination_inventory_id}};
// 非空顺序须逐个覆盖唯一字段，不能把未知或重复键带回导出正文。
bool valid_order(const FieldOrder& order,const std::set<StableId>& expected){return order.empty()||(order.size()==expected.size()&&std::set<StableId>(order.begin(),order.end())==expected);}
// 选择以品类计量；同品类不能重复列项，具体批次在报价时冻结。
Status validate_selection(const SuccessionPersonalSelection& value){
    if(value.money<0||value.goods.size()>4096)return common_failure("InvalidSuccessionPersonalSelection");
    std::set<StableId> seen;
    for(const auto& item:value.goods)if(item.item_id.empty()||item.item_id.size()>256||item.quantity<=0||!seen.insert(item.item_id).second)return common_failure("InvalidSuccessionItemSelection",item.item_id);
    return common_success();
}
// 长期条款仅列实际已选原批；已交割后不要求原批仍存在于库存。

// 二进制读取与JSON读取使用同一结构约束，数量及字段序不靠UI过滤。
Status visit_lot(CodecVisitor& c,SuccessionPersonalLot& lot){
    c.field("lot_id",lot.lot_id);c.field("item_id",lot.item_id);c.field("quantity",lot.quantity);common_checkpoint::strings(c,"field_order",lot.field_order);
    if(!c.status().ok)return c.status();return valid_order(lot.field_order,{"lot_id","item_id","quantity"})?c.status():c.reject("BoundaryFieldOrderMismatch");
}
}
// 对新域的JSON整树先拒绝重名，包含Common根、Owner、来源context及数组中的每一记录。
Status validate_succession_json_unique(const BoundaryValue& root){
    std::size_t remaining=1000000;
    const auto visit=[&](const auto& self,const BoundaryValue& value,std::size_t depth,const StableId& path)->Status{
        if(depth>64||remaining==0)return common_failure("SuccessionJsonResourceLimit",path);--remaining;
        if(const auto* object=value.as<BoundaryValue::Dict>()){
            std::set<StableId> keys;for(const auto& row:*object){const auto at=path+"/"+row.first;if(!keys.insert(row.first).second)return common_failure("DuplicateSuccessionJsonField",at);auto status=self(self,row.second,depth+1,at);if(!status.ok)return status;}
        }else if(const auto* array=value.as<BoundaryValue::Array>())for(std::size_t i=0;i<array->size();++i){auto status=self(self,(*array)[i],depth+1,path+"/"+std::to_string(i));if(!status.ok)return status;}
        return common_success();
    };
    return visit(visit,root,0,"");
}
// 明确拒绝未知字段、重复品类、负金额及非正数量；空选择表示不交私人财物。
Outcome<SuccessionPersonalSelection> decode_succession_personal_selection(const BoundaryValue& value){try{
    auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};
    using namespace boundary;fields(value,{"money","goods"},{},"succession/personal_selection");SuccessionPersonalSelection result;result.money=get_int(value,"money","");result.field_order=order(value);
    const auto& goods=array(field(value,"goods",""),"goods");if(goods.size()>4096)fail("InvalidSuccessionPersonalSelection","goods");
    for(const auto& item:goods){fields(item,{"item_id","quantity"},{},"succession/item");result.goods.push_back({get_string(item,"item_id",""),get_int(item,"quantity",""),order(item)});}
    const auto valid=validate_selection(result);if(!valid.ok)return {false,{},valid.error};return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 导出保作者或UI原选择顺序，不自行把未选资产补入。
BoundaryValue encode_succession_personal_selection(const SuccessionPersonalSelection& value){using namespace boundary;V::Array goods;for(const auto& item:value.goods)goods.push_back(ordered(obj({{"item_id",V{item.item_id}},{"quantity",V{item.quantity}}}),item.field_order));return ordered(obj({{"money",V{value.money}},{"goods",V{std::move(goods)}}}),value.field_order);}
// 冷条款只可恢复完整字段集合，不能以缺省值产生遗漏的权属字段。
Outcome<SuccessionPersonalTerms> decode_succession_personal_terms(const BoundaryValue& value){try{
    auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};
    using namespace boundary;fields(value,{"predecessor_actor_id","successor_actor_id","source_wallet_id","destination_wallet_id","source_inventory_id","destination_inventory_id","money","lots"},{},"succession/personal_terms");
    SuccessionPersonalTerms result;for(const auto& id:ids)result.*id.second=get_string(value,id.first,"");result.money=get_int(value,"money","");result.field_order=order(value);
    const auto& lots=array(field(value,"lots",""),"lots");if(lots.size()>4096)fail("InvalidSuccessionPersonalTerms","lots");
    for(const auto& lot:lots){fields(lot,{"lot_id","item_id","quantity"},{},"succession/lot");result.lots.push_back({get_string(lot,"lot_id",""),get_string(lot,"item_id",""),get_int(lot,"quantity",""),order(lot)});}
    const auto valid=validate_succession_personal_terms(result);if(!valid.ok)return {false,{},valid.error};return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 原批、收付双方与容器都随条款导出，避免后续按“全部家产”重算。
Status emit_succession_personal_terms_fields(ProjectionWriter& out,const SuccessionPersonalTerms& terms){for(const auto& id:ids)out.field(id.first,terms.*id.second);out.field("money",terms.money);out.sequence("lots",terms.lots,[](auto& w,const auto& lot){return w.object("",lot.field_order,[&](auto& v){v.field("lot_id",lot.lot_id);v.field("item_id",lot.item_id);return v.field("quantity",lot.quantity);});});return out.order_fields(terms.field_order);}
BoundaryValue encode_succession_personal_terms(const SuccessionPersonalTerms& terms){return family_projection::encode([&](auto& w){return emit_succession_personal_terms_fields(w,terms);});}
// 段版本登记归中央；本叶不修改旧Family1—8的字节与含义。
Status visit_succession_personal_terms(CodecVisitor& c,SuccessionPersonalTerms& terms){
    for(const auto& id:ids)c.field(id.first,terms.*id.second);c.field("money",terms.money);c.sequence("lots",terms.lots,[](CodecVisitor& v,SuccessionPersonalLot& lot){return v.object("entry",[&](CodecVisitor& p){return visit_lot(p,lot);});});common_checkpoint::strings(c,"field_order",terms.field_order);
    if(!c.status().ok)return c.status();if(!valid_order(terms.field_order,{"predecessor_actor_id","successor_actor_id","source_wallet_id","destination_wallet_id","source_inventory_id","destination_inventory_id","money","lots"}))return c.reject("BoundaryFieldOrderMismatch");const auto valid=validate_succession_personal_terms(terms);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
