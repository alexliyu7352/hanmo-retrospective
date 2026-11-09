// V§12.1/T/K 固定十根的精确附项；原消息/知识仍各一份，期间源另归原Social owner。
#pragma once
#include "resident/model/dynamic_contract.hpp"
#include "resident/model/common_model.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "social_knowledge_key.hpp"
#include "propagation_model.hpp"
namespace hanmo::resident {
struct ReportObservationKey { StableId report_id; };
struct MessageCreatedKey { StableId message_id; };
struct MessageDeliveredKey { StableId message_id; };
using SocialTimeKey=std::variant<ReportObservationKey,MessageCreatedKey,MessageDeliveredKey,SocialKnowledgeKey>;
// Legacy只能真实原整界来源；新的writer同候选捕获Moment，已有精确owner则只引用原源。
struct SocialLegacyBoundary { Integer tick=0; CatalogIdentity original_catalog_identity; };
struct SocialExactMoment { WorldMoment moment; };
struct SocialExactSource { MomentOwnerSource original_timed_source_ref; };
using SocialTimeSource=std::variant<SocialLegacyBoundary,SocialExactMoment,SocialExactSource>;
struct SocialSourceTimeBinding {
 SocialTimeKey key; SocialTimeSource time_source; SourceRef actual_source_ref;
 // 只有显式 5/6 的 Knowledge 行保存真实 learn 时 Common 尾；旧版及其他行拒绝此尾。
 std::optional<SourceRef> learned_after_common_fact;
};
struct SocialLegacyChannel { StableId original_profile; };
struct SocialS3BusinessLink { SourceRef original_business_fact_ref; };
using SocialMessageSource=std::variant<SocialLegacyChannel,SocialS3BusinessLink>;
struct SocialMessageExecutionBinding {
 StableId message_id; SocialMessageSource source_binding;
 std::optional<SourceRef> actual_delivery_ref,actual_dossier_access_ref;
 std::vector<SocialKnowledgeKey> actual_knowledge_keys;
 FixedLockedRuleInput channel_rule_ref;
};
// S3地址分支只保唯一业务ref；当前选择禁止启用，不把机构伪装成零actor。
struct SocialS3AddressBinding { SourceRef original_business_fact_ref; };
struct SocialCollectionCursor { std::optional<StableId> last_scanned_fact_id; WorldMoment through_moment; };
struct SocialTime2State {
 OrderedTable<SocialSourceTimeBinding> source_time_bindings;
 OrderedTable<SocialMessageExecutionBinding> message_execution_bindings;
 OrderedTable<SocialPropagationDecision> propagation_decisions;
 OrderedTable<SocialPropagationCooldown> propagation_cooldowns;
 SocialCollectionCursor collection_cursor;
};
// K规定的原五字段完整保留并追加真实learned_at；这是只读投影，不新增持久receipt表。
struct KnowledgeReceiptV2 {
 SubjectRef receiver; StableId report_id; std::optional<StableId> source_message_id;
 Integer learned_tick=0; SubjectRef source_subject; WorldMoment learned_at;
};
}
