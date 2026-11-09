"""原N1无活动边界冻结专用ccache普通entry为低RAM分包；仅本地准备，Space仍唯一owner上传。"""
from pathlib import Path
import argparse
import fcntl
import importlib.util
import json
import os
import re
import resource
import shutil
import tarfile
import sys
sys.dont_write_bytecode=True
_spec=importlib.util.spec_from_file_location('cache_dependency_identity',Path(__file__).with_name('depfile_incremental.py'))
D=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(D)
require=D.require


def scan_entries(root):
    """只遍历专用cache顶层hex桶，排临时/统计；拒链接和非普通文件，不触根部凭证。"""
    root=Path(root);require(root.is_dir() and not root.is_symlink(),'ordinary_dedicated_cache_required')
    rows=[]
    for top in sorted(root.iterdir()):
        if not re.fullmatch('[0-9a-f]',top.name):continue
        require(top.is_dir() and not top.is_symlink(),'cache_bucket_not_ordinary_directory')
        for current,dirs,files in os.walk(top,followlinks=False):
            dirs.sort();files.sort()
            for name in dirs:require(not (Path(current)/name).is_symlink(),'cache_symlink_forbidden')
            for name in files:
                path=Path(current)/name
                if name in ('stats','CACHEDIR.TAG') or name.endswith(('.lock','.tmp')):continue
                require(path.is_file() and not path.is_symlink(),'cache_entry_not_ordinary_file')
                item=D.ident(path);item['relative_path']=path.relative_to(root).as_posix();rows.append(item)
    return rows


def unpreserved(rows,previous):
    """按path+内容版本排重，旧包待上传仍有真实本地冻结依据，不冒已Space成功。"""
    known={(x['relative_path'],x['bytes'],x['sha256']) for x in previous}
    return [x for x in rows if (x['relative_path'],x['bytes'],x['sha256']) not in known]


def chunk_entries(rows,limit):
    """完整entry不切割；普通包按上限，大于限额的单entry显式独包。"""
    require(isinstance(limit,int) and limit>0,'positive_chunk_bytes_required')
    groups=[];group=[];total=0
    for row in rows:
        if group and total+row['bytes']>limit:groups.append(group);group=[];total=0
        group.append(row);total+=row['bytes']
        if total>=limit:groups.append(group);group=[];total=0
    if group:groups.append(group)
    return groups


def checked(ref):
    """不猜服务状态；仅验证请求明确指向的冻结文件原bytes。"""
    require(D.sha(ref['path'])==ref['sha256'],'snapshot_control_changed')
    if 'bytes' in ref:require(Path(ref['path']).stat().st_size==ref['bytes'],'snapshot_control_size_changed')
    return json.loads(Path(ref['path']).read_text())


def save(path,value):
    """所有本地原件只写一次，不覆旧包、失败残件或服务回执。"""
    with Path(path).open('x') as out:json.dump(value,out,ensure_ascii=False,indent=2);out.write('\n')


