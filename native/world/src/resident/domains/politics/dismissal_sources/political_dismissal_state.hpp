// 独立来源段只给原撤职保存缺少的签权和实际提交；原Politics/Organization visitor字节不变。
#pragma once
#include "political_dismissal_source.hpp"
namespace hanmo::resident {
inline constexpr const char* political_dismissal_sources_module="political_dismissal_sources";
// 作者明确逐辖区选专项权；启用目录没有该辖区时拒，不能退回没有历史来源的旧写路。
struct PoliticalDismissalSourceCatalog {Integer version=1;OrderedTable<PoliticalDismissalSourcePolicy> policies;};
// 真实结果已进入原Common cache/history后才填写；首版全部由受控本人零时提交。
struct PoliticalDismissalResultBinding {StableId actor_id,result_id;Integer request_sequence=0,result_revision=0;};
struct PoliticalDismissalSourceRecord {PoliticalDismissalSource source;std::optional<PoliticalDismissalResultBinding> actual_result;};
struct PoliticalDismissalSourceState {Integer version=1;OrderedTable<PoliticalDismissalSourceRecord> dismissals;};
// 不反调完整资格；业务冷门必须先核每条真实撤职/Office/Grant和成员生命控制再消费结果。
using PoliticalDismissalResultMatch=std::function<bool(const CommitResult&,const StableId&,const StableId&)>;
}
