// 舟战后仍用原编解编命令；原形成业务键按真实最初名单重放，不能因后来俘获而失效。
#pragma once
#include "naval_battle_state.hpp"
#include "naval_commands.hpp"
namespace hanmo::resident {
Outcome<NavalCommandQuote> preview_naval_postcombat_command(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId& actor,const NavalCommand&);
Outcome<NavalCommandReceipt> apply_naval_postcombat_command(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalState&,const NavalCatalog&,const StableId& actor,const NavalCommand&);
}
