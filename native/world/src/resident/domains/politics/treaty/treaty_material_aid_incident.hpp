// 原援助到期结果的客观条约事件读面；财务责任与历史欠付量仍由S5唯一反验。
#pragma once
#include "treaty_incident.hpp"
#include "resident/domains/local_economy/material_aid/material_aid.hpp"
namespace hanmo::resident {
// 只接受真实material_aid_late。当前后来收讫不抹除原到期切点的未履行事实。
class NativeTreatyMaterialAidIncidentRead final : public TreatyIncidentOriginalRead {
public:
 explicit NativeTreatyMaterialAidIncidentRead(const MaterialAidReadContext& finance):finance_(finance){}
 Outcome<TreatyIncidentOriginalView> read_actual_treaty_incident(
  const StableId&,Integer,const StableId&,const SourceRef&)const override;
 Status validate_incident_obligation_at_source(const TreatyObligationRef&,
  const TreatyIncidentOriginalView&)const override;
private:
 const MaterialAidReadContext& finance_;
};
}
