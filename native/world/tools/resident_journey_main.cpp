// 常驻P1的独立CLI边界：同一core重放、全态对照与分项计时，不含另一套规则引擎。
#include "resident/checkpoint/ordered_value.hpp"
#ifdef HANMO_RESIDENT_CORE_LINKED
#include "resident/kernel/boundary_dispatch.hpp"
#endif
#include <picosha2.h>
#include <algorithm>
#include <chrono>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <sys/resource.h>
#ifdef __linux__
#include <sched.h>
#endif

#ifndef HANMO_RESIDENT_SOURCE_SHA
#define HANMO_RESIDENT_SOURCE_SHA "unavailable"
#endif

namespace {
using namespace hanmo::resident;
using Value=BoundaryValue;
using Clock=std::chrono::steady_clock;
namespace fs=std::filesystem;
// 工具错误带可定位路径，不返回成功的空对象或空世界。
struct Failure : std::runtime_error { using std::runtime_error::runtime_error; };
// 帮助字段读取仅用于工具自身manifest，领域命令仍交唯一中央decoder。
const Value& required(const Value& value,const char* key) {
    const auto* found=value.find(key); if(!found) throw Failure(std::string("Missing manifest field: ")+key); return *found;
}
const std::string& string(const Value& value) {
    const auto* found=value.as<std::string>(); if(!found) throw Failure("Expected string"); return *found;
}
// 清单布尔严格核类型，损坏输入不能通过空指针崩溃逃过诊断。
bool boolean(const Value& value) {const auto* found=value.as<bool>();if(!found)throw Failure("Expected boolean");return *found;}
const Value::Array& array(const Value& value) {
    const auto* found=value.as<Value::Array>(); if(!found) throw Failure("Expected array"); return *found;
}
// 文件上限在读取前检查，防止独立CLI被无界fixture耗尽内存。
std::string read_bytes(const fs::path& path) {
    const auto size=fs::file_size(path); if(size>64u*1024u*1024u) throw Failure("Input exceeds 64MiB: "+path.string());
    std::ifstream file(path,std::ios::binary); if(!file) throw Failure("Cannot read: "+path.string());
    std::string bytes(static_cast<std::size_t>(size),'\0'); file.read(bytes.data(),static_cast<std::streamsize>(size));
    if(!file && size!=0) throw Failure("Incomplete read: "+path.string());
    return bytes;
}
Value parse(const std::string& bytes) {
    auto result=parse_boundary_json(bytes); if(!result.ok) throw Failure(result.error.original.code); return std::move(result.value);
}
Value load(const fs::path& path) {return parse(read_bytes(path));}
std::string dump(const Value& value) {
    auto result=render_boundary_json(value); if(!result.ok) throw Failure(result.error.original.code); return std::move(result.value);
}
[[maybe_unused]] void save(const fs::path& path,const Value& value) {
    std::ofstream file(path,std::ios::binary); if(!file) throw Failure("Cannot write: "+path.string()); file<<dump(value); if(!file) throw Failure("Incomplete write: "+path.string());
}
// 重复统计仍完整导出和比较；成功态可只留原始bytes摘要，差异态始终保留便于复核。
[[maybe_unused]] std::string state_evidence(const fs::path& path,const Value& value,bool digest_only) {
    const auto bytes=dump(value);const auto sha=picosha2::hash256_hex_string(bytes);
    if(!digest_only){std::ofstream file(path,std::ios::binary);if(!file)throw Failure("Cannot write state evidence");file<<bytes;if(!file)throw Failure("Incomplete state evidence");}
    return sha;
}
// 清单文件引用只能指向同一证据目录，拒绝绝对路径和向上跳出。
fs::path artifact_path(const fs::path& root,const Value& ref,const char* key) {
    const fs::path name=string(required(ref,key)); if(name.empty()||name.is_absolute()) throw Failure("Invalid artifact path");
    for(const auto& component:name) if(component=="..") throw Failure("Invalid artifact path");
    const auto base=fs::canonical(root),resolved=fs::canonical(root/name);
    const auto relative=resolved.lexically_relative(base); if(relative.empty()||*relative.begin()=="..") throw Failure("Artifact escapes evidence directory");
    return resolved;
}
// 显式生成Dictionary迭代序清单；数组次序和类型保留在原state比较中。
void orders(const Value& value,Value::Array path,Value::Array& output) {
    if(const auto* dict=value.as<Value::Dict>()) {
        Value::Array keys; for(const auto& item:*dict) keys.push_back({item.first});
        output.push_back({Value::Dict{{"path",{path}},{"keys",{std::move(keys)}}}});
        for(const auto& item:*dict) {auto next=path; next.push_back({item.first}); orders(item.second,std::move(next),output);}
    } else if(const auto* list=value.as<Value::Array>()) for(std::size_t i=0;i<list->size();++i) {auto next=path; next.push_back({static_cast<Integer>(i)}); orders((*list)[i],std::move(next),output);}
}
// 比较诊断分开值和字典序；旧codec的90处差异不能被统称已等价。
Value comparison(const BoundaryComparison& result) {
    Value::Dict row{{"values_equal",{result.values_equal}},{"dictionary_order_equal",{result.dictionary_order_equal}},{"exact",{result.exact()}}};
    if(result.value_difference) row.emplace_back("value_difference",Value{Value::Dict{{"path",{result.value_difference->path}},{"reason",{result.value_difference->reason}}}});
    if(result.order_difference) row.emplace_back("order_difference",Value{Value::Dict{{"path",{result.order_difference->path}},{"reason",{result.order_difference->reason}}}});
    return {std::move(row)};
}
// 以真实artifact SHA固定扩展catalog/seed；不借旧content_digest认证synthetic世界。
Value verified_artifact(const fs::path& root,const Value& ref) {
    const auto raw=read_bytes(artifact_path(root,ref,"file"));
    if(picosha2::hash256_hex_string(raw)!=string(required(ref,"sha256"))) throw Failure("Artifact SHA mismatch");
    auto value=parse(raw);
    if(ref.find("order_file")) {
        const auto order_raw=read_bytes(artifact_path(root,ref,"order_file"));
        if(picosha2::hash256_hex_string(order_raw)!=string(required(ref,"order_sha256"))) throw Failure("Order artifact SHA mismatch");
        Value::Array actual; orders(value,{},actual);
        if(!compare_boundary({std::move(actual)},parse(order_raw)).exact()) throw Failure("Dictionary order artifact mismatch");
    }
    return value;
}
// oracle载入是离线验证，既不运行GD也不构造任何假native规则结果。
struct Oracle {
    fs::path root; Value index,catalog,initial; std::string index_sha;
    std::map<std::string,Value> states,state_references;
    explicit Oracle(const fs::path& path):root(path.parent_path()) {
        const auto index_raw=read_bytes(path); index=parse(index_raw); index_sha=picosha2::hash256_hex_string(index_raw);
        const auto format=string(required(index,"format"));
        if(format!="HM-RESIDENT-JOURNEY-1000-ORACLE-INDEX/1"&&format!="HM-RESIDENT-JOURNEY-SCALE-ORACLE-INDEX/1") throw Failure("Unsupported oracle index");
        catalog=verified_artifact(root,required(index,"expanded_catalog"));
        initial=state(required(index,"initial_state"));
        if(format=="HM-RESIDENT-JOURNEY-SCALE-ORACLE-INDEX/1") {const auto* declared=required(index,"actor_count").as<Integer>();const auto* actors=required(initial,"actors").as<Value::Dict>();if(!declared||!actors||*declared<1||static_cast<std::size_t>(*declared)!=actors->size())throw Failure("Oracle actor count mismatch");}
        auto previous=string(required(required(index,"initial_state"),"sha256"));
        for(const auto& row:array(required(index,"commands"))) {
            const auto& before=required(row,"before"); const auto& after=required(row,"after");
            if(string(required(before,"sha256"))!=previous) throw Failure("Oracle command chain discontinuity");
            required(row,"command"); required(row,"preview"); required(row,"receipt");
            state(before); state(after); previous=string(required(after,"sha256"));
        }
        if(previous!=string(required(required(index,"final_state"),"sha256"))) throw Failure("Oracle final boundary mismatch");
    }
    // 同SHA引用必须逐字段一致；完整已核state只保一份，后续不反复解析大文件。
    const Value& state(const Value& ref) {
        const auto sha=string(required(ref,"sha256")); const auto found=states.find(sha);
        if(found!=states.end()) {if(!compare_boundary(state_references.at(sha),ref).exact()) throw Failure("Inconsistent reference for same artifact SHA"); return found->second;}
        auto value=verified_artifact(root,ref); state_references.emplace(sha,ref);
        return states.emplace(sha,std::move(value)).first->second;
    }
};
// Bridge证据原state字节须匹配摘要，且显式order文件须对应同一个state。
Value bridge_state(const fs::path& root,const Value& report,const std::string& sha) {
    const auto& states=required(report,"states"); const auto* entry=states.find(sha); if(!entry) throw Failure("Missing Bridge state artifact");
    const auto raw=read_bytes(artifact_path(root,*entry,"file")); if(picosha2::hash256_hex_string(raw)!=sha) throw Failure("Bridge state SHA mismatch");
    auto state=parse(raw); Value::Array expected_order; orders(state,{},expected_order);
    if(!compare_boundary({std::move(expected_order)},load(artifact_path(root,*entry,"order_file"))).exact()) throw Failure("Bridge state order artifact mismatch");
    return state;
}
// 独立完整对照命令/报价/回执及所有状态边界，不删军事/社会等后台域。
Value compare_bridge(Oracle& oracle,const fs::path& path) {
    auto report=load(path); if(string(required(report,"oracle_index_sha256"))!=oracle.index_sha) throw Failure("Bridge oracle identity mismatch");
    if(string(required(report,"format"))!="HM-RESIDENT-BRIDGE-PROBE/1") throw Failure("Unsupported Bridge probe report");
    const auto* ok=required(report,"ok").as<bool>(); if(!ok||!*ok) throw Failure("Bridge run did not complete");
    const auto& wanted=array(required(oracle.index,"commands")); const auto& actual=array(required(report,"commands"));
    Value::Array rows; bool exact=wanted.size()==actual.size(); Integer boundaries=0;
    const auto compare_state=[&](const Value& wanted_ref,const std::string& actual_sha) {++boundaries; return compare_boundary(oracle.state(wanted_ref),bridge_state(path.parent_path(),report,actual_sha));};
    const auto initial=compare_state(required(oracle.index,"initial_state"),string(required(report,"initial"))); exact=exact&&initial.exact();
    std::string previous=string(required(report,"initial"));
    for(std::size_t i=0;i<std::min(wanted.size(),actual.size());++i) {
        const auto& a=wanted[i]; const auto& b=actual[i];
        if(string(required(b,"before"))!=previous) throw Failure("Bridge command chain discontinuity");
        const auto cmd=compare_boundary(required(a,"command"),parse(string(required(b,"command_json"))));
        const auto preview=compare_boundary(required(a,"preview"),required(b,"preview")); const auto receipt=compare_boundary(required(a,"receipt"),required(b,"receipt"));
        const auto before=compare_state(required(a,"before"),string(required(b,"before"))); const auto after=compare_state(required(a,"after"),string(required(b,"after")));
        exact=exact&&cmd.exact()&&preview.exact()&&receipt.exact()&&before.exact()&&after.exact(); previous=string(required(b,"after"));
        rows.push_back({Value::Dict{{"index",{static_cast<Integer>(i)}},{"command",comparison(cmd)},{"preview",comparison(preview)},{"receipt",comparison(receipt)},{"before",comparison(before)},{"after",comparison(after)}}});
    }
    if(previous!=string(required(report,"final"))) throw Failure("Bridge final boundary mismatch");
    const auto final=compare_state(required(oracle.index,"final_state"),previous); exact=exact&&final.exact();
    return {Value::Dict{{"format",{std::string("HM-RESIDENT-BRIDGE-COMPARISON/1")}},{"ok",{exact}},{"full_chain_equivalent",{exact}},{"performance_accepted",{false}},{"commands_compared",{static_cast<Integer>(actual.size())}},{"state_boundaries_compared",{boundaries}},{"initial",comparison(initial)},{"final",comparison(final)},{"commands",{std::move(rows)}},{"scope",{std::string("Full value/type/array/dictionary-order comparison of one Bridge probe; no formal statistics or Main/storage acceptance")}}}};
}
// 所有时钟仅统计真实调用；全state export/compare位于独立诊断项。
[[maybe_unused]] Integer micros(Clock::time_point start) {return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();}
[[maybe_unused]] Value host_identity() {
    struct rusage usage{}; getrusage(RUSAGE_SELF,&usage);
    Integer cpus=0;
#ifdef __linux__
    cpu_set_t allowed; CPU_ZERO(&allowed); if(sched_getaffinity(0,sizeof(allowed),&allowed)==0) cpus=CPU_COUNT(&allowed);
#endif
    std::string model="unavailable",load_average="unavailable";
    std::ifstream cpuinfo("/proc/cpuinfo"); std::string line;
    while(std::getline(cpuinfo,line)) if(line.rfind("model name",0)==0) {model=line; break;}
    std::ifstream loadavg("/proc/loadavg"); std::getline(loadavg,load_average);
    return {Value::Dict{{"rss_scope",{std::string("Whole process including oracle fixtures, explicit exports and evidence; not isolated resident-core memory")}},{"cpu_model",{model}},{"load_average",{load_average}},{"source_sha",{std::string(HANMO_RESIDENT_SOURCE_SHA)}},{"peak_rss_kib",{static_cast<Integer>(usage.ru_maxrss)}},{"affinity_logical_cpus",{cpus}},{"compiler",{std::string(__VERSION__)}},{"process_samples",{Integer{1}}}}};
}
#ifdef HANMO_RESIDENT_CORE_LINKED
// 中央已按执行次序聚合阶段成本；此层只记录，不重计或重跑任何phase。
Value stage_times(const ReplayCommandTiming& timing) {
    Value::Dict fields; for(const auto& stage:timing.stages) fields.emplace_back(stage.first,Value{stage.second}); return {std::move(fields)};
}
// 原错误类型和phase/domain/source均写入证据，普通拒绝与能力缺失不得混淆。
Value rule_error_value(const RuleError& error) {
    const std::string kind=error.kind==ErrorKind::rule?"rule":error.kind==ErrorKind::unsupported_capability?"unsupported_capability":"internal";
    return {Value::Dict{{"code",{error.original.code}},{"path",{error.original.path}},{"kind",{kind}},{"phase",{error.phase}},{"domain",{error.domain}},{"command",{error.command}},{"source_id",{error.source_id}}}};
}
// 四条已有恢复trace逐命令执行；旧codec换序只按已登记checkpoint切换独立输入段。
Value replay_recovery(const fs::path& report_path,const fs::path& catalog_path,const fs::path& output) {
    fs::create_directories(output); const auto raw=read_bytes(report_path); const auto oracle=parse(raw); const auto catalog=load(catalog_path);
    if(string(required(oracle,"format"))!="HM-RESIDENT-JOURNEY-ORACLE/1"||!boolean(required(oracle,"ok")))throw Failure("Invalid recovery oracle");
    const bool restored=boolean(required(oracle,"restore_checkpoint")); Value::Array cases; Integer executed=0,blocked_faults=0,executed_faults=0,imports=0;bool ordinary_exact=true;
    for(const auto& input:array(required(oracle,"cases"))) {
        const auto case_id=string(required(input,"case_id"));const auto& trace=array(required(input,"trace"));if(trace.empty())throw Failure("Empty recovery trace");
        std::string current_sha=string(required(input,"initial"));auto current=bridge_state(report_path.parent_path(),oracle,current_sha);auto session=create_resident_replay_session();
        auto installed=session->initialize(catalog,current);Value::Array rows;bool case_exact=installed.ok;Integer case_executed=0,case_imports=0;
        if(!installed.ok){cases.push_back({Value::Dict{{"case_id",{case_id}},{"ok",{false}},{"error",rule_error_value(installed.error)}}});ordinary_exact=false;continue;}
        for(std::size_t i=0;i<trace.size();++i) {
            const auto& row=trace[i]; const auto before_sha=string(required(row,"before")); const auto expected_before=bridge_state(report_path.parent_path(),oracle,before_sha);
            if(before_sha!=current_sha) {
                bool registered=false;
                for(const auto& checkpoint:array(required(input,"checkpoints"))) if(boolean(required(checkpoint,"applied"))&&string(required(checkpoint,"state"))==current_sha&&string(required(checkpoint,"restored_state"))==before_sha)registered=true;
                if(!restored||!registered||!compare_boundary(current,expected_before).values_equal)throw Failure("Undeclared recovery input transition");
                // 新session只表示从已经冻结的legacy-restored seed继续；绝不冒称native旧档迁移。
                session=create_resident_replay_session();installed=session->initialize(catalog,expected_before);if(!installed.ok){rows.push_back({Value::Dict{{"index",{static_cast<Integer>(i)}},{"error",rule_error_value(installed.error)}}});case_exact=false;break;}
                ++imports;++case_imports;current_sha=before_sha;current=expected_before;
            }
            auto before=session->export_state();if(!before.ok)throw Failure(before.error.original.code);const auto before_diff=compare_boundary(expected_before,before.value);
            if(!before_diff.exact()){rows.push_back({Value::Dict{{"index",{static_cast<Integer>(i)}},{"before",comparison(before_diff)}}});case_exact=false;break;}
            const auto fault=string(required(row,"fault_injection")); bool armed_fault=false;Outcome<WorldIdentity> fault_identity;
            if(!fault.empty()) {
                if(fault!="after_full_advance"||string(required(required(row,"command"),"kind"))!="advance")throw Failure("Unsupported recorded fault probe");
                if(before_sha!=string(required(row,"after")))throw Failure("Fault row unexpectedly changes oracle state");
                auto armed=session->arm_late_failure_probe();
                if(!armed.ok) {
                    if(armed.error.original.code!="TestFaultInjectionDisabled"){rows.push_back({Value::Dict{{"index",{static_cast<Integer>(i)}},{"error",rule_error_value(armed.error)}}});case_exact=false;break;}
                    ++blocked_faults;rows.push_back({Value::Dict{{"index",{static_cast<Integer>(i)}},{"executed",{false}},{"blocked",{std::string("TestFaultInjectionDisabled; no execute-then-restore simulation")}},{"fault_injection",{fault}},{"before_sha256",{before_sha}},{"after_sha256",required(row,"after")},{"expected_receipt",required(row,"result")}}});continue;
                }
                fault_identity=session->identity();if(!fault_identity.ok)throw Failure(fault_identity.error.original.code);armed_fault=true;++executed_faults;
            }
            const auto start=Clock::now();auto result=session->execute(required(row,"command"));const auto usec=micros(start);++executed;++case_executed;
            if(!result.ok){rows.push_back({Value::Dict{{"index",{static_cast<Integer>(i)}},{"error",rule_error_value(result.error)}}});case_exact=false;break;}
            auto after=session->export_state();if(!after.ok)throw Failure(after.error.original.code);const auto after_sha=string(required(row,"after"));const auto expected_after=bridge_state(report_path.parent_path(),oracle,after_sha);
            const auto pd=compare_boundary(required(row,"preview"),result.value.preview),rd=compare_boundary(required(row,"result"),result.value.receipt),sd=compare_boundary(expected_after,after.value);
            bool identity_unchanged=true;
            if(armed_fault){auto now=session->identity();identity_unchanged=now.ok&&now.value.world_id==fault_identity.value.world_id&&now.value.execution_epoch==fault_identity.value.execution_epoch&&now.value.revision==fault_identity.value.revision&&now.value.tick==fault_identity.value.tick;}
            rows.push_back({Value::Dict{{"index",{static_cast<Integer>(i)}},{"label",required(row,"label")},{"execute_us",{usec}},{"preview",comparison(pd)},{"receipt",comparison(rd)},{"after",comparison(sd)},{"fault_injection_executed",{armed_fault}},{"native_identity_unchanged_for_fault",{identity_unchanged}}}});
            if(!pd.exact()||!rd.exact()||!sd.exact()||!identity_unchanged){save(output/(case_id+"-mismatch-state.json"),after.value);save(output/(case_id+"-mismatch-receipt.json"),result.value.receipt);save(output/(case_id+"-mismatch-preview.json"),result.value.preview);case_exact=false;break;}
            current_sha=after_sha;current=std::move(after.value);
        }
        if(case_exact&&current_sha!=string(required(input,"final")))case_exact=false;
        ordinary_exact=ordinary_exact&&case_exact;cases.push_back({Value::Dict{{"case_id",{case_id}},{"ordinary_commands_exact",{case_exact}},{"commands_executed",{case_executed}},{"legacy_restored_input_segments",{case_imports}},{"rows",{std::move(rows)}}}});
    }
    return {Value::Dict{{"format",{std::string("HM-RESIDENT-RECOVERY-PROBE/1")}},{"ok",{ordinary_exact&&blocked_faults==0}},{"full_recovery_accepted",{false}},{"ordinary_commands_exact",{ordinary_exact}},{"commands_executed",{executed}},{"blocked_fault_rows",{blocked_faults}},{"executed_fault_rows",{executed_faults}},{"all_recorded_command_rows_exact",{ordinary_exact&&blocked_faults==0}},{"legacy_restored_input_segments",{imports}},{"legacy_restored_source",{restored}},{"oracle_report_sha256",{picosha2::hash256_hex_string(raw)}},{"catalog_sha256",{picosha2::hash256_hex_string(read_bytes(catalog_path))}},{"cases",{std::move(cases)}},{"scope",{std::string("Existing oracle commands only; legacy-restored input segments retain their own order. Only recorded compile-time-enabled fault rows may execute; full_recovery_accepted stays false for broader migration/physical-slot scope. No normalization of90 historical cross-restore order differences.")}}}};
}
// 只从冻结抵达末态发出wait和advance，检验真实后台能力边界及全候选回滚。
Value probe_after_arrival(Oracle& oracle,const fs::path& output) {
    fs::create_directories(output);auto session=create_resident_replay_session();const auto& final=oracle.state(required(oracle.index,"final_state"));auto installed=session->initialize(oracle.catalog,final);
    if(!installed.ok)return {Value::Dict{{"ok",{false}},{"error",rule_error_value(installed.error)}}};
    const auto actor=string(required(final,"player_actor_id"));const auto sequence=required(final,"request_sequences").find(actor);if(!sequence||!sequence->as<Integer>())throw Failure("Missing final request sequence");
    const auto* revision=required(final,"revision").as<Integer>();if(!revision)throw Failure("Missing final revision");
    Value command{Value::Dict{{"kind",{std::string("wait")}},{"params",{Value::Dict{}}},{"actor_id",{actor}},{"request_seq",{*sequence->as<Integer>()+1}},{"expected_revision",{*revision}}}};
    auto waited=session->execute(command);if(!waited.ok)return {Value::Dict{{"ok",{false}},{"error",rule_error_value(waited.error)}}};
    if(!boolean(required(waited.value.receipt,"ok")))return {Value::Dict{{"ok",{false}},{"wait_receipt",waited.value.receipt}}};
    auto before=session->export_state();if(!before.ok)throw Failure(before.error.original.code);*command.find("kind")={std::string("advance")};*command.find("request_seq")={*sequence->as<Integer>()+2};*command.find("expected_revision")=required(before.value,"revision");
    auto advanced=session->execute(command);if(!advanced.ok)return {Value::Dict{{"ok",{false}},{"error",rule_error_value(advanced.error)}}};auto after=session->export_state();if(!after.ok)throw Failure(after.error.original.code);
    const auto diff=compare_boundary(before.value,after.value);const auto* code=advanced.value.receipt.find("code");const auto* kind=advanced.value.receipt.find("kind");const bool blocked=code&&code->as<StableId>()&&*code->as<StableId>()=="UnsupportedNativeCapability"&&kind&&kind->as<StableId>()&&*kind->as<StableId>()=="campaign_depart";
    save(output/"before-unsupported.json",before.value);save(output/"after-unsupported.json",after.value);
    return {Value::Dict{{"format",{std::string("HM-RESIDENT-AFTER-ARRIVAL-PROBE/1")}},{"ok",{blocked&&diff.exact()}},{"wait_receipt",waited.value.receipt},{"advance_preview",advanced.value.preview},{"advance_receipt",advanced.value.receipt},{"complete_rollback",comparison(diff)},{"scope",{std::string("One wait→advance from existing oracle final state; proves capability failure rather than a tick4 hard stop, no additional GD oracle generation")}}}};
}
// 单次完整链探针。正式统计另需独立进程与100已提交半日，不能由这一次推算p95。
Value replay(Oracle& oracle,const fs::path& output,Integer checkpoint_after=-1,bool digest_only=false) {
    if(checkpoint_after < -1 || (checkpoint_after>=0&&static_cast<std::size_t>(checkpoint_after)>=array(required(oracle.index,"commands")).size()))throw Failure("Checkpoint command index out of range");
    fs::create_directories(output); auto session=create_resident_replay_session(); if(!session) throw Failure("Resident replay factory returned null");
    const auto cold_start=Clock::now(); const auto installed=session->initialize(oracle.catalog,oracle.initial); const auto cold_us=micros(cold_start);
    Value::Dict report{{"format",{std::string("HM-RESIDENT-P1-PROBE/1")}},{"ok",{false}},{"full_chain_complete",{false}},{"performance_accepted",{false}},{"initialization_count",{Integer{1}}},{"initialization_us",{cold_us}}};
    report.emplace_back("oracle_index_sha256",Value{oracle.index_sha});
    const auto* actors=required(oracle.initial,"actors").as<Value::Dict>();if(!actors)throw Failure("Expected initial actor dictionary");report.emplace_back("initial_actor_count",Value{static_cast<Integer>(actors->size())});
    if(!installed.ok) {report.emplace_back("error",rule_error_value(installed.error)); return {std::move(report)};}
    auto current=session->export_state(); if(!current.ok) throw Failure(current.error.original.code);
    const auto initial_diff=compare_boundary(oracle.initial,current.value); report.emplace_back("initial_comparison",comparison(initial_diff));
    report.emplace_back("state_evidence_mode",Value{std::string(digest_only?"digest_on_success_full_on_mismatch":"full")});
    report.emplace_back("initial_state_sha256",Value{state_evidence(output/"state-initial.json",current.value,digest_only&&initial_diff.exact())});
    if(!initial_diff.exact()) return {std::move(report)};
    Value::Array command_reports; bool exact=true; Integer index=0,total_us=0,export_compare_us=0;
    const auto initial_tick=required(current.value,"tick").as<Integer>(); if(!initial_tick) throw Failure("Missing typed initial tick");
    const Integer start_tick=*initial_tick; Integer final_tick=start_tick;
    for(const auto& row:array(required(oracle.index,"commands"))) {
        const auto started=Clock::now(); auto result=session->execute(required(row,"command")); const auto execute_us=micros(started); total_us+=execute_us;
        if(!result.ok) {command_reports.push_back({Value::Dict{{"index",{index}},{"error",rule_error_value(result.error)},{"execute_us",{execute_us}}}}); exact=false; break;}
        const auto diagnostic_start=Clock::now(); auto after=session->export_state(); if(!after.ok) throw Failure(after.error.original.code);
        const auto pd=compare_boundary(required(row,"preview"),result.value.preview),rd=compare_boundary(required(row,"receipt"),result.value.receipt),sd=compare_boundary(oracle.state(required(row,"after")),after.value);
        const auto tick=required(after.value,"tick").as<Integer>(); if(!tick) throw Failure("Missing typed state tick"); final_tick=*tick;
        const auto state_sha=state_evidence(output/("state-"+std::to_string(index)+".json"),after.value,digest_only&&pd.exact()&&rd.exact()&&sd.exact());export_compare_us+=micros(diagnostic_start);
        command_reports.push_back({Value::Dict{{"index",{index}},{"state_sha256",{state_sha}},{"execute_us",{execute_us}},{"decode_us",{static_cast<Integer>(result.value.timing.decode_usec)}},{"preview_us",{static_cast<Integer>(result.value.timing.preview_usec)}},{"submit_us",{static_cast<Integer>(result.value.timing.submit_usec)}},{"dto_us",{static_cast<Integer>(result.value.timing.dto_usec)}},{"stage_us",stage_times(result.value.timing)},{"preview",comparison(pd)},{"receipt",comparison(rd)},{"state",comparison(sd)}}});
        // 首次差异直接保留实际有限回执；统一批次无需为看见拒绝原因再执行同一条链。
        if(!pd.exact()||!rd.exact()||!sd.exact()) {
            save(output/("mismatch-"+std::to_string(index)+".json"),Value{Value::Dict{{"command",required(row,"command")},{"actual_preview",result.value.preview},{"actual_receipt",result.value.receipt}}});
        }
        if(index==checkpoint_after&&pd.exact()&&rd.exact()&&sd.exact()) {
            const auto checkpoint_start=Clock::now(); const auto old_id=session->identity(); const auto checkpoint=session->checkpoint();
            if(!old_id.ok||!checkpoint.ok) {report.emplace_back("checkpoint_error",rule_error_value(!old_id.ok?old_id.error:checkpoint.error)); exact=false; break;}
            std::ofstream file(output/"core.checkpoint",std::ios::binary); file.write(reinterpret_cast<const char*>(checkpoint.value.data()),static_cast<std::streamsize>(checkpoint.value.size())); file.close(); if(!file) throw Failure("Checkpoint evidence write failed");
            const auto restored=session->restore(checkpoint.value); if(!restored.ok) {report.emplace_back("restore_error",rule_error_value(restored.error)); exact=false; break;}
            const auto new_id=session->identity(); const auto recovered=session->export_state();
            if(!new_id.ok||!recovered.ok) {report.emplace_back("restore_error",rule_error_value(!new_id.ok?new_id.error:recovered.error)); exact=false; break;}
            const auto cd=compare_boundary(after.value,recovered.value); const bool identity_ok=old_id.value.world_id==new_id.value.world_id&&old_id.value.revision==new_id.value.revision&&old_id.value.tick==new_id.value.tick&&new_id.value.execution_epoch>old_id.value.execution_epoch;
            report.emplace_back("checkpoint_roundtrip",Value{Value::Dict{{"after_command",{index}},{"identity_epoch_verified",{identity_ok}},{"state",comparison(cd)},{"bytes",{static_cast<Integer>(checkpoint.value.size())}},{"checkpoint_restore_us_not_gameplay",{micros(checkpoint_start)}}}});
            if(!cd.exact()||!identity_ok) {exact=false; break;}
        }
        ++index; if(!pd.exact()||!rd.exact()||!sd.exact()) {exact=false; break;}
    }
    report.emplace_back("observed_world_half_days",Value{final_tick-start_tick});
    report.emplace_back("commands",Value{std::move(command_reports)}); report.emplace_back("total_command_processing_us",Value{total_us}); report.emplace_back("export_compare_us_not_gameplay",Value{export_compare_us}); report.emplace_back("host",host_identity());
    report[1].second={exact}; report[2].second={exact};
    report.emplace_back("scope",Value{std::string("One real core chain probe; full-state export/diff excluded. Not formal P1 statistics or Main/physical-slot acceptance.")});
    return {std::move(report)};
}
#endif
// 所有模式都输出机器可核报告，缺真实core时能力失败不是空跑成功。
int run(int argc,char** argv) {
    if(argc==4&&std::string(argv[1])=="--compare") {auto result=compare_boundary(load(argv[2]),load(argv[3])); std::cout<<dump(comparison(result))<<'\n'; return result.exact()?0:1;}
    if(argc==3&&std::string(argv[1])=="--inspect-oracle") {
        Oracle oracle(argv[2]);
        std::cout<<dump({Value::Dict{{"ok",{true}},{"scope",{std::string("Artifact integrity only; no native gameplay or performance claim")}},{"commands",{static_cast<Integer>(array(required(oracle.index,"commands")).size())}},{"states",{static_cast<Integer>(oracle.states.size())}}}})<<'\n'; return 0;
    }
    if(argc==5&&std::string(argv[1])=="--replay-recovery") {
#ifdef HANMO_RESIDENT_CORE_LINKED
        auto result=replay_recovery(argv[2],argv[3],argv[4]);save(fs::path(argv[4])/"recovery-probe.json",result);std::cout<<dump(result)<<'\n';return boolean(required(result,"ok"))?0:1;
#else
        std::cerr<<"UnsupportedNativeCapability: resident_core\n";return 2;
#endif
    }
    if(argc==4&&std::string(argv[1])=="--probe-after-arrival") {
#ifdef HANMO_RESIDENT_CORE_LINKED
        Oracle oracle(argv[2]);auto result=probe_after_arrival(oracle,argv[3]);save(fs::path(argv[3])/"after-arrival-probe.json",result);std::cout<<dump(result)<<'\n';return boolean(required(result,"ok"))?0:1;
#else
        std::cerr<<"UnsupportedNativeCapability: resident_core\n";return 2;
#endif
    }
    if(argc==4&&std::string(argv[1])=="--compare-bridge") {
        Oracle oracle(argv[2]); auto result=compare_bridge(oracle,argv[3]); std::cout<<dump(result)<<'\n'; return boolean(required(result,"ok"))?0:1;
    }
    if(argc==4&&std::string(argv[1])=="--prepare-bridge") {
        Oracle oracle(argv[2]); const fs::path output=argv[3]; fs::create_directories(output); Value::Array files; Value::Dict hashes; Integer i=0;
        for(const auto& row:array(required(oracle.index,"commands"))) {const auto name="command-"+std::to_string(i++)+".json"; save(output/name,required(row,"command")); hashes.emplace_back(name,Value{picosha2::hash256_hex_string(read_bytes(output/name))}); files.push_back({name});}
        Value manifest{Value::Dict{{"format",{std::string("HM-RESIDENT-BRIDGE-INPUT/1")}},{"oracle_index",{fs::canonical(argv[2]).string()}},{"oracle_index_sha256",{oracle.index_sha}},{"catalog_file",{artifact_path(oracle.root,required(oracle.index,"expanded_catalog"),"file").string()}},{"initial_state_file",{artifact_path(oracle.root,required(oracle.index,"initial_state"),"file").string()}},{"catalog_sha256",required(required(oracle.index,"expanded_catalog"),"sha256")},{"initial_state_sha256",required(required(oracle.index,"initial_state"),"sha256")},{"command_sha256",{std::move(hashes)}},{"commands",{std::move(files)}}}};
        save(output/"prepared.json",manifest); std::cout<<dump({Value::Dict{{"ok",{true}},{"scope",{std::string("Prepared small exact command JSON only; full input files reused; no gameplay claim")}}}})<<'\n'; return 0;
    }
    if(argc==5&&std::string(argv[1])=="--replay-restored-at") {
#ifdef HANMO_RESIDENT_CORE_LINKED
        // 显式零基命令位置让测试覆盖真实新事实；不截短原命令链或重新生成oracle。
        Integer after=-1;const std::string argument=argv[4];auto parsed=std::from_chars(argument.data(),argument.data()+argument.size(),after);
        if(parsed.ec!=std::errc{}||parsed.ptr!=argument.data()+argument.size()||after<0)throw Failure("Invalid checkpoint command index");
        Oracle oracle(argv[2]);auto result=replay(oracle,argv[3],after);save(fs::path(argv[3])/"probe.json",result);std::cout<<dump(result)<<'\n';return boolean(required(result,"ok"))?0:1;
#else
        std::cerr<<"UnsupportedNativeCapability: resident_core\n";return 2;
#endif
    }
    if(argc==4&&(std::string(argv[1])=="--replay"||std::string(argv[1])=="--replay-restored"||std::string(argv[1])=="--replay-digests")) {
#ifdef HANMO_RESIDENT_CORE_LINKED
        const auto fixture_start=Clock::now();Oracle oracle(argv[2]);const auto fixture_us=micros(fixture_start);auto result=replay(oracle,argv[3],std::string(argv[1])=="--replay-restored"?0:-1,std::string(argv[1])=="--replay-digests");result.as<Value::Dict>()->emplace_back("oracle_fixture_load_verify_us_not_gameplay",Value{fixture_us});save(fs::path(argv[3])/"probe.json",result); std::cout<<dump(result)<<'\n'; return boolean(required(result,"ok"))?0:1;
#else
        std::cout<<"{\"ok\":false,\"error\":\"UnsupportedNativeCapability\",\"domain\":\"resident_core\",\"full_chain_complete\":false,\"performance_accepted\":false}\n"; return 2;
#endif
    }
    std::cerr<<"Usage: resident_journey --inspect-oracle oracle-index.json | --compare expected.json actual.json | --compare-bridge oracle-index.json report.json | --prepare-bridge oracle-index.json output-directory | --replay-recovery report.json catalog.json output-directory | --probe-after-arrival oracle-index.json output-directory | --replay or --replay-restored or --replay-digests oracle-index.json output-directory | --replay-restored-at oracle-index.json output-directory command-index\n"; return 2;
}
}
// 顶层失败保留明确非零退出码，绝不打印误导性的通过摘要。
int main(int argc,char** argv) {
    try {return run(argc,argv);}
#ifdef HANMO_RESIDENT_CORE_LINKED
    catch(hanmo::resident::checkpoint_job_detail::FailureException& failure) {
        // Borrow the original diagnostic only while its owning failure is alive.
        failure.error.try_materialize_rule_error();
        if(const auto* original=failure.error.rule_error())std::cerr<<"resident_journey: "<<original->original.code<<" at "<<original->original.path<<'\n';
        else std::cerr<<"resident_journey: "<<hanmo::resident::checkpoint_job_failure_code_name(failure.error.code())<<" required "<<failure.error.required()<<'\n';
        return 2;
    }
#endif
    catch(const std::exception& error) {std::cerr<<"resident_journey: "<<error.what()<<'\n'; return 2;}
}
