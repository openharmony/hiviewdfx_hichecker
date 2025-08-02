/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

declare function requireNapi(napiModuleName: string): any;
declare function registerArkUIObjectLifeCycleCallback(callback: (weakRef: WeakRef<object>, msg: string) => void);
declare function unregisterArkUIObjectLifeCycleCallback();

let util = requireNapi('util');
let fs = requireNapi('file.fs');
let hidebug = requireNapi('hidebug');
let cryptoFramework = requireNapi('security.cryptoFramework');
let jsLeakWatcherNative = requireNapi('hiviewdfx.jsleakwatchernative');

const ERROR_CODE_INVALID_PARAM = 401;
const ERROR_MSG_INVALID_PARAM = 'Parameter error. Please check!';
const ERROR_CODE_ENABLE_INVALID = 10801001;
const ERROR_MSG_ENABLE_INVALID = 'The parameter isEnabled invalid. Please check!';
const ERROR_CODE_CONFIG_INVALID = 10801002;
const ERROR_MSG_CONFIG_INVALID = 'The parameter config invalid. Please check!';
const ERROR_CODE_CALLBACK_INVALID = 10801003;
const ERROR_MSG_CALLBACK_INVALID = 'The parameter callback invalid. Please check!';


let errMap = new Map();
errMap.set(ERROR_CODE_INVALID_PARAM, ERROR_MSG_INVALID_PARAM);
errMap.set(ERROR_CODE_ENABLE_INVALID, ERROR_MSG_ENABLE_INVALID);
errMap.set(ERROR_CODE_CONFIG_INVALID, ERROR_MSG_CONFIG_INVALID);
errMap.set(ERROR_CODE_CALLBACK_INVALID, ERROR_MSG_CALLBACK_INVALID);

class BusinessError extends Error {
  constructor(code) {
    let msg = '';
    if (errMap.has(code)) {
      msg = errMap.get(code);
    } else {
      msg = ERROR_MSG_INVALID_PARAM;
    }
    super(msg);
    this.code = code;
  }
}

let enabled = false;
let watchObjMap = new Map();

const registry = new FinalizationRegistry((hash) => {
  if (watchObjMap.has(hash)) {
    watchObjMap.delete(hash);
  }
});
const MAX_FILE_NUM = 20;

function getLeakList() {
  return Array.from(watchObjMap.values());
}

function createHeapDumpFile(fileName, filePath, isRawHeap) {
  let suffix = isRawHeap ? '.rawheap' : '.heapsnapshot';
  let heapDumpFileName = fileName + suffix;
  let desFilePath = filePath + '/' + heapDumpFileName;
  if (isRawHeap) {
    jsLeakWatcherNative.dumpRawHeap(desFilePath);
  } else {
    hidebug.dumpJsHeapData(fileName);
    fs.moveFileSync('/data/storage/el2/base/files/' + heapDumpFileName, desFilePath, 0);
  }
  return getHeapDumpSHA256(desFilePath);
}

function getHeapDumpSHA256(filePath) {
  let md = cryptoFramework.createMd('SHA256');
  let heapDumpFile = fs.openSync(filePath, fs.OpenMode.READ_WRITE);
  let bufSize = 40960;
  let readSize = 0;
  let buf = new ArrayBuffer(bufSize);
  let readOptions = { offset: readSize, length: bufSize };
  let readLen = fs.readSync(heapDumpFile.fd, buf, readOptions);

  while (readLen > 0) {
    md.updateSync({ data: new Uint8Array(buf.slice(0, readLen)) });
    readSize += readLen;
    readOptions.offset = readSize;
    readLen = fs.readSync(heapDumpFile.fd, buf, readOptions);
  }
  fs.closeSync(heapDumpFile);

  let digestOutPut = md.digestSync();
  return Array.from(digestOutPut.data, byte => ('0' + byte.toString(16)).slice(-2)).join('').toUpperCase();
}

function deleteOldFile(filePath) {
  let listFileOption = {
    recursion: false,
    listNum: 0,
    filter: {
      displayName: ['*.heapsnapshot', '*.jsleaklist', '*.rawheap'],
    }
  };
  let files = fs.listFileSync(filePath, listFileOption);
  if (files.length > MAX_FILE_NUM) {
    const regex = /(\d+)\.(heapsnapshot|jsleaklist|rawheap)/;
    files.sort((a, b) => {
      const matchA = a.match(regex);
      const matchB = b.match(regex);
      const timeStampA = matchA ? parseInt(matchA[1]) : 0;
      const timeStampB = matchB ? parseInt(matchB[1]) : 0;
      return timeStampA - timeStampB;
    });
    for (let i = 0; i < files.length - MAX_FILE_NUM; i++) {
      fs.unlinkSync(filePath + '/' + files[i]);
      console.log(`File: ${files[i]} is deleted.`);
    }
  }  
}

function registerObject(obj, msg) {
  if (!obj) {
    return;
  }
  let objMsg = { hash: util.getHash(obj), name: obj.constructor.name, msg: msg };
  watchObjMap.set(objMsg.hash, objMsg);
  registry.register(obj, objMsg.hash);
}

