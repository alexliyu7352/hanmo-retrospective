// Shop3 只借人物原年龄基础；成年不代替同意、所有权或资格。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct PrivateShopContext;
struct LifeAgeSources;
class CurrentMomentRead;
struct PrivateShopAgeSources3 {
  const LifeAgeSources &basis;
  const CurrentMomentRead &clock;
};
// No invented birth_tick or persistent age copy. Original basis is re-read.
struct PrivateShopAdultRead {
  Integer age_years = 0;
  bool adult = false;
};
Outcome<PrivateShopAdultRead>
read_private_shop_party_adult(const PrivateShopContext &,
                              const StableId &actor);
Outcome<PrivateShopAdultRead> read_private_shop_party_adult_before_fact(
    const PrivateShopContext &, const StableId &actor, Integer projected_tick,
    const StableId &actual_fact);
// Synchronous only: snapshot refs are temporary owned strings. A collector
// must resolve their actual rows in basis roots; it must not retain this view,
// its strings, or actor input addresses. No Shop/permission callback occurs.
struct LifeAgeBasisSnapshot;
struct FactRecord;
class PrivateShopAgeBasisSink3 {
public:
  virtual ~PrivateShopAgeBasisSink3() = default;
  virtual Status basis(const LifeAgeSources &, const StableId &actor,
                       const FactRecord &actual_consumer,
                       const LifeAgeBasisSnapshot &) = 0;
};
Status visit_private_shop_age_basis_before_fact3(const PrivateShopContext &,
                                                 const StableId &actor,
                                                 Integer projected_tick,
                                                 const StableId &actual_fact,
                                                 PrivateShopAgeBasisSink3 &);
} // namespace hanmo::resident
