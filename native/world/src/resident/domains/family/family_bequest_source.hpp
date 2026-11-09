// 私产遗赠声明仅形成可撤回的死后处分依据；关系与控制许可均不代替本人声明。
#pragma once
#include "family_model.hpp"
#include "life_age_basis.hpp"
#include "succession_personal_assets.hpp"
namespace hanmo::resident {
class FamilyContinuationPeopleRead4;
struct FamilyBequestPolicy { bool enabled=false;Integer selected_version=1; };
// 作者必须显式选择独立遗赠语义，未知policy不降级为旧退隐交产。
Outcome<FamilyBequestPolicy> decode_family_bequest_policy(const BoundaryValue&);
Outcome<FamilyBequestPolicy> decode_family_bequest_policy_v2(const BoundaryValue&);
// 不冻结收件容器或保证未来财物仍在；真正交割须另核实际剩余与接收资格。
struct FamilyBequestTerms {
    StableId donor_actor_id,beneficiary_actor_id,relation_fact_id,source_wallet_id,source_inventory_id;
    Integer money=0;std::vector<SuccessionPersonalLot> lots;FieldOrder field_order;
};
struct FamilyBequestDeclarationSource {
    StableId scope="named_personal_assets_after_death_v1",scenario_id;WorldId world_id{};
    Integer tick=0,revision=0,donor_birth_tick=0,beneficiary_birth_tick=0;
    FamilyBequestTerms terms;FieldOrder field_order;
    std::optional<FamilyDecisionAgeSources4> age_sources;
};
struct FamilyBequestDeclarationQuote {
    WorldIdentity identity;FamilyBequestDeclarationSource source;SuccessionPersonalSelection selected;
    Integer source_spendable=0,source_balance=0,source_inventory_revision=0;
};
// 只核保存条款及原关系；不要求未来批次仍在，也不根据一份可解码JSON发放财物。
Status validate_family_bequest_source(const CommonStore&,const FamilyState&,const FamilyBequestDeclarationSource&,Integer selected_version=1,const FamilyContinuationPeopleRead4* people=nullptr,const StableId& actual_before_fact={});
// 明确只读本人现有私产，无预留/资源操作，也不受接收者此刻容量限制。
Outcome<FamilyBequestDeclarationQuote> preview_family_bequest_declaration(const CommonStore&,const FamilyState&,const FamilyBequestPolicy&,const StableId& donor,const StableId& beneficiary,const SuccessionPersonalSelection&,const FamilyContinuationPeopleRead4* people=nullptr);
Status revalidate_family_bequest_declaration(const CommonStore&,const FamilyState&,const FamilyBequestPolicy&,const FamilyBequestDeclarationQuote&,const FamilyContinuationPeopleRead4* people=nullptr);
bool family_bequest_source_equal(const FamilyBequestDeclarationSource&,const FamilyBequestDeclarationSource&);
}
