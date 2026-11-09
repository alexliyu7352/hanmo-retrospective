// 原 P3 显式目录、根与同 World 读装配；不提供缺源中立回退。
#pragma once
#include "world_read_stamp.hpp"
#include "original_world_read_boundary.hpp"
#include "native_field_application_read.hpp"
#include "resident/domains/military/formation_original_sources.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
struct ExactTimeProfile;
Outcome<Integer>
selected_native_political_relations_version(const CatalogIdentity &);
Status append_native_political_relations_modules(const BoundaryValue &,
                                                 CatalogIdentity &,
                                                 const ExactTimeProfile *);
Status prepare_native_political_relations_catalog(const BoundaryValue &,
                                                  PreparedCatalog &);
Status validate_native_political_relations_catalog(const PreparedCatalog &);
Status validate_native_political_relations_layout(const WorldBundle &);
// 世界和其真实原根必须比该 owner 活得更久；context 只在同步原调用中借用。
class NativePoliticalRelationsWorldRead final
    : public CheckpointAllocatedObject {
  const WorldBundle &world_;
  WorldCurrentMomentRead clock_;
  OriginalWorldReadBoundary boundary_;
  NativeFieldApplicationRead field_clock_;
  MilitaryAccess military_access_;
  NativeFormationSourceRead formations_;
  PoliticalRelationsReadContext context_;
  explicit NativePoliticalRelationsWorldRead(const WorldBundle &);

public:
  NativePoliticalRelationsWorldRead(const NativePoliticalRelationsWorldRead &) =
      delete;
  NativePoliticalRelationsWorldRead &
  operator=(const NativePoliticalRelationsWorldRead &) = delete;
  static Outcome<std::unique_ptr<NativePoliticalRelationsWorldRead>>
  create(const WorldBundle &);
  bool bound_to(const WorldBundle &) const noexcept;
  const PoliticalRelationsReadContext &context() const noexcept {
    return context_;
  }
  Status validate() const;
};
Status validate_native_political_relations_world(const WorldBundle &);
// 仅由原 fresh install 调用；不修冷档、不重新初始化已有历史。
Status initialize_native_political_relations_in_candidate(WorldBundle &);
} // namespace hanmo::resident