function executeRegister(config: Array<string>) {
  if (config.includes('CustomComponent')) {
    registerArkUIObjectLifeCycleCallback((weakRef, msg) => {
      if (!weakRef) {
        return;
      }
      let obj = weakRef.deref();
      registerObject(obj, msg);
    });
  }
  if (config.includes('Window')) {
    jsLeakWatcherNative.registerWindowLifeCycleCallback((obj) => {
      registerObject(obj, '');
    });
  }
  if (config.includes('NodeContainer') || config.includes('XComponent')) {
    jsLeakWatcherNative.registerArkUIObjectLifeCycleCallback((weakRef) => {
      if (!weakRef) {
        return;
      }
      let obj = weakRef.deref();
      registerObject(obj, '');
    });
  }
}

function dumpInner(filePath, needSandBox, isRawHeap) {
  if (!enabled) {
    return [];
  }
  if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
    throw new BusinessError(ERROR_CODE_INVALID_PARAM);
  }
  const fileTimeStamp = new Date().getTime().toString();
  try {
    const heapDumpSHA256 = createHeapDumpFile(fileTimeStamp, filePath, isRawHeap);
    let file = fs.openSync(filePath + '/' + fileTimeStamp + '.jsleaklist', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
    let leakObjList = getLeakList();
    let result = { snapshot_hash: heapDumpSHA256, leakObjList: leakObjList };
    fs.writeSync(file.fd, JSON.stringify(result));
    fs.closeSync(file);
  } catch (error) {
    console.log('Dump heaoSnapShot or LeakList failed! ' + error);
    return [];
  }

  try {
    deleteOldFile(filePath);
  } catch (e) {
    console.log('Delete old files failed! ' + e);
    return [];
  }
  if (needSandBox) {
    return [filePath + '/' + fileTimeStamp + '.jsleaklist', filePath + '/' + fileTimeStamp + '.rawheap'];
  } else {
    return [fileTimeStamp + '.jsleaklist', fileTimeStamp + '.heapsnapshot'];
  }
}

function shutdownJsLeakWatcher() {
  jsLeakWatcherNative.removeTask();
  jsLeakWatcherNative.unregisterArkUIObjectLifeCycleCallback();
  jsLeakWatcherNative.unregisterWindowLifeCycleCallback();
  unregisterArkUIObjectLifeCycleCallback();
  watchObjMap.clear();
}

let jsLeakWatcher = {
  watch: (obj, msg) => {
    if (obj === undefined || obj === null || msg === undefined || msg === null) {
      throw new BusinessError(ERROR_CODE_INVALID_PARAM);
    }
    if (!enabled) {
      return;
    }
    let objMsg = { hash: util.getHash(obj), name: obj.constructor.name, msg: msg };
    watchObjMap.set(objMsg.hash, objMsg);
    registry.register(obj, objMsg.hash);
  },
  check: () => {
    if (!enabled) {
      return '';
    }
    let leakObjList = getLeakList();
    return JSON.stringify(leakObjList);
  },
  dump: (filePath) => {
    if (filePath === undefined || filePath === null) {
      throw new BusinessError(ERROR_CODE_INVALID_PARAM);
    }
    return dumpInner(filePath, false, false);
  },
  enable: (isEnable) => {
    if (isEnable === undefined || isEnable === null) {
      throw new BusinessError(ERROR_CODE_INVALID_PARAM);
    }
    enabled = isEnable;
    if (!isEnable) {
      watchObjMap.clear();
    }
  },
  enableLeakWatcher: (isEnable: boolean, config: Array<string>, callback: Callback<Array<string>>) => {
    if (isEnable === undefined || isEnable === null) {
      throw new BusinessError(ERROR_CODE_ENABLE_INVALID);
    }
    if (config === undefined || config === null) {
      throw new BusinessError(ERROR_CODE_CONFIG_INVALID);
    }
    if (callback === undefined || callback === null) {
      throw new BusinessError(ERROR_CODE_CALLBACK_INVALID);
    }
    if (isEnable === enabled) {
      console.log('JsLeakWatcher is already started or stopped.');
      return;
    }
    enabled = isEnable;
    if (!isEnable) {
      shutdownJsLeakWatcher();
      return;
    }

    const validConfig = ['CustomComponent', 'Window', 'NodeContainer', 'XComponent', 'Ability'];
    for (let i = 0; i < config.length; i++) {
      if (!validConfig.includes(config[i])) {
        throw new BusinessError(ERROR_CODE_CONFIG_INVALID);
      }
    }
    if (config.length === 0) {
      config = validConfig;
    }

    const context : Context = getContext(this);
    const filePath : string = context ? context.filesDir : '/data/storage/el2/base/files/';

    jsLeakWatcherNative.handleGCTask(() => {
      ArkTools.forceFullGC();
    });
    jsLeakWatcherNative.handleDumpTask(() => {
      let fileArray = dumpInner(filePath, true, true);
      callback(fileArray);
    });
    jsLeakWatcherNative.handleShutdownTask(() => {
      enabled = false;
      shutdownJsLeakWatcher();
    });
    executeRegister(config);
  }
};

export default jsLeakWatcher;
