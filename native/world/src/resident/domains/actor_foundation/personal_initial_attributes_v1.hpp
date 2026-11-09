// 个人战入场只消费同World真属性/身体；不保存第二身体或属性账。
#pragma once
#include "effective_attributes_v1.hpp"
#include "resident/domains/personal/personal_model.hpp"
namespace hanmo::resident {
struct PersonalInitialAttributes {
  ActorAttributes effective;
  Integer hp = 0, fatigue = 0, injury = 0;
};
class PersonalInitialAttributesRead {
public:
  virtual ~PersonalInitialAttributesRead() = default;
  virtual bool bound_to(const CommonStore &, const PersonalState &,
                        const PersonalCatalog &) const noexcept = 0;
  virtual Status validate_context() const = 0;
  virtual Outcome<PersonalInitialAttributes>
  current(const StableId &) const = 0;
  // 只认所借Personal根中真会话、原started头与原M，不接受自报cut。
  virtual Outcome<PersonalInitialAttributes>
  before_start(const PersonalBattle &, const StableId &) const = 0;
};
class NativePersonalInitialAttributesRead final
    : public PersonalInitialAttributesRead {
public:
  NativePersonalInitialAttributesRead(
      const CharacterConditionSources &conditions,
      const GrowthPermanentSources &growth,
      const CharacterAttributeAbsentSourcesReadV1 &absence,
      const CurrentMomentRead &clock, const PersonalState &personal,
      const PersonalCatalog &catalog)
      : conditions_(conditions), growth_(growth), absence_(absence),
        clock_(clock), personal_(personal), catalog_(catalog) {}
  bool bound_to(const CommonStore &, const PersonalState &,
                const PersonalCatalog &) const noexcept override;
  Status validate_context() const override;
  Outcome<PersonalInitialAttributes> current(const StableId &) const override;
  Outcome<PersonalInitialAttributes>
  before_start(const PersonalBattle &, const StableId &) const override;

private:
  Status selected() const;
  const CharacterConditionSources &conditions_;
  const GrowthPermanentSources &growth_;
  const CharacterAttributeAbsentSourcesReadV1 &absence_;
  const CurrentMomentRead &clock_;
  const PersonalState &personal_;
  const PersonalCatalog &catalog_;
};
// 原Personal疲劳/伤势效率公式；有效武力已由五属性原composer求得。
Integer personal_initial_martial_v1(const PersonalInitialAttributes &);
// 必须由选中source1的原whole/冷恢复入口调用，不能仅热入场检查。
Status validate_personal_initial_attributes_sources_v1(
    const CommonStore &, const PersonalState &, const PersonalCatalog &,
    const PersonalInitialAttributesRead &);
} // namespace hanmo::resident
