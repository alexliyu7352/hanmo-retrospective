/* 汉末行记浏览器持久存储；只接收发行 profile 和存档封装，不执行游戏模拟。 */
(function publish(root) {
  'use strict';
  const MAX_BYTES = 64 * 1024 * 1024;
  const DB_VERSION = 2;
  const LEGACY_DATABASE = 'hanmo-e0';

  /** Return a typed adapter error without disguising failed writes as success. */
  function error(code, detail = '') {
    return {ok:false, code, detail:String(detail)};
  }

  /** Translate browser exceptions into stable, user-facing error categories. */
  function errorCode(cause) {
    if (cause?.name === 'QuotaExceededError') return 'QuotaExceeded';
    if (cause?.name === 'SecurityError' || cause?.name === 'NotAllowedError') return 'PermissionDenied';
    if (cause?.name === 'VersionError') return 'UnsupportedDatabaseVersion';
    if (cause?.name === 'InvalidStateError') return 'ConnectionClosed';
    return cause?.code || 'TransactionAborted';
  }

  /** Reject unknown slots and path-shaped identifiers before touching storage. */
  function validSlot(slot) {
    return typeof slot === 'string' && /^(?:auto-[1-3]|manual-(?:[1-9]|10))$/.test(slot);
  }

  /** Parse strict JSON while detecting duplicate keys, oversized strings and nesting. */
  function parseStrict(text, maxDepth = 32, maxString = 16384) {
    if (typeof text !== 'string') throw new Error('Expected text');
    let i = 0;
    /** Skip only the four whitespace characters permitted by JSON. */
    function whitespace() { while (i < text.length && /[\x20\t\r\n]/.test(text[i])) i++; }
    /** Decode one JSON string, letting the native parser validate all escapes. */
    function string() {
      const start=i++;
      while (i < text.length) {
        const c=text[i++];
        if(c==='\\') {i++;continue;}
        if(c==='"') {
          const value=JSON.parse(text.slice(start,i));
          if(value.length>maxString) throw new Error('Text field too long');
          return value;
        }
      }
      throw new Error('Unterminated string');
    }
    /** Parse one recursively bounded value with no executable content. */
    function value(depth) {
      if(depth>maxDepth) throw new Error('Nesting too deep');
      whitespace();const c=text[i];
      if(c==='"') return string();
      if(c==='{') {
        i++;whitespace();const obj=Object.create(null),keys=new Set();
        if(text[i]==='}') {i++;return obj;}
        for(;;) {
          whitespace();if(text[i]!=='"') throw new Error('Expected key');
          const key=string();if(keys.has(key)) throw new Error('Duplicate key');keys.add(key);
          whitespace();if(text[i++]!==':') throw new Error('Expected colon');
          obj[key]=value(depth+1);whitespace();const end=text[i++];
          if(end==='}') return obj;
          if(end!==',') throw new Error('Expected object separator');
        }
      }
      if(c==='[') {
        i++;whitespace();const list=[];
        if(text[i]===']') {i++;return list;}
        for(;;) {
          list.push(value(depth+1));whitespace();const end=text[i++];
          if(end===']') return list;
          if(end!==',') throw new Error('Expected array separator');
        }
      }
      for(const [literal,result] of [['true',true],['false',false],['null',null]]) {
        if(text.startsWith(literal,i)){i+=literal.length;return result;}
      }
      const match=/^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?/.exec(text.slice(i));
      if(!match) throw new Error('Invalid value');
      i+=match[0].length;const number=Number(match[0]);
      if(!Number.isFinite(number)) throw new Error('Non-finite number');return number;
    }
    const parsed=value(0);whitespace();if(i!==text.length) throw new Error('Trailing data');
    return parsed;
  }

  /** Validate an opaque HM-SAVE envelope; gameplay schema validation stays in Godot. */
  async function inspectEnvelope(text, expectedSlot = null) {
    if(typeof text!=='string' || text.length>MAX_BYTES) return error('OversizedInput');
    if(new TextEncoder().encode(text).length>MAX_BYTES) return error('OversizedInput');
    let e;
    try {e=parseStrict(text,32,MAX_BYTES);} catch(cause){return error('InvalidEnvelope',cause.message);}
    if(!e || typeof e!=='object' || Array.isArray(e) || e.magic!=='HM-SAVE') return error('InvalidEnvelope');
    if(e.format_version!==1) return error('UnsupportedFormat');
    for(const key of ['ruleset_id','content_version','engine_build','rng_backend','save_id']) {
      if(typeof e[key]!=='string' || !e[key] || e[key].length>256) return error('InvalidEnvelope',key);
    }
    if(expectedSlot !== null && e.save_id !== expectedSlot) return error('SaveIdMismatch');
    if(!Number.isSafeInteger(e.generation)||e.generation<0||e.generation>2147483647) return error('InvalidEnvelope','generation');
    if(typeof e.payload_json!=='string' || typeof e.payload_sha256!=='string' || !/^[0-9a-f]{64}$/.test(e.payload_sha256)) return error('InvalidEnvelope');
    const bytes=new TextEncoder().encode(e.payload_json);
    if(!Number.isInteger(e.payload_length)||e.payload_length!==bytes.length) return error('PayloadLengthMismatch');
    if(!root.crypto?.subtle) return error('CryptoUnavailable','HTTPS or localhost is required');
    const digest=await root.crypto.subtle.digest('SHA-256',bytes);
    const hex=Array.from(new Uint8Array(digest),b=>b.toString(16).padStart(2,'0')).join('');
    if(hex!==e.payload_sha256) return error('ChecksumMismatch');
    try {
      const payload=parseStrict(e.payload_json);
      if(!payload || typeof payload!=='object' || Array.isArray(payload)) return error('InvalidPayload');
    } catch(cause){return error('InvalidPayload',cause.message);}
    return {ok:true,envelope:e};
  }

  /** Manage two valid generations per slot using real IndexedDB transactions. */
  class Store {
    /** Retain the open connection and close cooperatively when an upgrade starts. */
    constructor(db) {this.db=db;db.onversionchange=()=>db.close();}

    /** Close the connection without deleting any user data. */
    close() {this.db.close();}

    /** Read raw slot metadata and both generations within one consistent transaction. */
    raw(slot) {
      return new Promise((resolve,reject)=>{
        let tx;
        try {tx=this.db.transaction(['slots','snapshots'],'readonly');} catch(e){reject(e);return;}
        let meta=null;const records={};
        const req=tx.objectStore('slots').get(slot);
        req.onsuccess=()=>{
          meta=req.result || null;
          if(!meta) return;
          for(const id of [meta.current,meta.previous].filter(Boolean)) {
            const q=tx.objectStore('snapshots').get(id);q.onsuccess=()=>{records[id]=q.result||null;};
          }
        };
        tx.oncomplete=()=>resolve({meta,records});
        tx.onabort=()=>reject(tx.error || new Error('Read aborted'));
        tx.onerror=()=>{};
      });
    }

    /** Load the newest valid snapshot, falling back without rewriting damaged data. */
    async get(slot) {
      if(!validSlot(slot)) return error('InvalidSlot');
      let raw;
      try {raw=await this.raw(slot);} catch(e){return error(errorCode(e),e.message);}
      if(!raw.meta) return {ok:false,code:'NotFound',generation:0};
      for(const [index,id] of [raw.meta.current,raw.meta.previous].entries()) {
        if(!id || !raw.records[id]) continue;
        const text=raw.records[id].text;
        const check=await inspectEnvelope(text,slot);
        if(check.ok && id===`${slot}:${check.envelope.generation}`) return {
          ok:true,generation:raw.meta.generation,snapshot_generation:check.envelope.generation,
          snapshot_id:id,recovered:index>0,text
        };
      }
      return {...error('CorruptSave'),generation:raw.meta.generation};
    }

    /** Validate, compare-and-swap, commit, then read back before reporting Saved. */
    async put(slot,text,expectedGeneration) {
      if(!validSlot(slot)) return error('InvalidSlot');
      if(!Number.isInteger(expectedGeneration)||expectedGeneration<0||expectedGeneration>=2147483647) return error('InvalidGeneration');
      const check=await inspectEnvelope(text,slot);if(!check.ok) return check;
      if(check.envelope.generation!==expectedGeneration+1) return error('InvalidGeneration');
      const previous=await this.get(slot);
      if(previous.generation!==undefined && previous.generation!==expectedGeneration) return error('GenerationConflict');
      if(!previous.ok && previous.code!=='NotFound' && previous.code!=='CorruptSave') return previous;
      const outcome=await new Promise(resolve=>{
        let tx,reason=null;
        try {
          tx=this.db.transaction(['slots','snapshots'],'readwrite');
          const slots=tx.objectStore('slots'),snapshots=tx.objectStore('snapshots');
          const get=slots.get(slot);
          get.onsuccess=()=>{
            const meta=get.result,actual=meta?.generation||0;
            if(actual!==expectedGeneration){reason=error('GenerationConflict');tx.abort();return;}
            const generation=actual+1,id=`${slot}:${generation}`;
            const backup=previous.ok?previous.snapshot_id:(meta?.current||null);
            try {
              snapshots.put({id,text});
              slots.put({id:slot,generation,current:id,previous:backup});
              for(const old of [meta?.current,meta?.previous]) {
                if(old && old!==id && old!==backup) snapshots.delete(old);
              }
            } catch(e){reason=error(errorCode(e),e.message);tx.abort();}
          };
          tx.oncomplete=()=>resolve({ok:true,generation:expectedGeneration+1});
          tx.onabort=()=>resolve(reason||error(errorCode(tx.error),tx.error?.message||'Transaction aborted'));
          tx.onerror=()=>{};
        } catch(e){resolve(error(errorCode(e),e.message));}
      });
      if(!outcome.ok) return outcome;
      const read=await this.get(slot);
      if(!read.ok || read.recovered || read.snapshot_generation!==outcome.generation || read.text!==text) return error('ReadbackFailed');
      return {ok:true,generation:outcome.generation,text};
    }

    /** Enumerate metadata only; loading a slot still validates its actual snapshot. */
    list() {
      return new Promise((resolve,reject)=>{
        let tx;
        try {tx=this.db.transaction('slots','readonly');} catch(e){reject(e);return;}
        const req=tx.objectStore('slots').getAll();
        tx.oncomplete=()=>resolve(req.result.sort((a,b)=>a.id.localeCompare(b.id)));
        tx.onabort=()=>reject(tx.error||new Error('Read aborted'));
      });
    }
  }

  /** 打开指定版本的本地库；缺省名称保留旧档，升级被阻止时绝不删除旧存储。 */
  async function open(name=LEGACY_DATABASE) {
    if(!root.indexedDB) throw Object.assign(new Error('IndexedDB unavailable'),{code:'StorageUnavailable'});
    return new Promise((resolve,reject)=>{
      let req,settled=false;
      try {req=root.indexedDB.open(name,DB_VERSION);} catch(e){reject(Object.assign(e,{code:errorCode(e)}));return;}
      req.onupgradeneeded=()=>{
        const db=req.result;
        for(const store of ['snapshots','slots','profile']) if(!db.objectStoreNames.contains(store)) db.createObjectStore(store,{keyPath:'id'});
      };
      req.onblocked=()=>{settled=true;reject(Object.assign(new Error('Close older tabs before upgrading'),{code:'UpgradeBlocked'}));};
      req.onerror=()=>{settled=true;reject(Object.assign(new Error(req.error?.message),{code:errorCode(req.error)}));};
      req.onsuccess=()=>{if(settled){req.result.close();return;}settled=true;resolve(new Store(req.result));};
    });
  }

  /** Download exact UTF-8 envelope text following a user's explicit click. */
  function download(text,filename='hanmo.hmsave') {
    const url=URL.createObjectURL(new Blob([text],{type:'application/json;charset=utf-8'}));
    const a=document.createElement('a');a.href=url;a.download=filename;a.click();
    setTimeout(()=>URL.revokeObjectURL(url),1000);return {ok:true};
  }

  /** Use the browser file chooser and return text without executing or importing it. */
  function pickFile() {
    return new Promise(resolve=>{
      const input=document.createElement('input');input.type='file';input.accept='.hmsave,.json';input.hidden=true;
      document.body.appendChild(input);
      input.addEventListener('cancel',()=>{input.remove();resolve(error('Cancelled'));},{once:true});
      input.addEventListener('change',async()=>{
        const file=input.files?.[0];input.remove();
        if(!file){resolve(error('Cancelled'));return;}
        if(file.size>MAX_BYTES){resolve(error('OversizedInput'));return;}
        try {resolve({ok:true,text:await file.text(),name:file.name});} catch(e){resolve(error('ReadFailed',e.message));}
      },{once:true});
      input.click();
    });
  }

  const stores=new Map();
  /** 旧调用省略或留空 profile 时继续打开原库；新发行标识使用互不冲突的独立名称，不自动回退读旧档。 */
  function profileDatabase(profile='') {
    if(profile==='') return LEGACY_DATABASE;
    if(typeof profile!=='string' || !/^[a-z][a-z0-9-]{0,63}$/.test(profile)) {
      throw Object.assign(new Error('Invalid release storage profile'),{code:'InvalidProfile'});
    }
    return `hanmo-profile:${profile}`;
  }

  /** 每项 Godot 请求携带自己的 profile；并行或交错请求固定绑定各自连接，失败开库只清除本库缓存。 */
  async function dispatch(operation,argumentsJSON,callback) {
    let result;
    try {
      const a=JSON.parse(argumentsJSON);
      const database=profileDatabase(a.profile);
      if(operation==='export') result=download(a.text,a.filename);
      else if(operation==='import') result=await pickFile();
      else {
        if(!stores.has(database)) stores.set(database,open(database).catch(e=>{stores.delete(database);throw e;}));
        const store=await stores.get(database);
        if(operation==='put') result=await store.put(a.slot,a.text,a.expected_generation);
        else if(operation==='get') result=await store.get(a.slot);
        else if(operation==='list') result={ok:true,slots:await store.list()};
        else result=error('UnknownOperation');
      }
    } catch(e){result=error(errorCode(e),e.message);}
    callback(JSON.stringify(result));
  }
  root.HMStorage={open,inspectEnvelope,parseStrict,validSlot,download,pickFile};
  root.HMBridge={dispatch};
})(globalThis);
