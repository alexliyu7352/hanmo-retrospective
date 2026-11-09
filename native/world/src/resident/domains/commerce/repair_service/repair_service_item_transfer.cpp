#include "repair_service_item_transfer.hpp"
namespace hanmo::resident::repair_service_detail {
Outcome<AppliedRepairItemTransfer> apply_original_repair_item_transfer(CommonStore& c,const CargoSelection& selected,const StableId& destination,const StableId& instance,const Owner& owner,const ReservationRef& reservation){
 return apply_original_unique_item_transfer_in_candidate(c,selected,destination,instance,owner,reservation);
}
}
