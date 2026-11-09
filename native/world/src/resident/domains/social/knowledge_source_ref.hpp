// KQ 原三字段来源地址纯型；不包含查询实现，不把地址形状当收到许可。
#pragma once
#include "resident/model/dynamic_contract.hpp"
#include <optional>
namespace hanmo::resident {
struct KnowledgeSourceRef {
  enum class Kind {
    known_report,
    offer_discovery,
    contact_receipt,
    legacy_memory
  };
  Kind kind = Kind::known_report;
  StableId source_id;
  std::optional<SubjectRef> source_subject;
};
} // namespace hanmo::resident
