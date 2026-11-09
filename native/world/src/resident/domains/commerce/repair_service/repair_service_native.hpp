// Same-world composition of the actual S7 original readers. Physical custody
// remains a mandatory real provider; this class supplies no permissive fallback.
#pragma once
#include "repair_service_agreement.hpp"
#include "repair_service_private_finance.hpp"
namespace hanmo::resident {
class RepairServicePhysicalOriginalRead {public:virtual ~RepairServicePhysicalOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept=0;
 virtual Status validate_original_repair_custody(const RepairServiceContractSource&,const RepairCustodyReceivedSource&)const=0;
 virtual Outcome<std::vector<MomentOwnerSource>> read_original_repair_terminal_continuity(const RepairServiceContractSource&,const MomentOwnerSource&)const=0;
 virtual Status validate_original_repair_return(const RepairServiceContractSource&,const RepairItemReturnedSource&)const=0;
};
class NativeRepairServiceOriginalRead final:public RepairServiceOriginalRead {
 const RepairServiceAgreementReadContext&agreements_;const NativeRepairPrivateFinancialRead&finance_;
 const RepairServicePhysicalOriginalRead&physical_;NativeRepairServiceAgreementRead agreement_read_;
public:NativeRepairServiceOriginalRead(const RepairServiceAgreementReadContext&a,const NativeRepairPrivateFinancialRead&f,const RepairServicePhysicalOriginalRead&p):agreements_(a),finance_(f),physical_(p),agreement_read_(a){}
 bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept override;
 Status validate_original_repair_agreement_coverage()const override;
 Status validate_original_repair_service_rules(const RepairServiceContractTerms&,const RepairPolicy&)const override;
 Status validate_original_repair_service_acceptance(const RepairServiceContractTerms&,const WorldMoment&,const RepairServiceContractSource*)const override;
 Outcome<RepairServiceFinancialParties> read_original_repair_service_parties(const RepairServiceContractSource&)const override;
 Outcome<RepairServicePaymentPermission> read_original_repair_payment_permission(const RepairServiceContractSource&,Integer,const WorldMoment&,RepairServicePaymentUse,const MomentOwnerSource&)const override;
 Status validate_original_repair_payment(const RepairServiceContractSource&,const MomentOwnerSource&,RepairServicePaymentUse,Integer,const WorldMoment&)const override;
 Status validate_original_repair_custody(const RepairServiceContractSource&,const RepairCustodyReceivedSource&)const override;
 Outcome<std::vector<MomentOwnerSource>> read_original_repair_terminal_continuity(const RepairServiceContractSource&,const MomentOwnerSource&)const override;
 Status validate_original_repair_return(const RepairServiceContractSource&,const RepairItemReturnedSource&)const override;
};
}
