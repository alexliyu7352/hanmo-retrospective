// 只读真实财政银行：typed冷恢复后的JSON导出必须仍能通过同目录冷边界。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/kernel/native_initial_state.hpp"
#include "resident/kernel/world_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
int checks=0;
void check(bool pass,const std::string& label){++checks;if(!pass)throw std::runtime_error(label);}
template<class T>T must(Outcome<T> result,const std::string& label){check(result.ok,label+":"+result.error.original.code+":"+result.error.original.path);return std::move(result.value);}
V json_file(const char* path){std::ifstream in(path);check(bool(in),"open catalog");std::ostringstream text;text<<in.rdbuf();return must(parse_boundary_json(text.str()),"parse catalog");}
Bytes bank_file(const char* path){std::ifstream in(path,std::ios::binary);check(bool(in),"open actual bank");return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
// 原bank所载完整身份用于同情景prepare，不建立新的演员或资产。
std::shared_ptr<const PreparedCatalog> selected(const V& catalog,const V& selection){
    const auto& source=boundary::field(selection,"catalog","selection");CatalogIdentity id;
    id.content_id=boundary::get_string(source,"content_id","catalog");id.content_version=boundary::get_string(source,"content_version","catalog");id.content_digest=boundary::get_string(source,"content_digest","catalog");id.ruleset_id=boundary::get_string(source,"ruleset_id","catalog");id.schema_sha256=boundary::get_string(source,"schema_sha256","catalog");id.artifact_sha256=boundary::get_string(source,"artifact_sha256","catalog");
    if(const auto* digest=source.find("pack_set_digest"))id.pack_set_digest=boundary::string(*digest,"pack_set_digest");
    for(const auto& row:boundary::array(boundary::field(source,"module_versions","catalog"),"modules"))id.module_versions.emplace_back(boundary::get_string(row,"name","module"),boundary::get_string(row,"version","module"));
    return must(prepare_native_catalog(catalog,boundary::get_string(selection,"scenario_id","selection"),id),"prepare actual selection");
}
}
int main(int argc,char** argv){try{
    check(argc==3,"usage: political_fiscal_json_roundtrip_test CATALOG REAL_BANK");
    const auto catalog=json_file(argv[1]);const auto original=bank_file(argv[2]);auto session=create_resident_replay_session();
    auto selection=must(session->inspect_checkpoint_selection(original),"inspect bank");auto installed=session->initialize_checkpoint(catalog,original);check(installed.ok,"restore original typed bank:"+installed.error.original.code);
    check(must(session->checkpoint(),"checkpoint original")==original,"typed restore preserves original bytes");
    const auto exported=must(session->export_state(),"export actual world");
    StableId policy_fact;for(const auto& row:boundary::object(boundary::field(exported,"facts","world"),"facts"))if(boundary::get_string(row.second,"kind","fact")=="political_fiscal_policy_changed"){check(boundary::get_string(row.second,"domain","fact")=="politics","actual policy domain");policy_fact=row.first;break;}
    check(!policy_fact.empty(),"bank includes actual policy change");
    auto prepared=selected(catalog,selection);auto seed=must(decode_world_seed(exported,*prepared),"decode actual exported world");WorldBundle candidate(prepared,std::move(seed));auto valid=candidate.validate();check(valid.ok,"validate exported world:"+valid.error.original.code);
    check(compare_boundary(exported,must(export_world_state(candidate),"re-export decoded world")).exact(),"exact JSON cold roundtrip");
    // 单独破坏事实kind或domain，不能因为接纳正式税策事实而接受未知政治事件。
    auto unknown_kind=exported;*unknown_kind.find("facts")->find(policy_fact)->find("kind")=V{StableId{"unknown_political_policy"}};auto kind_result=decode_world_seed(unknown_kind,*prepared);check(!kind_result.ok&&kind_result.error.original.code=="InvalidFactDomain","unknown political kind rejected");
    auto unknown_domain=exported;*unknown_domain.find("facts")->find(policy_fact)->find("domain")=V{StableId{"unknown_political_domain"}};auto domain_result=decode_world_seed(unknown_domain,*prepared);check(!domain_result.ok&&domain_result.error.original.code=="InvalidFactDomain","unknown domain rejected");
    check(must(session->checkpoint(),"checkpoint after failed cold candidates")==original,"cold rejection leaves installed world unchanged");
    std::cout<<"POLITICAL_FISCAL_JSON_ROUNDTRIP checks="<<checks<<" failures=0 commands=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"POLITICAL_FISCAL_JSON_ROUNDTRIP checks="<<checks<<" failures=1 "<<error.what()<<'\n';return 1;}}
