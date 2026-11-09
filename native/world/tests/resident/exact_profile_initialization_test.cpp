// Genuine factory behavior test using only the unchanged 02af0b88 public API.
// A newer authored fixture and explicit module identity are ordinary inputs.
#include "resident/kernel/native_initial_state.hpp"
#include "resident/storage/common_codec.hpp"
#include <picosha2.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
template<class T>T must(Outcome<T> r){if(!r.ok)throw std::runtime_error(r.error.original.code+":"+r.error.original.path);return std::move(r.value);}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("fixture path required");std::ifstream in(argv[1]);if(!in)throw std::runtime_error("fixture unreadable");std::ostringstream bytes;bytes<<in.rdbuf();auto source=must(parse_boundary_json(bytes.str()));using namespace boundary;NativeInitRequest request;request.scenario_id="engineering.exact_rest_only";request.world_seed="exact-boundary-initialization-red";auto& id=request.expected_catalog_identity;id.content_id=get_string(source,"content_id","");id.content_version=get_string(source,"content_version","");id.content_digest=get_string(source,"content_digest","");id.schema_sha256=get_string(source,"schema_sha256","");id.ruleset_id="hanmo.native.local_society/1";id.artifact_sha256=picosha2::hash256_hex_string(must(render_boundary_json(source)));id.module_versions={{"dynamic_world","local_society_v1"},{"local_economy","1"},{"organization","2"},{"affairs","1"},{"director","2"},{"regional_life","1"},{"world_timeline","2"},{"action_elapsed","2"},{"actor_start_context","2"},{"character_origin_records","1"},{"character_condition_extensions","1"}};
auto actual=make_native_initial_world(source,request);if(!actual.ok){std::cerr<<"FAIL: expected real exact_rest_only factory initialization success; actual ok=false code="<<actual.error.original.code<<" path="<<actual.error.original.path<<" kind="<<static_cast<int>(actual.error.kind)<<"\n";return 1;}std::cout<<"PASS: real exact_rest_only factory initialization succeeds\n";return 0;}catch(const std::exception& e){std::cerr<<"SETUP ERROR: "<<e.what()<<'\n';return 2;}}
