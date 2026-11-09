// 原 S3/Familly/P02/Social provenance 的中央有限拥有根映射；拒绝同值影子行。
#include "native_household_report_source_pins.hpp"
#include "native_household_purchase_report_world.hpp"
#include "native_character_family_world.hpp"
#include "native_face_to_face_sources.hpp"
#include "world_bundle.hpp"
#include "native_complete_world_sources.hpp"
#include "world_read_stamp.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <array>
#include <algorithm>
#include <limits>
#include <type_traits>
namespace hanmo::resident {
namespace {
using Slot = NativeHouseholdReportDependencySlot;
using RowOwner = NativeHouseholdReportRowOwner;
using ASlot = CharacterAvailabilityProvenanceSlot;
// 明示的 consumer/owner 表；不从动态行或未知 enum 推导注册。
struct RowRegistration {
  Slot slot;
  RowOwner owner;
};
constexpr std::array<RowRegistration, 28> row_registrations{
    {{Slot::plan, RowOwner::household_plans},
     {Slot::policy, RowOwner::household_policies},
     {Slot::population_selection, RowOwner::population_selections},
     {Slot::population_materialization, RowOwner::population_materializations},
     {Slot::population_materialization, RowOwner::population_transport_bindings},
     {Slot::population_materialization, RowOwner::population_transfers},
     {Slot::buyer_labor, RowOwner::buyer_labor},
     {Slot::population_initial, RowOwner::authored_cohorts},
     {Slot::population_initial, RowOwner::current_cohorts},
     {Slot::population_initial, RowOwner::population_heads},
     {Slot::population_initial, RowOwner::population_transport_bindings},
     {Slot::population_revision, RowOwner::population_revisions},
     {Slot::report, RowOwner::social_reports},
     {Slot::knowledge, RowOwner::social_knowledge},
     {Slot::knowledge_time, RowOwner::social_time_bindings},
     {Slot::report_time, RowOwner::social_time_bindings},
     {Slot::message_created_time, RowOwner::social_time_bindings},
     {Slot::message_delivered_time, RowOwner::social_time_bindings},
     {Slot::message_execution, RowOwner::social_message_execution},
     {Slot::sender_knowledge, RowOwner::social_knowledge},
     {Slot::sender_knowledge_time, RowOwner::social_time_bindings},
     {Slot::pending_message, RowOwner::social_messages},
     {Slot::message_business, RowOwner::communication_business},
     {Slot::message_delivery, RowOwner::communication_delivery},
     {Slot::dossier_access, RowOwner::communication_dossier},
     {Slot::population_revision, RowOwner::population_heads},
     {Slot::population_initial, RowOwner::population_transfers},
     {Slot::population_initial, RowOwner::population_losses}}};
// 校验原槽定义的 established 分类；typed 回调不得伪装成来源引用。
const CharacterAvailabilityProvenanceSlotSpec *availability_slot(ASlot slot) {
  for (const auto &s : character_availability_provenance_slots)
    if (s.slot == slot)
      return &s;
  return nullptr;
}
bool same(SourceRefBuildView a, SourceRefBuildView b) noexcept {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
Status mismatch(const char *code) {
  return common_failure(code, {}, ErrorKind::internal);
}
// 原有 OrderedTable 的地址身份，不接受复制相同键值构成的新行。
template <class T>
const StableId *actual_key(const OrderedTable<T> &table, const T &value) {
  for (const auto &entry : table.rows())
    if (&entry.second == &value)
      return &entry.first;
  return nullptr;
}
} // namespace
class NativeHouseholdReportPinCollector final {
  const WorldBundle &world_;
  const HouseholdPurchaseReportContext &report_;
  const CharacterAvailabilityRetentionContext &owned_;
  const CharacterAvailabilitySourceRetentionContext &sources_;
  NativeHouseholdReportSourcePins out_;
  Status status_ = common_success();
  // 首错不会被忽略 sink Status 的域 writer 擦掉。
  Status reject(Status status) {
    if (status_.ok && !status.ok)
      status_ = std::move(status);
    return status_;
  }
  Status append(const HouseholdPurchaseReportDisclosure &consumer,
                NativeHouseholdReportPinSlot slot, bool applied,
                NativeHouseholdReportDependency value) {
    if (!status_.ok)
      return status_;
    if (!world_.interaction.face_to_face_contacts ||
        world_.interaction.face_to_face_contacts->purchase_plan_disclosures
                .find(consumer.id) != &consumer)
      return reject(mismatch("HouseholdPinActualConsumerRequired"));
    out_.pins_.push_back({&consumer, slot, applied, std::move(value)});
    return common_success();
  }

public:
  NativeHouseholdReportPinCollector(
      const WorldBundle &world, const HouseholdPurchaseReportContext &report,
      const CharacterAvailabilityRetentionContext &owned,
      const CharacterAvailabilitySourceRetentionContext &sources)
      : world_(world), report_(report), owned_(owned), sources_(sources) {
    out_.world_ = &world;
  }
  const Status &status() const noexcept { return status_; }
  NativeHouseholdReportSourcePins finish() { return std::move(out_); }
  // 整段前缀逐项保真实 Common 存储身份；不是按 kind 猜 schema。
  Status common(const HouseholdPurchaseReportDisclosure &d, Slot slot,
                std::size_t position, const FactRecord &head,
                bool applied = true) {
    if (slot != Slot::actual_delivery && slot != Slot::actual_contact_prefix &&
        slot != Slot::controller_history_prefix &&
        slot != Slot::population_initial &&
        slot != Slot::population_materialization)
      return reject(mismatch("HouseholdPinCommonSlotNotRegistered"));
    auto actual = read_actual_common_fact_position(world_.common, head);
    if (!actual.ok)
      return reject({false, {}, std::move(actual.error)});
    if (actual.value != position ||
        world_.common.state().facts.find(head.id) != &head)
      return reject(mismatch("HouseholdPinCommonOwnerMismatch"));
    return append(
        d, slot, applied,
        NativeHouseholdReportCommonFactPin{&world_.common, &head, position});
  }
  // SourceRef 保留只接受原 S3 full reader 实际声明过的五种用途。
  Status source(const HouseholdPurchaseReportDisclosure &d, Slot slot,
                const SourceRef &ref, Integer schema, bool applied = true) {
    if (slot != Slot::actual_delivery && slot != Slot::original_observation &&
        slot != Slot::original_permission && slot != Slot::actor_location &&
        slot != Slot::population_location &&
        slot != Slot::population_revision && slot != Slot::social_time_source)
      return reject(mismatch("HouseholdPinSourceSlotNotRegistered"));
    if (!valid_source_ref(ref) || schema < 1)
      return reject(mismatch("HouseholdPinActualSchemaRequired"));


    (out_.payload_memory_).grow(0);
    NativeHouseholdReportNamedSourcePin value{{ref.domain, ref.kind, ref.id},
                                              schema};
    return append(d, slot, applied, std::move(value));
  }
  // 每个 owned 行须由本方法调用者先以真实表验证，注册再核用途/owner 组合。
  Status row(const HouseholdPurchaseReportDisclosure &d, Slot slot, RowOwner owner,
             const void *root, const void *value, const StableId *key,
             std::size_t ordinal = 0, bool applied = true) {
    bool registered = false;
    for (const auto &r : row_registrations)
      if (r.slot == slot && r.owner == owner)
        registered = true;
    if (!registered || !root || !value ||
        (owner != RowOwner::population_revisions && !key))
      return reject(mismatch("HouseholdPinOwnedRowRegistrationRequired"));
    return append(
        d, slot, applied,
        NativeHouseholdReportOwnedRowPin{owner, root, value, key, ordinal});
  }
  // 六个 established 槽回原领域 resolver，未知军事/Sea 版本不猜别名。
  Status availability(const HouseholdPurchaseReportDisclosure &d, ASlot slot,
                      SourceRefBuildView ref, Integer schema, bool applied) {
    const auto *spec = availability_slot(slot);
    if (!spec || !spec->established_source_identity)
      return reject(mismatch("HouseholdPinEstablishedSlotRequired"));
    auto actual = resolve_character_availability_source_retention_ref(
        sources_, slot, ref, schema);
    if (!actual.ok)
      return reject({false, {}, std::move(actual.error)});
    return append(d, slot, applied, std::move(actual.value));
  }
  // 五种无 SourceRef 原对象严格解析原 owner；不是依类型名注册伪事实。
  template <class T>
  Status owned(const HouseholdPurchaseReportDisclosure &d, ASlot slot,
               const T &value, bool applied) {
    const auto *spec = availability_slot(slot);
    if (!spec || spec->established_source_identity)
      return reject(mismatch("HouseholdPinTypedSlotRequired"));
    auto actual = resolve_character_availability_retention_ref(owned_, value);
    if (!actual.ok)
      return reject({false, {}, std::move(actual.error)});
    return append(d, slot, applied, std::move(actual.value));
  }
  const WorldBundle &world() const noexcept { return world_; }
  const HouseholdPurchaseReportContext &report() const noexcept {
    return report_;
  }
};
namespace {
// 全历史前缀与十一槽共用实际 consumer，不把不同告知的引用混成同一个键。
class ControllerPins final : public CommonActualFactPrefixSink,
                             public CharacterAvailabilityProvenanceSink {
  NativeHouseholdReportPinCollector &out_;
  const HouseholdPurchaseReportDisclosure &d_;

public:
  ControllerPins(NativeHouseholdReportPinCollector &out,
                 const HouseholdPurchaseReportDisclosure &d)
      : out_(out), d_(d) {}
  Status actual_prefix_fact(std::size_t p, const FactRecord &f) override {
    return out_.common(d_, Slot::controller_history_prefix, p, f);
  }
  Status append(ASlot s, SourceRefBuildView r, Integer v, bool b) override {
    return out_.availability(d_, s, r, v, b);
  }
  Status original_origin(const CharacterOriginRecordV2 &r, bool b) override {
    return out_.owned(d_, ASlot::original_origin, r, b);
  }
  Status created_registration(const RegistrationSourceV2 &r, bool b) override {
    return out_.owned(d_, ASlot::created_registration, r, b);
  }
  Status birth_allocation(const InitialCapabilityAllocation &r,
                          bool b) override {
    return out_.owned(d_, ASlot::birth_allocation, r, b);
  }
  Status original_birth(const BirthFactRef &r, bool b) override {
    return out_.owned(d_, ASlot::original_birth, r, b);
  }
  Status born_creation(const FactRecord &r, bool b) override {
    return out_.owned(d_, ASlot::born_creation, r, b);
  }
};
// 同栈取原前缀，不另建 ID/源表，也不把最后一个头冒整个前缀。
class ContactPins final : public CommonActualFactPrefixSink {
  NativeHouseholdReportPinCollector &out_;
  const HouseholdPurchaseReportDisclosure &d_;

public:
  ContactPins(NativeHouseholdReportPinCollector &out,
              const HouseholdPurchaseReportDisclosure &d)
      : out_(out), d_(d) {}
  Status actual_prefix_fact(std::size_t p, const FactRecord &f) override {
    return out_.common(d_, Slot::actual_contact_prefix, p, f);
  }
};
// S3 已完整读过的业务原行在这里再核 World 实址；不是再实现采购或知情规则。
class BusinessPins final : public HouseholdPurchaseReportProvenanceSink {
  NativeHouseholdReportPinCollector &out_;
  const HouseholdPurchaseReportDisclosure &d_;
  template <class T>
  Status table(Slot slot, RowOwner owner, const void *root,
               const OrderedTable<T> &t, const T &row) {
    const auto *key = actual_key(t, row);
    if (!key)
      return mismatch("HouseholdPinOriginalTableRowRequired");
    return out_.row(d_, slot, owner, root, &row, key);
  }

