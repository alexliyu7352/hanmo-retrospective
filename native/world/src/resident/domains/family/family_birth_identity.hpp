// 出生身份和先天抽样的纯核验；不造收养事实、不把成年原型技艺复制给新生儿。
#pragma once
#include "family_birth_lineage.hpp"
#include "resident/domains/family/family_model.hpp"
namespace hanmo::resident {
// 精确重演记录中的原PCG抽样序列，只算临时state，不改变当前世界随机流。
Status validate_birth_random_receipt(const CommonStore&,const FamilyLineageCatalog&,const FamilyBirthRecord&);
// Common动态人物入口按具名出生事实核实；未来改名/成年开放须额外真实事实，不默许现态漂移。
Status validate_biological_actor_identity(const CommonStore&,const CommonCatalog&,const FamilyCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const ActorRecord&);
}
