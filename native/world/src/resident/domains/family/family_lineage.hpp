// 生物亲缘真值目录与纯读合同；不将同姓、收养、配偶或相似外貌转换为血亲。
#pragma once
#include "birth_endowment.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// 最多两位已记录亲代；不足两位明确表示资料未全，不是该人物没有父母。
struct BiologicalOrigin { StableId actor_id;std::vector<StableId> parent_actor_ids; };
struct FamilyLineageCatalog {
    bool enabled=false;
    OrderedTable<BiologicalOrigin> origins;
    OrderedTable<OrderedTable<Integer>> original_innate_levels;
    BirthEndowmentPolicy endowment;
};
struct BiologicalRelationRead {
    WorldIdentity source_identity;
    StableId first_actor_id,second_actor_id;
    bool known_direct_line=false,known_siblings=false;
    bool first_parents_complete=false,second_parents_complete=false;
    std::vector<StableId> common_parent_actor_ids;
};
// 仅原作者人物能有初始记录；动态人物须另有唯一出生来源，不能回写到此目录。
Status validate_family_lineage_catalog(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&);
// 只供已验证世界的权威业务规则使用，不是未获知人物的族谱公开接口。
Outcome<BiologicalRelationRead> read_original_biological_relation(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const StableId& first,const StableId& second);
// null保留未知亲代或未知该族资料；不会从父母技能反推先天档位。
Outcome<InnateParentSource> read_original_innate_parent(const CommonStore&,const CommonCatalog&,const FamilyLineageCatalog&,const std::optional<StableId>& actor,const StableId& family_id);
Outcome<FamilyLineageCatalog> decode_family_lineage_catalog(const BoundaryValue&);
}
