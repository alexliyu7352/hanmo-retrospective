// 显式情景能力与旧收据边界的纯组件门；不替代新World注册或历史银行消费。
#include "resident/domains/interaction/party_invitation_profile.hpp"
#include "resident/domains/interaction/party_invitation.hpp"
#include "resident/storage/common_codec.hpp"
#include "hanmo/resident/command_contract.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int count=0;
// 任何构建配置都实际断言。
void check(bool ok,const char* why){++count;if(!ok)throw std::runtime_error(why);}}
int main(){try{
    using V=BoundaryValue;const std::vector<StableId> caps{"native_dynamic_world","multi_actor","dialogue","companions"};auto old=boundary::obj({{"id",V{StableId{"old"}}}});auto a=decode_party_invitation_profile(old,caps);check(a.ok&&!a.value,"installed union does not enable old scene");
    auto selected=boundary::obj({{"party_invitation_profile",V{StableId{"authored_node_v1"}}},{"dynamic_world",boundary::obj({{"profile",V{StableId{"local_society_v1"}}}})}});selected.as<V::Dict>()->emplace_back("party_invitations",boundary::obj({{"version",V{Integer{1}}},{"invitations",V{V::Array{}}}}));const auto before=selected;
    a=decode_party_invitation_profile(selected,caps);check(a.ok&&a.value&&compare_boundary(before,selected).exact(),"selected explicit profile pure enabled");check(!decode_party_invitation_profile(selected,{}).ok,"missing package capability");for(const auto& missing:caps){auto reduced=caps;reduced.erase(std::remove(reduced.begin(),reduced.end(),missing),reduced.end());check(!decode_party_invitation_profile(selected,reduced).ok,"every existing capability dependency required");}check(!decode_party_invitation_profile(selected,{"party_invitations_v1"}).ok,"obsolete redundant name alone cannot grant native party semantics");
    auto bad=selected;*bad.find("party_invitation_profile")=V{StableId{"implicit_follow_v1"}};check(!decode_party_invitation_profile(bad,caps).ok,"unknown profile");bad=selected;*bad.find("dynamic_world")->find("profile")=V{StableId{"legacy"}};check(!decode_party_invitation_profile(bad,caps).ok,"native scene required");bad=selected;*bad.find("party_invitation_profile")=V{Integer{1}};check(!decode_party_invitation_profile(bad,caps).ok,"typed profile required");check(!decode_party_invitation_profile(V{Integer{1}},caps).ok,"typed scene required");
    auto no_block=selected;no_block.as<V::Dict>()->pop_back();check(!decode_party_invitation_profile(no_block,caps).ok,"enabled profile needs own authored block");auto unexpected=old;unexpected.as<V::Dict>()->emplace_back("party_invitations",*selected.find("party_invitations"));check(!decode_party_invitation_profile(unexpected,caps).ok,"old scenario cannot inherit authored block");bad=selected;*bad.find("party_invitations")->find("version")=V{Integer{2}};check(!decode_party_invitation_profile(bad,caps).ok,"author version explicit");
    PartyInvitationCatalog catalog;std::vector<std::pair<StableId,StableId>> modules{{"common","5"},{"party_invitations","1"}};
    check(validate_party_invitation_layout(false,{{"common","5"}},nullptr).ok,"old layout unchanged");check(validate_party_invitation_layout(true,modules,&catalog).ok,"selected catalog and marker agree");check(!validate_party_invitation_layout(true,{},&catalog).ok,"missing marker");check(!validate_party_invitation_layout(true,modules,nullptr).ok,"missing compiled catalog");check(!validate_party_invitation_layout(false,modules,nullptr).ok,"old profile cannot carry marker");check(!validate_party_invitation_layout(false,{},&catalog).ok,"old profile cannot carry directory");auto duplicate=modules;duplicate.push_back(modules.back());check(!validate_party_invitation_layout(true,duplicate,&catalog).ok,"duplicate marker");modules.back().second="2";check(!validate_party_invitation_layout(true,modules,&catalog).ok,"unknown module version");
    CommonState state;auto shared=std::make_shared<CommonCatalog>();CommonStore common(std::move(state),shared);check(validate_party_invitation_history(common,false).ok,"empty old history");
    auto receipt=std::make_shared<CommitResult>();receipt->kind="join_party";common.state().results.insert("old",receipt);check(validate_party_invitation_history(common,false).ok,"original recruitment stays legal");
    for(const auto* kind:{"party_accept","party_leave"}){
        common.state().results={};common.state().history.clear();receipt=std::make_shared<CommitResult>();receipt->kind=kind;common.state().results.insert("new",receipt);auto snapshot=encode_common_state(common.state());
        check(!validate_party_invitation_history(common,false).ok,"cached new receipt requires profile");check(validate_party_invitation_history(common,true).ok,"enabled receipt accepted by gate");check(compare_boundary(snapshot,encode_common_state(common.state())).exact(),"history check pure");
        common.state().results={};common.state().history.push_back({0,kind,"actor","result",{}});check(!validate_party_invitation_history(common,false).ok,"history-only new receipt requires profile");
    }
    std::cout<<"party invitation profile checks="<<count<<" failures=0; pure gate only\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
