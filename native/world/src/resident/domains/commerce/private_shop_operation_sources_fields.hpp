// One field callback for cold JSON, checkpoint, projection and retention census.
#pragma once
#include "private_shop_operation_sources_model.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/kernel/fixed_service_work_period_codec_fields.hpp"
namespace hanmo::resident::private_shop_operation_source_fields {
using namespace exact_fields;
template<class C>Status rule(C&c,ActorFoundationRuleRef&v){required_ref(c,"id",v.id);schema(c,"version",v.version);return required_ref(c,"catalog_digest",v.catalog_digest);}
template<class C>Status acceptance(C&c,PrivateShopOperationAcceptance&v){
 required_ref(c,"operation_ref",v.operation_ref);
 c.object("original_shop_epoch_ref",[&](auto&x){return source(x,v.original_shop_epoch_ref);});
 c.object("market_definition_ref",[&](auto&x){return rule(x,v.market_definition_ref);});
 c.object("accepted_at",[&](auto&x){return moment(x,v.accepted_at);});
 c.object("committed_service_window",[&](auto&x){x.object("from",[&](auto&y){return moment(y,v.committed_service_window.from);});return x.object("to",[&](auto&y){return moment(y,v.committed_service_window.to);});});
 c.object("actual_presence_source_ref",[&](auto&x){return source(x,v.actual_presence_source_ref);});
 if(!c.status().ok)return c.status();
 if(take(compare_world_moments(v.accepted_at,v.committed_service_window.from))||take(compare_world_moments(v.committed_service_window.from,v.committed_service_window.to))>=0)return c.reject("InvalidShopCommittedServiceWindow",v.operation_ref);
 return c.status();
}
template<class C>Status labor(C&c,PrivateShopLaborAppliedSource&v){
 required_ref(c,"id",v.id);required_ref(c,"operation_ref",v.operation_ref);
 c.object("work_period_ref",[&](auto&x){return fixed_period_fields::ref(x,v.work_period_ref);});
 c.object("occurred_at",[&](auto&x){return moment(x,v.occurred_at);});
 c.optional("original_staff_service_ref",v.original_staff_service_ref,[&](auto&x,auto&r){return source(x,r);});
 if(!c.status().ok)return c.status();
 const auto*b=std::get_if<ActorActionWorkBinding>(&v.work_period_ref.binding);
 if(!b||b->action_id!=v.operation_ref||b->action_kind!="private_shop_operate"||
  b->source_owner_ref.domain!="private_shops"||b->source_owner_ref.kind!="private_shop_operating_started"||
  b->source_owner_ref.id!=v.operation_ref||v.work_period_ref.period_ordinal<0||
  v.work_period_ref.period_ordinal>=2||take(compare_world_moments(v.occurred_at,v.work_period_ref.until)))
  return c.reject("InvalidPrivateShopLaborPeriod",v.id);
 return c.status();
}
template<class C>Status terminal(C&c,PrivateShopOperationTerminalSource&v){
 required_ref(c,"operation_ref",v.operation_ref);
 c.object("original_result_ref",[&](auto&x){return source(x,v.original_result_ref);});
 c.object("occurred_at",[&](auto&x){return moment(x,v.occurred_at);});
 c.object("actual_cause_ref",[&](auto&x){x.object("source_ref",[&](auto&y){return source(y,v.actual_cause_ref.source_ref);});return schema(x,"source_schema",v.actual_cause_ref.source_schema);});
 if(c.status().ok&&(v.original_result_ref.domain!="private_shops"||
  (v.original_result_ref.kind!="private_shop_operating_completed"&&v.original_result_ref.kind!="private_shop_operating_interrupted")))return c.reject("PrivateShopOriginalTerminalAddressRequired",v.operation_ref);
 return c.status();
}
template<class C>Status state(C&c,PrivateShopOperationSourcesState&v){
 c.field("schema_version",v.schema_version);
 if(c.status().ok&&v.schema_version!=1)return c.reject("UnsupportedPrivateShopOperationSourcesVersion");
 std::size_t index=0;
 c.sequence("accepted_operations",v.accepted_operations,[&](auto&x,auto&row){acceptance(x,row);if(x.status().ok)for(std::size_t n=0;n<index;++n)if(v.accepted_operations[n].operation_ref==row.operation_ref)return x.reject("DuplicatePrivateShopOperationAcceptance",row.operation_ref);++index;return x.status();});
 index=0;
 c.sequence("labor_applied_sources",v.labor_applied_sources,[&](auto&x,auto&row){labor(x,row);if(x.status().ok)for(std::size_t n=0;n<index;++n){const auto&old=v.labor_applied_sources[n];if(old.id==row.id||(old.operation_ref==row.operation_ref&&old.work_period_ref.period_ordinal==row.work_period_ref.period_ordinal))return x.reject("DuplicatePrivateShopLaborSource",row.id);}++index;return x.status();});
 index=0;
 c.sequence("terminal_sources",v.terminal_sources,[&](auto&x,auto&row){terminal(x,row);if(x.status().ok)for(std::size_t n=0;n<index;++n){const auto&old=v.terminal_sources[n];if(old.operation_ref==row.operation_ref||same_source_ref(old.original_result_ref,row.original_result_ref))return x.reject("DuplicatePrivateShopTerminalSource",row.operation_ref);}++index;return x.status();});
 return c.status();
}
}
