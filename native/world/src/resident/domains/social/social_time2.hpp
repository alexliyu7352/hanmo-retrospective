// 原Social
// writer的精确来源适配；公开读取只返回已有知识，不采集、不送信、不补learn。
#pragma once
#include "resident/kernel/dynamic_result_time2.hpp"
#include "social_model.hpp"
namespace hanmo::resident {
// typed key唯一编码仅作原表索引，不分ID；业务关系仍必须逐字段反查。
StableId social_time_key(const SocialTimeKey &);
StableId social_subject_key(const SubjectRef &);
// 新 owning
// 消费者持有真实键内存直到最后一次原表访问；键字节仍由唯一编码器产生。
class OwnedSocialIndexKey {
  CheckpointMemoryCharge memory_;
  StableId value_;

public:
  explicit OwnedSocialIndexKey(const SubjectRef &);
  explicit OwnedSocialIndexKey(const SocialTimeKey &);
  OwnedSocialIndexKey(const OwnedSocialIndexKey &) = delete;
  OwnedSocialIndexKey &operator=(const OwnedSocialIndexKey &) = delete;
  OwnedSocialIndexKey(OwnedSocialIndexKey &&) noexcept = default;
  OwnedSocialIndexKey &operator=(OwnedSocialIndexKey &&) = delete;
  const StableId &value() const noexcept { return value_; }
};
bool same_social_subject(const SubjectRef &, const SubjectRef &);
const KnownReport *find_social_knowledge(const DynamicSocialState &,
                                         const SocialKnowledgeKey &);
// Same actual key grammar, compared without constructing temporary string keys.
const KnownReport *find_social_knowledge(const DynamicSocialState &,
                                         const SubjectRef &, const StableId &);
const OrderedTable<KnownReport> *
find_social_knowledge_table(const DynamicSocialState &, const SubjectRef &);
// Full selected owner collection consumes real reportability and actual
// participant disclosure; the old supply-only entry remains a compatibility
// path.
Status collect_exact_dynamic_reports(CommonStore &, SocialState &,
                                     const ExactResultTimeRead &,
                                     const ExactResultReadStamp &,
                                     const WorldMoment &,
                                     const CatalogIdentity &);
Status collect_exact_supply_reports(CommonStore &, SocialState &,
                                    const ExactResultTimeRead &,
                                    const ExactResultReadStamp &,
                                    const WorldMoment &,
                                    const CatalogIdentity &,
                                    const DirectorCatalog *);
// 整界原观察沿真实旧源精度；精确owner只能通过静态原源引用反读，缺handler技术拒绝。
Outcome<WorldMoment> read_social_bound_moment(const SocialSourceTimeBinding &,
                                              const ExactResultTimeRead &,
                                              const ExactResultReadStamp &);
// 完整typed源、投影、每项来源行及消息/知识双向覆盖；不在冷读重新调用writer。
Status validate_social_time2(const CommonStore &, const SocialState &,
                             const ExactResultTimeRead &,
                             const ExactResultReadStamp &,
                             const CatalogIdentity &);
// Validates the immutable Report projection against its one original result
// owner.
Status validate_exact_report_source(const CommonStore &, const Report &,
                                    const ExactResultTimeRead &,
                                    const ExactResultReadStamp &);
// Borrowed original row and exact applied time; valid only in this unmodified
// Social owner scope. No duplicate receiver/report/message strings are
// allocated.
struct KnowledgeReceiptViewV2 {
  const KnownReport *original = nullptr;
  WorldMoment learned_at;
};
Outcome<std::optional<KnowledgeReceiptViewV2>>
read_knowledge_receipt_view_v2(const CommonStore &, const SocialState &,
                               const ExactResultTimeRead &, const SubjectRef &,
                               const StableId &, const ExactResultReadStamp &);
// Compatibility owning projection. New immediate consumers use the borrowed
// view.
Outcome<std::optional<KnowledgeReceiptV2>>
read_knowledge_receipt_v2(const CommonStore &, const SocialState &,
                          const ExactResultTimeRead &, const SubjectRef &,
                          const StableId &, const ExactResultReadStamp &);
// 同实际world只读片签发的深拥有history值；绑定精确source身份，不授权后继业务采用。
class ExactSocialReportRead {
  CheckpointMemoryCharge memory_;
  std::shared_ptr<const ExactResultTimeRead> source_;
  CheckpointVector<KnowledgeReceiptV2> receipts_;
  ExactSocialReportRead(CheckpointMemoryCharge memory,
                        std::shared_ptr<const ExactResultTimeRead> source)
      : memory_(std::move(memory)), source_(std::move(source)) {}
  friend Outcome<std::shared_ptr<const ExactSocialReportRead>>
  capture_exact_social_report_read(const CommonStore &, const SocialState &,
                                   std::shared_ptr<const ExactResultTimeRead>,
                                   const CatalogIdentity &);

public:
  Outcome<const KnowledgeReceiptV2 *>
  knowledge_receipt_v2_borrowed(const SubjectRef &, const StableId &,
                                const ExactResultReadStamp &) const;
  Outcome<std::optional<KnowledgeReceiptV2>>
  knowledge_receipt_v2(const SubjectRef &, const StableId &,
                       const ExactResultReadStamp &) const;
};
Outcome<std::shared_ptr<const ExactSocialReportRead>>
capture_exact_social_report_read(const CommonStore &, const SocialState &,
                                 std::shared_ptr<const ExactResultTimeRead>,
                                 const CatalogIdentity &);
} // namespace hanmo::resident
