// 整编共享边界只验本轮新增的钱货原语与回执冷通路，不重跑旧资产矩阵。
#include "resident/kernel/siege_integration.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/organization/organization_validation.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
// 原批次整体转移保留身份和元数据；晚拒绝与savepoint都能恢复原资源版本。
static void lots(){
 auto catalog=std::make_shared<CommonCatalog>();catalog->organization_ids={"old","new"};catalog->items.insert("grain",{"grain",1,1});CommonState seed;seed.next_instance_id=30;Owner old{Owner::Kind::organization,"old",{}},next{Owner::Kind::organization,"new",{}};
 LotView lot{"lot:4","grain",{},3,old,{}};lot.field_order={"quantity","id","item_id","owner","reserved_by"};lot.owner.field_order={"id","kind"};seed.inventories.insert("source",{"source",old,20,4,{lot}});seed.inventories.insert("target",{"target",next,20,9,{}});seed.wallets.insert("wallet",{"wallet",old,10,{}});CommonStore common(seed,catalog);
 auto request=MoveIntegrationLot{"source","target","lot:4",3,old,next};auto version=common.read_version({ResourceKey::Kind::inventory,"source"});auto mark=common.begin_savepoint();assert(mark.ok);assert(common.apply_resources({{request},{version}}).ok);const auto& moved=common.inventory("target")->lots.front();assert(moved.id==lot.id&&moved.item_id==lot.item_id&&moved.quantity==3&&moved.field_order==lot.field_order&&moved.owner.field_order==lot.owner.field_order&&moved.owner.id=="new"&&common.inventory("source")->lots.empty()&&common.state().next_instance_id==30);assert(common.rollback_savepoint(mark.value).ok&&common.inventory("target")->lots.empty()&&common.read_version({ResourceKey::Kind::inventory,"source"}).version==version.version);
 assert(!common.apply_resources({{request,WalletDelta{"absent",-1,true}}, {}}).ok&&common.inventory("source")->lots.size()==1&&common.inventory("target")->lots.empty());request.expected_quantity=2;assert(!common.apply_resources({{request}, {}}).ok);request.expected_quantity=3;common.state().inventories.find("source")->lots[0].reserved_by="action:1";assert(!common.apply_resources({{request},{}}).ok);common.state().inventories.find("source")->lots[0].reserved_by.clear();common.state().inventories.find("target")->capacity=2;assert(!common.apply_resources({{request},{}}).ok&&common.inventory("target")->lots.empty());
}
// JSON和二进制同守命令形状；缺明细、错variant和额外同意布尔均拒绝。
static void receipt(){using V=BoundaryValue;auto valid=boundary::obj({{"proposal_id",V{StableId("proposal:1")}},{"proposal_key",V{StableId("key")}},{"decision",V{StableId("accept")}}});assert(decode_resident_command("siege_integration_decide",valid).ok);valid.as<V::Dict>()->emplace_back("consent",V{true});assert(!decode_resident_command("siege_integration_decide",valid).ok);
 SiegeIntegrationEffect effect;effect.proposal_id="proposal:1";effect.proposal_key="key";effect.actor_action_source_id="action:2";effect.decision_source_id="decision:3";effect.status="pending";auto value=std::make_shared<CommitResult>();value->ok=true;value->kind="siege_integration_decide";value->result_id="result:4";value->revision=1;value->details=std::make_shared<ReceiptDetails>(ReceiptDetails{effect,{}});std::shared_ptr<const CommitResult> source=value;
 auto json=encode_resident_receipt({true,source,{}});assert(json.ok&&decode_resident_receipt(json.value).ok);CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.schema_features=common_checkpoint::typed_resource_references;assert(visit_resident_receipt(writer,source).ok&&writer.finish().ok);std::shared_ptr<const CommitResult> restored;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));reader.schema_features=common_checkpoint::typed_resource_references;assert(visit_resident_receipt(reader,restored).ok&&reader.finish().ok);assert(encode_resident_receipt({true,restored,{}}).ok);
 *json.value.find("details")=V{V::Dict{}};assert(!decode_resident_receipt(json.value).ok);value->details.reset();source=value;CodecVisitor::Buffer bad;CodecVisitor reject(bad);reject.schema_features=common_checkpoint::typed_resource_references;assert(!visit_resident_receipt(reject,source).ok);
}
// 历史读口只允许保留精确资源引用，旧grant在真实当前产权变化后仍不能取得使用权。
static void resource_history(){
 auto catalog=std::make_shared<CommonCatalog>();catalog->organization_ids={"old","new"};CommonState seed;
 Owner old{Owner::Kind::organization,"old",{}},next{Owner::Kind::organization,"new",{}};
 ActorRecord actor;actor.id="officer";actor.alive=true;actor.available=true;seed.actors.insert(actor.id,actor);
 seed.wallets.insert("treasury",{"treasury",old,10,{}});seed.inventories.insert("public",{"public",old,20,0,{}});CommonStore common(seed,catalog);
 OrganizationCatalog organization;organization.enabled=true;OrganizationRecord org;org.id="old";org.kind="city";org.headquarters_site="camp";org.office_ids={"office"};organization.organizations.insert(org.id,org);
 OfficeState office;office.id="office";office.organization_id="old";office.holder_actor_id="officer";office.grant_ids={"grant"};organization.offices.insert(office.id,office);
 Grant grant;grant.id="grant";grant.office_id="office";grant.allowed_command_kinds={"mobilize_reserve"};grant.scopes={{ScopeRef::Kind::site,"camp"}};grant.wallet_ids={"treasury"};grant.inventory_ids={"public"};grant.amount_limit=4;grant.continuity_policy_id="continue_under_organization";organization.grants.insert(grant.id,grant);
 auto initialized=make_initial_organization(common,organization);assert(initialized.ok);const auto state=initialized.value;LocalEconomyState economy;
 ActingAuthority authority{"old","office",0,"officer"};auto authorize=[&](){return check_authority(common,state,organization,authority,"mobilize_reserve",{ScopeRef::Kind::site,"camp"},1);};
 assert(validate_organization(common,state,organization,economy).ok&&authorize().ok);
 assert(common.apply_resources({{SetInventoryOwner{"public",next}},{}}).ok);
 auto strict=validate_organization(common,state,organization,economy);assert(!strict.ok&&strict.error.original.code=="AuthorityResourceMismatch");
 OrganizationResourceHistoryRead history=[](const ResourceKey& resource,const StableId& owner){return owner=="old"&&((resource.kind==ResourceKey::Kind::inventory&&resource.id=="public")||(resource.kind==ResourceKey::Kind::wallet&&resource.id=="treasury"));};
 assert(validate_organization(common,state,organization,economy,nullptr,nullptr,history).ok);
 auto denied=authorize();assert(!denied.ok&&denied.error.original.code=="AuthorityResourceMismatch"&&denied.error.original.path=="public");
 assert(common.apply_resources({{SetWalletOwner{"treasury",next}},{}}).ok);
 assert(validate_organization(common,state,organization,economy,nullptr,nullptr,history).ok);denied=authorize();assert(!denied.ok&&denied.error.original.path=="treasury");
 OrganizationResourceHistoryRead absent=[](const ResourceKey&,const StableId&){return false;};assert(!validate_organization(common,state,organization,economy,nullptr,nullptr,absent).ok);
 common.state().inventories.erase("public");assert(!validate_organization(common,state,organization,economy,nullptr,nullptr,history).ok);
 assert(organization.grants.find("grant")->inventory_ids==std::vector<StableId>{"public"}&&state.offices.find("office")->organization_id=="old");
}
// 只重跑新历史授权边界时保留显式入口，不连带重复已通过的钱货/回执组。
int main(int argc,char** argv){if(argc==2&&std::string(argv[1])=="--resource-history"){resource_history();std::cout<<"siege resource history PASS\n";return 0;}lots();receipt();resource_history();std::cout<<"siege integration boundary PASS\n";}
