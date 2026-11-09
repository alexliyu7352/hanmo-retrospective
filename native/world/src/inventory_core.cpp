// 机械复用已验证的v1库存循环；类型策略和错误时点保持，新增profile仅允许有损拒绝改为extras保全。
#include "inventory_core.hpp"
#include "typed_wire.hpp"
#include <limits>
namespace hanmo::detail {
std::vector<InventoryState> decode_inventories(const Value& source,const std::map<std::string,std::set<std::string>>& references,const std::set<std::string>& actions,const std::map<std::string,std::int64_t>& items,bool preserve_extras) {
        std::vector<InventoryState> result;
        std::set<std::string> inventories,lots;
        const auto& rows=*require_value<Array>(source,"/payload/inventories");
        for(std::size_t i=0;i<rows.size();++i) {
            const auto path=at("/payload/inventories",std::to_string(i)); const auto& row=*require_value<Dict>(rows[i],path);
            if(!preserve_extras) fields(row,{"id","owner","capacity","revision","lots"},{},path);
            else for(const auto* key:{"id","owner","capacity","revision","lots"}) if(!row.count(key)) fail("MissingProjectionField",at(path,key));
            InventoryState inventory;
            if(preserve_extras) { inventory.extras=row; for(const auto* key:{"id","owner","capacity","revision","lots"}) inventory.extras.erase(key); }
            inventory.id=*require_value<std::string>(row.at("id"),at(path,"id"));
            if(inventory.id.empty() || !inventories.insert(inventory.id).second) fail("DuplicateInventoryId",at(path,"id"));
            inventory.owner=row.at("owner"); owner(inventory.owner,at(path,"owner"),&references);
            inventory.capacity=whole(row.at("capacity"),1,2147483647,at(path,"capacity"));
            inventory.revision=whole(row.at("revision"),0,2147483647,at(path,"revision"));
            const auto& batch=*require_value<Array>(row.at("lots"),at(path,"lots")); std::int64_t weight=0;
            for(std::size_t j=0;j<batch.size();++j) {
                const auto lp=at(at(path,"lots"),std::to_string(j)); const auto& original=*require_value<Dict>(batch[j],lp);
                for(const auto* key:{"id","item_id","quantity","owner","reserved_by"}) if(!original.count(key)) fail("MissingProjectionField",at(lp,key));
                Lot lot; lot.extras=original;
                for(const auto* key:{"id","item_id","quantity","owner","reserved_by"}) lot.extras.erase(key);
                lot.id=*require_value<std::string>(original.at("id"),at(lp,"id")); lot.item_id=*require_value<std::string>(original.at("item_id"),at(lp,"item_id"));
                if(lot.id.empty() || !lots.insert(lot.id).second) fail("DuplicateLotId",at(lp,"id"));
                if(!items.count(lot.item_id)) fail("UnknownItem",at(lp,"item_id"));
                lot.quantity=whole(original.at("quantity"),1,2147483647,at(lp,"quantity"));
                lot.owner=original.at("owner"); owner(lot.owner,at(lp,"owner"),&references);
                lot.reserved_by=*require_value<std::string>(original.at("reserved_by"),at(lp,"reserved_by"));
                if(!lot.reserved_by.empty() && !actions.count(lot.reserved_by)) fail("DanglingReservation",at(lp,"reserved_by"));
                add_weight(weight,lot.quantity,items.at(lot.item_id),lp);
                if(weight>inventory.capacity) fail("OverCapacity",path);
                inventory.lots.push_back(std::move(lot));
            }
            result.push_back(std::move(inventory));
        }
        return result;
}
// 库存及批次完整重组后求确定性摘要，仅作无损审计，不是新的资产权威。
Value inventory_record(const InventoryState& inventory) {
    Array lots; lots.reserve(inventory.lots.size());
    for(const auto& lot:inventory.lots) {
        Dict row=lot.extras;
        row.emplace("id",Value{lot.id}); row.emplace("item_id",Value{lot.item_id}); row.emplace("quantity",Value{lot.quantity});
        row.emplace("owner",lot.owner); row.emplace("reserved_by",Value{lot.reserved_by});
        lots.emplace_back();
        lots.back().data.emplace<Dict>(std::move(row));
    }
    Dict row=inventory.extras;
    row.emplace("id",Value{inventory.id}); row.emplace("owner",inventory.owner); row.emplace("capacity",Value{inventory.capacity});
    row.emplace("revision",Value{inventory.revision}); row.emplace("lots",Value{std::move(lots)});
    return Value{std::move(row)};
}
// 原count按完整owner、item及reservation匹配，不重复计算同一lot。
Result<std::int64_t> inventory_count(const InventoryState& inventory,const std::string& item_id,const std::vector<Value>& owners,const std::string& reservation,const std::string& path) {
    std::int64_t total=0;
    for(const auto& lot:inventory.lots) {
        if(lot.item_id!=item_id || lot.reserved_by!=reservation || std::find(owners.begin(),owners.end(),lot.owner)==owners.end()) continue;
        if(total>std::numeric_limits<std::int64_t>::max()-lot.quantity) return {false,0,{"ProjectionOverflow",path}};
        total+=lot.quantity;
    }
    return {true,total,{}};
}
// 原v1摘要格式保持；新profile的extras重建回原键集，不增加人工extras层。
std::string component_digest(const InventoryState& inventory) { return sha256(encode_value(inventory_record(inventory)).dump()); }
} // 唯一库存算法供旧兼容与新store复用。
