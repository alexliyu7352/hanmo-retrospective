// 捕获旧世界时保留原PreparedCatalog全部实际拥有图；不以身份或序列化长度估算。
#pragma once
#include "resident/checkpoint/checkpoint_retention.hpp"
namespace hanmo::resident {
struct CatalogRetentionFailure{RuleError error;};
Status retain_prepared_catalog(const std::shared_ptr<const PreparedCatalog>&,CheckpointRetentionCensus&);
}
