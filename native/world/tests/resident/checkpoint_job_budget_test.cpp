// 独立首错拥有门：不构造world/BoundaryValue，故障注入只限此小叶的operator new。
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/domains/commerce/household_retail.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include "resident/domains/growth/growth_market_history.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/kernel/checkpoint_phase_timing.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string_view>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
using namespace hanmo::resident;
namespace {
std::atomic<std::size_t> live_bytes{0}, calls{0}, large_limit{SIZE_MAX},
    large_calls{0}, bounded_allocator_calls{0};
std::atomic<bool> deny_new{false};
struct alignas(std::max_align_t) Header {
  std::size_t size;
};
} // namespace
void *operator new(std::size_t n) {
  ++calls;
  if (n >= large_limit)
    ++large_calls;
  if (deny_new)
    throw std::bad_alloc();
  auto p = static_cast<Header *>(std::malloc(sizeof(Header) + (n ? n : 1)));
  if (!p)
    throw std::bad_alloc();
  p->size = n;
  live_bytes += n;
  return p + 1;
}
void operator delete(void *p) noexcept {
  if (p) {
    auto h = static_cast<Header *>(p) - 1;
    live_bytes -= h->size;
    std::free(h);
  }
}
void operator delete(void *p, std::size_t) noexcept { ::operator delete(p); }
namespace {
int checks = 0, failures = 0;
using Code = CheckpointJobFailureCode;
void check(bool v, const char *label) {
  ++checks;
  if (!v) {
    ++failures;
    std::cerr << "FAIL " << label << '\n';
  }
}
template <class T> T must(CheckpointJobResult<T> r) {
  if (!r.ok) {
    r.error.try_materialize_rule_error();
    throw std::runtime_error(r.error.rule_error()
                                 ? r.error.rule_error()->original.code
                                 : "fixed failure");
  }
  return std::move(r.value);
}
} // namespace
// 仅本测试的瞬态类型：验证未知内存形状不再阻挡，不登记为持久模型。
struct UnregisteredAccountingProbe {
  std::string payload;
};
// 原shared工厂必须保真实对象与job资源；最后一个owner销毁时只析构一次。
struct SharedOwnershipProbe {
  explicit SharedOwnershipProbe(bool &destroyed) : destroyed_(&destroyed), payload(80, 's') {}
  ~SharedOwnershipProbe() { *destroyed_ = true; }
  bool *destroyed_;
  std::string payload;
};
// 有限allocator只允许两项；用小输入证明max_size拒绝先于清空旧候选和allocate。
template <class T> struct TwoItemAllocator {
  using value_type = T;
  TwoItemAllocator() noexcept = default;
  template <class U> TwoItemAllocator(const TwoItemAllocator<U> &) noexcept {}
  std::size_t max_size() const noexcept { return 2; }
  T *allocate(std::size_t count) {
    ++bounded_allocator_calls;
    return std::allocator<T>{}.allocate(count);
  }
  void deallocate(T *value, std::size_t count) noexcept {
    std::allocator<T>{}.deallocate(value, count);
  }
  template <class U> bool operator==(const TwoItemAllocator<U> &) const noexcept { return true; }
  template <class U> bool operator!=(const TwoItemAllocator<U> &) const noexcept { return false; }
};
int main() {
  try {
    // /3 明确空槽仍由typed owner具名且成对编码；任何非空数在分配前拒绝。
    {
      struct Sink final : CheckpointWriteSink {
        CodecVisitor::Buffer bytes;
        Status write(const std::uint8_t *data, std::size_t size) override {
          bytes.insert(bytes.end(), data, data + size);
          return common_success();
        }
      } sink;
      const auto &profile = engineering_checkpoint_capacity_profile();
      const std::string_view name = "continuation_sources";
      CodecVisitor writer(sink, profile, "political_passage",
                          profile.max_checkpoint_bytes);
      const auto written = writer.empty_sequence(name);
      CodecVisitor::Buffer reference;
      CodecVisitor original(reference);
      std::vector<Integer> empty;
      original.sequence(name, empty, [](CodecVisitor &v, Integer &n) {
        return v.field("n", n);
      });
      check(written.ok && writer.finish().ok && original.finish().ok &&
                sink.bytes == reference,
            "explicit empty source slot preserves original sequence wire");
      CodecDecodeBudget budget(profile);
      CodecVisitor read(static_cast<const CodecVisitor::Buffer &>(sink.bytes),
                        profile, "political_passage", budget);
      check(read.empty_sequence(name).ok && read.finish().ok,
            "typed explicit empty source slot round trips");
      auto nonempty = sink.bytes;
      const auto count_offset = 1u + 8u + name.size();
      for (std::size_t n = 0; n < 8; ++n)
        nonempty[count_offset + n] = 255;
      CodecDecodeBudget bad_budget(profile);
      CodecVisitor bad(static_cast<const CodecVisitor::Buffer &>(nonempty),
                       profile, "political_passage", bad_budget);
      large_limit = 1024 * 1024;
      const auto before = large_calls.load();
      const auto refused = bad.empty_sequence(name);
      large_limit = SIZE_MAX;
      check(!refused.ok &&
                refused.error.original.code ==
                    "UnsupportedCheckpointNonemptySequence" &&
                large_calls == before,
            "nonempty unsupported source slot rejects count before large "
            "allocation");
      auto wrong_tag = sink.bytes;
      wrong_tag[0] = 5;
      CodecDecodeBudget tag_budget(profile);
      CodecVisitor tag_reader(static_cast<const CodecVisitor::Buffer &>(wrong_tag),
                              profile, "political_passage", tag_budget);
      check(tag_reader.empty_sequence(name).error.original.code == "CheckpointTypeMismatch",
            "empty source slot rejects a wrong actual wire tag without a collection registry");
      auto wrong_name = sink.bytes;
      wrong_name[1 + 8] ^= 1;
      CodecDecodeBudget name_budget(profile);
      CodecVisitor name_reader(static_cast<const CodecVisitor::Buffer &>(wrong_name),
                               profile, "political_passage", name_budget);
      check(name_reader.empty_sequence(name).error.original.code == "CheckpointFieldMismatch",
            "empty source slot compares every actual wire name byte to its typed owner");
      auto truncated = sink.bytes;
      truncated.resize(count_offset + 3);
      CodecDecodeBudget short_budget(profile);
      CodecVisitor short_read(
          static_cast<const CodecVisitor::Buffer &>(truncated), profile,
          "political_passage", short_budget);
      check(!short_read.empty_sequence(name).ok,
            "truncated empty source count is rejected");
      auto bad_end = sink.bytes;
      bad_end.back() = 6;
      CodecDecodeBudget end_budget(profile);
      CodecVisitor end_read(static_cast<const CodecVisitor::Buffer &>(bad_end),
                            profile, "political_passage", end_budget);
      check(!end_read.empty_sequence(name).ok,
            "empty source sequence requires original closing tag");
      auto extra = sink.bytes;
      extra.push_back(0);
      CodecDecodeBudget extra_budget(profile);
      CodecVisitor extra_read(static_cast<const CodecVisitor::Buffer &>(extra),
                              profile, "political_passage", extra_budget);
      check(extra_read.empty_sequence(name).ok && !extra_read.finish().ok,
            "empty source slot does not swallow trailing bytes");
    }
    // 删除数量表仍不授损坏输入分配许可；实际剩余字节不足时保持原候选不动。
    {
      auto profile = engineering_checkpoint_capacity_profile();
      profile.max_section_bytes_by_registered_name.emplace("probe", profile.max_checkpoint_bytes);
      CodecVisitor::Buffer input;
      CodecVisitor writer(input);
      std::vector<Integer> empty;
      writer.sequence("rows", empty, [](CodecVisitor &v, Integer &n) { return v.field("n", n); });
      const auto count_offset = 1u + 8u + std::string_view("rows").size();
      input[count_offset] = 100;
      CodecDecodeBudget budget(profile);
      CodecVisitor reader(static_cast<const CodecVisitor::Buffer &>(input), profile, "probe", budget);
      std::vector<Integer> candidate{17, 23};
      large_limit = 100 * sizeof(Integer);
      const auto before = large_calls.load();
      const auto refused = reader.sequence("rows", candidate,
                                            [](CodecVisitor &v, Integer &n) { return v.field("n", n); });
      large_limit = SIZE_MAX;
      check(!refused.ok && refused.error.original.code == "TruncatedCheckpoint" &&
                candidate == std::vector<Integer>({17, 23}) && large_calls == before &&
                budget.candidate_bytes == 0,
            "count beyond remaining bytes refuses before candidate resizing without a registry");
    }
    {
      auto profile = engineering_checkpoint_capacity_profile();
      profile.max_section_bytes_by_registered_name.emplace("probe", profile.max_checkpoint_bytes);
      CodecVisitor::Buffer input;
      CodecVisitor writer(input);
      std::vector<Integer> source{1, 2, 3};
      writer.sequence("rows", source, [](CodecVisitor &v, Integer &n) { return v.field("n", n); });
      CodecDecodeBudget budget(profile);
      CodecVisitor reader(static_cast<const CodecVisitor::Buffer &>(input), profile, "probe", budget);
      std::vector<Integer, TwoItemAllocator<Integer>> candidate{9};
      const auto before = bounded_allocator_calls.load();
      const auto refused = reader.sequence("rows", candidate,
                                            [](CodecVisitor &v, Integer &n) { return v.field("n", n); });
      check(!refused.ok && refused.error.original.code == "CheckpointCapacityExceeded" &&
                refused.error.original.path == "checkpoint/capacity/collection_allocation/probe/rows" &&
                candidate.size() == 1 && candidate[0] == 9 && bounded_allocator_calls == before,
            "allocator max_size refuses before destroying or reallocating the old typed candidate");
    }
    static_assert(!std::is_copy_constructible<CheckpointJobFailure>::value);
    static_assert(
        !std::is_copy_constructible<CheckpointJobResult<Done>>::value);
    static_assert(
        std::is_nothrow_move_constructible<CheckpointJobFailure>::value);
    static_assert(!std::is_copy_constructible<CodecDecodeBudget>::value);
    // 诊断窗仅借本线程栈帧：无堆分配，嵌套/显式结束/异常均还原，不能污染后续调用。
    {
      using namespace checkpoint_phase_timing;
      Frame outer, inner;
      constexpr auto index = static_cast<std::size_t>(Stage::save_validate);
      const auto allocations = calls.load();
      deny_new = true;
      {
        Capture capture(outer);
        {
          Scope phase(Stage::save_validate);
          phase.stop();
          phase.stop();
        }
        {
          Capture nested(inner);
          Scope phase(Stage::save_validate);
        }
        {
          Scope phase(Stage::save_validate);
        }
      }
      {
        Scope inactive(Stage::save_validate);
      }
      deny_new = false;
      check(calls == allocations && outer.samples[index].calls == 2 &&
                inner.samples[index].calls == 1 && !outer.overflow &&
                !inner.overflow,
            "fixed timing frames allocate nothing and restore nested capture "
            "once");
      bool caught = false;
      try {
        Capture capture(outer);
        Scope phase(Stage::save_validate);
        throw 42;
      } catch (int) {
        caught = true;
      }
      {
        Scope inactive(Stage::save_validate);
      }
      check(caught && outer.samples[index].calls == 3 &&
                inner.samples[index].calls == 1,
            "timing exception unwind records its stage and restores inactive "
            "TLS");
    }
    auto p = engineering_checkpoint_capacity_profile();
    auto slot = engineering_slot_capacity_profile();
    const auto make = [&] {
      return must(
          CheckpointJobBudget::create(p, slot));
    };
    // 记帐层已删除；兼容字段不授内存额度，但实际token/allocator拥有与首错门仍须严格。
    {
      auto cp = p;
      auto sp = slot;
      auto j = must(CheckpointJobBudget::create(cp, sp));
      check(!j.memory_accounting_enabled() && j.usage().total_live == 0 &&
                j.usage().total_peak == 0,
            "production default disables control and all job memory ledgers");
      auto held = must(j.reserve(CheckpointJobUse::candidate_scratch, 256));
      check(
          held.valid() && held.belongs_to(j) && held.bytes() == 256 &&
              held.grow(64).ok && held.bytes() == 320 &&
              j.usage().total_live == 0 && j.usage().candidate_live == 0 &&
              j.usage().scratch_live == 0,
          "disabled ledger keeps real token extent and original job identity");
      check(!held.release_destroyed(321) && held.release_destroyed(64) &&
                held.bytes() == 256 && j.usage().total_live == 0,
            "disabled accounting retains paired logical-extent underflow "
            "rejection");
      auto resource =
          must(checkpoint_memory_resource(j, CheckpointJobUse::candidate));
      ScopedCheckpointMemory memory(resource);
      CheckpointVector<Integer> values(32, Integer(7));
      check(values.size() == 32 && values[31] == 7 &&
                values.get_allocator().resource() == resource &&
                values.get_allocator().allocation_owner()->resource() == resource &&
                j.usage().total_live == 0,
            "production allocator still owns real independent storage without "
            "census totals");
      CheckpointMemoryCharge lease(0);
      check(lease.resource() == resource,
            "zero-amount lifecycle lease retains the actual original resource");
      CodecDecodeBudget decode(cp, j);
      ScopedCheckpointJsonDecode json(decode, j, "estate", resource);
      check(decode.charge_candidate(4096).ok && decode.candidate_bytes == 0 &&
                decode.belongs_to(j),
            "JSON candidate accounting is off without bypassing the same-job "
            "binding");
      auto candidate = decode.take_job_reservation();
      bool moved_rejected = false;
      try {
        decode.charge_candidate(0);
      } catch (checkpoint_job_detail::FailureException &e) {
        moved_rejected = e.error.code() == Code::reservation_mismatch;
      }
      check(candidate.valid() && candidate.bytes() == 0 && moved_rejected,
            "disabled decoder cannot restart after zero-amount token handoff");
      bool json_closed = false;
      try {
        json.default_values<UnregisteredAccountingProbe>();
      } catch (const checkpoint_job_detail::FailureException &e) {
        json_closed = e.error.code() == Code::reservation_mismatch;
      }
      check(json_closed && candidate.valid(),
            "JSON default-values lifecycle stays sealed after its candidate token is handed off");
      CheckpointRetentionCensus census(resource, CheckpointJobUse::encode);
      std::string text(80, 'r');
      census.owner(values.get_allocator().allocation_owner());
      check(census.accounts(resource) &&
                census.visit_shared_object(&text) &&
                !census.visit_shared_object(&text),
            "retention keeps owner identity and cycle deduplication without "
            "summing bytes");
      auto bytes =
          must(CheckpointJobBytes::allocate(j, CheckpointJobUse::slot, 16));
      const std::uint8_t input[]{4, 9};
      check(bytes.belongs_to(j) && bytes.write(14, input, 2).ok &&
                bytes.bytes()[15] == 9 && j.usage().total_live == 0,
            "unmetered JobBytes still allocates and enforces its real write "
            "range");
      auto standalone = CodecDecodeBudget(cp);
      check(
          standalone.charge_candidate(4096).ok &&
              standalone.candidate_bytes == 0,
          "standalone production decoder has no hidden candidate memory limit");
    }
    {
      auto j = must(CheckpointJobBudget::create(p, slot));
      auto token = must(j.reserve(CheckpointJobUse::slot, 8));
      CheckpointJobBytes::Buffer too_large(16);
      auto bad = CheckpointJobBytes::adopt_reserved(std::move(too_large),
                                                    std::move(token));
      check(!bad.ok && bad.error.code() == Code::reservation_mismatch &&
                token.valid() && too_large.size() == 16,
            "disabled ledger never accepts a buffer beyond the real token "
            "extent");
    }
    {
      auto j = must(CheckpointJobBudget::create(p, slot));
      auto token = must(j.reserve(CheckpointJobUse::slot, 8));
      j.cancel();
      auto bad = token.grow(0);
      check(!bad.ok && bad.error.code() == Code::cancelled &&
                token.bytes() == 8,
            "production zero growth still observes cancellation and retains "
            "cleanup extent");
    }
    {
      auto j = must(CheckpointJobBudget::create(p, slot));
      auto bad = j.reserve(static_cast<CheckpointJobUse>(8), 0);
      auto next = j.reserve(CheckpointJobUse::slot, 0);
      check(!bad.ok && bad.error.code() == Code::invalid_use && !next.ok &&
                next.error.code() == Code::already_failed,
            "production zero accounting preserves invalid-use and first-error "
            "refusal");
    }
    // 粗文件门和真实领域拥有仍在；默认形状表退役不得改写任何字段或复制行为。
    {
      check(p.max_checkpoint_bytes == 256000000 &&
                slot.max_slot_bytes == 256000000,
            "coarse checkpoint and whole-slot limit use decimal bytes");
      check(
          checkpoint_count_fits_remaining(1, 9, 8) &&
              !checkpoint_count_fits_remaining(2, 9, 8) &&
              !checkpoint_count_fits_remaining(0, 8, 9),
          "coarse count guard includes equality and rejects cursor underflow");

      CodecDecodeBudget lifecycle(p);
      check(charge_checkpoint_defaults<UnregisteredAccountingProbe>(lifecycle).ok &&
                lifecycle.candidate_bytes == 0,
            "unknown accounting type needs no default-shape registration");
      EstateRecord estate;
      estate.id = std::string(80, 'e');
      estate.resident_actor_ids.push_back(std::string(80, 'a'));
      auto copied = estate;
      copied.resident_actor_ids.front().front() = 'b';
      check(copied.id == estate.id &&
                estate.resident_actor_ids.front().front() == 'a',
            "estate still makes real independent ordinary member copies");
    }
    {
      auto j = make();
      auto resource =
          must(checkpoint_memory_resource(j, CheckpointJobUse::candidate));
      ScopedCheckpointMemory memory(resource);
      CodecDecodeBudget budget(p, j);
      ScopedCheckpointJsonDecode decoding(budget, j, "estate", resource);
      auto reserved = budget.charge_candidate(256000001);
      check(reserved.ok && budget.candidate_bytes == 0,
            "no candidate accounting path can replace the independent file-size cap");
      decoding.default_values<UnregisteredAccountingProbe>();
      check(budget.belongs_to(j) &&
                resource->shares_job(*current_checkpoint_memory()),
            "JSON decode keeps the real job/resource while accounting is "
            "relaxed");
    }
    // JSON/Boundary不能拿另一个job的allocator或已移交token恢复写入权限。
    {
      auto j = make();
      auto foreign = make();
      auto resource = must(checkpoint_memory_resource(j, CheckpointJobUse::candidate));
      auto other_resource = must(checkpoint_memory_resource(foreign, CheckpointJobUse::candidate));
      ScopedCheckpointMemory memory(resource);
      CodecDecodeBudget budget(p, j);
      bool json_rejected = false, boundary_rejected = false;
      try {
        ScopedCheckpointJsonDecode invalid(budget, j, "estate", other_resource);
      } catch (const checkpoint_job_detail::FailureException &e) {
        json_rejected = e.error.code() == Code::reservation_mismatch;
      }
      try {
        ScopedCheckpointBoundaryDecode invalid(budget, j, other_resource);
      } catch (const checkpoint_job_detail::FailureException &e) {
        boundary_rejected = e.error.code() == Code::reservation_mismatch;
      }
      check(json_rejected && boundary_rejected && budget.belongs_to(j) &&
                !ScopedCheckpointJsonDecode::current() && !ScopedCheckpointBoundaryDecode::current() &&
                current_checkpoint_memory() == resource,
            "foreign JSON and Boundary allocators are refused without installing a context");
      ScopedCheckpointBoundaryDecode boundary(budget, j, resource);
      auto owner = budget.take_job_reservation();
      bool sealed = false;
      try {
        boundary.default_values<UnregisteredAccountingProbe>();
      } catch (const checkpoint_job_detail::FailureException &e) {
        sealed = e.error.code() == Code::reservation_mismatch;
      }
      check(sealed && owner.valid() && owner.belongs_to(j),
            "Boundary default-values lifecycle stays sealed after token handoff");
    }
    {
      auto j = make();
      CodecDecodeBudget budget(p, j);
      j.cancel();
      bool refused = false;
      try {
        charge_checkpoint_defaults<UnregisteredAccountingProbe>(budget);
      } catch (const checkpoint_job_detail::FailureException &e) {
        refused = e.error.code() == Code::cancelled;
      }
      check(refused && budget.candidate_bytes == 0,
            "removed default-shape lookup cannot bypass cancellation");
    }
    // JSON已解析数组仍有粗数量门；技术门不分配或制造一份同样巨大的测试数组。
    {
      auto j = make();
      auto resource = must(checkpoint_memory_resource(j, CheckpointJobUse::candidate));
      ScopedCheckpointMemory memory(resource);
      CodecDecodeBudget budget(p, j);
      ScopedCheckpointJsonDecode json(budget, j, "estate", resource);
      json.collection("rows", checkpoint_coarse_file_limit_bytes);
      bool refused = false;
      try {
        json.collection("rows", checkpoint_coarse_file_limit_bytes + 1);
      } catch (const checkpoint_job_detail::FailureException &e) {
        refused = e.error.rule_error() &&
                  e.error.rule_error()->original.code == "CheckpointCapacityExceeded" &&
                  e.error.rule_error()->original.path == "checkpoint/capacity/collection_items" &&
                  e.error.rule_error()->reason == std::optional<StableId>("256000001");
      }
      check(refused && budget.candidate_bytes == 0,
            "JSON count exactly at the coarse limit is allowed and one above keeps the real first error");
    }
    // 两个原库缺上下文入口必须可正常展开；不包裹整个测试的 job。
    {
      struct ContextUnwindProbe {
        bool &destroyed;
        bool &restored;
        CheckpointMemoryResource *memory;
        ScopedCheckpointJsonDecode *json;
        ScopedCheckpointBoundaryDecode *boundary;
        ~ContextUnwindProbe() noexcept {
          destroyed = true;
          restored = current_checkpoint_memory().get() == memory &&
                     ScopedCheckpointJsonDecode::current() == json &&
                     ScopedCheckpointBoundaryDecode::current() == boundary;
        }
      };
      const auto previous_memory = current_checkpoint_memory();
      auto *const previous_json = ScopedCheckpointJsonDecode::current();
      auto *const previous_boundary = ScopedCheckpointBoundaryDecode::current();
      check(!previous_memory && !previous_json && !previous_boundary,
            "missing-context probes start without memory/JSON/Boundary owners");
      {
        auto j = make();
        auto resource =
            must(checkpoint_memory_resource(j, CheckpointJobUse::candidate));
        bool destroyed = false, restored = false, caught = false;
        try {
          // 先造 probe，确保 memory 先析构，probe 再观察真正恢复后的 TLS。
          ContextUnwindProbe probe{destroyed, restored, previous_memory.get(),
                                   previous_json, previous_boundary};
          ScopedCheckpointMemory memory(resource);
          check(current_checkpoint_memory() == resource &&
                    !ScopedCheckpointJsonDecode::current() &&
                    !ScopedCheckpointBoundaryDecode::current(),
                "visit-depth negative probe uses real memory without JSON owner");
          CheckpointJsonVisitDepth depth(true);
        } catch (const checkpoint_job_detail::FailureException &e) {
          caught = e.error.code() == Code::json_context_required &&
                   e.error.required() == 0;
        }
        check(caught,
              "visit-depth original throw preserves json-context code/zero required");
        check(destroyed && restored &&
                  current_checkpoint_memory() == previous_memory &&
                  ScopedCheckpointJsonDecode::current() == previous_json &&
                  ScopedCheckpointBoundaryDecode::current() == previous_boundary,
              "visit-depth throw unwinds local guard and restores all context TLS");
      }
      {
        bool destroyed = false, restored = false, caught = false;
        try {
          ContextUnwindProbe probe{destroyed, restored, previous_memory.get(),
                                   previous_json, previous_boundary};
          checkpoint_decode_boundary_failure("MissingContextProbe", {},
                                             ErrorKind::internal);
        } catch (const checkpoint_job_detail::FailureException &e) {
          caught = e.error.code() == Code::json_context_required &&
                   e.error.required() == 0;
        }
        check(caught,
              "boundary original throw preserves json-context code/zero required");
        check(destroyed && restored &&
                  current_checkpoint_memory() == previous_memory &&
                  ScopedCheckpointJsonDecode::current() == previous_json &&
                  ScopedCheckpointBoundaryDecode::current() == previous_boundary,
              "boundary throw unwinds local guard and preserves all context TLS");
      }
    }
    // 原字段计划API退役；实际资源生命周期和普通领域独立复制仍须保持。
    {
      auto j = make();
      auto resource =
          must(checkpoint_memory_resource(j, CheckpointJobUse::candidate));
      ScopedCheckpointMemory memory(resource);
      CheckpointMemoryCharge reserved(0);
      check(reserved.resource() &&
                reserved.resource()->shares_job(*resource),
            "zero census keeps a real same-job resource");
      CheckpointMemoryCharge upstream(64);
      upstream.grow(0);
      check(upstream.resource() == resource,
            "append retains the existing same-job resource without a byte ledger");

      GrowthShop shop;
      shop.id = std::string(80, 's');
      shop.representative_roles.push_back(std::string(80, 'r'));
      HouseholdRetailSettlement retail;
      retail.id = std::string(80, 'h');
      retail.terms.item_id = std::string(80, 'i');
      MarketMovementRecord movement;
      movement.business_fact_id = std::string(80, 'f');
      check(movement.kind == "growth_market_movement",
            "retiring the default-shape table preserves the real market default kind");
      ProductionReceipt production;
      production.id = std::string(80, 'p');
      production.cause_fact_ids.push_back(std::string(80, 'c'));
      auto copied_shop = shop;
      auto copied_production = production;
      copied_shop.representative_roles.front().front() = 'x';
      copied_production.cause_fact_ids.front().front() = 'y';
      check(copied_shop.id == shop.id &&
                shop.representative_roles.front().front() == 'r' &&
                copied_production.id == production.id &&
                production.cause_fact_ids.front().front() == 'c',
            "unmetered market payloads still make independent real copies");
    }
    {
      auto j = make();
      check(j.usage().total_live == 0 && j.usage().total_peak == 0,
            "job creation has no hidden control-memory ledger");
      auto r = must(j.reserve(CheckpointJobUse::encode, 100));
      check(r.add_constraints(CheckpointJobUse::candidate_scratch).ok &&
                r.bytes() == 100 && r.belongs_to(j) && j.usage().total_live == 0,
            "additional uses preserve one same-job token and its actual extent");
      auto moved = std::move(r);
      check(!r.valid() && moved.grow(20).ok, "move and grow");
      auto other = must(j.reserve(static_cast<CheckpointJobUse>(7), 30));
      check(moved.merge(std::move(other)).ok && !other.valid() &&
                moved.bytes() == 150,
            "same-job equal-use merge transfers each actual token extent once");
      check(moved.release_destroyed(10) && moved.bytes() == 140 &&
                j.usage().total_live == 0 && j.usage().candidate_live == 0,
            "destroyed extent reduces only its owning token, not a removed ledger");
      check(!moved.release_destroyed(141), "token extent release rejects underflow");
      moved = CheckpointJobReservation{};
      check(!moved.valid() && moved.bytes() == 0 && j.usage().total_live == 0,
            "destroying token clears actual ownership without keeping a ledger");
    }
    {
      auto j = make();
      auto a =
          must(CheckpointJobBytes::allocate(j, CheckpointJobUse::slot, 30));
      auto b =
          must(CheckpointJobBytes::copy(j, CheckpointJobUse::slot, a.bytes()));
      check(a.belongs_to(j) && b.belongs_to(j) && a.bytes().data() != b.bytes().data() &&
                a.bytes().size() == 30 && b.bytes().size() == 30 && j.usage().total_live == 0,
            "both outputs retain distinct actual storage under the same job");
      const std::uint8_t x[]{1, 2};
      check(a.write(2, x, 2).ok && a.bytes()[2] == 1 && b.bytes()[2] == 0,
            "bounded write changes only its independently owned output");
      b = std::move(a);
      check(!a.belongs_to(j) && a.bytes().empty() && b.belongs_to(j) &&
                b.bytes()[2] == 1 && j.usage().total_live == 0,
            "Bytes move transfers ownership and destroys the replaced buffer");
    }
    // token的实际容量和归属仍是拒绝边界；取消记帐不能允许跨job/跨use合并。
    {
      auto j = make();
      auto other = make();
      auto left = must(j.reserve(CheckpointJobUse::slot, 8));
      auto right = must(other.reserve(CheckpointJobUse::slot, 12));
      auto denied = left.merge(std::move(right));
      check(!denied.ok && denied.error.code() == Code::reservation_mismatch &&
                left.valid() && left.bytes() == 8 && right.valid() && right.bytes() == 12 &&
                right.belongs_to(other) && right.grow(0).ok,
            "cross-job merge refuses without consuming either owner or poisoning the other job");
    }
    {
      auto j = make();
      auto left = must(j.reserve(CheckpointJobUse::slot, 8));
      auto right = must(j.reserve(CheckpointJobUse::encode, 12));
      auto denied = left.merge(std::move(right));
      check(!denied.ok && denied.error.code() == Code::reservation_mismatch &&
                left.bytes() == 8 && right.valid() && right.bytes() == 12,
            "different token uses cannot merge even within one job");
    }
    {
      auto j = make();
      auto left = must(j.reserve(CheckpointJobUse::slot, UINT64_MAX));
      auto right = must(j.reserve(CheckpointJobUse::slot, 1));
      auto denied = left.merge(std::move(right));
      check(!denied.ok && denied.error.code() == Code::size_overflow &&
                left.bytes() == UINT64_MAX && right.valid() && right.bytes() == 1,
            "token merge overflow refuses without losing either retained extent");
    }
    {
      auto j = make();
      auto token = must(j.reserve(CheckpointJobUse::slot, 8));
      CheckpointJobBytes::Buffer bytes(8);
      bytes.reserve(64);
      auto denied = CheckpointJobBytes::adopt_reserved(std::move(bytes), std::move(token));
      check(!denied.ok && denied.error.code() == Code::reservation_mismatch &&
                token.valid() && token.bytes() == 8 && bytes.size() == 8 && bytes.capacity() >= 64,
            "adoption checks real buffer capacity rather than only logical size");
    }
    for (unsigned fault = 0; fault != 3; ++fault) {
      auto j = make();
      auto bytes = must(CheckpointJobBytes::allocate(j, CheckpointJobUse::slot, 8));
      const std::uint8_t data[]{4, 9};
      check(bytes.write(6, data, 2).ok, "exact-ending Bytes write is allowed");
      const auto before = bytes.bytes();
      auto denied = fault == 0 ? bytes.write(9, data, 1)
                   : fault == 1 ? bytes.write(7, data, 2)
                                : bytes.write(0, nullptr, 1);
      auto later = bytes.write(0, data, 1);
      check(!denied.ok && denied.error.code() == Code::write_range && bytes.bytes() == before &&
                !later.ok && later.error.code() == Code::already_failed,
            "out-of-range or null write keeps bytes unchanged and seals the original first error");
    }
    {
      auto j = make();
      auto changed = p;
      changed.max_section_bytes_by_registered_name.begin()->second--;
      auto bad = j.check_profile(changed);
      check(!bad.ok && bad.error.code() == Code::profile_mismatch,
            "actual section-limit map remains part of the same-job profile fingerprint");
      auto again = j.reserve(CheckpointJobUse::slot, 0);
      check(!again.ok && again.error.code() == Code::already_failed &&
                !again.error.rule_error(),
            "later operation fixed refusal");
      check(bad.error.try_materialize_rule_error() &&
                bad.error.rule_error()->kind == ErrorKind::internal,
            "first fixed description materializes in its held window");
      const auto *pointer = bad.error.rule_error();
      check(bad.error.try_materialize_rule_error() &&
                pointer == bad.error.rule_error(),
            "materialization no second copy");
    }
    {
      auto j = make();
      auto changed = slot;
      changed.max_slot_header_bytes--;
      check(j.check_profile(changed).error.code() == Code::profile_mismatch,
            "actual slot header limit remains frozen in the same-job identity");
    }
    {
      auto j = make();
      CodecDecodeBudget budget(p, j);
      check(budget.charge_candidate(21).ok && budget.candidate_bytes == 0 &&
                budget.belongs_to(j) && j.usage().candidate_live == 0,
            "bound decoder checks original job without accumulating candidate bytes");
      auto owner = budget.take_job_reservation();
      bool threw = false;
      try {
        budget.charge_candidate(0);
      } catch (checkpoint_job_detail::FailureException &e) {
        threw = e.error.code() == Code::reservation_mismatch;
      }
      check(threw && owner.valid() && owner.belongs_to(j) && owner.bytes() == 0,
            "moved budget cannot restart even zero bytes and keeps the handed-off owner");
    }
    {
      auto old = CodecDecodeBudget::metered_for_verification(p);
      check(old.charge_candidate(5).ok && old.charge_candidate(UINT64_MAX).ok &&
                old.candidate_bytes == 0 && !old.memory_accounting_enabled(),
            "historical standalone budget alias cannot reopen a candidate ledger");
    }
    {
      auto j = make();
      auto r = must(j.reserve(CheckpointJobUse::slot, 20));
      j.cancel();
      auto bad = r.grow(0);
      check(!bad.ok && bad.error.code() == Code::cancelled && r.bytes() == 20,
            "cancel preserves retained allocation");
      r = CheckpointJobReservation{};
      check(!r.valid() && r.bytes() == 0 && j.usage().cancelled && j.usage().total_live == 0,
            "cancelled cleanup destroys its token without reviving the job");
    }
    {
      auto j = make();
      auto r = must(j.reserve(CheckpointJobUse::slot, 8));
      CheckpointJobBytes::Buffer bytes(8);
      j.cancel();
      auto bad =
          CheckpointJobBytes::adopt_reserved(std::move(bytes), std::move(r));
      check(!bad.ok && bad.error.code() == Code::cancelled && r.valid() && bytes.size() == 8,
            "cancelled adoption retains original cleanup owner");
    }
    // 删除额度拒绝后仍注入真实allocator失败；不得把OOM假装为空成功输出。
    {
      auto j = make();
      large_limit = 8000;
      large_calls = 0;
      deny_new = true;
      auto rejected = CheckpointJobBytes::allocate(j, CheckpointJobUse::slot, 8000);
      deny_new = false;
      large_limit = SIZE_MAX;
      check(!rejected.ok && large_calls == 1 && rejected.value.bytes().empty() &&
                rejected.error.code() == Code::allocation_failed,
            "actual payload allocation failure returns no partial owned bytes");
      const auto before = calls.load();
      auto next = j.reserve(CheckpointJobUse::slot, 0);
      check(!next.ok && next.error.code() == Code::already_failed && calls == before,
            "real allocator failure seals the original job without another allocation");
    }
    {
      auto j = make();
      auto r = must(j.reserve(CheckpointJobUse::slot, 1));
      auto bad = r.grow(UINT64_MAX);
      check(!bad.ok && r.bytes() == 1, "overflow unchanged");
    }
    {
      auto tiny = slot;
      auto alias = must(CheckpointJobBudget::create_metered_for_verification(p, tiny));
      auto token = must(alias.reserve(CheckpointJobUse::slot, 8000));
      check(!alias.memory_accounting_enabled() && token.bytes() == 8000 &&
                token.belongs_to(alias) && alias.usage().total_live == 0,
            "historical job factory cannot reopen removed control or payload quotas");
      auto fixed = CheckpointJobFailure::fixed(Code::invalid_job);
      const auto before = calls.load();
      check(!fixed.try_materialize_rule_error() && calls == before,
            "no-job fixed description cannot manufacture an unowned diagnostic");
    }
    {
      auto invalid = p;
      invalid.revision = 0;
      const auto before = calls.load();
      auto rejected =
          CheckpointJobBudget::create(invalid, slot);
      check(!rejected.ok && rejected.error.code() == Code::invalid_profile &&
                calls == before,
            "invalid create no dynamic diagnostic");
    }
    {
      auto j = make();
      auto r = must(j.reserve(CheckpointJobUse::slot, 1));
      auto first = r.grow(UINT64_MAX);
      const auto before = calls.load();
      auto later = r.grow(UINT64_MAX);
      check(!first.ok && first.error.code() == Code::size_overflow &&
                !later.ok && later.error.code() == Code::already_failed &&
                calls == before,
            "overflow after first failure is fixed already_failed without "
            "allocation");
    }
    {
      auto j = make();
      auto r = must(j.reserve(CheckpointJobUse::slot, 1));
      j.cancel();
      auto failed = r.grow(UINT64_MAX);
      check(!failed.ok && failed.error.code() == Code::cancelled &&
                r.bytes() == 1,
            "cancel precedes overflow and preserves owner");
    }
    // 首错结果比profile/job façade长寿，归还最后failure
    // lease后state与错误真实释放。
    {
      const auto baseline = live_bytes.load();
      CheckpointJobFailure failure;
      {
        auto cp = p;
        auto sp = slot;
        auto j =
            must(CheckpointJobBudget::create(cp, sp));
        failure = j.capture_capacity_failure({"checkpoint_bytes"}, 256000001, cp.profile_id, cp.revision);
      }
      check(live_bytes > baseline && failure.code() == Code::legacy_error,
            "failure owns state after facade/profile destruction");
      check(failure.try_materialize_rule_error() &&
                failure.rule_error()->source_id == p.profile_id &&
                failure.rule_error()->original.path == "checkpoint/capacity/checkpoint_bytes" &&
                failure.rule_error()->reason == std::optional<StableId>("256000001"),
            "real coarse-file diagnostic remains exact through the failure lease");
      auto moved = std::move(failure);
      check(moved.rule_error() != nullptr && !failure.rule_error(),
            "failure move transfers sole handle");
      moved = CheckpointJobFailure{};
      check(live_bytes == baseline,
            "last failure destroys diagnostic and state without cycle");
    }
    {
      auto j = make();
      RuleError raw;
      raw.original = {std::string(80, 'E'), "checkpoint/producer"};
      raw.kind = ErrorKind::internal;
      const auto *pointer = raw.original.code.data();
      auto failure = j.capture_failure(std::move(raw));
      check(failure.rule_error() &&
                failure.rule_error()->original.code.data() == pointer,
            "first legacy error moved once not copied");
      const auto before = calls.load();
      auto later = j.reserve(CheckpointJobUse::slot, 1);
      check(later.error.code() == Code::already_failed && calls == before,
            "subsequent failure fixed no allocation");
    }
    // 首错窗口是实际诊断的有限拥有边界，不属于已经取消的候选内存额度。
    for (bool long_profile : {false, true}) {
      auto j = make();
      const std::string oversized(8192, 'D');
      const std::string_view resource = long_profile ? std::string_view("checkpoint_bytes")
                                                     : std::string_view(oversized);
      const std::string_view profile_id = long_profile ? std::string_view(oversized)
                                                       : std::string_view(p.profile_id);
      const auto before = calls.load();
      auto failure = j.capture_capacity_failure({resource}, 256000001, profile_id, p.revision);
      check(failure.code() == Code::diagnostic_capacity && failure.required() > oversized.size() &&
                !failure.rule_error() && calls == before,
            "oversized resource or profile refuses diagnostic construction before allocation");
      auto later = j.reserve(CheckpointJobUse::slot, 0);
      check(!later.ok && later.error.code() == Code::already_failed &&
                failure.code() == Code::diagnostic_capacity,
            "diagnostic overflow remains the original first error");
    }
    // 256MB是实际档限，不是候选内存账；超限受信配置在任何job分配前拒绝。
    {
      auto invalid = p;
      invalid.max_checkpoint_bytes = 256000001;
      const auto before = calls.load();
      auto denied = CheckpointJobBudget::create(invalid, slot);
      check(!denied.ok && denied.error.code() == Code::invalid_profile && calls == before,
            "a job profile cannot raise the whole-file 256000000-byte cap");
      auto invalid_slot = slot;
      invalid_slot.max_slot_bytes = 256000001;
      const auto slot_before = calls.load();
      auto slot_denied = CheckpointJobBudget::create(p, invalid_slot);
      check(!slot_denied.ok && slot_denied.error.code() == Code::invalid_profile && calls == slot_before,
            "a slot profile cannot raise the same approved coarse file cap");
    }
    // 真allocator继续做数量溢出/取消检查；这些失败都必须先于实际new。
    {
      auto j = make();
      auto resource = must(checkpoint_memory_resource(j, CheckpointJobUse::candidate));
      CheckpointAllocator<Integer> allocator(resource);
      const auto before = calls.load();
      bool overflow = false;
      try {
        auto *unexpected = allocator.allocate(SIZE_MAX);
        allocator.deallocate(unexpected, SIZE_MAX);
      } catch (const std::bad_alloc &) {
        overflow = true;
      }
      check(overflow && calls == before, "real allocator element multiplication refuses before new");
      j.cancel();
      const auto cancelled_before = calls.load();
      bool cancelled = false;
      try {
        auto *unexpected = allocator.allocate(1);
        allocator.deallocate(unexpected, 1);
      } catch (const checkpoint_job_detail::FailureException &e) {
        cancelled = e.error.code() == Code::cancelled;
      }
      check(cancelled && calls == cancelled_before,
            "real allocator cancellation refuses before payload allocation");
    }
    {
      bool destroyed = false;
      std::shared_ptr<SharedOwnershipProbe> value;
      std::weak_ptr<CheckpointMemoryResource> resource_lifetime;
      {
        auto j = make();
        auto resource = must(checkpoint_memory_resource(j, CheckpointJobUse::candidate));
        resource_lifetime = resource;
        ScopedCheckpointMemory memory(resource);
        value = checkpoint_make_shared<SharedOwnershipProbe>(destroyed);
        const auto owner = checkpoint_shared_allocation_owner(value);
        check(owner && owner->resource() == resource && value->payload == std::string(80, 's'),
              "shared factory records the actual original allocator owner");
      }
      auto alias = value;
      value.reset();
      check(!destroyed && !resource_lifetime.expired() && alias->payload.size() == 80,
            "shared alias keeps object and original job resource alive beyond facade and scope");
      alias.reset();
      check(destroyed && resource_lifetime.expired(),
            "last shared owner destroys the real object and releases its resource without a cycle");
    }
    // 可控有限OOM仅测组件固定返回，不扩签整个typed world/异常运行时。
    {
      deny_new = true;
      auto failed =
          CheckpointJobBudget::create(p, slot);
      deny_new = false;
      check(!failed.ok && failed.error.code() == Code::allocation_failed &&
                !failed.error.rule_error(),
            "create allocation failure returns fixed description");
    }
    {
      auto j = make();
      auto token = must(j.reserve(CheckpointJobUse::slot, 1));
      deny_new = true;
      auto failed = token.grow(UINT64_MAX);
      bool materialized = failed.error.try_materialize_rule_error();
      deny_new = false;
      check(!failed.ok && failed.error.code() == Code::size_overflow &&
                token.bytes() == 1 && !materialized,
            "real extent overflow returns while diagnostic allocation is denied");
      const auto before = calls.load();
      check(
          !failed.error.try_materialize_rule_error() && calls == before,
          "failed materialization is one attempt and retains fixed descriptor");
    }
    {
      CheckpointJobBytes out;
      {
        auto j = make();
        out = must(CheckpointJobBytes::allocate(j, CheckpointJobUse::slot, 12));
      }
      std::atomic<bool> consumed{false};
      std::thread worker([bytes = std::move(out), &consumed]() mutable {
        consumed = bytes.bytes().size() == 12;
      });
      worker.join();
      check(consumed && !out.bytes().size(),
            "cross-thread sole Bytes owner survives facade");
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
  } catch (const std::exception &e) {
    deny_new = false;
    std::cerr << "FAIL exception " << e.what() << '\n';
    return 1;
  } catch (...) {
    deny_new = false;
    std::cerr << "FAIL unexpected exception\n";
    return 1;
  }
}
