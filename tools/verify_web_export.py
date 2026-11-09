#!/usr/bin/env python3
"""用独立浏览器档点击真实 Godot 导出，核验基础操作和存档；不注入玩法状态，视觉风格与跨平台代表档另验。"""
from __future__ import annotations
import argparse
from functools import partial
import hashlib
from http.server import ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import threading
import time

from playwright.sync_api import sync_playwright
from serve import GameHandler


class QuietHandler(GameHandler):
    """复用正式本地服务器的 MIME；浏览器请求与错误存入报告，不刷访问日志。"""
    def log_message(self, fmt: str, *args: object) -> None:
        """不打印常规访问日志。"""


class BrowserCheck:
    """只操作真实输入并读取存档回执；坐标对应当前 1280×800 基础布局，改版后须重新目视核验。"""
    def __init__(self, folder: Path, evidence: Path, executable: str):
        """绑定已导出目录与全新证据目录，禁止复用日常浏览器档或覆盖既有验证。"""
        self.folder,self.evidence,self.executable = folder,evidence,executable
        self.checks,self.console,self.responses = [],[],[]
        self.profile = 'tests-web-export'
        self.page = None

    def check(self, condition: bool, label: str) -> None:
        """记录实际断言；失败立即停止依赖动作并保留此前截图与消息。"""
        self.checks.append({'label':label,'ok':bool(condition)})
        if not condition: raise AssertionError(label)

    def screenshot(self, name: str) -> None:
        """保存当前真实画布，不合成图片或以字形检查替代画面复核。"""
        self.page.screenshot(path=str(self.evidence/(name+'.png')))

    def click(self, x: int, y: int) -> None:
        """通过浏览器鼠标点击当前画布，留出真实界面及异步操作刷新时间。"""
        self.page.mouse.click(x,y)
        self.page.wait_for_timeout(350)

    def boot(self, context, url: str) -> None:
        """只在原 HTML 入口点击启动，观察现有存储桥接回调，不代替 Godot 调用业务命令。"""
        self.page = context.new_page()
        # 提前启用文件对话框拦截；画布原始鼠标事件不像 DOM 定位点击那样等待自动操作检查。
        self.page.on('filechooser',lambda chooser: None)
        self.page.on('console',lambda message:self.console.append({'type':message.type,'text':message.text}))
        self.page.on('pageerror',lambda error:self.console.append({'type':'pageerror','text':str(error)}))
        self.page.on('response',lambda response:self.responses.append({'url':response.url,'status':response.status}))
        self.page.goto(url)
        self.page.evaluate('''() => {
          window.saveReceipts=[]; window.saveRequests=[];
          const dispatch=HMBridge.dispatch;
          HMBridge.dispatch=(operation,args,callback)=>{
            window.saveRequests.push({operation,args:JSON.parse(args),active:navigator.userActivation.isActive});
            return dispatch(operation,args,text=>{
              window.saveReceipts.push({operation,args:JSON.parse(args),result:JSON.parse(text)});
              callback(text);
            });
          };
        }''')
        started = time.monotonic()
        self.page.get_by_role('button',name='进入旅途').click()
        self.page.locator('#launch').wait_for(state='hidden',timeout=60000)
        self.page.wait_for_timeout(600)
        self.check(time.monotonic()-started<60,'原 Godot Web 引擎实际启动并离开加载层')

    def receipt(self, operation: str, since: int = 0) -> dict:
        """等候真实 Godot 按钮请求的存储结果，不直接写 IndexedDB 或制造回调。"""
        self.page.wait_for_function('([kind,since])=>saveReceipts.slice(since).some(x=>x.operation===kind)',arg=[operation,since],timeout=15000)
        return self.page.evaluate('([kind,since])=>saveReceipts.slice(since).find(x=>x.operation===kind)',[operation,since])

    def receipts_count(self) -> int:
        """读取观察记录的位置，防止前一次成功被当作本次操作完成。"""
        return self.page.evaluate('saveReceipts.length')

    def download(self, name: str) -> dict:
        """点击游戏导出按钮取得浏览器真实下载文件，核验正式封装与载荷摘要。"""
        with self.page.expect_download(timeout=15000) as event: self.click(1085,36)
        path = self.evidence/(name+'.hmsave')
        event.value.save_as(path)
        envelope = json.loads(path.read_text(encoding='utf-8'))
        self.check(envelope['magic']=='HM-SAVE' and hashlib.sha256(envelope['payload_json'].encode()).hexdigest()==envelope['payload_sha256'],'浏览器下载包含实际完整存档及正确摘要：'+name)
        return {'path':path,'state':json.loads(envelope['payload_json']),'text':path.read_text(encoding='utf-8')}

    def import_file(self, path: Path) -> None:
        """由游戏按钮打开真实浏览器文件选择器，文件文本仍交给原游戏严格解码。"""
        before = self.receipts_count()
        try:
            with self.page.expect_file_chooser(timeout=15000) as event: self.click(1155,36)
        except Exception:
            self.screenshot('failed-file-chooser')
            observed = self.page.evaluate('({requests:saveRequests,receipts:saveReceipts,inputs:[...document.querySelectorAll("input")].map(x=>({type:x.type,hidden:x.hidden}))})')
            (self.evidence/'failed-file-chooser.json').write_text(json.dumps(observed,ensure_ascii=False,indent=2)+'\n')
            raise
        event.value.set_files(path)
        result = self.receipt('import',before)
        self.check(result['result']['ok'],'浏览器文件选择器实际读取所选文件')
        self.page.wait_for_timeout(500)

    def run(self, playwright, base: str) -> None:
        """验证真实输入、跨浏览器进程恢复、好坏文件导入及 profile 隔离，不代替最终军事和世代档互通。"""
        options = {'executable_path':self.executable,'headless':True,'viewport':{'width':1280,'height':800},
                   'accept_downloads':True,'args':['--no-sandbox','--disable-dev-shm-usage','--enable-unsafe-swiftshader']}
        profile_dir = self.evidence/'browser-profile'
        context = playwright.chromium.launch_persistent_context(str(profile_dir),**options)
        try:
            self.boot(context,base+'/?profile='+self.profile)
            self.screenshot('01-title-chinese')
            self.click(326,354)
            self.click(320,738)
            self.click(653,289)
            before = self.receipts_count(); self.click(946,36)
            saved = self.receipt('put',before)
            self.check(saved['result']['ok'] and saved['args']['profile']==self.profile,'游戏保存实际写入指定独立 profile')
            state = json.loads(json.loads(saved['args']['text'])['payload_json'])
            self.check(state['tick']==1 and state['actors'][state['player_actor_id']]['location']['place_id']=='west.office','正常等候和移动改变同一世界时间与真实场所')
            self.screenshot('02-saved-office')
            exported = self.download('first-export')
            self.check(exported['state']==state,'下载的世界与刚保存的完整状态相同')
            self.screenshot('03-downloaded')
        finally:
            context.close()
        context = playwright.chromium.launch_persistent_context(str(profile_dir),**options)
        try:
            self.boot(context,base+'/?profile='+self.profile)
            before = self.receipts_count(); self.click(326,407)
            restored = self.receipt('get',before)
            self.check(restored['result']['ok'] and restored['result']['text']==saved['args']['text'],'关闭整个浏览器进程后，继续旅途读取同一持久化字节')
            again = self.download('after-restart')
            self.check(again['state']==state,'继续旅途真实恢复人物、时间、财物与约定')
            self.screenshot('04-restored-office')
            self.click(120,738); self.click(320,738)
            moved = self.download('before-import')
            self.check(moved['state']['tick']==2 and moved['state']['actors'][state['player_actor_id']]['location']['place_id']=='west.street','导入前通过正常输入继续生活，产生不同实际世界')
            bad = self.evidence/'invalid.hmsave'; bad.write_text('{bad file',encoding='utf-8')
            self.import_file(bad)
            self.screenshot('05-invalid-import-retained')
            retained = self.download('after-invalid-import')
            self.check(retained['state']==moved['state'],'损坏文件没有覆盖当前真实旅途')
            self.import_file(exported['path'])
            self.screenshot('06-imported-original')
            imported = self.download('after-valid-import')
            self.check(imported['state']==state,'正常文件导入把运行世界恢复到下载时的原状态')
            self.check(not any(x['operation']=='put' for x in self.page.evaluate('saveReceipts')),'导入与载入没有未经保存按钮写入存档槽')
            self.page.close()
            other = 'tests-web-export-other'
            self.boot(context,base+'/?profile='+other)
            before = self.receipts_count(); self.click(326,407)
            missing = self.receipt('get',before)
            self.check(missing['result'].get('code')=='NotFound','另一 profile 的继续按钮不会读取已有 profile 存档')
            self.screenshot('07-empty-other-profile')
            self.click(326,354)
            before = self.receipts_count(); self.click(946,36)
            other_saved = self.receipt('put',before)
            self.check(other_saved['result']['ok'] and other_saved['args']['profile']==other,'另一 profile 可以保存自己的新旅途')
            self.page.close()
            self.boot(context,base+'/?profile='+self.profile)
            before = self.receipts_count(); self.click(326,407)
            first = self.receipt('get',before)
            self.check(first['result']['text']==saved['args']['text'],'第二 profile 写入后，第一 profile 存档完整保留')
            self.screenshot('08-first-profile-retained')
            self.check(not [x for x in self.console if x['type'] in ['error','pageerror']],'实际网页无脚本错误或浏览器资源错误')
            self.check(not [x for x in self.responses if x['status']>=400],'实际页面、引擎、内容包与音频依赖均成功加载')
        finally:
            context.close()


