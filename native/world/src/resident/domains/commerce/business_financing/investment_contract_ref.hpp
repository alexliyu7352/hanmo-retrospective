// Original 05 F2 / P1 address. This is not an accepted financing contract.
#pragma once
#include "resident/kernel/source_ref.hpp"
#include <cstdint>
namespace hanmo::resident {
struct InvestmentContractRef {StableId contract_id;std::uint64_t terms_revision=0;};
}
