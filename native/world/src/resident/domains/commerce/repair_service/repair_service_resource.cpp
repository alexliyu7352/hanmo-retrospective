// Domain adapter only. The shared S5 mechanism owns the actual two Common legs.
#include "repair_service_internal.hpp"
namespace hanmo::resident::repair_service_detail {
AppliedRepairMoneyLegs apply_original_repair_money(CommonStore&c,const StableId&payer,const StableId&payee,Integer amount){return take(service_finance::transfer_money_in_candidate(c,payer,payee,amount));}
}
