// 世界适配只借当前政治/军事/成员权源；原条约不向后来的占领方自动让与。
#pragma once
#include "treaty_passage_model.hpp"
#include "resident/domains/politics/takeover/politics_takeover_territory.hpp"
namespace hanmo::resident {
class TreatyPassageTerritoryAccess final:public TreatyPassageControlRead {
public:
    TreatyPassageTerritoryAccess(const CommonStore& common,const PoliticsState& state,const PoliticsCatalog& catalog,const PoliticalTakeoverContext* takeover=nullptr):common_(common),state_(state),catalog_(catalog),takeover_(takeover){}
    Outcome<TreatyPassageControlSource> treaty_control_now(const StableId&)const override;
    Outcome<TreatyPassageControlSource> treaty_control_before_signature(const StableId&,const StableId&,Integer)const override;
private:
    const CommonStore& common_;const PoliticsState& state_;const PoliticsCatalog& catalog_;const PoliticalTakeoverContext* takeover_;
};
class TreatyPassageAccess final:public TreatyPassageAdmissionRead {
public:
    TreatyPassageAccess(TreatyPassageContext context,const MembershipResolver& military):context_(context),military_(military){}
    Outcome<bool> treaty_personal_passage(const StableId&,const StableId&,const StableId&)const override;
private:
    TreatyPassageContext context_;const MembershipResolver& military_;
};
}
