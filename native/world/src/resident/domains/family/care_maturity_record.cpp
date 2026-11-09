// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 只承接原 MaturityResult 的 typed 保存，不构造结果、不操作 CareGoal。
#include "care_maturity_record.hpp"
#include "care_maturity_record_codec_fields.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"

namespace hanmo::resident {
// 原 binary visitor 与 JSON 投影使用同一字段表，禁止缺来源时补造历史。
Status visit_care_maturity_record(CodecVisitor &visitor,
                                  MaturityResult &value) {
  return care_maturity_codec::record(visitor, value);
}

// 投影 archive 只读成员；实际拥有记录和原 source 不在编码中改变。
Status emit_care_maturity_record_fields(ProjectionWriter &writer,
                                        const MaturityResult &value) {
  ActorFoundationProjectionArchive archive(writer);
  return care_maturity_codec::record(archive,
                                     const_cast<MaturityResult &>(value));
}

// 边界值仅是序列化结果，不作为可写业务根或自由字段容器。
Outcome<BoundaryValue>
encode_care_maturity_record(const MaturityResult &value) {
  BoundaryValue encoded;
  BoundaryProjectionWriter writer(encoded);
  auto status = writer.object("", [&](ProjectionWriter &out) {
    return emit_care_maturity_record_fields(out, value);
  });
  if (status.ok)
    status = writer.finish();
  return {status.ok, std::move(encoded), status.error};
}

// 完整结束检查拒绝额外或缺少字段；不会为旧档合成 MaturityResult。
Outcome<MaturityResult>
decode_care_maturity_record(const BoundaryValue &input) {
  MaturityResult value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = care_maturity_codec::record(visitor, value);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}

} // namespace hanmo::resident
