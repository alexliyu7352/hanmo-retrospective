#include "resident/kernel/catalog_section_observation.hpp"
// 正常作者与原生准入共守精确offer/角色来源，重复或未知条款直接拒绝。
#include "task_authored_continuation_catalog.hpp"
#include "task_model.hpp"
#include <set>
namespace hanmo::resident { namespace {
// 直接BoundaryValue调用也不能借重复键使不同读口获得不同目录。
void unique(const BoundaryValue& value,const StableId& path){std::set<StableId> keys;for(const auto& row:boundary::object(value,path))if(!keys.insert(row.first).second)boundary::fail("DuplicateTaskContinuationField",path+"/"+row.first);}
}
Outcome<TaskAuthoredContinuationCatalog> decode_task_authored_continuation_catalog(const BoundaryValue& value){try{
    using namespace boundary;unique(value,"authored_task_continuation");fields(value,{"version","message_addresses"},{},"authored_task_continuation");
    const auto version=get_int(value,"version","authored_task_continuation");if(version!=1&&version!=2)unsupported("UnsupportedAuthoredTaskContinuationVersion","authored_task_continuation");
    TaskAuthoredContinuationCatalog result;result.version=version;const auto& entries=array(field(value,"message_addresses","authored_task_continuation"),"message_addresses");
    if(entries.size()>4096)fail("TaskContinuationAddressLimit","message_addresses");std::set<StableId> offers;
    for(const auto& entry:entries){unique(entry,"message_address");fields(entry,{"offer_id","recipient_role_id"},{},"message_address");TaskMessageAddress address{get_string(entry,"offer_id","message_address"),get_string(entry,"recipient_role_id","message_address")};
        if(address.offer_id.empty()||address.recipient_role_id.empty()||!offers.insert(address.offer_id).second)fail("InvalidTaskContinuationAddress","message_address");result.message_addresses.push_back(std::move(address));}
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
Status validate_task_authored_continuation_catalog(const TaskAuthoredContinuationCatalog& policy,const TaskCatalog& tasks,const CommonCatalog& common){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 124, "TaskAuthoredContinuationCatalog", "validate_task_authored_continuation_catalog", &policy);

    if(policy.version!=1&&policy.version!=2)return common_failure("UnsupportedAuthoredTaskContinuationVersion");std::set<StableId> offers;
    if(policy.message_addresses.size()>4096)return common_failure("TaskContinuationAddressLimit");
    for(const auto& address:policy.message_addresses){const auto* offer=tasks.offers.find(address.offer_id);const auto* role=common.roles.find(address.recipient_role_id);
        if(!offers.insert(address.offer_id).second||!offer||offer->template_id!="message"||!role||role->initial_actor!=offer->goal.receiver_actor||!common.places.find(role->place_id)
            ||std::find(role->services.begin(),role->services.end(),"deliver_message")==role->services.end())return common_failure("InvalidTaskContinuationAddress",address.offer_id);}
    return common_success();
}
}
