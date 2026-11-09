// Linux x86_64 测试专用 PC 采样；不解栈、不修改业务 job、状态或验证。
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "field_pc_sampler.hpp"
#include <atomic>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <type_traits>
#include <ucontext.h>
#include <unistd.h>
#if !defined(__linux__) || !defined(__x86_64__)
#error "field_pc_sampler requires the explicitly reviewed Linux x86_64 ucontext ABI"
#endif
namespace hanmo::resident::field_pc_sampler {
namespace {
constexpr unsigned capacity=65536, max_windows=9, map_capacity=65536;
static_assert(std::is_same_v<pthread_t,unsigned long>);
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<unsigned long>::is_always_lock_free);
static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
struct Record {
  std::atomic<std::uintptr_t> pc{0};
  std::atomic<unsigned long> thread{0};
  std::atomic<unsigned> window{0}, committed{0};
};
// 静态拥有期覆盖最后一个迟到 handler；正常路径永不在运行期重用记录槽。
Record records[capacity];
std::atomic<unsigned> enabled{0}, active_window{0}, inflight{0}, next_record{0};
std::atomic<unsigned> owner_signals{0}, non_owner_signals{0}, outside_window{0};
std::atomic<unsigned> overflow{0}, invalid_context{0}, foreign_signals{0}, drained{0};
std::atomic<unsigned long> owner_thread{0};
struct Window {
  std::uint64_t epoch=0, call=0;
  std::int64_t target=0;
  unsigned kind=0, depth=0;
  bool closed=false;
  char before_maps[map_capacity]{}, after_maps[map_capacity]{};
  std::size_t before_size=0, after_size=0;
};
Window windows[max_windows];
unsigned window_count=0, interval=0;
bool installed=false, ever_started=false, stopped=false;
const char* failure=nullptr;
struct sigaction original_action{};
struct itimerval original_timer{};
sigset_t original_mask{};
timespec cpu_resolution{};
struct PreserveErrno {int value=errno;~PreserveErrno(){errno=value;}};
void fail(const char* reason) noexcept {if(!failure)failure=reason;}
void handler(int signal_number,siginfo_t* info,void* context) noexcept {
  // 专用诊断进程停用后保留此静态 handler；迟到信号只读原子开关即返。
  if(!enabled.load(std::memory_order_seq_cst))return;
  inflight.fetch_add(1,std::memory_order_seq_cst);
  if(!enabled.load(std::memory_order_seq_cst)){
    inflight.fetch_sub(1,std::memory_order_seq_cst);return;
  }
  const int saved=errno;
  const auto thread=::pthread_self();
  // ITIMER_PROF 是全进程 CPU 信号。非 owner 投递只计丢样，不偷绑主线程窗口。
  if(signal_number!=SIGPROF || !info || info->si_code!=SI_KERNEL) {
    foreign_signals.fetch_add(1,std::memory_order_relaxed);
  } else if(thread!=owner_thread.load(std::memory_order_relaxed)) {
    non_owner_signals.fetch_add(1,std::memory_order_relaxed);
  } else if(!enabled.load(std::memory_order_seq_cst)) {
    outside_window.fetch_add(1,std::memory_order_relaxed);
  } else {
    const auto window=active_window.load(std::memory_order_acquire);
    if(!window)outside_window.fetch_add(1,std::memory_order_relaxed);
    else {
      owner_signals.fetch_add(1,std::memory_order_relaxed);
      if(!context)invalid_context.fetch_add(1,std::memory_order_relaxed);
      else {
        const auto slot=next_record.fetch_add(1,std::memory_order_relaxed);
        if(slot>=capacity)overflow.fetch_add(1,std::memory_order_relaxed);
        else {
          const auto* interrupted=static_cast<const ucontext_t*>(context);
          const auto pc=static_cast<std::uintptr_t>(interrupted->uc_mcontext.gregs[REG_RIP]);
          records[slot].pc.store(pc,std::memory_order_relaxed);
          records[slot].thread.store(thread,std::memory_order_relaxed);
          records[slot].window.store(window,std::memory_order_relaxed);
          records[slot].committed.store(1,std::memory_order_release);
        }
      }
    }
  }
  inflight.fetch_sub(1,std::memory_order_seq_cst);
  errno=saved;
}
bool block(sigset_t& previous) noexcept {
  sigset_t set;::sigemptyset(&set);::sigaddset(&set,SIGPROF);
  if(::pthread_sigmask(SIG_BLOCK,&set,&previous)!=0){fail("block_sigprof_failed");return false;}
  return true;
}
bool mask(const sigset_t& value) noexcept {
  if(::pthread_sigmask(SIG_SETMASK,&value,nullptr)!=0){fail("restore_mask_failed");return false;}
  return true;
}
bool quiet() noexcept {
  enabled.store(0,std::memory_order_seq_cst);active_window.store(0,std::memory_order_release);
  const itimerval zero{};
  if(::setitimer(ITIMER_PROF,&zero,nullptr)!=0){fail("disarm_failed");return false;}
  // 只有正常上下文等待；handler 不等锁、不旋转。迟到非 owner 不写任何槽。
  unsigned attempts=0;
  while(inflight.load(std::memory_order_seq_cst)!=0 && attempts++<10000)::sched_yield();
  if(inflight.load(std::memory_order_seq_cst)!=0){fail("inflight_not_quiet");return false;}
  sigset_t set;::sigemptyset(&set);::sigaddset(&set,SIGPROF);const timespec zero_wait{};
  for(unsigned n=0;n<1024;++n){
    siginfo_t info{};const auto result=::sigtimedwait(&set,&info,&zero_wait);
    if(result==SIGPROF){
      if(info.si_code!=SI_KERNEL){foreign_signals.fetch_add(1,std::memory_order_relaxed);fail("foreign_sigprof_pending");}
      else drained.fetch_add(1,std::memory_order_relaxed);
      continue;
    }
    if(result<0&&errno==EAGAIN)return true;
    if(result<0&&errno==EINTR)continue;
    fail("pending_drain_failed");return false;
  }
  fail("pending_drain_limit");return false;
}
bool maps(char* output,std::size_t& used) noexcept {
  used=0;const auto fd=::open("/proc/self/maps",O_RDONLY|O_CLOEXEC);
  if(fd<0){fail("maps_open_failed");return false;}
  bool good=true;
  while(used<map_capacity){const auto n=::read(fd,output+used,map_capacity-used);if(n>0)used+=static_cast<std::size_t>(n);else if(n==0)break;else if(errno!=EINTR){good=false;break;}}
  if(used==map_capacity){char extra;const auto n=::read(fd,&extra,1);if(n!=0)good=false;}
  if(::close(fd)!=0)good=false;
  if(!good)fail("maps_incomplete");return good;
}
bool write_all(int fd,const char* data,std::size_t size) noexcept {
  while(size){const auto n=::write(fd,data,size);if(n>0){data+=n;size-=static_cast<std::size_t>(n);}else if(n<0&&errno==EINTR)continue;else {fail("report_write_failed");return false;}}
  return true;
}
template<class... Args>bool line(int fd,const char* format,Args... args) noexcept {
  char buffer[1024];const int n=::snprintf(buffer,sizeof(buffer),format,args...);
  if(n<0||static_cast<std::size_t>(n)>=sizeof(buffer)){fail("report_line_overflow");return false;}
  return write_all(fd,buffer,static_cast<std::size_t>(n));
}
bool map_lines(int fd,unsigned window,const char* phase,const char* text,std::size_t size) noexcept {
  std::size_t at=0;
  while(at<size){std::size_t end=at;while(end<size&&text[end]!='\n')++end;
    if(!line(fd,"FIELD_PC_SAMPLE MAP window=%u phase=%s raw=",window,phase)||!write_all(fd,text+at,end-at)||!write_all(fd,"\n",1))return false;
    at=end<size?end+1:end;
  }
  return true;
}
}
const char* error() noexcept {return failure?failure:"none";}
bool start(unsigned requested) noexcept {
  PreserveErrno preserve;
  if(ever_started||installed){fail("sampler_already_started");return false;}
  if(requested<1000||requested>5000){fail("invalid_interval_us");return false;}
  if(!block(original_mask))return false;
  if(::sigismember(&original_mask,SIGPROF)==1){fail("sigprof_previously_blocked");mask(original_mask);return false;}
  sigset_t pending;
  if(::sigpending(&pending)!=0||::sigismember(&pending,SIGPROF)==1){fail("sigprof_preexisting_pending");mask(original_mask);return false;}
  if(::getitimer(ITIMER_PROF,&original_timer)!=0||::sigaction(SIGPROF,nullptr,&original_action)!=0){fail("query_original_signal_state_failed");mask(original_mask);return false;}
  if(original_timer.it_value.tv_sec||original_timer.it_value.tv_usec||original_timer.it_interval.tv_sec||original_timer.it_interval.tv_usec){fail("existing_prof_timer");mask(original_mask);return false;}
  if((original_action.sa_flags&SA_SIGINFO)||(original_action.sa_handler!=SIG_DFL&&original_action.sa_handler!=SIG_IGN)){fail("existing_prof_handler");mask(original_mask);return false;}
  owner_thread.store(::pthread_self(),std::memory_order_relaxed);interval=requested;
  if(::clock_getres(CLOCK_PROCESS_CPUTIME_ID,&cpu_resolution)!=0){fail("cpu_clock_resolution_unavailable");mask(original_mask);return false;}
  struct sigaction action{};action.sa_sigaction=handler;action.sa_flags=SA_SIGINFO|SA_RESTART;::sigemptyset(&action.sa_mask);::sigaddset(&action.sa_mask,SIGPROF);
  if(::sigaction(SIGPROF,&action,nullptr)!=0){fail("install_handler_failed");mask(original_mask);return false;}
  installed=ever_started=true;
  if(!mask(original_mask)){stop();return false;}
  return true;
}
bool begin_window(std::uint64_t epoch,std::int64_t target,unsigned kind,std::uint64_t call,std::uint32_t depth) noexcept {
  PreserveErrno preserve;
  if(!installed||stopped||failure||::pthread_self()!=owner_thread.load()||window_count>=max_windows||kind>=3||target<1||target>3||!epoch||!call||active_window.load()!=0){fail("invalid_window_start");return false;}
  sigset_t previous;if(!block(previous))return false;
  if(!quiet()||failure){mask(previous);return false;}
  auto& w=windows[window_count];w.epoch=epoch;w.target=target;w.kind=kind;w.call=call;w.depth=depth;
  if(!maps(w.before_maps,w.before_size)){mask(previous);return false;}
  ++window_count;active_window.store(window_count,std::memory_order_release);enabled.store(1,std::memory_order_seq_cst);
  itimerval timer{};timer.it_value.tv_usec=interval;timer.it_interval.tv_usec=interval;
  if(::setitimer(ITIMER_PROF,&timer,nullptr)!=0){fail("arm_failed");quiet();mask(previous);return false;}
  return mask(previous);
}
void end_window() noexcept {
  PreserveErrno preserve;
  if(!installed||stopped)return;
  if(::pthread_self()!=owner_thread.load()){fail("non_owner_window_end");return;}
  const auto id=active_window.load(std::memory_order_acquire);
  if(!id)return;
  sigset_t previous;if(!block(previous))return;
  if(quiet()){auto& w=windows[id-1];maps(w.after_maps,w.after_size);w.closed=true;}
  mask(previous);
}
bool stop() noexcept {
  PreserveErrno preserve;
  if(stopped)return failure==nullptr;
  if(!installed)return !failure;
  if(::pthread_self()!=owner_thread.load()){fail("non_owner_stop");return false;}
  end_window();sigset_t previous;if(!block(previous))return false;
  if(!quiet()){mask(previous);return false;}
  // 专用诊断进程一次取得空闲 SIGPROF，直到退出保留 disabled 静态 handler。
  // 不把 owner drain 宣称为全线程排空，也不恢复 SIG_DFL 暴露迟到终止竞态。
  // 只恢复原空 timer 与原 owner mask；普通 mode 从未安装本 handler。
  if(::setitimer(ITIMER_PROF,&original_timer,nullptr)!=0)fail("restore_timer_failed");
  stopped=true;mask(original_mask);
  if(overflow.load()||invalid_context.load()||foreign_signals.load())fail("sample_loss_or_conflict");
  return failure==nullptr;
}
bool write_report(int fd) noexcept {
  PreserveErrno preserve;
  if(!stopped||!installed||enabled.load()){fail("report_requires_stopped_sampler");return false;}
  const auto reserved=next_record.load(std::memory_order_acquire);const auto count=reserved<capacity?reserved:capacity;
  if(!line(fd,"FIELD_PC_SAMPLE META version=1 mode=INTERRUPTED_PC_SELF_ONLY inclusive=NOT_OBSERVED interval_us=%u cpu_clock_resolution_ns=%lld timer_resolution=NOT_MEASURED capacity=%u windows=%u owner_pthread=%lu source=Linux_x86_64_ucontext_REG_RIP thread_scope=OWNER_DELIVERY_ONLY signal_clock=PROCESS_CPU action=RETAINED_DISABLED_UNTIL_PROCESS_EXIT late_non_owner=EXCLUDED_NO_WINDOW\n",interval,static_cast<long long>(cpu_resolution.tv_sec)*1000000000LL+cpu_resolution.tv_nsec,capacity,window_count,owner_thread.load()))return false;
  for(unsigned i=0;i<window_count;++i){const auto& w=windows[i];if(!w.closed)fail("window_not_closed");
    if(!line(fd,"FIELD_PC_SAMPLE WINDOW token=%u epoch=%" PRIu64 " target=%" PRId64 " kind=%u production_call=%" PRIu64 " depth=%u closed=%u\n",i+1,w.epoch,w.target,w.kind,w.call,w.depth,unsigned(w.closed))||!map_lines(fd,i+1,"before",w.before_maps,w.before_size)||!map_lines(fd,i+1,"after",w.after_maps,w.after_size))return false;
  }
  unsigned committed=0;
  for(unsigned i=0;i<count;++i){const auto& row=records[i];if(!row.committed.load(std::memory_order_acquire)){fail("record_not_committed");continue;}++committed;
    if(!line(fd,"FIELD_PC_SAMPLE PC seq=%u window=%u pthread=%lu pc=0x%" PRIxPTR "\n",i,row.window.load(),row.thread.load(),row.pc.load()))return false;
  }
  if(!line(fd,"FIELD_PC_SAMPLE COUNTS owner_deliveries=%u committed=%u non_owner_dropped=%u outside_window=%u capacity_dropped=%u invalid_context=%u foreign_sigprof=%u pending_drained=%u timer_coalescing_loss=NOT_MEASURED\n",owner_signals.load(),committed,non_owner_signals.load(),outside_window.load(),overflow.load(),invalid_context.load(),foreign_signals.load(),drained.load()))return false;
  if(!committed)fail("zero_observed_pc_samples");
  return line(fd,"FIELD_PC_SAMPLE REPORT status=%s error=%s self=OBSERVED_PC_ONLY inclusive=NOT_OBSERVED formal_gate=false\n",failure?"INCOMPLETE":"COMPLETE",error())&&!failure;
}
}
