// 独立1版尾；不向Shop1 visitor加空字段，也不新增Common事实或ReceiptData标签。
#pragma once
#include "private_shop_command_results.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PrivateShopCommandResultsCatalog& v){a("version",v.version);}
template<class A>void record(A& a,PrivateShopCommandResultBinding& v){a("id",v.id);a("source_kind",v.source_kind);a("submitter_actor_id",v.submitter_actor_id);a("result_id",v.result_id);a("result_kind",v.result_kind);a("source_tick",v.source_tick);a("from_tick",v.from_tick);a("result_tick",v.result_tick);a("allocation_begin",v.allocation_begin);a("request_seq",v.request_seq);a("revision",v.revision);}
template<class A>void record(A& a,PrivateShopCommandResultsState& v){a("version",v.version);a("bindings",v.bindings);a("control_bindings",v.control_bindings);}
}
namespace hanmo::resident {
Outcome<PrivateShopCommandResultsCatalog> decode_private_shop_command_results_catalog(const BoundaryValue&);
// 独立作者选择只准明确1版与原Shop1能力，旧未选情景不生成状态尾。
Outcome<std::optional<PrivateShopCommandResultsCatalog>> decode_private_shop_command_results_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<PrivateShopCommandResultsState> decode_private_shop_command_results_state(const BoundaryValue&);
class ProjectionWriter;
Status emit_private_shop_command_results_state_fields(ProjectionWriter&,const PrivateShopCommandResultsState&);
BoundaryValue encode_private_shop_command_results_state(const PrivateShopCommandResultsState&);
Status visit_private_shop_command_results_checkpoint(CodecVisitor&,PrivateShopCommandResultsState&);
}
