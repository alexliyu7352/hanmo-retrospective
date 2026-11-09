// Same original dispatcher with an explicitly borrowed complete candidate lease.
#pragma once
#include "command_types.hpp"
namespace hanmo::resident {
struct WorldBundle;struct CompleteExactWorldReadBindings;
Outcome<std::shared_ptr<const PreviewResult>> prepare_command_with_exact_bindings(
    const WorldBundle&,const StableId&,const CommandPayload&,const CompleteExactWorldReadBindings&);
Outcome<ReceiptDetails> apply_command_with_exact_bindings(
    WorldBundle&,const StableId&,const PreparedDomainOp&,const CompleteExactWorldReadBindings&);
}
