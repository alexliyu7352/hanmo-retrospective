// 仅协议替身测试单worker/旧引用/阅读/取消，不把它称作真实规则或Bridge性能证据。
#include "resident_bridge_queue.hpp"
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
using namespace hanmo::resident;
// 可控执行门让测试证明poll/取消不join当前事务，而非测一个偶然很快的空函数。
struct Gate { std::mutex mutex; std::condition_variable wake; bool entered=false,released=false; int advances=0,cancels=0; std::thread::id writer; };
struct ProtocolExecutor final:ResidentRunExecutor {
    explicit ProtocolExecutor(std::shared_ptr<Gate> g):gate(std::move(g)){}
    std::shared_ptr<Gate> gate; RunProgress state;
    Outcome<RunProgress> begin(const BeginRun& request) override {gate->writer=std::this_thread::get_id(); state.identity=request.expected; state.run_id=request.run_id; return {true,state,{}};}
    Outcome<RunProgress> resume(const ResumeRun& request) override {
        assert(gate->writer==std::this_thread::get_id());
        std::unique_lock<std::mutex> lock(gate->mutex); gate->entered=true; gate->wake.notify_all(); gate->wake.wait(lock,[&]{return gate->released;});
        ++gate->advances; ++state.committed_half_days; ++state.identity.revision; state.permit_seq=request.permit_seq; return {true,state,{}};
    }
    Outcome<RunProgress> cancel(const RunRef&) override {assert(gate->writer==std::this_thread::get_id()); ++gate->cancels; state.stop=RunStop::cancelled; return {true,state,{}};}
};
// 等待协议静止只用于测试；产品主线程使用非阻塞poll。
ResidentBridgeProgress settled(ResidentBridgeQueue& queue,const RunRef& ref) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(std::chrono::steady_clock::now()<deadline) {auto p=queue.poll(ref); assert(p.ok); if(!p.value.running&&!p.value.pending) return p.value; std::this_thread::yield();}
    assert(false&&"protocol worker did not settle"); return {};
}
int main() {
    auto gate=std::make_shared<Gate>(); ResidentBridgeQueue queue(std::make_unique<ProtocolExecutor>(gate));
    WorldIdentity identity; identity.world_id[0]=1; identity.execution_epoch=2; RunRef ref{identity.world_id,2,"run:1"};
    assert(queue.begin({identity,"run:1","actor.player","action:1",4}).ok); settled(queue,ref); assert(gate->advances==0);
    auto stale=ref; stale.execution_epoch=1; assert(!queue.poll(stale).ok&&!queue.cancel(stale).ok);
    auto other=ref; other.world_id[0]=2; assert(!queue.poll(other).ok);
    assert(!queue.grant({identity,"run:1",1,2}).ok);
    assert(queue.grant({identity,"run:1",1,1}).ok);
    {std::unique_lock<std::mutex> lock(gate->mutex); assert(gate->wake.wait_for(lock,std::chrono::seconds(3),[&]{return gate->entered;}));}
    auto quick=std::async(std::launch::async,[&]{auto snapshot=queue.poll(ref); assert(snapshot.ok&&snapshot.value.running); return queue.pause_for_reading(ref);});
    assert(quick.wait_for(std::chrono::seconds(1))==std::future_status::ready&&quick.get().ok);
    assert(!queue.grant({identity,"run:1",2,1}).ok);
    assert(queue.cancel(ref).ok);
    {std::lock_guard<std::mutex> lock(gate->mutex); gate->released=true; gate->wake.notify_all();}
    const auto last=settled(queue,ref); assert(gate->advances==1&&gate->cancels==1&&last.cached.stop==RunStop::cancelled);
    assert(last.reading_paused); assert(queue.resume_after_reading(ref).ok); assert(gate->advances==1);
    queue.close(); assert(!queue.poll(ref).ok);
    std::cout<<"resident bridge protocol checks passed; not gameplay/performance evidence\n";
}
