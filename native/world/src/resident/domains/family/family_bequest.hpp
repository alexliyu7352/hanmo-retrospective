// 独立family_bequest/1只保存声明和撤回；死亡通知、接受、清算各自另有来源。
#pragma once
#include "family_bequest_source.hpp"
#include "family_death.hpp"
#include "family_life_history4.hpp"
namespace hanmo::resident {
struct FamilyBequestDeclaration { StableId kind="family_bequest_declaration";FamilyBequestDeclarationSource source;FieldOrder field_order; };
struct FamilyBequestRevocation { StableId kind="family_bequest_revocation",declaration_id,donor_actor_id;Integer tick=0;FieldOrder field_order; };
struct FamilyBequestState { OrderedTable<FamilyBequestDeclaration> declarations;OrderedTable<FamilyBequestRevocation> revocations;FieldOrder field_order; };
// 只读已经验证的旧Family+legacy控制链，不捕获本账副本或把当前玩家倒填历史。
using FamilyBequestPlayerRead=std::function<Outcome<StableId>(Integer tick,const StableId& before_fact_id)>;
struct FamilyBequestRevocationQuote { WorldIdentity identity;StableId declaration_id,donor_actor_id; };
Status validate_family_bequests(const CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyBequestPolicy&,const FamilyBequestState&,const FamilyBequestPlayerRead&,const FamilyLifeHistoryRead4* life4=nullptr,const FamilyContinuationPeopleRead4* people=nullptr);
// 声明唯一且幂等；所有钱货/预留保持原样，末端来源失败完整退回Common和本账。
Outcome<StableId> record_family_bequest(CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyBequestPolicy&,FamilyBequestState&,const FamilyBequestPlayerRead&,const FamilyBequestDeclarationQuote&,const FamilyLifeHistoryRead4* life4=nullptr,const FamilyContinuationPeopleRead4* people=nullptr);
Outcome<FamilyBequestRevocationQuote> preview_family_bequest_revocation(const CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyBequestPolicy&,const FamilyBequestState&,const FamilyBequestPlayerRead&,const StableId& declaration_id,const FamilyLifeHistoryRead4* life4=nullptr,const FamilyContinuationPeopleRead4* people=nullptr);
Outcome<StableId> revoke_family_bequest(CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyBequestPolicy&,FamilyBequestState&,const FamilyBequestPlayerRead&,const FamilyBequestRevocationQuote&,const FamilyLifeHistoryRead4* life4=nullptr,const FamilyContinuationPeopleRead4* people=nullptr);
// 只返回真实未撤回的原权源；不能直接当作受领同意或资源recipe。
const FamilyBequestDeclaration* active_family_bequest(const FamilyBequestState&,const StableId& donor,StableId* declaration_id=nullptr);
Outcome<FamilyBequestState> decode_family_bequests(const BoundaryValue&);
BoundaryValue encode_family_bequests(const FamilyBequestState&);
Status visit_family_bequests_checkpoint(CodecVisitor&,FamilyBequestState&);
Outcome<FamilyBequestState> decode_family_bequests_v2(const BoundaryValue&);
BoundaryValue encode_family_bequests_v2(const FamilyBequestState&);
Status visit_family_bequests_checkpoint_v2(CodecVisitor&,FamilyBequestState&);
}
