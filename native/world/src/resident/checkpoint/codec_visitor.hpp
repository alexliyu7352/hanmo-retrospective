// 各域共用的有界typed检查点访客；只保字段字节与原序，不知道游戏字段或规则。
#pragma once
#include "checkpoint_capacity.hpp"
#include "checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "checkpoint_stream.hpp"
#include "hanmo/resident/model_contract.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

namespace hanmo::resident {
// reader仅借用输入到finish；writer输出只属于当前检查点，不留世界或域引用。
struct CodecVisitor {
  using Buffer = std::vector<std::uint8_t>;
  // 旧无 profile 入口及少数热调用仍借此常量；不借关账改变它们的业务准入。
  // 原生 /2 的粗档/数量上限来自下方实际 CapacityProfile。
  static constexpr std::size_t max_bytes = 64u * 1024u * 1024u,
                               max_items = 1000000u, max_depth = 64u;
  explicit CodecVisitor(Buffer &output) : output_(&output) { output.clear(); }
  explicit CodecVisitor(const Buffer &input) : input_(&input) {
    if (input.size() > max_bytes)
      reject("CheckpointTooLarge");
  }
  // /2写口直接消费借用字节；全job只由外层一个固定chunk聚合，不复制字段字符串。
  CodecVisitor(CheckpointWriteSink &sink, const CapacityProfile &profile,
               std::string_view domain, std::uint64_t byte_limit,
               CheckpointJobBudget *job = nullptr)
      : sink_(&sink), profile_(&profile), job_(job), byte_limit_(byte_limit) {
    configure_v2();
    if (result_.ok)
      own_domain(domain);
  }
  // /2 小档 reader 显式共享同一候选 job；拒绝已移交或错 job 的构造。
  CodecVisitor(const Buffer &input, const CapacityProfile &profile,
               std::string_view domain, CodecDecodeBudget &budget)
      : input_(&input), profile_(&profile), budget_(&budget) {
    byte_limit_ = section_limit(profile, domain);
    configure_v2();
    if (result_.ok)
      own_domain(domain);
    if (result_.ok && input.size() > byte_limit_)
      capacity("section_bytes", input.size());
  }
  // 文件/私有stage的有限区间reader；不把一段或整个检查点复制成Bytes。
  CodecVisitor(CheckpointReadSource &source, std::uint64_t begin,
               std::uint64_t length, const CapacityProfile &profile,
               std::string_view domain, CodecDecodeBudget &budget)
      : source_(&source), source_begin_(begin), source_length_(length),
        profile_(&profile), budget_(&budget) {
    byte_limit_ = section_limit(profile, domain);
    configure_v2();
    if (result_.ok)
      own_domain(domain);
    if (begin > source.size() || length > source.size() - begin)
      reject("TruncatedCheckpoint");
    if (result_.ok && length > byte_limit_)
      capacity("section_bytes", length);
  }
  // 可测实际输出，不以sizeof结构体冒充序列化长度。
  std::uint64_t encoded_bytes() const { return encoded_bytes_; }
  // 外层已核定schema选择的特性位；不跳字段、不探测兼容格式，也不单独序列化。
  std::uint32_t schema_features = 0;
  // P06有限Common选择器逐段赋值，不借位或从payload空值推断能力。
  bool item_instance_refs_enabled = false, lot_property_refs_enabled = false,
       domain_activity_reservation_refs_enabled = false;
  // Location/3仅由原Common有限K轴选择授予，旧tag读取不猜新carrier。
  bool carrier_location_enabled = false;
  // 已选择的Naval独立段版本，禁止单凭feature256授权战斗回执；本字段不写入字节。
  Integer naval_codec_version = 0;
  // Original Repair agreement receipts exist only in the explicitly selected
  // item_instances/3. This borrowed selection is not serialized or inferred.
  Integer item_instances_codec_version = 0;
  // Explicit original continuation and tactical-command owner selections.
  // Neither is inferred from a receipt variant or the legacy military codec.
  Integer control_continuation_codec_version = 0;
  Integer dynamic_military_codec_version = 0;
  // 既有 Field 回执只由真实 Field /1 身份授权；此运行上下文不写入档。
  Integer field_battle_codec_version = 0;
  // Original Work command receipts are selected only by the actual /6 module.
  Integer task_work_execution_codec_version = 0;
  // 已验原 module 的有限命令回执选择，不写入旧字段或猜测空根。
  Integer household_purchase_codec_version = 0;
  Integer face_to_face_codec_version = 0;
  // 正式继位独立module版本，仅外层已验身份赋值，不写入旧访客字节。
  Integer political_head_handover_codec_version = 0;
  Integer political_successor_round_codec_version = 0;
  Integer political_term_handover_codec_version = 0;
  Integer political_office_reception_codec_version = 0;
  Integer political_office_resignation_codec_version = 0;
  Integer political_subordinate_appointment_codec_version = 0;
  Integer political_governance_agency_codec_version = 0;
  // 正式继位独立module版本，仅外层已验身份赋值，不写入旧访客字节。
  Integer political_succession_codec_version = 0;
  // 仅军事段已核版本选择设置；本上下文字段本身不进入旧检查点字节。
  Integer military_codec_version = 0;
  bool reading() const { return input_ != nullptr || source_ != nullptr; }
  Status status() const { return result_; }
  // 原typed root/variant
  // reader也可在构造私有候选前调用；不动writer/旧无budget入口。
  template <class T> Status prepare_default_values(std::uint64_t count = 1) {
    if (reading() && budget_ && result_.ok) {
      auto charged = charge_checkpoint_defaults<T>(*budget_, count);
      if (!charged.ok)
        accept(std::move(charged));
    }
    return result_;
  }