def freeze(request,lock_handle):
    """调用者持原queue.lock且原scheduler已终态/active空；整个snapshot保持该锁。"""
    root=Path(request['cache_dir']);out=Path(request['output_directory'])
    require(out.is_absolute() and not out.exists(),'fresh_snapshot_directory_required')
    require(request['kind']=='QUIESCENT_N1_CCACHE_SNAPSHOT_REQUEST','explicit_snapshot_request_required')
    terminal=checked(request['scheduler_terminal']);active=checked(request['active_calls'])
    require(terminal['status'] in ('complete','failed','drained','blocked','packed_memory_stop','bounded_memory_stop','disk_low_watermark') and not active,'cache_writers_not_quiescent')
    path_stat=Path(request['queue_lock']).stat();fd_stat=os.fstat(lock_handle.fileno())
    require((path_stat.st_dev,path_stat.st_ino)==(fd_stat.st_dev,fd_stat.st_ino),'wrong_queue_lock')
    profile=checked(request['profile'])
    require(profile['cache_dir']==str(root) and profile['ccache']==request['ccache_identity'],'cache_profile_identity_changed')
    config=Path(profile['configuration']['path']).read_text()
    require('hard_link = false\n' in config and 'file_clone = false\n' in config and 'remote_storage = # explicit empty value\n' in config,'self_contained_local_cache_required')
    require(D.sha(profile['configuration']['path'])==profile['configuration']['sha256'],'cache_configuration_drift')
    scheduler_ref=request['scheduler'];require(D.sha(scheduler_ref['path'])==scheduler_ref['sha256'],'original_scheduler_changed')
    spec=importlib.util.spec_from_file_location('cache_original_memory_guard',scheduler_ref['path']);scheduler=importlib.util.module_from_spec(spec);spec.loader.exec_module(scheduler)
    def admission(required=0):
        """沿实际可用物理内存与原1GB磁盘底线，不凭上次读数绕过。"""
        memory=scheduler.memory_snapshot();require(memory['visible_available_bytes']>=4*1024**3,'cache_snapshot_available_below_4GiB')
        require(shutil.disk_usage(out.parent).free>=1_000_000_000+required,'cache_snapshot_disk_floor')
    admission();rows=scan_entries(root);previous=[];archives=[]
    for ref in request.get('previous_indexes',[]):
        prior=checked(ref);require(prior['cache_dir']==str(root),'previous_snapshot_other_cache')
        for archive in prior['archives']:
            require(D.sha(archive['path'])==archive['sha256'],'previous_frozen_archive_missing_or_changed')
        previous+=prior.get('preserved_entry_versions',prior['entries']);archives+=prior['archives']
    selected=unpreserved(rows,previous);out.mkdir()
    for number,group in enumerate(chunk_entries(selected,request.get('chunk_bytes',64*1024**2))):
        admission(sum(x['bytes'] for x in group)+1024**2)
        name=f'cache-{number:04d}.tar.gz';pending=out/(name+'.writing');final=out/name
        with tarfile.open(str(pending),'w|gz',compresslevel=1) as archive:
            for item in group:
                admission()
                path=Path(item['path']);require(D.ident(path)=={k:v for k,v in item.items() if k!='relative_path'},'cache_entry_changed_before_archive')
                info=archive.gettarinfo(str(path),arcname='cache/'+item['relative_path']);require(info.isfile(),'non_file_cache_payload')
                info.uid=info.gid=0;info.uname=info.gname='';info.mode=0o600
                with path.open('rb') as stream:archive.addfile(info,stream)
                require(D.ident(path)=={k:v for k,v in item.items() if k!='relative_path'},'cache_entry_changed_during_archive')
        pending.rename(final);archives.append({'path':str(final),'bytes':final.stat().st_size,'sha256':D.sha(final)})
    require(scan_entries(root)==rows,'cache_membership_changed_during_snapshot')
    result={'kind':'QUIESCENT_CCACHE_BYTES_SNAPSHOT_NOT_SPACE_RECEIPT','cache_dir':str(root),
            'ccache_identity':request['ccache_identity'],'profile':request['profile'],'scheduler_terminal':request['scheduler_terminal'],
            'new_entry_versions':len(selected),'entry_versions_in_current_view':len(rows),'entries':rows,
            'preserved_entry_versions':list({(x['relative_path'],x['bytes'],x['sha256']):x for x in previous+rows}.values()),
            'archives':list({x['sha256']:x for x in archives}.values()),'new_TUs':0,'restore_tested':False,'space_uploaded':False,
            'excluded':'tmp, statistics, locks, root-level files; entries are preserved bytes, not an asserted cache hit'}
    save(out/'snapshot-index.json',result);return result


def main():
    """显式子进程nice19/AS256MiB，不安装/调用ccache；可继承中央真实原锁FD。"""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--request',required=True);parser.add_argument('--sha256',required=True);parser.add_argument('--lock-fd',type=int);args=parser.parse_args()
    require(D.sha(args.request)==args.sha256,'cache_snapshot_request_changed');request=json.loads(Path(args.request).read_text())
    os.nice(max(0,19-os.getpriority(os.PRIO_PROCESS,0)));resource.setrlimit(resource.RLIMIT_AS,(256*1024**2,256*1024**2))
    with (os.fdopen(os.dup(args.lock_fd),'a') if args.lock_fd is not None else open(request['queue_lock'],'a')) as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        result=freeze(request,lock)
        index=Path(request['output_directory'])/'snapshot-index.json'
        print(json.dumps({'kind':result['kind'],'new_entry_versions':result['new_entry_versions'],'index':{'path':str(index),'sha256':D.sha(index)},'space_uploaded':False}),flush=True)
    return 0


if __name__=='__main__':raise SystemExit(main())
