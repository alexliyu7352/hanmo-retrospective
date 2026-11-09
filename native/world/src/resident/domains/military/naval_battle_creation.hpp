// 海战建场只消费已经截停的真实双舰队和原军职控制口，不分配船、人或第二套旅行时钟。
#pragma once
#include "naval_battle_state.hpp"
namespace hanmo::resident {
// battle_id/contact.id须由同一Common候选分配；上层先验证并落原Travel接敌锚点。
// 只在原contact_install中暂建无AI初帧，随后登记准备来源再一次生成AI；不能发布此暂态。
Outcome<NavalBattleRecord> create_naval_battle_frame(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const NavalContactFact&,const StableId& battle_id);
Outcome<NavalBattleRecord> create_naval_battle_record(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const NavalContactFact&,const StableId& battle_id,const NavalGunneryPreparation* =nullptr);
}
