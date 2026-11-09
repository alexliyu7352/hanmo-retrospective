#include "repair_service_native.hpp"
namespace hanmo::resident {
bool NativeRepairServiceOriginalRead::bound_to(const CommonStore&c,const ItemInstancesState&i)const noexcept{return agreement_read_.bound_to(c,i)&&finance_.bound_to(c,i)&&physical_.bound_to(c,i);}
Status NativeRepairServiceOriginalRead::validate_original_repair_agreement_coverage()const{return validate_repair_service_agreements(agreements_);}
Status NativeRepairServiceOriginalRead::validate_original_repair_service_rules(const RepairServiceContractTerms&t,const RepairPolicy&p)const{return agreement_read_.validate_original_repair_service_rules(t,p);}
Status NativeRepairServiceOriginalRead::validate_original_repair_service_acceptance(const RepairServiceContractTerms&t,const WorldMoment&m,const RepairServiceContractSource*c)const{return agreement_read_.validate_original_repair_service_acceptance(t,m,c);}
Outcome<RepairServiceFinancialParties>NativeRepairServiceOriginalRead::read_original_repair_service_parties(const RepairServiceContractSource&t)const{return finance_.read_original_repair_service_parties(t);}
Outcome<RepairServicePaymentPermission>NativeRepairServiceOriginalRead::read_original_repair_payment_permission(const RepairServiceContractSource&t,Integer n,const WorldMoment&m,RepairServicePaymentUse u,const MomentOwnerSource&r)const{return finance_.read_original_repair_payment_permission(t,n,m,u,r);}
Status NativeRepairServiceOriginalRead::validate_original_repair_payment(const RepairServiceContractSource&t,const MomentOwnerSource&r,RepairServicePaymentUse u,Integer n,const WorldMoment&m)const{return finance_.validate_original_repair_payment(t,r,u,n,m);}
Status NativeRepairServiceOriginalRead::validate_original_repair_custody(const RepairServiceContractSource&t,const RepairCustodyReceivedSource&r)const{return physical_.validate_original_repair_custody(t,r);}
Outcome<std::vector<MomentOwnerSource>>NativeRepairServiceOriginalRead::read_original_repair_terminal_continuity(const RepairServiceContractSource&t,const MomentOwnerSource&r)const{return physical_.read_original_repair_terminal_continuity(t,r);}
Status NativeRepairServiceOriginalRead::validate_original_repair_return(const RepairServiceContractSource&t,const RepairItemReturnedSource&r)const{return physical_.validate_original_repair_return(t,r);}
}
