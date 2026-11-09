// 消费三个自然检查点；仅实际递交且仍在当前会面的提议显示等候。
#define DIPLOMACY_BOUNDARY_HELPERS_ONLY
#include "diplomacy_boundary_test.cpp"
#undef DIPLOMACY_BOUNDARY_HELPERS_ONLY
namespace {
// 在独立会话恢复原检查点，并先确认字节不变。
std::unique_ptr<ResidentReplaySession> restored(const V& catalog,const char* path){
    std::ifstream input(path,std::ios::binary);check(bool(input),"Open natural checkpoint");Bytes bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    auto session=create_resident_replay_session();need(session->initialize_checkpoint(catalog,bytes),"Restore natural checkpoint");check(bytes==need(session->checkpoint(),"Restored checkpoint bytes"),"Restore changed source bytes");return session;
}
// 原只读页面至多给出一个等候动作；读取不得改写检查点。
V reply_wait(ResidentReplaySession& session){
    const auto before=need(session.checkpoint(),"Before limited wait projection");const auto page=diplomatic_page(session);V found=boundary::obj({});Integer count=0;
    for(const auto& row:rows(page))for(const auto& a:boundary::array(field(row,"actions"),"actions"))if(text(a,"command")=="advance"&&text(a,"text")=="等候答复半日"){++count;found=a;}
    check(count<=1,"Duplicate wait control for one actual proposal");check(before==need(session.checkpoint(),"After limited wait projection"),"Wait projection mutated the world");return found;
}
}
// 验证未递交、已递交、离开会面和已答复四类真实来源。
int main(int argc,char** argv){try{
    check(argc==6,"CATALOG ACTIVE_BANK DELIVERED_BANK REPLIED_BANK RESULT");const auto catalog=read(argv[1]);
    auto active=restored(catalog,argv[2]);check(!reply_wait(*active).find("command"),"Active undelivered action exposed reply wait");
    auto delivered=restored(catalog,argv[3]);const auto wait=reply_wait(*delivered);check(wait.find("command"),"Actual delivered pending meeting has no lawful wait control");readonly_quote(*delivered,wait);
    const auto tick=need(delivered->identity(),"Pending tick").tick;submit_action(*delivered,wait);check(need(delivered->identity(),"After explicit wait").tick==tick+1,"Wait did not use exactly one original half-day");const auto after=snapshot(*delivered);check(after.diplomacy.replies.size()==1&&after.diplomacy.offers.rows().front().second.status==DiplomaticOfferStatus::accepted_pending_ratification,"Explicit wait did not retain actual pending-ratification result");check(!reply_wait(*delivered).find("command"),"Completed reply still offers pending wait");
    auto away=restored(catalog,argv[3]);submit(*away,"end_interaction",boundary::obj({}));check(!reply_wait(*away).find("command"),"Ending the actual meeting retained its wait control");
    auto replied=restored(catalog,argv[4]);check(!reply_wait(*replied).find("command"),"Historical reply exposes a fresh pending wait");save(argv[5],true);std::cout<<"DIPLOMATIC_REPLY_WAIT "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& error){if(argc==6)try{save(argv[5],false,error.what());}catch(...){}std::cerr<<"DIPLOMATIC_REPLY_WAIT FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
