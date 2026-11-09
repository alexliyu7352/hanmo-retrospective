#include "resident/domains/military/military_terms.hpp"
#include "resident/domains/military/military_command_contract.hpp"
// 军政冷边界只产出typed域；热规则不解析JSON，输出片段由中央按A全局事实/行动序合成。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
class ScopedCheckpointJsonDecode;
struct MilitaryActionTimeState;
struct FieldBattleWorldState;
namespace military_original_action_codec {
Outcome<OfficeTerms> decode_office_terms(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Status visit_office_terms(CodecVisitor&,OfficeTerms&);
Status emit_office_terms_fields(ProjectionWriter&,const OfficeTerms&);
Outcome<MilitaryTerms> decode_terms(const BoundaryValue&);
Outcome<MilitaryTerms> decode_terms(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Outcome<MilitaryActionResult> decode_result(const BoundaryValue&);
Outcome<MilitaryActionResult> decode_result(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Status visit_terms(CodecVisitor&,MilitaryTerms&);
Status visit_result(CodecVisitor&,MilitaryActionResult&);
Status emit_terms_fields(ProjectionWriter&,const MilitaryTerms&);
Status emit_result_fields(ProjectionWriter&,const MilitaryActionResult&);
}
enum class MilitaryProjectionRootTable {offices,garrisons,military_units,armies,campaigns,strongholds,sieges};
Outcome<bool> emit_military_action_params_record_projection(ProjectionWriter&,std::string_view,const MilitaryState&,const StableId&);
Outcome<bool> emit_military_fact_record_projection(ProjectionWriter&,std::string_view,const MilitaryState&,const StableId&,Integer occupation_source_schema=1);
Outcome<bool> emit_military_battle_record_projection(ProjectionWriter&,std::string_view,const MilitaryState&,const StableId&);
Outcome<bool> emit_military_root_record_projection(ProjectionWriter&,std::string_view,const MilitaryState&,MilitaryProjectionRootTable,const StableId&);
Status emit_military_state_fields(ProjectionWriter&,const MilitaryState&,Integer occupation_source_schema=1);
Status emit_military_action_payload_fields(ProjectionWriter&,const MilitaryState&);
Status emit_military_fact_fields(ProjectionWriter&,const MilitaryState&,Integer occupation_source_schema=1);
Status emit_military_battle_fields(ProjectionWriter&,const MilitaryState&);
// 作者能力解析供正式目录与Native预准入共用，禁止三份模块表各自猜测启用条件。
Outcome<std::optional<MilitaryRequestPolicy>> decode_military_request_policy(const BoundaryValue& data,const StableId& scenario_id);
Outcome<MilitaryCatalog> decode_military_catalog(const BoundaryValue& data,const StableId& scenario_id);
// Bootstrap admits only selected header-only physical facts. The actual owner
// source gate must run after its original ActionTime/authority adjunct attaches.
struct MilitaryHeaderBootstrapSelection {
 Integer authority_time_schema=0;
 Integer action_time_schema=0;
 Integer training_completion_time_schema=0;
 Integer battle_lifecycle_schema=0;
 // 只借同次已解码的原 owned 段；不从 header 或空 params 反推选型。
 const MilitaryActionTimeState* action_time=nullptr;
 const FieldBattleWorldState* field_battle=nullptr;
};
// 有限原 office/form 头与活动行动的结构拥有门；完整时间/资源校验仍由真 World 执行。
Outcome<bool> military_action_time_owns_fact_header(const MilitaryActionTimeState&,const FactRecord&);
Outcome<bool> military_action_time_owns_active_action(const MilitaryActionTimeState&,const ActionCoreView&);
Outcome<MilitaryState> decode_military_seed(const BoundaryValue& state,Integer occupation_source_schema=1);
Outcome<MilitaryState> decode_military_seed(const BoundaryValue&,Integer occupation_source_schema,const MilitaryHeaderBootstrapSelection&);
Outcome<BoundaryValue> export_military_state(const MilitaryState&,Integer occupation_source_schema=1);
Outcome<BoundaryValue> encode_military_quote(const MilitaryQuote&,const MilitaryCommand&);
Outcome<MilitaryCommand> decode_military_command(const StableId& kind,const BoundaryValue& params);
}
