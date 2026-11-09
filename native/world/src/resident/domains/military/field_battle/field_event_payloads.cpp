// opaque 壳每次复制都 clone 真实 typed 值；资源保持原候选 job 生命周期。
#include "field_event_registry.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <stdexcept>
namespace hanmo::resident {
struct FieldEventPayloads::Impl {
  CheckpointMemoryCharge lifetime{0};
  CheckpointVector<std::shared_ptr<FieldEventPayloadValue>> values;
};
FieldEventPayloads::FieldEventPayloads() = default;
FieldEventPayloads::~FieldEventPayloads() = default;
FieldEventPayloads::FieldEventPayloads(FieldEventPayloads &&) noexcept = default;
FieldEventPayloads::FieldEventPayloads(const FieldEventPayloads &other) {
  if (!other.impl_) return;
  impl_ = checkpoint_make_shared<Impl>();
  impl_->values.reserve(other.impl_->values.size());
  for (const auto &value : other.impl_->values) {
    if (!value) throw std::logic_error("FieldEventPayloadOwnerRequired");
    auto copy = value->clone();
    if (!copy || copy.get() == value.get() || copy->type_id() != value->type_id())
      throw std::logic_error("FieldEventPayloadCloneMismatch");
    impl_->values.push_back(std::move(copy));
  }
}
FieldEventPayloads &FieldEventPayloads::operator=(FieldEventPayloads other) {
  swap(other); return *this;
}
void FieldEventPayloads::swap(FieldEventPayloads &other) noexcept { impl_.swap(other.impl_); }
const FieldEventPayloadValue *FieldEventPayloads::find(Integer id) const noexcept {
  if (impl_) for (const auto &value : impl_->values)
    if (value && value->type_id() == id) return value.get();
  return nullptr;
}
FieldEventPayloadValue &FieldEventPayloads::ensure(Integer id) {
  const auto &entry = require_field_event_payload_registration(id);
  if (auto *value = find(id)) return *const_cast<FieldEventPayloadValue *>(value);
  auto value = entry.create();
  if (!value || value->type_id() != id) throw std::logic_error("FieldEventPayloadFactoryMismatch");
  if (!impl_) impl_ = checkpoint_make_shared<Impl>();
  impl_->values.push_back(std::move(value));
  return *impl_->values.back();
}
void FieldEventPayloads::visit_sources(FieldEventSourceSink &sink) const {
  // 原五个字段次序固定；空槽没有来源，绝不从别的当前状态倒填。
  for (const auto *entry : field_event_payload_registry())
    if (const auto *value = find(entry->id)) value->visit_sources(sink);
}
} // namespace hanmo::resident
