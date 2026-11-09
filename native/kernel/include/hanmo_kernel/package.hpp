// 玩法包（设计第 9 节）：自己的表、数据、事件处理与可选的自有动词。
// 包只用内核的动作和事件，不直接调用别的包；要协作靠事件，或调用别的包登记的公开查询。
// 拿掉任何一个包，内核照样编得过、世界照样能跑。
#pragma once

#include "fact.hpp"

namespace hanmo::kernel {

// 包的清单。kernel_abi 与内核的 kAbiVersion 不符就不装。
struct Manifest {
  const char *name = nullptr;
  uint32_t version = 0;
  uint32_t kernel_abi = kAbiVersion;
};

// 包登记的事实种类：保存级别、有多容易被看见（0 只有当事人知道）、有多要紧。
struct FactKindDecl {
  const char *name = nullptr;
  FactClass cls = FactClass::Normal;
  uint8_t visibility = 0;
  uint16_t salience = 0;
};

// 包登记的所知种类（价格、战况、任免……）与它有多要紧。
struct ClaimKindDecl {
  const char *name = nullptr;
  uint16_t salience = 0;
};

// 订阅条件：哪种事实、哪个地方、跟谁有关；为 0 的项不限。订阅收到的是真值，只用来算规则。
struct Subscription {
  FactKindId kind;
  PlaceId place;
  ActorId actor;
};

// 开局前包向内核登记的东西，内核据此分配编号。
class Declarations {
 public:
  virtual ~Declarations() = default;

  virtual FactKindId fact_kind(const FactKindDecl &) = 0;
  virtual ClaimKindId claim_kind(const ClaimKindDecl &) = 0;
  virtual Verb verb(const char *name) = 0;
  virtual RelationKindId relation_kind(const char *name) = 0;
  virtual void subscribe(const Subscription &) = 0;
  // 公开查询：别的包可按名字调用（像 D-Bus），依赖要单向。
  virtual void publish_query(const char *name) = 0;
};

// 包在处理函数里能做的事。改世界只能经 submit；自有动词要先用 check 查完它要用的全部基本动作，
// 再在 apply_verb 里逐个 submit，保证必定成功。
class Context {
 public:
  virtual ~Context() = default;

  virtual const WorldView &world() const = 0;
  virtual const FactView &facts() const = 0;
  virtual Reject check(const Action &) const = 0;
  virtual Outcome submit(const Action &) = 0;
  // 包事实（例：一场会战的结果、一次任免），带起因。
  virtual FactId emit(const Fact &, Span<Cause> causes) = 0;
  virtual ProcessId start_process(const Process &) = 0;
  virtual void end_process(ProcessId, bool interrupted) = 0;
  // 告示：贴在地方上，来到这里、识字的人看得懂。
  virtual void post_notice(PlaceId, const Claim &) = 0;
  // 信：造一件东西，内容在写信这一刻封存，由人带着走，会延误、丢失、被截。
  virtual Ref write_letter(ActorId writer, Span<Claim> content) = 0;
  virtual Rng rng() = 0;
  // 调别的包登记的公开查询；没有这个查询返回假。
  virtual bool query(const char *name, const void *in, void *out) const = 0;
};

// 状态观察：此人在此地四下看，包报出看得见的东西（可带误差、可信度）。
class ObservationSink {
 public:
  virtual ~ObservationSink() = default;
  virtual void seen(const Claim &, uint16_t credence) = 0;
};

// 存档段：每个包的表各成一段、各带版本，内核原样保存，不懂内容。
class SegmentWriter {
 public:
  virtual ~SegmentWriter() = default;
  virtual void write(const void *data, size_t bytes) = 0;
};

class SegmentReader {
 public:
  virtual ~SegmentReader() = default;
  // 读不够返回假。
  virtual bool read(void *data, size_t bytes) = 0;
  virtual size_t remaining() const = 0;
};

// 半日一次的一致性检查：报出问题即视为程序错误，内核保留上一个好档并停下。
class ValidationSink {
 public:
  virtual ~ValidationSink() = default;
  virtual void problem(const char *what, uint32_t detail) = 0;
};

class Package {
 public:
  virtual ~Package() = default;

  virtual Manifest manifest() const = 0;
  virtual void declare(Declarations &) = 0;
  // 开局：建自己的表、读自己的内容。
  virtual void start(Context &) {}
  // 订阅的事实发生时调用（真值）。
  virtual void on_fact(Context &, const Fact &) {}
  virtual void on_process_end(Context &, const Process &, bool /*interrupted*/) {}
  // 半日推进到指定段时调用。
  virtual void on_phase(Context &, Phase) {}
  // 自有动词：check 查完全部前提；apply 必定成功。
  virtual Reject check_verb(const Context &, const Action &) const {
    return Reject{RejectCode::PackageRule, PackageId{}, 0};
  }
  virtual void apply_verb(Context &, const Action &) {}
  // 本包的事实被人看见时，变成哪条所知；不想让人知道就返回假。
  virtual bool claim_of(const Fact &, Claim &) const { return false; }
  // 此人在此地四下看时，本包有哪些状态看得见。
  virtual void observe(const Context &, ActorId /*who*/, PlaceId /*where*/, ObservationSink &) const {}
  // 本包登记的公开查询。
  virtual bool query(const char * /*name*/, const void * /*in*/, void * /*out*/) const { return false; }
  virtual void validate(const Context &, ValidationSink &) const {}
  virtual void save(SegmentWriter &) const {}
  // saved_version 是存档时本包的版本；读不了就返回假，内核把这段留在旁边、不挡读档。
  virtual bool load(SegmentReader &, uint32_t /*saved_version*/) { return true; }
};

}  // namespace hanmo::kernel
