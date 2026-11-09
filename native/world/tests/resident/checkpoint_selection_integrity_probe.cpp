// 使用真实银行验证封套选择边界；领域冷恢复由各域实际 Session 继续验证。
#include "resident/checkpoint/checkpoint_selection.hpp"
#include "resident/kernel/world_codec.hpp"
#include <picosha2.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <algorithm>
using namespace hanmo::resident;
static int checks=0, failures=0;
static void check(bool ok,const std::string& name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
static Bytes read(const char* path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static Bytes reseal(Bytes bytes){constexpr std::size_t prefix=25;const std::string raw(bytes.begin()+prefix,bytes.end()-64);const auto digest=picosha2::hash256_hex_string(raw);std::copy(digest.begin(),digest.end(),bytes.end()-64);return bytes;}
static void reject(const CheckpointEnvelope& env,const std::string& name){auto encoded=encode_checkpoint(env);check(encoded.ok,name+" encoded");if(!encoded.ok)return;auto result=inspect_world_checkpoint_selection(encoded.value);check(!result.ok,name+" rejected");if(!result.ok)std::cout<<name<<" "<<result.error.original.code<<'\n';}
int main(int argc,char** argv){
 if(argc!=3)return 2;
 for(int input=1;input<argc;++input){
  const auto bytes=read(argv[input]);check(!bytes.empty(),"bank present");
  auto decoded=inspect_checkpoint_selected(bytes,checkpoint_domains_from_identity);check(decoded.ok,"original envelope");
  auto selected=inspect_world_checkpoint_selection(bytes);check(selected.ok,"original selection");
  if(!decoded.ok||!selected.ok){if(!selected.ok)std::cerr<<selected.error.original.code<<'\n';continue;}
  auto env=decoded.value;
  auto changed=env;changed.sections.pop_back();reject(changed,"missing section");
  changed=env;changed.sections.back().domain="unknown_domain";reject(changed,"unknown section");
  changed=env;changed.sections.back().codec_version=999;reject(changed,"unknown section version");
  changed=env;std::swap(changed.sections[0],changed.sections[1]);reject(changed,"section order");
  changed=env;changed.identity.catalog.module_versions.push_back({"unknown_module","1"});reject(changed,"unknown marker");
  changed=env;changed.identity.catalog.module_versions.push_back(changed.identity.catalog.module_versions.front());reject(changed,"duplicate marker");
  changed=env;changed.identity.catalog.module_versions.front().second="999";reject(changed,"marker version");
  changed=env;auto& modules=changed.identity.catalog.module_versions;modules.erase(std::remove_if(modules.begin(),modules.end(),[](const auto& p){return p.first=="envoy_tasks";}),modules.end());reject(changed,"missing paired marker");
  changed=env;for(auto& p:changed.identity.catalog.module_versions)if(p.first=="envoy_tasks")p.second=p.second=="1"?"2":"1";reject(changed,"paired version mismatch");
  // 通过原字段 visitor 定位真实段名，再重签摘要，确实触及 reader 的重复段门。
  Bytes old_field,new_field;std::string old_name=env.sections.back().domain,new_name=env.sections.front().domain;
  CodecVisitor ow(old_field),nw(new_field);ow.field("domain",old_name);nw.field("domain",new_name);
  auto bad=bytes;auto at=std::search(bad.begin(),bad.end()-64,old_field.begin(),old_field.end());check(at!=bad.end()-64,"domain field located");
  if(at!=bad.end()-64){const auto offset=at-bad.begin();bad.erase(at,at+old_field.size());bad.insert(bad.begin()+offset,new_field.begin(),new_field.end());bad=reseal(std::move(bad));auto result=inspect_world_checkpoint_selection(bad);check(!result.ok&&result.error.original.code=="InvalidCheckpointDomain","duplicate section reader");}
  bad=bytes;bad.back()=bad.back()=='0'?'1':'0';auto digest=inspect_world_checkpoint_selection(bad);check(!digest.ok&&digest.error.original.code=="CheckpointDigestMismatch","digest checked first");
  check(bytes==read(argv[input]),"original bank unchanged");
 }
 std::cout<<"CHECKPOINT_SELECTION_INTEGRITY_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
