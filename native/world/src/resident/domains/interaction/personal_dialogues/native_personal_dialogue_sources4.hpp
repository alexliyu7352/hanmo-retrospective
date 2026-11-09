// Complete4 的原人物会面来源桥，只借真实人物/出生/控制根，不拥有第二人物账。
#pragma once
#include "known_personal_dialogue_contract.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_contract.hpp"
namespace hanmo::resident {
class BornCharacterSourceRead;
struct ProfileControlEligibilitySources;
class NativePersonalDialogueSources4 final {
  const CommonStore &common_;
  const InteractionState &interactions_;
  const FamilyContinuationPeopleRead4 &people_;
  const ProfileControlEligibilitySources &control_;
  const BornCharacterSourceRead *born_;
  BornPersonalDialogueSources born_sources_;
  KnownPersonalDialogueSources known_sources_;
  // 只把原 BirthFactRef 的已验证字段投影为既有会面 DTO，不回算生日。
  Outcome<std::optional<BornActorSource>>
  read_born(const StableId &, const SourceRef *, Integer) const;
  // 历史控制只读真实 opening 之前的原控制链，不反调关系/Legacy 完整验证。
  Outcome<StableId> player_before(Integer, const StableId &,
                                  bool born_opening) const;

public:
  // 外层 owning graph 保活全部来源；无 Birth 选择可不借
  // born，但遇出生人物必拒缺源。
  NativePersonalDialogueSources4(const CommonStore &, const InteractionState &,
                                 const FamilyContinuationPeopleRead4 &,
                                 const ProfileControlEligibilitySources &,
                                 const BornCharacterSourceRead *);
  NativePersonalDialogueSources4(const NativePersonalDialogueSources4 &) =
      delete;
  NativePersonalDialogueSources4 &
  operator=(const NativePersonalDialogueSources4 &) = delete;
  NativePersonalDialogueSources4(NativePersonalDialogueSources4 &&) = delete;
  NativePersonalDialogueSources4 &
  operator=(NativePersonalDialogueSources4 &&) = delete;
  // 构造阶段仅核同根与原时刻，不读取尚未封口的候选业务结果。
  Status validate_bindings() const;
  const BornPersonalDialogueSources &born_sources() const noexcept {
    return born_sources_;
  }
  const KnownPersonalDialogueSources &known_sources() const noexcept {
    return known_sources_;
  }
};
} // namespace hanmo::resident
