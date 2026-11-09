// 原接敌事实的side0军令和拓扑决定接近场；今日Owner/当前位置不参与历史选场。
#pragma once
#include "resident/domains/military/military_refs.hpp"
namespace hanmo::resident {
Status validate_military_field_contact_source(const MilitaryState&,const MilitaryCatalog&,const MilitaryContactFact&);
}
