// 新市场/法律/私铺/店单仅追加独立段；错误pair和旧格式伪装必须在标题选择前拒绝。
#include "resident/kernel/world_codec.hpp"
#include "resident/checkpoint/checkpoint_selection.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>
using namespace hanmo::resident;
namespace {
Integer checks=0;
void check(bool value){++checks;if(!value)throw std::runtime_error("shop checkpoint selection check "+std::to_string(checks));}
CheckpointIdentity base_identity(){CheckpointIdentity value;value.catalog.ruleset_id="hanmo.native.local_society/1";value.catalog.module_versions={{"dynamic_world","local_society_v1"},{"local_economy","1"},{"organization","2"},{"affairs","1"},{"director","2"},{"regional_life","1"},{"politics","1"}};return value;}
void add(CheckpointIdentity& value,const StableId& name,const StableId& version="1"){value.catalog.module_versions.emplace_back(name,version);}
void erase(CheckpointIdentity& value,const StableId& name){auto& rows=value.catalog.module_versions;rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){return row.first==name;}),rows.end());}
}
int main(){
 const auto original=base_identity();auto old=checkpoint_domains_from_identity(original);check(old.ok);
 for(const auto* name:{"growth_market_history","political_shop_licensing","private_shops","task_shop_orders"})check(std::none_of(old.value.begin(),old.value.end(),[&](const auto& row){return row.name==name;}));
 auto selected=original;for(const auto* name:{"growth_market_history","political_shop_licensing","private_shops","task_shop_orders"})add(selected,name);
 auto fresh=checkpoint_domains_from_identity(selected);check(fresh.ok);check(fresh.value.size()==old.value.size()+4);
 for(std::size_t i=0;i<old.value.size();++i)check(fresh.value[i].name==old.value[i].name&&fresh.value[i].codec_version==old.value[i].codec_version);
 const std::vector<StableId> tail={"growth_market_history","political_shop_licensing","private_shops","task_shop_orders"};
 for(std::size_t i=0;i<tail.size();++i)check(fresh.value[old.value.size()+i].name==tail[i]&&fresh.value[old.value.size()+i].codec_version==1);
 for(const auto* required:{"growth_market_history","political_shop_licensing","private_shops","politics","regional_life"}){auto wrong=selected;erase(wrong,required);check(!checkpoint_domains_from_identity(wrong).ok);}
 for(const auto& name:tail){auto duplicate=selected;add(duplicate,name);check(!checkpoint_domains_from_identity(duplicate).ok);auto version=selected;erase(version,name);add(version,name,"2");check(!checkpoint_domains_from_identity(version).ok);}
 auto market=original;add(market,"growth_market_history");auto market_only=checkpoint_domains_from_identity(market);check(market_only.ok&&market_only.value.size()==old.value.size()+1);
 auto no_order=selected;erase(no_order,"task_shop_orders");check(checkpoint_domains_from_identity(no_order).ok);
 auto task_only=original;add(task_only,"task_shop_orders");check(!checkpoint_domains_from_identity(task_only).ok);
 auto law_only=original;add(law_only,"political_shop_licensing");check(!checkpoint_domains_from_identity(law_only).ok);
 auto unknown=selected;add(unknown,"private_shops_fake");check(!checkpoint_domains_from_identity(unknown).ok);
 auto again=checkpoint_domains_from_identity(original);check(again.ok&&again.value.size()==old.value.size());
 std::cout<<"PRIVATE_SHOP_CHECKPOINT_SELECTION "<<checks<<"/0\n";
}