  // 借实际 typed key 查唯一时间行，不构造未计量的社会复合字符串键。
  Outcome<const SocialSourceTimeBinding *>
  time_binding(const SocialTime2State &time, Slot slot, const StableId &id,
               const SubjectRef *receiver = nullptr) const {
    const SocialSourceTimeBinding *found = nullptr;
    for (const auto &entry : time.source_time_bindings.rows()) {
      const auto &key = entry.second.key;
      bool matches = false;
      if (slot == Slot::report_time) {
        const auto *k = std::get_if<ReportObservationKey>(&key);
        matches = k && k->report_id == id;
      } else if (slot == Slot::message_created_time) {
        const auto *k = std::get_if<MessageCreatedKey>(&key);
        matches = k && k->message_id == id;
      } else if (slot == Slot::message_delivered_time) {
        const auto *k = std::get_if<MessageDeliveredKey>(&key);
        matches = k && k->message_id == id;
      } else if (slot == Slot::sender_knowledge_time && receiver) {
        const auto *k = std::get_if<SocialKnowledgeKey>(&key);
        matches = k && k->report_id == id &&
                  same_social_subject(k->receiver, *receiver);
      } else
        return {false, nullptr,
                mismatch("HouseholdPinTimeKeyNotRegistered").error};
      if (matches) {
        if (found)
          return {false, nullptr,
                  mismatch("HouseholdPinDuplicateTimeKey").error};
        found = &entry.second;
      }
    }
    if (!found)
      return {false, nullptr,
              mismatch("HouseholdPinOriginalTimeBindingRequired").error};
    return {true, found, {}};
  }
  // 时间拥有行与其真实 MomentOwnerSource 分别保留；participant 并非 Common 头。
  Status time(const SocialState &social, Slot slot,
              const SocialSourceTimeBinding &binding) {
    auto kept = table(slot, RowOwner::social_time_bindings, &social,
                      social.dynamic->time2->source_time_bindings, binding);
    if (!kept.ok)
      return kept;
    if (const auto *exact =
            std::get_if<SocialExactSource>(&binding.time_source))
      return out_.source(d_, Slot::social_time_source,
                         exact->original_timed_source_ref.source_ref,
                         exact->original_timed_source_ref.source_schema);
    return common_success();
  }

public:
  BusinessPins(NativeHouseholdReportPinCollector &out,
               const HouseholdPurchaseReportDisclosure &d)
      : out_(out), d_(d) {}
  Status household(const HouseholdPurchaseContext &h,
                   const HouseholdPurchasePlan &plan,
                   const HouseholdPurchasePolicy &policy,
                   const PopulationSelection &selection,
                   const LaborAllocation &labor) override {
    const auto &x = out_.report();
    const auto &w = out_.world();
    if (&h != &x.purchase || !w.household_purchases ||
        &h.state != &*w.household_purchases || &h.economy != &w.local_economy ||
        !w.population_transport || &h.population != &*w.population_transport)
      return mismatch("HouseholdPinPurchaseRootMismatch");
    auto v = table(Slot::plan, RowOwner::household_plans, &h.state, h.state.plans,
                   plan);
    if (!v.ok)
      return v;
    v = table(Slot::policy, RowOwner::household_policies, &h.state,
              h.state.policies, policy);
    if (!v.ok)
      return v;
    v = table(Slot::population_selection, RowOwner::population_selections,
              &h.population, h.population.selections, selection);
    if (!v.ok)
      return v;
    v = table(Slot::buyer_labor, RowOwner::buyer_labor, &h.economy,
              h.economy.labor_allocations, labor);
    if (!v.ok)
      return v;
    // selection 冷读也检查后来物化的真行；其时间依原 registration
    // 头，不读今日人数。
    const auto *materialized = h.population.materializations.find(selection.id);
    if (materialized) {
      if (!materialized->result_cohort_ref.transport_binding_id)
        return mismatch("HouseholdPinMaterializationBindingRequired");
      const auto *binding = h.population.transport_bindings.find(
          *materialized->result_cohort_ref.transport_binding_id);
      if (!binding || binding->transport_cohort_id !=
                          materialized->result_cohort_ref.cohort_id)
        return mismatch("HouseholdPinMaterializationBindingMismatch");
      const auto &source = binding->registration_source_ref;
      const auto *head = w.common.state().facts.find(source.id);
      const auto *consumer = w.common.state().facts.find(d_.id);
      if (!head || !consumer || head->domain != source.domain ||
          head->kind != source.kind)
        return mismatch("HouseholdPinMaterializationHeadRequired");
      auto position = read_actual_common_fact_position(w.common, *head);
      auto cutoff = read_actual_common_fact_position(w.common, *consumer);
      if (!position.ok)
        return {false, {}, std::move(position.error)};
      if (!cutoff.ok)
        return {false, {}, std::move(cutoff.error)};
      const bool applied = position.value < cutoff.value;
      const auto *key =
          actual_key(h.population.materializations, *materialized);
      const auto *binding_key =
          actual_key(h.population.transport_bindings, *binding);
      if (!key || !binding_key)
        return mismatch("HouseholdPinMaterializationOwnerMismatch");
      v = out_.row(d_, Slot::population_materialization,
                   RowOwner::population_materializations, &h.population,
                   materialized, key, 0, applied);
      if (!v.ok)
        return v;
      v = out_.row(d_, Slot::population_materialization,
                   RowOwner::population_transport_bindings, &h.population, binding,
                   binding_key, 0, applied);
      if (!v.ok)
        return v;
      v = out_.common(d_, Slot::population_materialization, position.value,
                      *head, applied);
      if (!v.ok)
        return v;
      if (materialized->transfer_ref) {
        const auto *transfer =
            h.economy.population_transfers.find(*materialized->transfer_ref);
        if (!transfer)
          return mismatch("HouseholdPinMaterializationTransferRequired");
        const auto *transfer_key =
            actual_key(h.economy.population_transfers, *transfer);
        v = out_.row(d_, Slot::population_materialization,
                     RowOwner::population_transfers, &h.economy, transfer,
                     transfer_key, 0, applied);
        if (!v.ok)
          return v;
      }
    }
    return common_success();
  }
  Status social(const SocialState &s, const Report &report,
                const ActorReportKnowledgeReceiptView &known) override {
    const auto &w = out_.world();
    const auto &x = out_.report();
    if (&s != &w.social || &s != &x.disclosure.social || !s.dynamic ||
        !s.dynamic->time2 || known.report != &report ||
        !known.knowledge.original || !known.acquisition)
      return mismatch("HouseholdPinSocialRootMismatch");
    const auto &dynamic = *s.dynamic;
    auto v =
        table(Slot::report, RowOwner::social_reports, &s, dynamic.reports, report);
    if (!v.ok)
      return v;
    const auto &k = *known.knowledge.original;
    const auto *kt = find_social_knowledge_table(dynamic, k.receiver);
    if (!kt || find_social_knowledge(dynamic, k.receiver, k.report_id) != &k)
      return mismatch("HouseholdPinKnownReportOwnerMismatch");
    v = table(Slot::knowledge, RowOwner::social_knowledge, &s, *kt, k);
    if (!v.ok)
      return v;
    v = time(s, Slot::knowledge_time, *known.acquisition);
    if (!v.ok)
      return v;
    auto observed = time_binding(*dynamic.time2, Slot::report_time, report.id);
    if (!observed.ok)
      return {false, {}, std::move(observed.error)};
    v = time(s, Slot::report_time, *observed.value);
    if (!v.ok)
      return v;
    if (known.message) {
      const auto &message = *known.message;
      v = table(Slot::pending_message, RowOwner::social_messages, &s,
                dynamic.pending_messages, message);
      if (!v.ok)
        return v;
      const auto *execution =
          dynamic.time2->message_execution_bindings.find(message.id);
      if (!execution || execution->message_id != message.id)
        return mismatch("HouseholdPinMessageExecutionRequired");
      v = table(Slot::message_execution, RowOwner::social_message_execution, &s,
                dynamic.time2->message_execution_bindings, *execution);
      if (!v.ok)
        return v;
      for (const auto slot :
           {Slot::message_created_time, Slot::message_delivered_time}) {
        auto bound = time_binding(*dynamic.time2, slot, message.id);
        if (!bound.ok)
          return {false, {}, std::move(bound.error)};
        v = time(s, slot, *bound.value);
        if (!v.ok)
          return v;
      }
      // 原消息知情读只反读 sender 的原 Knowledge/时间，不递归另一次消息链。
      const auto *sender_table =
          find_social_knowledge_table(dynamic, message.sender);
      const auto *sender =
          find_social_knowledge(dynamic, message.sender, report.id);
      if (!sender_table || !sender)
        return mismatch("HouseholdPinSenderKnowledgeRequired");
      v = table(Slot::sender_knowledge, RowOwner::social_knowledge, &s,
                *sender_table, *sender);
      if (!v.ok)
        return v;
      auto learned = time_binding(*dynamic.time2, Slot::sender_knowledge_time,
                                  report.id, &message.sender);
      if (!learned.ok)
        return {false, {}, std::move(learned.error)};
      v = time(s, Slot::sender_knowledge_time, *learned.value);
      if (!v.ok)
        return v;
    }
    const auto *links = x.controller_knowledge.communications;
    if (known.business || known.delivery || known.dossier) {
      if (!w.interaction.communication_links ||
          links != &*w.interaction.communication_links)
        return mismatch("HouseholdPinCommunicationRootMismatch");
    }
    if (known.business) {
      v = table(Slot::message_business, RowOwner::communication_business, links,
                links->links, *known.business);
      if (!v.ok)
        return v;
    }
    if (known.delivery) {
      v = table(Slot::message_delivery, RowOwner::communication_delivery, links,
                links->deliveries, *known.delivery);
      if (!v.ok)
        return v;
    }
    if (known.dossier) {
      v = table(Slot::dossier_access, RowOwner::communication_dossier, links,
                links->dossier_accesses, *known.dossier);
      if (!v.ok)
        return v;
    }
    return common_success();
  }
  Status population(const HouseholdCohortEnvironmentContext &p,
                    const CivilianPopulationRef &ref,
                    const PopulationRevisionView &checked) override {
    const auto &w = out_.world();
    const auto &x = out_.report();
    if (&p != &x.population || &p.economy != &w.local_economy ||
        !w.population_transport || &p.population != &*w.population_transport ||
        &p.economy_catalog != w.catalog->local_economy.get())
      return mismatch("HouseholdPinPopulationRootMismatch");
    if (checked.reference.cohort_id != ref.cohort_id ||
        checked.reference.population_revision != ref.population_revision ||
        checked.reference.transport_binding_id != ref.transport_binding_id)
      return mismatch("HouseholdPinPopulationProjectionMismatch");
    // 原 count projection 已验证这些实际行；这里只保拥有表/真实头，不猜事实
    // schema。
    for (const auto &source : checked.original_count_sources) {
      Status kept = common_success();
      if (source.domain == "local_economy" &&
          source.kind == "population_transfer") {
        const auto *r = p.economy.population_transfers.find(source.id);
        if (!r)
          return mismatch("HouseholdPinPopulationTransferMissing");
        kept = table(Slot::population_initial, RowOwner::population_transfers,
                     &p.economy, p.economy.population_transfers, *r);
      } else if (source.domain == "local_economy" &&
                 source.kind == "population_loss") {
        const auto *r = p.economy.population_losses.find(source.id);
        if (!r)
          return mismatch("HouseholdPinPopulationLossMissing");
        kept = table(Slot::population_initial, RowOwner::population_losses,
                     &p.economy, p.economy.population_losses, *r);
      } else
        return mismatch("HouseholdPinPopulationCountOwnerRequired");
      if (!kept.ok)
        return kept;
      const auto *head = w.common.state().facts.find(source.id);
      if (!head || head->domain != source.domain || head->kind != source.kind)
        return mismatch("HouseholdPinPopulationCountHeadRequired");
      auto pos = read_actual_common_fact_position(w.common, *head);
      if (!pos.ok)
        return {false, {}, std::move(pos.error)};
      kept = out_.common(d_, Slot::population_initial, pos.value, *head);
      if (!kept.ok)
        return kept;
    }
    // 原群体 Condition 尚没有在本选型登记真实 typed 保留 owner，不能猜
    // schema=1。
    if (checked.condition_source_ref)
      return common_failure("HouseholdPinPopulationConditionOwnerRequired", {},
                            ErrorKind::unsupported_capability);
    const auto *current = p.economy.cohorts.find(ref.cohort_id);
    const auto *initial = p.economy_catalog.cohorts.find(ref.cohort_id);
    if (!current)
      return mismatch("HouseholdPinCohortMissing");
    auto v = table(Slot::population_initial, RowOwner::current_cohorts, &p.economy,
                   p.economy.cohorts, *current);
    if (!v.ok)
      return v;
    if (initial) {
      v = table(Slot::population_initial, RowOwner::authored_cohorts,
                &p.economy_catalog, p.economy_catalog.cohorts, *initial);
      if (!v.ok)
        return v;
    }
    const auto *head = p.population.source_revisions.find(ref.cohort_id);
    if (!head)
      return mismatch("HouseholdPinPopulationHeadMissing");
    v = table(Slot::population_initial, RowOwner::population_heads, &p.population,
              p.population.source_revisions, *head);
    if (!v.ok)
      return v;
    if (ref.transport_binding_id) {
      const auto *binding =
          p.population.transport_bindings.find(*ref.transport_binding_id);
      if (!binding)
        return mismatch("HouseholdPinPopulationTransportMissing");
      v = table(Slot::population_initial, RowOwner::population_transport_bindings,
                &p.population, p.population.transport_bindings, *binding);
      if (!v.ok)
        return v;
    }
    return common_success();
  }
  Status population_revision(const PopulationTransportState &p,
                             const PopulationRevisionSource &r,
                             const MomentOwnerSource &source,
                             bool before) override {
    const auto &w = out_.world();
    if (!w.population_transport || &p != &*w.population_transport)
      return mismatch("HouseholdPinPopulationRevisionRootMismatch");
    for (std::size_t n = 0; n < p.revision_sources.size(); ++n)
      if (&p.revision_sources[n] == &r) {
        auto v =
            out_.row(d_, Slot::population_revision, RowOwner::population_revisions,
                     &p, &r, nullptr, n, before);
        if (!v.ok)
          return v;
        return out_.source(d_, Slot::population_revision, source.source_ref,
                           source.source_schema, before);
      }
    return mismatch("HouseholdPinPopulationRevisionNotActual");
  }
  Status source(HouseholdPurchaseReportProvenanceRole role, const SourceRef &r,
                Integer schema) override {
    switch (role) {
    case HouseholdPurchaseReportProvenanceRole::actual_disclosure:
      return out_.source(d_, Slot::actual_delivery, r, schema);
    case HouseholdPurchaseReportProvenanceRole::original_observation:
      return out_.source(d_, Slot::original_observation, r, schema);
    case HouseholdPurchaseReportProvenanceRole::original_permission:
      return out_.source(d_, Slot::original_permission, r, schema);
    case HouseholdPurchaseReportProvenanceRole::actor_location:
      return out_.source(d_, Slot::actor_location, r, schema);
    case HouseholdPurchaseReportProvenanceRole::population_location:
      return out_.source(d_, Slot::population_location, r, schema);
    }
    return mismatch("HouseholdPinUnknownBusinessRole");
  }
};
} // namespace
bool NativeHouseholdReportSourcePins::contains_source(
    SourceRefBuildView ref) const noexcept {
  for (const auto &pin : pins_) {
    bool found = std::visit(
        [&](const auto &value) -> bool {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, NativeHouseholdReportNamedSourcePin>)
            return same(SourceRefBuildView{value.source.domain,
                                           value.source.kind, value.source.id},
                        ref);
          else if constexpr (std::is_same_v<T,
                                            NativeHouseholdReportCommonFactPin>)
            return value.row && value.row->domain == ref.domain &&
                   value.row->kind == ref.kind && value.row->id == ref.id;
          else if constexpr (std::is_same_v<
                                 T, CharacterAvailabilitySourceRetentionRef>)
            return same(value.source, ref);
          else if constexpr (std::is_same_v<T,
                                            CharacterAvailabilityRetentionRef>)
            return std::visit(
                [&](const auto &owned) -> bool {
                  using O = std::decay_t<decltype(owned)>;
                  if constexpr (std::is_same_v<
                                    O,
                                    CharacterOriginalBirthOwnedRetentionRef> ||
                                std::is_same_v<
                                    O, CharacterBornCreationOwnedRetentionRef>)
                    return owned.header && owned.header->domain == ref.domain &&
                           owned.header->kind == ref.kind &&
                           owned.header->id == ref.id;
                  else
                    return false;
                },
                value.identity);
          else
            return false;
        },
        pin.dependency);
    if (found)
      return true;
  }
  return false;
}
Outcome<NativeHouseholdReportSourcePins>
collect_native_household_report_source_pins(
    const WorldBundle &world, const NativeCompleteWorldCoreSources &core,
    const NativeHouseholdFoundationLease &household,
    const NativeHouseholdPurchaseReportWorldRead &report) {
  if (!world.catalog || !world.actor_foundation ||
      !world.catalog->actor_foundation ||
      &core.characters.early.world != &world ||
      !report.bound_to(world, household))
    return {
        false, {}, mismatch("HouseholdPinSameWorldOwningGraphRequired").error};
  auto selected =
      validate_native_face_to_face_layout(*world.catalog, world.interaction);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!world.interaction.face_to_face_contacts ||
      world.interaction.face_to_face_contacts->version != 3)
    return {false, {}, mismatch("HouseholdPinFace3Required").error};
  auto bindings = report.validate_bindings();
  if (!bindings.ok)
    return {false, {}, std::move(bindings.error)};
  auto context = report.context();
  if (!context.ok)
    return {false, {}, std::move(context.error)};
  if (!context.value)
    return {false, {}, mismatch("HouseholdPinContextRequired").error};
  auto body = core.characters.family.body().sources();
  if (!body.ok)
    return {false, {}, std::move(body.error)};
  if (&body.value.common != &world.common ||
      &body.value.state != &*world.actor_foundation ||
      &body.value.catalog != world.catalog->actor_foundation.get())
    return {false, {}, mismatch("HouseholdPinConditionOwnerMismatch").error};
  const auto *age = core.characters.family.life_age();
  if (age) {
    const auto a = age->bindings();
    if (!world.life_age_checks || !world.catalog->life_age_checks ||
        &a.state != &*world.life_age_checks ||
        &a.catalog != world.catalog->life_age_checks.get() ||
        &a.sources.common != &world.common ||
        &a.sources.foundation != &*world.actor_foundation ||
        a.sources.foundation_catalog.identity.content_digest !=
            world.catalog->identity.content_digest)
      return {false, {}, mismatch("HouseholdPinAgeOwnerMismatch").error};
  }
  CharacterAvailabilityRetentionContext owned{body.value, &world.family_birth};
  CharacterAvailabilitySourceRetentionContext established{
      body.value, age,
      world.life_age_checks ? &*world.life_age_checks : nullptr,
      world.catalog->life_age_checks.get()};
  NativeHouseholdReportPinCollector sink(world, *context.value, owned,
                                         established);
  for (const auto &entry : world.interaction.face_to_face_contacts
                               ->purchase_plan_disclosures.rows()) {
    const auto &d = entry.second;


    auto memory = CheckpointMemoryCharge(0);
    const SourceRef source{"interaction", household_purchase_report_source_kind,
                           entry.first};
    const auto *head = world.common.state().facts.find(entry.first);
    if (!head)
      return {
          false, {}, mismatch("HouseholdPinActualDeliveryHeadRequired").error};
    auto position = read_actual_common_fact_position(world.common, *head);
    if (!position.ok)
      return {false, {}, std::move(position.error)};
    auto valid = sink.common(d, Slot::actual_delivery, position.value, *head);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    BusinessPins business(sink, d);
    valid = visit_household_purchase_report_provenance(*context.value, source,
                                                       1, business);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    ContactPins contact(sink, d);
    valid = visit_actual_common_fact_prefix(world.common,
                                            d.actual_contact_prefix, contact);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    ControllerPins controller(sink, d);
    valid =
        report.visit_controller_history_pins(source, controller, controller);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    if (!sink.status().ok)
      return {false, {}, sink.status().error};
  }
  return {true, sink.finish(), {}};
}
// 只在原回收入口、第一次删除之前构造真实图；不得从任一图工厂回调本函数。
Outcome<std::optional<NativeHouseholdReportGrowthPins>>
read_native_household_report_growth_pins(
    const WorldBundle &world, const std::vector<StableId> &existing) {
  if (!world.catalog)
    return {false, {}, mismatch("HouseholdPinWorldCatalogRequired").error};
  auto valid =
      validate_native_face_to_face_layout(*world.catalog, world.interaction);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  if (!world.interaction.face_to_face_contacts)
    return {true, std::nullopt, {}};
  if (!world.read_stamps || !world.world_timeline)
    return {false, {}, mismatch("HouseholdPinOriginalClockRequired").error};
  auto at = world.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  std::unique_ptr<PhaseCurrentReadScope> phase;
  if (!world.has_active_read_phase()) {


    auto memory = CheckpointMemoryCharge(0);
    const StableId name{"household_report_growth_retention"};
    const StableId provider{"interaction"};
    auto entered = world.enter_phase_read_context(
        name, provider, {at.value, at.value}, at.value);
    if (!entered.ok)
      return {false, {}, std::move(entered.error)};
    phase = std::move(entered.value);
  }
  auto core = world.complete_exact_time_bindings();
  if (!core.ok)
    return {false, {}, std::move(core.error)};
  if (!core.value || !core.value->reads().original_core ||
      !core.value->reads().household_foundation)
    return {
        false, {}, mismatch("HouseholdPinOriginalOwningGraphRequired").error};
  auto dependencies = make_native_household_report_dependencies(world);
  if (!dependencies.ok)
    return {false, {}, std::move(dependencies.error)};
  auto owner = NativeHouseholdPurchaseReportWorldRead::create(
      world, *core.value->reads().original_core,
      *core.value->reads().household_foundation, std::move(dependencies.value));
  if (!owner.ok)
    return {false, {}, std::move(owner.error)};
  if (!owner.value)
    return {
        false, {}, mismatch("HouseholdPinOriginalOwningGraphRequired").error};
  valid = owner.value->validate_sources();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  auto actual = collect_native_household_report_source_pins(
      world, *core.value->reads().original_core,
      *core.value->reads().household_foundation, *owner.value);
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  // 只投影 Growth 真活动，绝不把 contains=false 当 owner release 的许可。
  CheckpointVector<const StableId *> additional;
  for (const auto &row : world.growth.life_activities.rows()) {
    const SourceRefBuildView source{"growth", "life_activity", row.first};
    if (actual.value.contains_source(source) &&
        std::find(existing.begin(), existing.end(), row.first) ==
            existing.end())
      additional.push_back(&row.first);
  }
  if (additional.size() >
      std::numeric_limits<std::size_t>::max() - existing.size())
    throw std::bad_alloc();
  const auto count = existing.size() + additional.size();


  NativeHouseholdReportGrowthPins result;
  (result.memory).grow(0);
  result.ids.reserve(count);
  for (const auto &id : existing)
    result.ids.push_back(checkpoint_fresh_string_copy(id));
  for (const auto *id : additional)
    result.ids.push_back(checkpoint_fresh_string_copy(*id));
  // 结果无 World 借指针。return 后 actual/report/core/phase 全部析构，调用方才
  // erase。
  return {true,
          std::optional<NativeHouseholdReportGrowthPins>{std::move(result)},
          {}};
}
} // namespace hanmo::resident
