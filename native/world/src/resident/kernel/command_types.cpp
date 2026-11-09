// The sole ReceiptDetails owning copy boundary. Wire/schema stays unchanged.
#include "command_types.hpp"
#include "world_copy_plan.hpp"
#include <utility>
namespace hanmo::resident {namespace {
CheckpointCopyLease reserve_details(const ReceiptDetails& value){

    return CheckpointCopyLease::reserve();
}
CheckpointCopyLease reserve_data(const ReceiptData& value,const FieldOrder& order){

    std::visit([&](const auto& payload){},value);

    return CheckpointCopyLease::reserve();
}
}
ReceiptDetails::ReceiptDetails(const ReceiptData& value,const FieldOrder& order)
    :copy_memory(reserve_data(value,order)),data(value),field_order(order){}
ReceiptDetails::ReceiptDetails(const ReceiptDetails& value)
    :copy_memory(reserve_details(value)),data(value.data),field_order(value.field_order){}
ReceiptDetails::ReceiptDetails(ReceiptDetails&& value)
    :copy_memory(value.copy_memory),data(std::move(value.data)),field_order(std::move(value.field_order)){
    value.copy_memory={};
}
ReceiptDetails& ReceiptDetails::operator=(const ReceiptDetails& value){
    if(this!=&value){ReceiptDetails fresh(value);swap(fresh);}return *this;
}
ReceiptDetails& ReceiptDetails::operator=(ReceiptDetails&& value){
    if(this!=&value){ReceiptDetails fresh(std::move(value));swap(fresh);}return *this;
}
void ReceiptDetails::swap(ReceiptDetails& value){
    if(this==&value)return;
    auto left=copy_memory,right=value.copy_memory;
    auto both=CheckpointCopyLease::hold_both(left,right);
    copy_memory=both;value.copy_memory=both;
    using std::swap;swap(data,value.data);swap(field_order,value.field_order);
    copy_memory=std::move(right);value.copy_memory=std::move(left);
}
}
