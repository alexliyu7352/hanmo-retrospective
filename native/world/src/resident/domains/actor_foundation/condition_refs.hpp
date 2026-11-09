#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
struct InitialConditionSeedRef {StableId seed_key;};
struct ConditionReceiptRefV2 {StableId result_id;};
// Condition/8 only: one actual application in independent character-injury-sources/1.
// The referenced row owns the finite original source, never a second body balance.
struct CharacterInjurySourceRefV1 {StableId result_id;};
using CharacterConditionSourceRef=std::variant<InitialConditionSeedRef,ConditionReceiptRefV2>;
}