def main() -> int:
    """显式指定内部包和全新证据目录，在 /data 独立浏览器档完成实际操作，输出可复核范围。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory',type=Path,required=True)
    parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--browser',default=os.environ.get('CHROMIUM_BIN') or shutil.which('google-chrome') or shutil.which('chromium'))
    args = parser.parse_args()
    if not args.browser or not (args.directory/'BUILD.json').is_file(): parser.error('需要本机浏览器与实际导出清单')
    if args.evidence.exists() and any(args.evidence.iterdir()): parser.error('证据目录必须为空，不能覆盖旧浏览器档')
    args.evidence.mkdir(parents=True,exist_ok=True)
    checker = BrowserCheck(args.directory.resolve(),args.evidence.resolve(),args.browser)
    server = ThreadingHTTPServer(('127.0.0.1',0),partial(QuietHandler,directory=str(args.directory.resolve())))
    worker = threading.Thread(target=server.serve_forever,daemon=True);worker.start()
    failure = None
    try:
        with sync_playwright() as playwright: checker.run(playwright,f'http://127.0.0.1:{server.server_port}')
    except Exception as exc:
        failure = str(exc)
    finally:
        server.shutdown();worker.join(timeout=3);server.server_close()
    report = {'ok':failure is None,'failure':failure,'checks':checker.checks,'console':checker.console,
              'responses':checker.responses,'export':str(args.directory.resolve()),
              'limits':['当前基础布局的真实点击；视觉风格不是最终验收','中文字形截图仍需目视复核',
                        '最终军事与世代 Linux/Web 代表存档往返尚未执行','本次使用软件 WebGL，不能替代目标硬件性能验收']}
    (args.evidence/'REPORT.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('WEB_EXPORT_CHECKS='+str(len(checker.checks))+' FAILURES='+str(1 if failure else 0))
    if failure: print(failure)
    return 1 if failure else 0


if __name__ == '__main__':
    raise SystemExit(main())
