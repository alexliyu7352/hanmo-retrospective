// Field 事件仅拥有已登记的强类型值；不承载未知字典，也不产生业务来源许可。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/kernel/source_ref.hpp"
#include <memory>
namespace hanmo::resident {
// 来源出口只借真实 typed SourceRef；调用方继续使用原完整来源校验。
class FieldEventSourceSink {
public:
  virtual ~FieldEventSourceSink() = default;
  virtual void source(const SourceRef &) = 0;
};
// 每种实际载荷负责真 clone 与析构；不允许无类型 void 载荷或跨候选共享写。
class FieldEventPayloadValue {
public:
  virtual ~FieldEventPayloadValue() = default;
  virtual Integer type_id() const noexcept = 0;
  virtual std::shared_ptr<FieldEventPayloadValue> clone() const = 0;
  virtual void visit_sources(FieldEventSourceSink &) const = 0;
};
// 稳定公共拥有壳；新增子型不改变事件/会话布局。
class FieldEventPayloads {
public:
  FieldEventPayloads();
  ~FieldEventPayloads();
  FieldEventPayloads(const FieldEventPayloads &);
  FieldEventPayloads(FieldEventPayloads &&) noexcept;
  FieldEventPayloads &operator=(FieldEventPayloads);
  void swap(FieldEventPayloads &) noexcept;
  FieldEventPayloadValue &ensure(Integer);
  const FieldEventPayloadValue *find(Integer) const noexcept;
  void visit_sources(FieldEventSourceSink &) const;
private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};
} // namespace hanmo::resident
