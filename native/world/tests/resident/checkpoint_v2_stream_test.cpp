// /2物理合同回归只用真实typed visitor与拥有快照；不替代全世界候选语义验收。
#include "resident/checkpoint/checkpoint_selection.hpp"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <picosha2.h>
using namespace hanmo::resident;
using Bytes = CodecVisitor::Buffer;
namespace {
int checks = 0, failures = 0;
void check(bool value, const char *text) {
  ++checks;
  if (!value) {
    ++failures;
    std::cerr << "FAIL " << text << '\n';
  }
}
// fixture私有拥有字段；显式计数证明每个producer仅访问一次，漂移值用于识别重复编码。
class FixtureLease final : public CheckpointSnapshotLease {
public:
  FixtureLease(CheckpointIdentity identity, std::string text = "x",
               std::size_t repeats = 1, bool drift = false)
      : identity_(std::move(identity)), text_(std::move(text)), rows_(repeats),
        drift_(drift) {}
  const CheckpointIdentity &identity() const override { return identity_; }
  Integer visits() const { return visits_; }
  Status visit_section(const StableId &, CodecVisitor &c) const override {
    ++visits_;
    auto &rows = const_cast<std::vector<int> &>(rows_);
    auto &text = const_cast<std::string &>(text_);
    Integer stamp = drift_ ? visits_ : 7;
    c.field("stamp", stamp);
    return c.sequence("rows", rows, [&](CodecVisitor &v, int &) {
      return v.field("text", text);
    });
  }

private:
  CheckpointIdentity identity_;
  std::string text_;
  std::vector<int> rows_;
  bool drift_;
  mutable Integer visits_ = 0;
};
// 故障producer仍先写真实typed字段；外层不得在其完成前触外部sink。
class InterruptedLease final : public CheckpointSnapshotLease {
public:
  explicit InterruptedLease(CheckpointIdentity identity, CheckpointJobBudget *cancel = nullptr)
      : identity_(std::move(identity)), cancel_(cancel) {}
  const CheckpointIdentity &identity() const override { return identity_; }
  Status visit_section(const StableId &, CodecVisitor &visitor) const override {
    ++visits;
    Integer value = 7;
    visitor.field("stamp", value);
    if (cancel_) {
      cancel_->cancel();
      return visitor.status();
    }
    return visitor.reject("InjectedProducerFailure");
  }
  mutable Integer visits = 0;
private:
  CheckpointIdentity identity_;
  CheckpointJobBudget *cancel_;
};
CapacityProfile profile() {
  auto p = engineering_checkpoint_capacity_profile();
  p.profile_id = "checkpoint-v2-test";
  p.max_checkpoint_bytes = 100u * 1024u * 1024u;
  p.max_header_bytes = 64 * 1024;
  p.max_section_bytes_by_registered_name = {{"a", p.max_checkpoint_bytes},
                                            {"z", p.max_checkpoint_bytes},
                                            {"common", p.max_checkpoint_bytes}};
  p.stream_chunk_bytes = 4096;
  p.max_staging_disk_bytes = 200u * 1024u * 1024u;
  return p;
}
CheckpointIdentity identity() {
  CheckpointIdentity i;
  i.world_id[0] = 19;
  i.revision = 9;
  i.tick = 3;
  i.catalog.content_id = "fixture";
  i.catalog.content_version = "1";
  i.catalog.ruleset_id = "test";
  i.catalog.module_versions = {{"world_timeline", "2"}};
  return i;
}
std::uint64_t number(const Bytes &b, std::size_t at, unsigned n) {
  std::uint64_t result = 0;
  for (unsigned i = 0; i < n; ++i)
    result |= std::uint64_t(b[at + i]) << (8 * i);
  return result;
}
void put(Bytes &b, std::size_t at, std::uint64_t value, unsigned n) {
  for (unsigned i = 0; i < n; ++i)
    b[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
void rehash(Bytes &b) {
  picosha2::hash256(b.begin(), b.end() - 32, b.end() - 32, b.end());
}
// 独立固定端序/typed字段组装用于逐字节golden，不调用被测visitor编码。
void add(Bytes &b, std::uint64_t value, unsigned n) {
  for (unsigned i = 0; i < n; ++i)
    b.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void text(Bytes &b, const std::string &value) {
  add(b, value.size(), 8);
  b.insert(b.end(), value.begin(), value.end());
}
void field(Bytes &b, std::uint8_t tag, const std::string &name) {
  b.push_back(tag);
  text(b, name);
}
void string_field(Bytes &b, const std::string &name, const std::string &value) {
  field(b, 4, name);
  text(b, value);
}
Bytes independent_header(const CheckpointIdentity &i) {
  Bytes b;
  string_field(
      b, "world_id",
      std::string(reinterpret_cast<const char *>(i.world_id.data()), 16));
  field(b, 1, "revision");
  add(b, i.revision, 8);
  field(b, 1, "tick");
  add(b, i.tick, 8);
  field(b, 5, "catalog");
  string_field(b, "content_id", i.catalog.content_id);
  string_field(b, "content_version", i.catalog.content_version);
  string_field(b, "artifact_sha256", i.catalog.artifact_sha256);
  string_field(b, "content_digest", i.catalog.content_digest);
  string_field(b, "ruleset_id", i.catalog.ruleset_id);
  string_field(b, "schema_sha256", i.catalog.schema_sha256);
  field(b, 5, "pack_set_digest");
  field(b, 3, "present");
  b.push_back(0);
  b.push_back(6);
  field(b, 7, "module_versions");
  add(b, i.catalog.module_versions.size(), 8);
  for (const auto &p : i.catalog.module_versions) {
    string_field(b, "name", p.first);
    string_field(b, "version", p.second);
  }
  b.push_back(8);
  b.push_back(6);
  return b;
}
Bytes independent_payload(Integer stamp = 7) {
  Bytes b;
  field(b, 1, "stamp");
  add(b, stamp, 8);
  field(b, 7, "rows");
  add(b, 1, 8);
  string_field(b, "text", "x");
  b.push_back(8);
  return b;
}
Bytes independent_file(const CheckpointIdentity &i) {
  auto h = independent_header(i), p = independent_payload();
  const std::string magic = "HM-RESIDENT-CHECKPOINT/2\n";
  Bytes b(magic.begin(), magic.end());
  const auto first = magic.size() + 20 + h.size() + 55 * 2;
  const auto total = first + p.size() * 2 + 32;
  add(b, total, 8);
  add(b, h.size(), 8);
  add(b, 2, 4);
  b.insert(b.end(), h.begin(), h.end());
  CheckpointDigest hash{};
  picosha2::hash256(p.begin(), p.end(), hash.begin(), hash.end());
  for (int j = 0; j < 2; ++j) {
    add(b, 1, 2);
    b.push_back(j ? 'z' : 'a');
    add(b, 1, 4);
    add(b, first + p.size() * j, 8);
    add(b, p.size(), 8);
    b.insert(b.end(), hash.begin(), hash.end());
  }
  b.insert(b.end(), p.begin(), p.end());
  b.insert(b.end(), p.begin(), p.end());
  b.resize(total);
  rehash(b);
  return b;
}
// 实际临时文件证明单次编码后输出仍走固定chunk，并可按同一物理合同反查。
class FileTransport final : public CheckpointWriteSink,
                            public CheckpointReadSource {
public:
  FileTransport() : file(std::tmpfile()) {}
  ~FileTransport() {
    if (file)
      std::fclose(file);
  }
  Status write(const std::uint8_t *data, std::size_t n) override {
    maximum = std::max(maximum, n);
    if (!file || std::fwrite(data, 1, n, file) != n)
      return {false, {}, {{"TestWriteFailed", ""}}};
    count += n;
    return {true, {}, {}};
  }
  std::uint64_t size() const override { return count; }
  Status read(std::uint64_t at, std::uint8_t *out, std::size_t n) override {
    if (!file || at > std::uint64_t(LONG_MAX) ||
        std::fseek(file, static_cast<long>(at), SEEK_SET) ||
        std::fread(out, 1, n, file) != n)
      return {false, {}, {{"TestReadFailed", ""}}};
    return {true, {}, {}};
  }
  std::FILE *file;
  std::uint64_t count = 0;
  std::size_t maximum = 0;
};
// 小fixture留存输出只用于独立摘要对照，生产大档测试使用上面的真实文件sink。
class MemorySink final : public CheckpointWriteSink {
public:
  Bytes bytes;
  Status write(const std::uint8_t *data, std::size_t n) override {
    bytes.insert(bytes.end(), data, data + n);
    return {true, {}, {}};
  }
};
class RejectSink final : public CheckpointWriteSink {
public:
  Status write(const std::uint8_t *, std::size_t) override {
    return {false, {}, {{"InjectedShortWrite", ""}}};
  }
};
// 第一次实际chunk写入后取消；可同时注入短写，检验原首错不被取消覆盖。
class CancelSink final : public CheckpointWriteSink {
public:
  CancelSink(CheckpointJobBudget &job, bool fail) : job_(job), fail_(fail) {}
  Status write(const std::uint8_t *, std::size_t size) override {
    ++writes;
    maximum = std::max(maximum, size);
    job_.cancel();
    if (fail_)
      return {false, {}, {{"InjectedShortWrite", ""}}};
    return {true, {}, {}};
  }
  std::size_t writes = 0, maximum = 0;
private:
  CheckpointJobBudget &job_;
  bool fail_;
};
// 只报告文件长度，证明粗上限拒绝发生在任何读入/分配之前。
class OversizeSource final : public CheckpointReadSource {
public:
  std::uint64_t size() const override { return 256000001; }
  Status read(std::uint64_t, std::uint8_t *, std::size_t) override {
    ++reads;
    return {false, {}, {{"UnexpectedRead", "checkpoint"}}};
  }
  int reads = 0;
};
} // namespace
int main() {
  {
    auto coarse = engineering_checkpoint_capacity_profile();
    OversizeSource source;
    const CheckpointDomainSelector selector = [](const CheckpointIdentity &)
        -> Outcome<std::vector<CheckpointDomain>> { return {true, {}, {}}; };
    const auto denied = inspect_checkpoint_v2_stream(source, selector, coarse);
    check(!denied.ok &&
              denied.error.original.code == "CheckpointCapacityExceeded" &&
              source.reads == 0,
          "256000000-byte cap rejects the file before any read");
    coarse.max_checkpoint_bytes = 256000001;
    check(
        !validate_capacity_profile(coarse).ok,
        "a trusted profile cannot silently raise the approved whole-file cap");
  }
  {
    auto coarse = engineering_checkpoint_capacity_profile();
    coarse.max_section_bytes_by_registered_name.emplace(
        "pilot", coarse.max_checkpoint_bytes);
    Bytes damaged;
    field(damaged, 7, "rows");
    add(damaged, 100, 8);
    damaged.push_back(8);
    CodecDecodeBudget budget(coarse);
    CodecVisitor reader(damaged, coarse, "pilot", budget);
    std::vector<std::string> rows{"old candidate value"};
    auto denied =
        reader.sequence("rows", rows, [](CodecVisitor &v, std::string &row) {
          return v.field("text", row);
        });
    check(!denied.ok && denied.error.original.code == "TruncatedCheckpoint" &&
              rows == std::vector<std::string>{"old candidate value"},
          "count exceeding remaining bytes refuses before candidate resize");
  }
  auto p = profile();
  auto i = identity();
  auto lease = std::make_shared<const FixtureLease>(i);
  std::vector<CheckpointDomain> domains{{"z", 1}, {"a", 1}};
  std::vector<CheckpointSectionProducer> producers{{"z", 1, lease},
                                                   {"a", 1, lease}};
  auto encoded = encode_checkpoint_v2_bytes(i, producers, domains, p,
                                            p.max_checkpoint_bytes);
  check(encoded.ok, "v2 encode");
  check(lease->visits() == 2, "each producer visits exactly once for Bytes output");
  if (!encoded.ok) {
    std::cerr << encoded.error.original.code << ' '
              << encoded.error.original.path << '\n';
    return 1;
  }
  MemorySink memory;
  auto receipt = encode_checkpoint_v2(i, producers, domains, p, memory);
  check(receipt.ok && memory.bytes == encoded.value,
        "stream and Bytes same exact encoder");
  check(lease->visits() == 4, "each producer visits exactly once for stream output");
  CheckpointDigest full{}, envelope{};
  picosha2::hash256(memory.bytes.begin(), memory.bytes.end(), full.begin(),
                    full.end());
  picosha2::hash256(memory.bytes.begin(), memory.bytes.end() - 32,
                    envelope.begin(), envelope.end());
  check(receipt.ok && receipt.value.full_checkpoint_sha256 == full,
        "receipt full digest includes internal trailer");
  check(receipt.ok && receipt.value.envelope_sha256 == envelope &&
            envelope != full,
        "envelope digest distinct from HMCB binding digest");
  check(encoded.value == independent_file(i),
        "independent complete wire golden");
  auto selected = [&](const CheckpointIdentity &)
      -> Outcome<std::vector<CheckpointDomain>> { return {true, domains, {}}; };
  auto decoded = inspect_checkpoint_v2_selected(encoded.value, selected, p);
  check(decoded.ok && decoded.value.container_version == 2 &&
            decoded.value.sections[0].domain == "a",
        "sorted v2 decode");
  check(decoded.ok && !encode_checkpoint(decoded.value).ok,
        "v2 envelope cannot silently downgrade through legacy API");
  check(decode_checkpoint_v2_bytes(encoded.value, i.catalog, domains, p).ok,
        "exact catalog/domain decode");
  auto wrong = i.catalog;
  wrong.content_id = "other";
  check(decode_checkpoint_v2_bytes(encoded.value, wrong, domains, p)
                .error.original.code == "CheckpointCatalogMismatch",
        "catalog mismatch first semantic gate");
  const std::size_t magic = sizeof("HM-RESIDENT-CHECKPOINT/2\n") - 1;
  const auto header = number(encoded.value, magic + 8, 8);
  const auto directory = magic + 20 + header;
  const auto first_payload = number(encoded.value, directory + 7, 8);
  auto bulky_lease =
      std::make_shared<const FixtureLease>(i, std::string(4096, 'b'));
  auto bulky = encode_checkpoint_v2_bytes(
      i, {{"a", 1, bulky_lease}, {"z", 1, bulky_lease}}, domains, p,
      p.max_checkpoint_bytes);
  check(bulky.ok, "retained second-phase fixture encodes");
  if (bulky.ok) {
    auto restored_bulky = inspect_checkpoint_v2_selected(bulky.value, selected, p);
    check(restored_bulky.ok && restored_bulky.value.sections.size() == 2 &&
              restored_bulky.value.sections[0].payload == restored_bulky.value.sections[1].payload,
          "ordinary payload copy keeps both actual section bytes without a memory quota");
    if (restored_bulky.ok && restored_bulky.value.sections.size() == 2) {
      CodecDecodeBudget budget(p);
      const auto &payload = restored_bulky.value.sections[0].payload;
      CodecVisitor reader(payload, p, "a", budget);
      Integer stamp = 0;
      std::vector<std::string> rows;
      reader.field("stamp", stamp);
      reader.sequence("rows", rows, [](CodecVisitor &v, std::string &row) { return v.field("text", row); });
      check(reader.finish().ok && stamp == 7 && rows == std::vector<std::string>{std::string(4096, 'b')},
            "copied section decodes the exact real typed value without collection registration");
    }
  }
  auto bad = encoded.value;
  bad.back() ^= 1;
  check(inspect_checkpoint_v2_selected(bad, selected, p).error.original.code ==
            "CheckpointDigestMismatch",
        "whole raw digest mismatch");
  bad = encoded.value;
  put(bad, magic, bad.size() + 1, 8);
  rehash(bad);
  check(inspect_checkpoint_v2_selected(bad, selected, p).error.original.code ==
            "CheckpointLengthMismatch",
        "declared actual length exact");
  bad = encoded.value;
  put(bad, magic + 16, 257, 4);
  rehash(bad);
  check(inspect_checkpoint_v2_selected(bad, selected, p).error.original.code ==
            "CheckpointDomainLimit",
        "section count before directory allocation");
  bad = encoded.value;
  put(bad, magic + 8, UINT64_MAX, 8);
  rehash(bad);
  check(inspect_checkpoint_v2_selected(bad, selected, p).error.kind ==
            ErrorKind::internal,
        "huge header resource before allocation");
  bad = encoded.value;
  bad[directory + 2] = 'z';
  rehash(bad);
  check(inspect_checkpoint_v2_selected(bad, selected, p).error.original.code ==
            "CheckpointDirectoryOrderMismatch",
        "directory ascending unique");
  bad = encoded.value;
  put(bad, directory + 7, first_payload + 1, 8);
  rehash(bad);
  check(inspect_checkpoint_v2_selected(bad, selected, p).error.original.code ==
            "CheckpointSectionRangeMismatch",
        "gap rejected");
  bad = encoded.value;
  put(bad, directory + 15, UINT64_MAX, 8);
  rehash(bad);
  check(inspect_checkpoint_v2_selected(bad, selected, p).error.original.code ==
            "CheckpointSectionRangeMismatch",
        "overflow payload range rejected");
  bad = encoded.value;
  bad[first_payload] ^= 1;
  rehash(bad);
  check(inspect_checkpoint_v2_selected(bad, selected, p).error.original.code ==
            "CheckpointSectionDigestMismatch",
        "persection raw digest");
  auto unknown = domains;
  unknown[0].codec_version = 2;
  check(!decode_checkpoint_v2_bytes(encoded.value, i.catalog, unknown, p).ok,
        "exact registered versions");
  auto small = p;
  small.max_checkpoint_bytes = 512;
  small.max_header_bytes = 512;
  small.max_bigint_encoded_bytes = 512;
  for (auto &x : small.max_section_bytes_by_registered_name)
    x.second = 512;
  check(!encode_checkpoint_v2_bytes(i, producers, domains, small, 512).ok,
        "total budget before emit");
  auto untouched = std::make_shared<const FixtureLease>(i);
  auto no_transport = encode_checkpoint_v2_bytes(i, {{"a", 1, untouched}}, {{"a", 1}}, p, 100);
  check(!no_transport.ok && no_transport.error.original.code == "CheckpointCapacityExceeded" &&
            untouched->visits() == 0 && no_transport.value.empty(),
        "insufficient Bytes transport limit rejects before producer or buffer growth");
  // 精确整档边界包含目录及32字节尾摘要；不足一字节须在任何输出前拒绝。
  auto exact_file = p;
  exact_file.max_checkpoint_bytes = encoded.value.size();
  exact_file.max_header_bytes = exact_file.max_checkpoint_bytes;
  exact_file.max_bigint_encoded_bytes = exact_file.max_checkpoint_bytes;
  for (auto &entry : exact_file.max_section_bytes_by_registered_name)
    entry.second = exact_file.max_checkpoint_bytes;
  MemorySink exact_sink;
  auto exact_result = encode_checkpoint_v2(i, producers, domains, exact_file, exact_sink);
  check(exact_result.ok && exact_sink.bytes == encoded.value,
        "exact whole-file limit includes directory and digest trailer");
  auto below_file = exact_file;
  --below_file.max_checkpoint_bytes;
  below_file.max_header_bytes = below_file.max_checkpoint_bytes;
  below_file.max_bigint_encoded_bytes = below_file.max_checkpoint_bytes;
  for (auto &entry : below_file.max_section_bytes_by_registered_name)
    entry.second = below_file.max_checkpoint_bytes;
  MemorySink below_sink;
  auto below_result = encode_checkpoint_v2(i, producers, domains, below_file, below_sink);
  check(!below_result.ok && below_result.error.original.code == "CheckpointCapacityExceeded" &&
            below_sink.bytes.empty() && below_result.value.total_bytes == 0,
        "one byte over whole-file cap refuses buffer growth before external output");
  auto failed_producer = std::make_shared<const InterruptedLease>(i);
  MemorySink producer_sink;
  auto producer_result = encode_checkpoint_v2(i, {{"a", 1, failed_producer}}, {{"a", 1}}, p, producer_sink);
  check(!producer_result.ok && producer_result.error.original.code == "InjectedProducerFailure" &&
            failed_producer->visits == 1 && producer_sink.bytes.empty() && producer_result.value.total_bytes == 0,
        "failed single producer exposes no partial stream or success receipt");

  auto invalid = p;
  invalid.stream_chunk_bytes = 0;
  check(!validate_capacity_profile(invalid).ok, "zero chunk invalid profile");
  MemorySink unknown_section;
  auto unknown_result = encode_checkpoint_v2(i, {{"not_registered", 1, lease}}, {{"not_registered", 1}}, p, unknown_section);
  check(!unknown_result.ok && unknown_result.error.original.code == "UnregisteredCheckpointDomain" &&
            unknown_section.bytes.empty(),
        "unknown section still refuses before any output at the real envelope boundary");
  bad = encoded.value;
  bad[directory + 2] = 'q';
  rehash(bad);
  check(inspect_checkpoint_v2_selected(bad, selected, p).error.original.code == "UnregisteredCheckpointDomain",
        "unknown wire section is refused even with a valid total digest");
  auto drift = std::make_shared<const FixtureLease>(i, "x", 1, true);
  auto once = encode_checkpoint_v2_bytes(i, {{"a", 1, drift}}, {{"a", 1}}, p,
                                          p.max_checkpoint_bytes);
  check(once.ok && drift->visits() == 1,
        "a producer is never called again to emit its measured bytes");
  if (once.ok) {
    auto restored = decode_checkpoint_v2_bytes(once.value, i.catalog, {{"a", 1}}, p);
    check(restored.ok && restored.value.sections[0].payload == independent_payload(1),
          "directory and whole hashes bind the first and only serialized bytes");
  }

  auto mixed = producers;
  mixed[0].snapshot = std::make_shared<const FixtureLease>(i);
  check(encode_checkpoint_v2_bytes(i, mixed, domains, p, p.max_checkpoint_bytes)
                .error.original.code == "CheckpointSnapshotLeaseMismatch",
        "one immutable lease required");
  auto foreign = i;
  foreign.tick++;
  check(encode_checkpoint_v2_bytes(foreign, producers, domains, p,
                                   p.max_checkpoint_bytes)
                .error.original.code == "CheckpointSnapshotIdentityMismatch",
        "identity bound to lease");
  auto depth_profile = p;
  depth_profile.max_nesting_depth = 1;
  MemorySink nested_sink;
  CodecVisitor nested(nested_sink, depth_profile, "a",
                      depth_profile.max_checkpoint_bytes);
  auto depth_status = nested.object("outer", [](CodecVisitor &v) {
    return v.object("inner", [](CodecVisitor &x) { return x.status(); });
  });
  check(!depth_status.ok && depth_status.error.kind == ErrorKind::internal &&
            depth_status.error.original.code == "CheckpointCapacityExceeded" &&
            depth_status.error.source_id == depth_profile.profile_id &&
            depth_status.error.reason == std::optional<StableId>("2"),
        "v2 depth budget is technical with profile and needed depth");
  RejectSink reject;
  check(encode_checkpoint_v2(i, producers, domains, p, reject)
                .error.original.code == "InjectedShortWrite",
        "sink error preserved");
  // 原/1仍使用旧visitor、旧魔数、hex摘要及旧目录顺序。
  CheckpointEnvelope old;
  old.identity = i;
  old.sections = {{"z", 1, independent_payload()},
                  {"a", 1, independent_payload()}};
  auto legacy = encode_checkpoint(old);
  check(legacy.ok &&
            legacy.value[sizeof("HM-RESIDENT-CHECKPOINT/1\n") - 2] == '\n',
        "legacy encode remains available");
  auto legacy_decode = inspect_checkpoint(legacy.value, domains);
  check(legacy_decode.ok && legacy_decode.value.sections[0].domain == "z" &&
            legacy_decode.value.container_version == 1,
        "legacy order preserved");
  check(!inspect_checkpoint(encoded.value, domains).ok,
        "legacy does not reinterpret v2");
  // 删记帐后仍逐值核真实typed解码；损坏数量必须在分配前拒绝，不靠预算表兜底。
  const auto payload = independent_payload();
  CodecDecodeBudget budget(p);
  CodecVisitor reader(payload, p, "a", budget);
  Integer stamp = 0;
  reader.field("stamp", stamp);
  std::vector<std::string> rows;
  auto status = reader.sequence("rows", rows, [](CodecVisitor &v, std::string &value) {
    return v.field("text", value);
  });
  check(status.ok && reader.finish().ok && stamp == 7 && rows == std::vector<std::string>{"x"},
        "typed candidate values survive without allocation accounting");
  Bytes string_input;
  std::string string_source(16, 's');
  CodecVisitor string_writer(string_input);
  string_writer.field("value", string_source);
  const Bytes &immutable_string = string_input;
  CodecDecodeBudget string_budget(p);
  CodecVisitor string_reader(immutable_string, p, "a", string_budget);
  std::string string_target(2000, 'o');
  check(string_reader.field("value", string_target).ok && string_reader.finish().ok &&
            string_target == string_source,
        "typed string replaces old owned value exactly");
  Bytes vector_input;
  std::vector<Integer> vector_source{1, 2, 3, 4};
  CodecVisitor vector_writer(vector_input);
  vector_writer.sequence("rows", vector_source,
                        [](CodecVisitor &v, Integer &n) { return v.field("n", n); });
  const Bytes &immutable_vector = vector_input;
  CodecDecodeBudget vector_budget(p);
  CodecVisitor vector_reader(immutable_vector, p, "a", vector_budget);
  std::vector<Integer> vector_target{9, 8, 7};
  check(vector_reader.sequence("rows", vector_target,
                               [](CodecVisitor &v, Integer &n) { return v.field("n", n); }).ok &&
            vector_reader.finish().ok && vector_target == vector_source,
        "typed sequence replaces old owned values without changing order");
  Bytes strings_input;
  std::vector<std::string> strings_source{string_source};
  CodecVisitor strings_writer(strings_input);
  strings_writer.sequence("rows", strings_source,
                         [](CodecVisitor &v, std::string &value) { return v.field("value", value); });
  const Bytes &immutable_strings = strings_input;
  CodecDecodeBudget strings_budget(p);
  CodecVisitor strings_reader(immutable_strings, p, "a", strings_budget);
  std::vector<std::string> strings_target{"old", "extra"};
  check(strings_reader.sequence("rows", strings_target,
                                [](CodecVisitor &v, std::string &value) { return v.field("value", value); }).ok &&
            strings_reader.finish().ok && strings_target == strings_source,
        "nested string sequence keeps real values without a default-shape table");
  // 原宏/helper仍用真实typed名称；目录section有限选择与字段wire精确匹配承担类型门。
  const auto &registered = engineering_checkpoint_capacity_profile();
  for (const auto *key :
       {"military/return_facts", "military/muster_facts",
        "military/resignation_facts", "military/delivery_facts",
        "military/settlement_facts", "military/offices", "military/garrisons",
        "military/military_units", "military/campaigns", "military/strongholds",
        "military/supply_facts", "military/arrival_facts", "social/relations",
        "social/memories", "social/events", "action_elapsed/input_basis_refs",
        "action_elapsed/previous_input_basis_refs"}) {
    std::string name = key;
    const auto split = name.find('/');
    MemorySink sink;
    CodecVisitor registered_writer(sink, registered, name.substr(0, split),
                                   registered.max_checkpoint_bytes);
    std::vector<Integer> empty;
    check(registered_writer
              .sequence(
                  name.substr(split + 1), empty,
                  [](CodecVisitor &v, Integer &n) { return v.field("n", n); })
              .ok,
          "original owner-named macro/helper collection still serializes");
  }
  // 字段名来自owner而不是档案选择；直接损坏wire名称或tag都必须真实拒绝。
  for (bool wrong_tag : {false, true}) {
    auto payload = independent_payload();
    const std::size_t rows_at = 1 + 8 + std::string_view("stamp").size() + 8;
    if (wrong_tag)
      payload[rows_at] = 5;
    else
      payload[rows_at + 1 + 8] = 'x';
    CodecDecodeBudget budget(p);
    CodecVisitor reader(static_cast<const Bytes &>(payload), p, "a", budget);
    Integer stamp = 0;
    std::vector<std::string> rows{"old"};
    reader.field("stamp", stamp);
    auto result = reader.sequence("rows", rows,
                                   [](CodecVisitor &v, std::string &row) { return v.field("text", row); });
    check(!result.ok && result.error.original.code ==
                           (wrong_tag ? "CheckpointTypeMismatch" : "CheckpointFieldMismatch") &&
              rows == std::vector<std::string>{"old"},
          "typed reader rejects unknown actual wire tag/name before touching the old candidate");
  }
  // 显式同job入口沿原encoder逐字节一致；输入、输出和候选仍受同一生命周期约束。
  auto slot = engineering_slot_capacity_profile();
  // 生产profile已无纯内存额度字段；真实wire、同job与取消门仍在。
  {
    auto production_profile = p;
    auto production_slot = slot;
    auto production = CheckpointJobBudget::create(production_profile, production_slot);
    check(production.ok, "production job creation without allocation accounting");
    auto bytes = encode_checkpoint_v2_bytes(i, producers, domains, production_profile,
                                            p.max_checkpoint_bytes, production.value);
    check(bytes.ok && bytes.value.bytes() == encoded.value,
          "unmetered production encoder keeps the original byte golden");
    auto restored = decode_checkpoint_v2_bytes(bytes.value, i.catalog, domains,
                                                production_profile, production.value);
    check(restored.ok && restored.value.value().sections[0].payload ==
                              decoded.value.sections[0].payload,
          "production cold read keeps actual payload with no hidden memory total");
    auto foreign = CheckpointJobBudget::create(production_profile, production_slot);
    auto refused = inspect_checkpoint_v2_selected(bytes.value, domains,
                                                   production_profile, foreign.value);
    check(!refused.ok && refused.error.try_materialize_rule_error() &&
              refused.error.rule_error()->kind == ErrorKind::internal,
          "production unmetered input still rejects a different job owner");
    production.value.cancel();
    check(!encode_checkpoint_v2_bytes(i, producers, domains, production_profile,
                                      p.max_checkpoint_bytes, production.value).ok,
          "production zero accounting still rejects a cancelled encoder job");
  }
  auto budgeted = CheckpointJobBudget::create(p, slot);
  check(budgeted.ok, "job creation");
  auto &job = budgeted.value;
  auto owned = encode_checkpoint_v2_bytes(i, producers, domains, p,
                                          p.max_checkpoint_bytes, job);
  check(owned.ok && owned.value.bytes() == encoded.value,
        "same-job encoder byte golden");
  auto owned_decoded =
      decode_checkpoint_v2_bytes(owned.value, i.catalog, domains, p, job);
  check(owned_decoded.ok && owned_decoded.value.value().sections[0].payload ==
                                decoded.value.sections[0].payload,
        "same-job physical decode exact payload");
  owned_decoded.value = CheckpointJobEnvelope{};
  check(owned.value.belongs_to(job), "input retains original job after candidate destruction");
  auto other = CheckpointJobBudget::create(p, slot);
  auto wrong_job =
      inspect_checkpoint_v2_selected(owned.value, domains, p, other.value);
  check(!wrong_job.ok && wrong_job.error.try_materialize_rule_error() &&
            wrong_job.error.rule_error()->kind == ErrorKind::internal,
        "other job input technical refusal");
  auto cancelled = CheckpointJobBudget::create(p, slot);
  cancelled.value.cancel();
  check(!encode_checkpoint_v2_bytes(i, producers, domains, p,
                                    p.max_checkpoint_bytes, cancelled.value)
             .ok,
        "cancel before encode refuses");
  {
    auto fresh = CheckpointJobBudget::create(p, slot);
    auto interrupted = std::make_shared<const InterruptedLease>(i, &fresh.value);
    MemorySink sink;
    auto result = encode_checkpoint_v2(i, {{"a", 1, interrupted}}, {{"a", 1}}, p, sink, fresh.value);
    check(!result.ok && result.error.code() == CheckpointJobFailureCode::cancelled &&
              interrupted->visits == 1 && sink.bytes.empty() && result.value.total_bytes == 0,
          "producer cancellation prevents all stream output and success receipt");
  }
  for (const auto chunk_bytes : {std::uint64_t(16), p.stream_chunk_bytes}) {
    auto chunk_profile = p;
    chunk_profile.stream_chunk_bytes = chunk_bytes;
    auto fresh = CheckpointJobBudget::create(chunk_profile, slot);
    CancelSink sink(fresh.value, false);
    auto result = encode_checkpoint_v2(i, producers, domains, chunk_profile, sink, fresh.value);
    check(!result.ok && result.error.code() == CheckpointJobFailureCode::cancelled &&
              sink.writes == 1 && sink.maximum <= chunk_bytes && result.value.total_bytes == 0,
          "cancellation in the first or final chunk retains failure without further writes");
  }
  {
    auto fresh = CheckpointJobBudget::create(p, slot);
    CancelSink sink(fresh.value, true);
    auto result = encode_checkpoint_v2(i, producers, domains, p, sink, fresh.value);
    check(!result.ok && result.error.rule_error() &&
              result.error.rule_error()->original.code == "InjectedShortWrite" && sink.writes == 1,
          "short-write first error survives concurrent cancellation");
  }
  FileTransport job_file;
  auto saved = encode_checkpoint_v2(i, producers, domains, p, job_file, job);
  check(saved.ok && owned.value.belongs_to(job),
        "stream save keeps prior output bound to the same job");
  std::fflush(job_file.file);
  auto indexed = inspect_checkpoint_v2_stream(job_file, domains, p, job);
  check(indexed.ok && indexed.value.value().receipt.full_checkpoint_sha256 ==
                          receipt.value.full_checkpoint_sha256,
        "real file same-job index");
  indexed.value = CheckpointJobStreamIndex{};
  check(owned.value.belongs_to(job), "destroying index does not revoke live input ownership");
  auto long_i = i;
  long_i.catalog.content_id = std::string(2000, 'L');
  auto long_lease = std::make_shared<const FixtureLease>(long_i);
  auto long_bytes = encode_checkpoint_v2_bytes(
      long_i, {{"z", 1, long_lease}, {"a", 1, long_lease}}, domains, p,
      p.max_checkpoint_bytes, job);
  auto long_result = decode_checkpoint_v2_bytes(
      long_bytes.value, long_i.catalog, domains, p, job);
  auto short_result =
      decode_checkpoint_v2_bytes(owned.value, i.catalog, domains, p, job);
  check(long_result.ok && short_result.ok,
        "long and short identity results created");
  long_result.value = std::move(short_result.value);
  check(long_result.value.value().identity.catalog.content_id ==
                i.catalog.content_id &&
            long_result.value.value().identity.catalog.content_id.capacity() <
                100,
        "move assignment destroys old large identity allocation");
  // 真实容器首错返回之后，job façade和输入都销毁；失败结果仍拥有受控legacy
  // view。
  auto failed_decode = [&]() {
    auto fresh = CheckpointJobBudget::create(p, slot);
    auto damaged = encoded.value;
    damaged.back() ^= 1;
    auto input = CheckpointJobBytes::copy(fresh.value, CheckpointJobUse::slot, damaged);
    return inspect_checkpoint_v2_selected(input.value, domains, p, fresh.value);
  }();
  check(!failed_decode.ok && failed_decode.error.try_materialize_rule_error() &&
            failed_decode.error.rule_error()->original.code == "CheckpointDigestMismatch" &&
            failed_decode.value.value().sections.empty(),
        "actual corrupt input keeps its first error after job destruction without a partial candidate");
  auto failed_catalog = [&]() {
    auto fresh = CheckpointJobBudget::create(p, slot);
    auto input = CheckpointJobBytes::copy(fresh.value, CheckpointJobUse::slot,
                                          encoded.value);
    return decode_checkpoint_v2_bytes(input.value, wrong, domains, p,
                                      fresh.value);
  }();
  check(!failed_catalog.ok && failed_catalog.error.rule_error() &&
            failed_catalog.error.rule_error()->original.code ==
                "CheckpointCatalogMismatch",
        "actual catalog mismatch keeps first error beyond candidate and job "
        "destruction");
  auto failed_sink = [&]() {
    auto fresh = CheckpointJobBudget::create(p, slot);
    RejectSink sink;
    return encode_checkpoint_v2(i, producers, domains, p, sink, fresh.value);
  }();
  check(!failed_sink.ok && failed_sink.error.rule_error() &&
            failed_sink.error.rule_error()->original.code ==
                "InjectedShortWrite",
        "actual stream failure retains first legacy error beyond facade");
  for (const auto *key :
       {"dynamic_social/source_time_bindings",
        "dynamic_social/message_execution_bindings",
        "dynamic_social/propagation_decisions",
        "dynamic_social/propagation_cooldowns",
        "dynamic_social/actual_knowledge_keys",
        "dynamic_social/module_versions", "social_period_sources/openings",
        "social_period_sources/closures",
        "social_period_sources/due_message_ids",
        "social_period_sources/delivered_message_ids",
        "social_period_sources/module_versions"}) {
    std::string name = key;
    auto split = name.find('/');
    MemorySink target;
    CodecVisitor writer(target, registered, name.substr(0, split),
                        registered.max_checkpoint_bytes);
    std::vector<Integer> values;
    check(writer
              .sequence(name.substr(split + 1), values,
                        [](CodecVisitor &c, Integer &value) {
                          return c.field("value", value);
                        })
              .ok,
          "Social owner-named helper collection still serializes");
  }
  check(registered.max_section_bytes_by_registered_name.count("social_period_sources") == 1,
        "actual Social section remains in the finite section allowlist");
  auto large = std::make_shared<const FixtureLease>(
      i, std::string(1024u * 1024u, 'q'), 65);
  FileTransport file;
  auto streamed =
      encode_checkpoint_v2(i, {{"common", 1, large}}, {{"common", 1}}, p, file);
  check(streamed.ok && streamed.value.total_bytes > 64u * 1024u * 1024u && large->visits() == 1,
        "single serialization supports a file beyond legacy64MiB");
  check(file.maximum <= p.stream_chunk_bytes, "sink chunk stays bounded");
  if (streamed.ok) {
    std::fflush(file.file);
    auto index = inspect_checkpoint_v2_stream(
        file,
        [](const CheckpointIdentity &)
            -> Outcome<std::vector<CheckpointDomain>> {
          return {true, {{"common", 1}}, {}};
        },
        p);
    check(index.ok &&
              index.value.receipt.envelope_sha256 ==
                  streamed.value.envelope_sha256 &&
              index.value.receipt.full_checkpoint_sha256 ==
                  streamed.value.full_checkpoint_sha256,
          "actual temp file stream reinspection");
  }
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
