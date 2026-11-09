// 原撤职的本人/辖区纯前界；完整人物和原接管史先验后使用，不回调全政治或私业。
#pragma once
#include "political_dismissal_state.hpp"
#include "resident/domains/politics/succession/political_succession_control.hpp"
namespace hanmo::resident {
// 只接受原politics撤职实际头；任意同域事实不能取得历史控制读资格。
Outcome<StableId> read_political_dismissal_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer,const StableId& actual_fact);
struct PoliticalDismissalTerritoryContext {
 const CommonStore& common;const PoliticsState& politics;const PoliticsCatalog& catalog;
 const PoliticalSuccessionOccupationContext* occupations=nullptr;
};
// 原同治所辖区核实际资产应用/登记；后来失控不抹旧签字，此前失控必须另有连续性制度。
Status validate_political_dismissal_seats(const PoliticalDismissalTerritoryContext&,const PoliticalDismissalSourcePolicy&,Integer,const StableId& actual_fact={});
}
