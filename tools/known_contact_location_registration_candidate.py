"""从固定Git源生成本人历史接触页接缝；不改当前中央或新增持久化/知识权限。"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess

BASE='bf33ca55f030f5d27b35fcda9041be9370964871'
ROOT='native/world/src/resident/domains/interaction/known_contacts/'
SHARED=['native/world/src/resident/kernel/live_view_queries.cpp','native/world/resident.cmake','game/ui/native_resident_backend.gd','game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','tools/verify.py']

def replace(source,old,new,count=1):
    """原锚点数量必须精确，不以宽匹配覆盖后继共享源。"""
    if source.count(old)!=count:raise ValueError((old,source.count(old),count))
    return source.replace(old,new)

def transform(path,source):
    """只挂原只读请求队列、有限场景页和实际编译清单。"""
    if path.endswith('/live_view_queries.cpp'):
        source='#include "resident/domains/interaction/known_contacts/known_contact_location_world.hpp"\n'+source
        anchor='    if(query=="personal_appointments")'
        return replace(source,anchor,'    if(query=="known_contact_locations"){boundary::fields(args,{},{"cursor"},"args");return query_known_contact_location_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}\n'+anchor)
    if path.endswith('/native_resident_backend.gd'):
        return source+'''\n## 复用已选原会面或文书能力；此页只说明最后亲见，不新增来源或位置信息。
func known_contact_locations_enabled() -> bool:
\tvar scenario: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{})
\treturn _installed and (scenario.get("personal_appointment_profile","")=="witnessed_personal_meetings_v1" or scenario.get("task_message_handoff_profile","") in ["observed_letter_delivery_v1","observed_letter_delivery_and_report_v2"])
'''
    if path.endswith('/resident_page_requests.gd') or path.endswith('/screen_composer.gd'):
        anchor='const PersonalAppointmentPanel=preload("res://ui/personal_appointment_panel.gd")'
        source=replace(source,anchor,anchor+'\nconst KnownContactLocationPanel=preload("res://ui/known_contact_location_panel.gd")')
        if path.endswith('/resident_page_requests.gd'):
            anchor='\tif not PersonalAppointmentPanel.prepare(host): return false'
            return replace(source,anchor,anchor+'\n\tif kernel.has_method("known_contact_locations_enabled") and kernel.known_contact_locations_enabled():\n\t\tif not KnownContactLocationPanel.prepare(host): return false')
        out=[];count=0
        for line in source.splitlines():
            out.append(line)
            if 'PersonalAppointmentPanel.build(' in line:
                count+=1;indent=line[:len(line)-len(line.lstrip())]
                out.append(indent+'if m._resident_mode and m.kernel.has_method("known_contact_locations_enabled") and m.kernel.known_contact_locations_enabled() and KnownContactLocationPanel.visible_here(m): KnownContactLocationPanel.build(m,_sidebar_section(m,stage,"KnownContactLocationArea","已知去向"))')
        if count!=2:raise ValueError(('scene mounts',count))
        return '\n'.join(out)+'\n'
    if path.endswith('/resident.cmake'):
        source=replace(source,'    src/resident/domains/interaction/appointments/personal_appointment_presence.cpp\n','    src/resident/domains/interaction/appointments/personal_appointment_presence.cpp\n'+''.join('    src/resident/domains/interaction/known_contacts/'+name+'.cpp\n' for name in ['known_contact_location','known_contact_location_view','known_contact_location_world']),2)
        source=replace(source,'    src/resident/domains/interaction/appointments/personal_appointment_presence.hpp\n','    src/resident/domains/interaction/appointments/personal_appointment_presence.hpp\n'+''.join('    src/resident/domains/interaction/known_contacts/'+name+'.hpp\n' for name in ['known_contact_location','known_contact_location_view','known_contact_location_world']))
        for name in ['known_contact_location','known_contact_message']:
            source+=f'\n# 本人原接触来源与未知去向窄门；不代整World或GUI。\nadd_executable(test_resident_{name} tests/resident/{name}_test.cpp)\ntarget_link_libraries(test_resident_{name} PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_{name} PRIVATE src)\ntarget_compile_options(test_resident_{name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_{name} COMMAND test_resident_{name})\n'
        return source
    if path.endswith('/verify.py'):
        return replace(source,'TESTS = [',"TESTS = ['test_known_contact_location_panel', ")
    raise ValueError(path)

def main():
    """保固定前像OID与独立候选；公共唯一写者另行审合。"""
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);parser.add_argument('--base',default=BASE);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    base=subprocess.check_output(['git','-C',str(args.repo),'rev-parse',args.base],text=True).strip();rows=[];patch=[]
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        original=subprocess.check_output(['git','-C',str(args.repo),'show',base+':'+path]);data=transform(path,original.decode()).encode();target=args.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);rows.append({'path':path,'base_oid':oid(original),'result_oid':oid(data)});patch.extend(difflib.unified_diff(original.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    (args.output/'candidate.patch').write_text(''.join(patch));(args.output/'manifest.json').write_text(json.dumps({'base':base,'shared':rows,'state':'SOURCE_ONLY','query':'known_contact_locations','profile':'reuse exact personal_appointments/1 or task_message_handoffs/1|2; no new marker, feature, command or persistence','scope':'only original direct two-person historical contact; current remote position is unknown','unrun':['behavior','full World','Godot controls','GUI']},indent=2)+'\n');print(args.output/'manifest.json')

if __name__=='__main__':main()
