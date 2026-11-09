// 条约只借具名辖区权源和当前行旅资格，不把签字记录变成任意开门权限。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct TreatyPassageControlSnapshot {
    StableId jurisdiction_id,organization_id,occupation_fact_id;
    Integer revision=0;
};
struct TreatyPassageControlSource {
    WorldIdentity identity;
    TreatyPassageControlSnapshot control;
};
// 历史口只认本模块的真实签署头；不得以任意tick或任意事实截取政治控制。
class TreatyPassageControlRead {
public:
    virtual ~TreatyPassageControlRead()=default;
    virtual Outcome<TreatyPassageControlSource> treaty_control_now(const StableId& jurisdiction_id)const=0;
    virtual Outcome<TreatyPassageControlSource> treaty_control_before_signature(const StableId& jurisdiction_id,const StableId& signature_fact_id,Integer tick)const=0;
};
// Travel只取得同候选的有限许可结论；条款不覆盖军团、征调、所有权或其他admission条件。
class TreatyPassageAdmissionRead {
public:
    virtual ~TreatyPassageAdmissionRead()=default;
    virtual Outcome<bool> treaty_personal_passage(const StableId& actor_id,const StableId& place_id,const StableId& terms_id)const=0;
};
}
