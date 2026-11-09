// 原结果唯一终态发表：原 mutable 开始头不能充当 append-cursor 的终态事件。
#pragma once
#include "growth_fixed_service_model.hpp"
#include <string_view>
namespace hanmo::resident {
struct GrowthState;class CommonStore;class GrowthActionTimeRead;struct ActionTimeRegistration;
// 只持原结果与真实终态来源；没有第二奖励、完成状态或可重新结算的数值。
struct GrowthTerminalPublication {
 StableId publication_id,action_id;
 LifeFixedOutcomeRef original_result_ref;
 SourceRef terminal_source_ref;
 Integer terminal_source_schema=1;
 WorldMoment occurred_at;
};
struct GrowthTerminalPublicationView {
 const GrowthTerminalPublication* publication=nullptr;
 StableId actor_id,action_kind,rule_version;
};
// 原Action/领域占用已释放、原结果已落定、S0已采用同一终态后调用。
// 每action唯一；重入必须反验既有完整来源，绝不再分配ID/再append。
Outcome<StableId> append_growth_terminal_publication_in_candidate(CommonStore&,GrowthState&,const GrowthActionTimeRead&,const ActionTimeRegistration&,const StableId& action_id);
Outcome<GrowthTerminalPublicationView> read_growth_terminal_publication(const CommonStore&,const GrowthState&,const GrowthActionTimeRead&,const ActionTimeRegistration&,const StableId& publication_id);
Status validate_growth_terminal_publications(const CommonStore&,const GrowthState&,const GrowthActionTimeRead&,const std::vector<ActionTimeRegistration>& selected);
class CodecVisitor;class ProjectionWriter;struct BoundaryValue;
Status visit_growth_terminal_publications_v1(CodecVisitor&,OrderedTable<GrowthTerminalPublication>&);
Status emit_growth_terminal_publications_v1_projection(ProjectionWriter&,std::string_view,const OrderedTable<GrowthTerminalPublication>&);
Outcome<BoundaryValue> encode_growth_terminal_publications_v1(const OrderedTable<GrowthTerminalPublication>&);
Outcome<OrderedTable<GrowthTerminalPublication>> decode_growth_terminal_publications_v1(const BoundaryValue&);
}