  // 最外层必须消费所有输入；尾随数据和未闭合对象不能被当成成功。
  Status finish() {
    if (depth_ != 0 || (reading() && offset_ != input_size()))
      reject("InvalidCheckpointBoundary");
    return result_;
  }
  // 首错固定；领域validator可用此函数报告其自己的错误，后续访问不改变首错。
  Status reject(std::string_view code, std::string_view path = "checkpoint") {
    reject_parts(code, {path}, ErrorKind::rule);
    return result_;
  }
  // 整数逐位保留，禁止以double中转int64；binary64仅允许有限值。
  Status field(std::string_view name, Integer &value) {
    if (!header(1, name))
      return result_;
    std::uint64_t bits = 0;
    if (!reading())
      std::memcpy(&bits, &value, 8);
    number(bits);
    if (reading() && result_.ok)
      std::memcpy(&value, &bits, 8);
    return result_;
  }
  // 显式无符号typed槽仍用原整数标签与8个原位字节；不经signed或浮点中转。
  // 只有真实领域schema的uint64字段调用，JSON表示由该schema另行明确。
  Status unsigned_field(std::string_view name, std::uint64_t &value) {
    if (!header(1, name))
      return result_;
    number(value);
    return result_;
  }
  Status field(std::string_view name, double &value) {
    if (!header(2, name))
      return result_;
    std::uint64_t bits = 0;
    if (!reading()) {
      if (!std::isfinite(value))
        return reject("NonFiniteCheckpointValue", name);
      std::memcpy(&bits, &value, 8);
    }
    number(bits);
    if (reading() && result_.ok) {
      std::memcpy(&value, &bits, 8);
      if (!std::isfinite(value))
        reject("NonFiniteCheckpointValue", name);
    }
    return result_;
  }
  Status field(std::string_view name, bool &value) {
    if (!header(3, name))
      return result_;
    std::uint8_t byte = value ? 1 : 0;
    octet(byte);
    if (reading() && result_.ok) {
      if (byte > 1)
        return reject("InvalidCheckpointBoolean", name);
      value = byte != 0;
    }
    return result_;
  }
  Status field(std::string_view name, std::string &value) {
    if (header(4, name))
      text(value, profile_ && (name == "numerator" || name == "denominator")
                      ? profile_->max_bigint_encoded_bytes
                      : 0);
    return result_;
  }
  // 领域对象的字段访问顺序由领域owner提供，名字/标签不符即拒绝而非跳过未知字段。
  template <class Fn> Status object(std::string_view name, Fn visit) {
    if (!header(5, name) || !enter())
      return result_;
    auto status = visit(*this);
    if (result_.ok && !status.ok)
      accept(std::move(status));
    leave(6);
    return result_;
  }
  template <class T, class Allocator, class Fn>
  Status sequence(std::string_view name, std::vector<T, Allocator> &values,
                  Fn visit) {
    if (!header(7, name) || !enter())
      return result_;
    std::uint64_t count = values.size();
    number(count);
    if (result_.ok &&
        (count > values.max_size() ||
         count > std::numeric_limits<std::size_t>::max() / sizeof(T))) {
      if (profile_)
        capacity_parts({"collection_allocation/", domain_, "/", name}, count);
      else
        reject("CheckpointCollectionTooLarge", name);
    } else if (!profile_ && count > max_items)
      reject("CheckpointCollectionTooLarge", name);
    // 至少每项一个标签字节，避免损坏的长度在尚无数据时制造巨大分配。
    if (reading() && result_.ok &&
        !checkpoint_count_fits_remaining(count, input_size(), offset_))
      reject("TruncatedCheckpoint", name);
    if (reading() && result_.ok && budget_) {
      Status charged;
      if constexpr (std::is_same_v<Allocator, CheckpointAllocator<T>>)
        charged = budget_->account_allocator_candidate(
            count * sizeof(T), current_checkpoint_memory());
      else
        charged = budget_->charge_candidate(count * sizeof(T));
      if (!charged.ok)
        accept(std::move(charged));
    }
    if (reading() && result_.ok)
      prepare_default_values<T>(count);
    if (reading() && result_.ok) {
      if (profile_) {
        using Vector = std::vector<T, Allocator>;
        // 原普通vector累计预留不退款；实际allocator则随旧block销毁归还其原账。
        // 读端新容器绑定当前已验job，不沿旧候选allocator串入另一个job。
        {
          Vector old(values.get_allocator());
          old.swap(values);
        }
        if constexpr (std::is_same_v<Allocator, CheckpointAllocator<T>>) {
          Vector decoded(static_cast<std::size_t>(count),
                         Allocator{current_checkpoint_memory()});
          values.swap(decoded);
        } else {
          // 当前锁定libstdc++ count构造精确申请count项，保原普通vector语义。
          Vector decoded(static_cast<std::size_t>(count),
                         values.get_allocator());
          values.swap(decoded);
        }
      } else
        values.resize(static_cast<std::size_t>(count));
    }
    if (result_.ok)
      for (auto &value : values) {
        auto status = visit(*this, value);
        if (!status.ok) {
          if (result_.ok)
            accept(std::move(status));
          break;
        }
      }
    leave(8);
    return result_;
  }
  // 物理子表读入同一个私有候选向量；不造第二份记录集合或复制已解码行。
  // 外层候选负责失败原子性；仅增加尾项，领域owner按调用前size核新增区间的owner/顺序。
  template <class T, class Allocator, class Fn>
  Status append_sequence(std::string_view name,
                         std::vector<T, Allocator> &values, Fn visit) {
    static_assert(
        std::is_nothrow_move_constructible_v<T>,
        "checkpoint append requires non-copying noexcept row transfer");
    return append_sequence(name, values, std::move(visit),
                           [](auto &destination, T &original) -> Status {
                             destination.emplace_back(std::move(original));
                             return {true, {}, {}};
                           });
  }
  // 原物理数组复用同一typed转移；不让vector以move_if_noexcept深复制旧记录。
  template <class T, class Allocator, class Fn, class Transfer>
  Status append_sequence(std::string_view name,
                         std::vector<T, Allocator> &values, Fn visit,
                         Transfer &&transfer) {
    if (!reading())
      return reject("CheckpointReadSequenceOnWriter", name);
    if (!header(7, name) || !enter())
      return result_;
    std::uint64_t count = 0;
    number(count);
    const auto begin = values.size();
    if (!profile_ && (begin > max_items || count > max_items - begin))
      reject("CheckpointCollectionTooLarge", name);
    if (result_.ok &&
        !checkpoint_count_fits_remaining(count, input_size(), offset_))
      reject("TruncatedCheckpoint", name);
    const auto limit = std::min<std::size_t>(
        values.max_size(), std::numeric_limits<std::size_t>::max() / sizeof(T));
    if (result_.ok && (begin > limit || count > limit - begin)) {
      if (profile_)
        capacity("collection_allocation",
                 count > UINT64_MAX - begin ? UINT64_MAX : count + begin);
      else
        reject("CheckpointCollectionTooLarge", name);
    }
    if (result_.ok) {
      if constexpr (std::is_same_v<Allocator, CheckpointAllocator<T>>) {
        if (budget_) {
          auto checked = budget_->account_allocator_candidate(
              0, values.get_allocator().resource());
          if (!checked.ok)
            accept(std::move(checked));
        }
      }
    }
    if (result_.ok && count)
      prepare_default_values<T>(count);
    if (result_.ok && count) {
      const auto total = begin + static_cast<std::size_t>(count);
      if (total > values.capacity()) {
        // 新空壳先预留，旧块仍由原owner持有；每个旧行只能经显式回调搬一次。
        if (budget_) {
          Status charged;
          if constexpr (std::is_same_v<Allocator, CheckpointAllocator<T>>)
            charged = budget_->account_allocator_candidate(
                total * sizeof(T), current_checkpoint_memory());
          else
            charged = budget_->charge_candidate(total * sizeof(T));
          if (!charged.ok)
            accept(std::move(charged));
        }
        if (result_.ok) {
          using Vector = std::vector<T, Allocator>;
          auto allocator = [&]() {
            if constexpr (std::is_same_v<Allocator, CheckpointAllocator<T>>)
              return Allocator{current_checkpoint_memory()};
            else
              return values.get_allocator();
          }();
          Vector grown(allocator);
          grown.reserve(total);
          for (auto &old : values) {
            const auto size = grown.size(), capacity = grown.capacity();
            const auto *allocation = grown.data();
            auto moved = transfer(grown, old);
            if (!moved.ok) {
              accept(std::move(moved));
              break;
            }
            if (grown.size() != size + 1 || grown.capacity() != capacity ||
                grown.data() != allocation) {
              reject_parts("CheckpointDecodedTransferShapeMismatch", {name},
                           ErrorKind::internal);
              break;
            }
          }
          if (result_.ok) {
            grown.resize(total);
            values.swap(grown);
          }
        }
      } else
        values.resize(total);
      if (result_.ok)
        for (std::size_t i = begin; i < total; ++i) {
          auto status = visit(*this, values[i]);
          if (!status.ok) {
            if (result_.ok)
              accept(std::move(status));
            break;
          }
        }
    }
    leave(8);
    return result_;
  }
  // 已由原字段解码的单行移入同一候选；不读取或写入任何标签、长度和字段。
  // 普通容器的新增 block 仍由原 decoder reservation 保活，typed allocator
  // 不重复收费。
  template <class T, class Allocator>
  Status append_decoded_value(std::vector<T, Allocator> &values, T &&value,
                              std::string_view record_kind) {
    static_assert(
        std::is_nothrow_move_constructible_v<T>,
        "checkpoint append requires non-copying noexcept row transfer");
    return append_decoded_value(values, std::move(value), record_kind,
                                [](auto &destination, T &original) -> Status {
                                  destination.emplace_back(std::move(original));
                                  return {true, {}, {}};
                                });
  }
  // 原owner显式提供可抛typed转移：只搬一行到已预留壳；分数仍由其真实allocator复制。
  // 转移可能已消费前像，任何失败都必须撤销整个私有decoder候选，不能在原候选重试。
  template <class T, class Allocator, class Transfer>
  Status append_decoded_value(std::vector<T, Allocator> &values, T &&value,
                              std::string_view record_kind,
                              Transfer &&transfer) {
    if (!reading())
      return reject("CheckpointReadAppendOnWriter", record_kind);
    if (!result_.ok)
      return result_;
    for (auto &old : values)
      if (std::addressof(old) == std::addressof(value))
        return reject("CheckpointAppendAliasedValue", record_kind);
    if constexpr (std::is_same_v<Allocator, CheckpointAllocator<T>>) {
      if (budget_) {
        auto checked = budget_->account_allocator_candidate(
            0, values.get_allocator().resource());
        if (!checked.ok) {
          accept(std::move(checked));
          return result_;
        }
      }
    }
    const auto limit = std::min<std::size_t>(
        values.max_size(), std::numeric_limits<std::size_t>::max() / sizeof(T));
    if (values.size() >= limit) {
      if (profile_)
        capacity_parts({"collection_allocation/", record_kind}, UINT64_MAX);
      else
        reject("CheckpointCollectionTooLarge", record_kind);
      return result_;
    }
    const auto total = values.size() + 1;
    if (!profile_ && total > max_items)
      return reject("CheckpointCollectionTooLarge", record_kind);
    auto one = [&](auto &destination, T &original) -> bool {
      const auto count = destination.size(), capacity = destination.capacity();
      const auto *allocation = destination.data();
      auto moved = transfer(destination, original);
      if (!moved.ok) {
        accept(std::move(moved));
        return false;
      }
      if (destination.size() != count + 1 ||
          destination.capacity() != capacity ||
          destination.data() != allocation) {
        reject_parts("CheckpointDecodedTransferShapeMismatch", {record_kind},
                     ErrorKind::internal);
        return false;
      }
      return true;
    };
    if (total > values.capacity()) {
      if (budget_) {
        Status charged;
        if constexpr (std::is_same_v<Allocator, CheckpointAllocator<T>>)
          charged = budget_->account_allocator_candidate(
              total * sizeof(T), current_checkpoint_memory());
        else
          charged = budget_->charge_candidate(total * sizeof(T));
        if (!charged.ok) {
          accept(std::move(charged));
          return result_;
        }
      }
      using Vector = std::vector<T, Allocator>;
      auto allocator = [&]() {
        if constexpr (std::is_same_v<Allocator, CheckpointAllocator<T>>)
          return Allocator{current_checkpoint_memory()};
        else
          return values.get_allocator();
      }();
      Vector grown(allocator);
      grown.reserve(total);
      for (auto &old : values)
        if (!one(grown, old))
          return result_;
      if (!one(grown, value))
        return result_;
      values.swap(grown);
    } else if (!one(values, value))
      return result_;
    return result_;
  }
  // 明确不支持任何元素的已选空槽；仍严格消费原数组标签/count/结尾。
  // 非零在分配或读取未知行之前拒绝，不能把未知持久行伪装成标量。
  Status empty_sequence(std::string_view name) {
    if (!header(7, name) || !enter()) return result_;
    std::uint64_t count = 0;
    number(count);
    if (result_.ok && count != 0)
      reject("UnsupportedCheckpointNonemptySequence", name);
    leave(8);
    return result_;
  }
  // 按索引借用已有记录的只写视图；count由同一冻结集合实际计算，回调不构造第二份记录表。
  template <class Fn>
  Status sequence_view(std::string_view name, std::uint64_t count,
                       Fn emit_index) {
    if (reading())
      return reject("CheckpointWriteSequenceOnReader", name);
    if (!header(7, name) || !enter())
      return result_;
    number(count);
    if (!profile_ && count > max_items)
      reject("CheckpointCollectionTooLarge", name);
    for (std::uint64_t index = 0; result_.ok && index < count; ++index) {
      auto status = emit_index(*this, index);
      if (!status.ok)
        accept(std::move(status));
    }
    leave(8);
    return result_;
  }
  // 只写range复用sequence的同一tag/count/元素/结尾；过滤视图借原行，不复制记录或ID表。
  // Range::size与迭代须来自同一冻结snapshot；元素回调不得修改源字段。
  template <class Range, class Fn>
  Status write_sequence(std::string_view name, const Range &values, Fn visit) {
    if (reading())
      return reject("CheckpointWriteSequenceOnReader", name);
    if (!header(7, name) || !enter())
      return result_;
    const auto expected = static_cast<std::uint64_t>(values.size());
    auto count = expected;
    number(count);
    if (!profile_ && count > max_items)
      reject("CheckpointCollectionTooLarge", name);
    std::uint64_t visited = 0;
    if (result_.ok)
      for (const auto &value : values) {
        if (visited == expected) {
          reject("CheckpointSourceChanged", name);
          break;
        }
        auto status = visit(*this, value);
        ++visited;
        if (!status.ok) {
          if (result_.ok)
            accept(std::move(status));
          break;
        }
      }
    if (result_.ok && visited != expected)
      reject("CheckpointSourceChanged", name);
    leave(8);
    return result_;
  }
  template <class T, class Fn>
  Status optional(std::string_view name, std::optional<T> &value, Fn visit) {
    return object(name, [&](CodecVisitor &codec) {
      bool present = value.has_value();
      codec.field("present", present);
      if (codec.reading() && codec.status().ok) {
        if (present) {
          codec.prepare_default_values<T>();
          if (codec.status().ok)
            value.emplace();
        } else
          value.reset();
      }
      if (codec.status().ok && present) {
        auto status = visit(codec, *value);
        if (result_.ok && !status.ok)
          accept(std::move(status));
      }
      return codec.status();
    });
  }

private:
  Buffer *output_ = nullptr;
  const Buffer *input_ = nullptr;
  std::size_t offset_ = 0, depth_ = 0;
  CheckpointWriteSink *sink_ = nullptr;
  CheckpointReadSource *source_ = nullptr;
  std::uint64_t source_begin_ = 0, source_length_ = 0;
  const CapacityProfile *profile_ = nullptr;
  CodecDecodeBudget *budget_ = nullptr;
  CheckpointJobBudget *job_ = nullptr;
  CheckpointJobReservation
      domain_memory_; // domain is destroyed before its charge.
  StableId domain_;
  std::uint64_t byte_limit_ = 0, encoded_bytes_ = 0;
  Status result_{true, {}, {}};
  std::uint64_t input_size() const noexcept {
    return source_ ? source_length_ : input_ ? input_->size() : 0;
  }
  // 精确读取单一已验区间，所有真实I/O都按受信chunk切片；不持额外scratch副本。
  void read_raw(std::uint8_t *out, std::size_t size) {
    if (!result_.ok)
      return;
    if (offset_ > input_size() || size > input_size() - offset_) {
      reject("TruncatedCheckpoint");
      return;
    }
    if (input_) {
      std::memcpy(out, input_->data() + offset_, size);
      offset_ += size;
      return;
    }
    while (size && result_.ok) {
      const auto n = std::size_t(
          std::min<std::uint64_t>(size, profile_->stream_chunk_bytes));
      if (!n) {
        capacity("stream_chunk_bytes", 1);
        return;
      }
      auto read = source_->read(source_begin_ + offset_, out, n);
      if (!read.ok) {
        accept(std::move(read));
        return;
      }
      offset_ += n;
      out += n;
      size -= n;
    }
  }
  static std::uint64_t section_limit(const CapacityProfile &profile,
                                     std::string_view domain) noexcept {
    if (domain == "$header")
      return profile.max_header_bytes;
    for (const auto &entry : profile.max_section_bytes_by_registered_name)
      if (std::string_view(entry.first) == domain)
        return entry.second;
    return 0;
  }
  void own_domain(std::string_view domain) {
    if (job_ || (budget_ && budget_->job_bound())) {
      if (job_) {
        auto held = job_->reserve(CheckpointJobUse::encode, 0);
        if (!held.ok)
          throw checkpoint_job_detail::FailureException{std::move(held.error)};
        domain_memory_ = std::move(held.value);
      } else
        domain_memory_ = budget_->reserve_scratch(0);
    }
    std::string value(domain);
    domain_.swap(value);
  }
  // Internal diagnostics are constructed once in the original job window.
  // Foreign callback Status values are moved into that same first-error owner;
  // their producer's original allocation is still its own precharge obligation.
  void accept(Status &&status) {
    if (!result_.ok || status.ok)
      return;
    if (job_)
      throw checkpoint_job_detail::FailureException{
          job_->capture_failure(std::move(status.error))};
    if (budget_)
      result_ = budget_->accept_failure(std::move(status));
    else
      result_ = std::move(status);
  }
  void reject_parts(std::string_view code,
                    std::initializer_list<std::string_view> path,
                    ErrorKind kind) {
    if (!result_.ok)
      return;
    if (job_)
      throw checkpoint_job_detail::FailureException{
          job_->capture_boundary_failure(code, path, kind)};
    if (budget_)
      result_ = budget_->reject_boundary(code, path, kind);
    else
      result_ = {false, {}, make_checkpoint_boundary_error(code, path, kind)};
  }
  // 新配置检查不触旧构造/旧首错顺序；所有新资源错误带受信配置身份。
  void configure_v2() {
    if (!valid_capacity_profile_shape(*profile_))
      reject_parts("InvalidCapacityProfile", {"checkpoint/capacity"},
                   ErrorKind::internal);
    if (result_.ok &&
        (!byte_limit_ || byte_limit_ > profile_->max_checkpoint_bytes))
      capacity("visitor_bytes", byte_limit_);
    if (result_.ok && budget_ && &budget_->profile != profile_)
      reject("CheckpointBudgetProfileMismatch");
    if (result_.ok && job_) {
      auto checked = job_->check_profile(*profile_);
      if (!checked.ok)
        throw checkpoint_job_detail::FailureException{std::move(checked.error)};
    }
  }
  void capacity_parts(std::initializer_list<std::string_view> resource,
                      std::uint64_t required) {
    if (!result_.ok)
      return;
    if (job_)
      throw checkpoint_job_detail::FailureException{
          job_->capture_capacity_failure(
              resource, required, profile_->profile_id, profile_->revision)};
    if (budget_)
      result_ = budget_->reject_capacity(resource, required);
    else
      result_ = {false,
                 {},
                 make_checkpoint_capacity_error(resource, required,
                                                profile_->profile_id,
                                                profile_->revision)};
  }
  void capacity(std::string_view resource, std::uint64_t required) {
    capacity_parts({resource}, required);
  }
  // sink全写合同避免短写被当成功；计数始终先检查差值再相加。
  void emit(const std::uint8_t *data, std::size_t size) {
    if (!result_.ok)
      return;
    if (size > byte_limit_ - encoded_bytes_) {
      capacity("visitor_bytes", size > UINT64_MAX - encoded_bytes_
                                    ? UINT64_MAX
                                    : encoded_bytes_ + size);
      return;
    }
    auto wrote = sink_->write(data, size);
    if (!wrote.ok) {
      accept(std::move(wrote));
      return;
    }
    encoded_bytes_ += size;
  }
  // 固定小端独立于宿主；所有写入合计上限保护检查点外层。
  void octet(std::uint8_t &value) {
    if (!result_.ok)
      return;
    if (reading()) {
      read_raw(&value, 1);
    } else if (sink_)
      emit(&value, 1);
    else {
      if (output_->size() >= max_bytes) {
        reject("CheckpointTooLarge");
        return;
      }
      output_->push_back(value);
    }
  }
  void number(std::uint64_t &value) {
    std::uint64_t read = 0;
    for (unsigned i = 0; i < 8 && result_.ok; ++i) {
      std::uint8_t byte = static_cast<std::uint8_t>(value >> (8 * i));
      octet(byte);
      read |= std::uint64_t(byte) << (8 * i);
    }
    if (reading() && result_.ok)
      value = read;
  }
  void text(std::string &value, std::uint64_t specific_limit = 0) {

    std::uint64_t count = value.size();
    number(count);
    if (!result_.ok)
      return;
    if (profile_) {
      if (count > byte_limit_) {
        capacity("string_bytes", count);
        return;
      }
      if (specific_limit && count > specific_limit) {
        capacity("bigint_encoded_bytes", count);
        return;
      }
    } else if (count > max_bytes) {
      reject("CheckpointStringTooLarge");
      return;
    }
    if (reading()) {
      if (!checkpoint_count_fits_remaining(count, input_size(), offset_)) {
        reject("TruncatedCheckpoint");
        return;
      }
      if (budget_) {
        if (count > SIZE_MAX) {
          capacity("string_bytes", count);
          return;
        }
        auto charge = budget_->charge_candidate(0);
        if (!charge.ok) {
          accept(std::move(charge));
          return;
        }
      }
      if (profile_) {
        // 先核长度、剩余字节与当前 job，再用普通字符串拥有实际解码值。
        {
          std::string old;
          old.swap(value);
        }
        std::string decoded(static_cast<std::size_t>(count), '\0');
        read_raw(reinterpret_cast<std::uint8_t *>(decoded.data()),
                 static_cast<std::size_t>(count));
        if (result_.ok)
          value.swap(decoded);
      } else {
        value.assign(reinterpret_cast<const char *>(input_->data() + offset_),
                     static_cast<std::size_t>(count));
        offset_ += static_cast<std::size_t>(count);
      }
    } else if (sink_)
      emit(reinterpret_cast<const std::uint8_t *>(value.data()), value.size());
    else {
      if (count > max_bytes - output_->size()) {
        reject("CheckpointTooLarge");
        return;
      }
      output_->insert(output_->end(), value.begin(), value.end());
    }
  }
  bool header(std::uint8_t expected, std::string_view name) {
    std::uint8_t tag = expected;
    octet(tag);
    if (result_.ok && tag != expected)
      reject("CheckpointTypeMismatch", name);
    if (profile_) {
      std::uint64_t count = name.size();
      number(count);
      if (!result_.ok)
        return false;
      if (reading()) {
        if (!checkpoint_count_fits_remaining(count, input_size(), offset_)) {
          reject("TruncatedCheckpoint");
          return false;
        }
        if (count != name.size()) {
          reject("CheckpointFieldMismatch", name);
          return false;
        }
        for (std::size_t at = 0; at < name.size() && result_.ok; ++at) {
          std::uint8_t byte = 0;
          read_raw(&byte, 1);
          if (result_.ok && byte != static_cast<std::uint8_t>(name[at]))
            reject("CheckpointFieldMismatch", name);
        }
      } else
        emit(reinterpret_cast<const std::uint8_t *>(name.data()), name.size());
      return result_.ok;
    }
    std::string actual(name);
    text(actual);
    if (result_.ok && actual != name)
      reject("CheckpointFieldMismatch", name);
    return result_.ok;
  }
  bool enter() {
    if (profile_ && depth_ >= profile_->max_nesting_depth) {
      capacity("nesting_depth", std::uint64_t(depth_) + 1);
      return false;
    }
    if (!profile_ && depth_ >= max_depth) {
      reject("CheckpointTooDeep");
      return false;
    }
    ++depth_;
    return true;
  }
  void leave(std::uint8_t expected) {
    --depth_;
    std::uint8_t tag = expected;
    octet(tag);
    if (result_.ok && tag != expected)
      reject("CheckpointTypeMismatch");
  }
};
} // namespace hanmo::resident
