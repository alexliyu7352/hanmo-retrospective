"""在完整Family/Creation/Birth之后检查预约当事人的真实存在与死因前界；不直接写中央。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
from task_extension_registration_candidate import replace
BASE='243c998284d3d87589111164d5eb163f8c99ddaf'
PATHS=['native/world/src/resident/kernel/world_bundle.cpp','native/world/resident.cmake']

def transform(path,s):
    """只移动整预约晚门，旧前序、Plan与接册原钩均保留。"""
    if path.endswith('/world_bundle.cpp'):
        line='    valid=validate_personal_appointment_world(*this);if(!valid.ok)return valid;\n'
        s=replace(s,line,'')
        anchor='    valid=validate_family_birth_world(*this);if(!valid.ok)return valid;\n'
        return replace(s,anchor,anchor+'    // 原创建/出生与军事死因已完整验证，再核预约本人来源和实际缓存；不以今日alive倒推旧承诺。\n'+line)
    if path.endswith('/resident.cmake'):
        anchor='    src/resident/domains/interaction/appointments/personal_appointment_receipts.cpp'
        if s.count(anchor)!=2:raise ValueError('two actual source lists required')
        s=s.replace(anchor,anchor+'\n    src/resident/domains/interaction/appointments/personal_appointment_presence.cpp')
        anchor='    src/resident/domains/interaction/appointments/personal_appointment_boundary.hpp'
        s=replace(s,anchor,anchor+'\n    src/resident/domains/interaction/appointments/personal_appointment_presence.hpp')
        if '    src/resident/domains/family/actor_presence_read.hpp' not in s:
            anchor='    src/resident/domains/family/actor_age_read.hpp'
            s=replace(s,anchor,anchor+'\n    src/resident/domains/family/actor_presence_read.hpp')
        s+='\n# 预约本人来源只读坏档门，不以现存人物补造旧在场。\nadd_executable(test_resident_personal_appointment_presence tests/resident/personal_appointment_presence_test.cpp)\ntarget_link_libraries(test_resident_personal_appointment_presence PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_personal_appointment_presence PRIVATE src)\ntarget_compile_options(test_resident_personal_appointment_presence PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_personal_appointment_presence COMMAND test_resident_personal_appointment_presence)\n'
        return s
    raise ValueError(path)

def main():
    """生成不可变两共享叶及精确基准OID，S4独立头来源在manifest明确。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',required=True,type=Path);p.add_argument('--base',default=BASE);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
    if a.output.exists():raise ValueError('fresh output required')
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();rows=[];patch=[]
    for path in PATHS:
        old=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);new=transform(path,old.decode()).encode();target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(new);rows.append({'path':path,'base_oid':oid(old),'result_oid':oid(new)});patch.extend(difflib.unified_diff(old.decode().splitlines(True),new.decode().splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':a.base,'shared':rows,'dependency':{'commit':'a26a894eb75bcb894d7ca07d64e93a63f91ddcac','path':'native/world/src/resident/domains/family/actor_presence_read.hpp'},'status':'SOURCE_ONLY','scope':'after full Family/Creation/Birth plus original military validation; no Growth convalescence acceptance claim'},ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
