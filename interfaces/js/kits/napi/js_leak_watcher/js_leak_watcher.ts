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
let application = requireNapi('app.ability.application');
let bundleManager = requireNapi('bundle.bundleManager');

const JSLEAK_ROOT_DIR_NAME = 'jsleak';
const ERROR_CODE_INVALID_PARAM = 401;
const ERROR_MSG_INVALID_PARAM = 'Parameter error. Please check!';
const ERROR_CODE_ENABLE_INVALID = 10801001;
const ERROR_MSG_ENABLE_INVALID = 'The parameter isEnabled invalid. Please check!';
const ERROR_CODE_CONFIG_INVALID = 10801002;
const ERROR_MSG_CONFIG_INVALID = 'The parameter config invalid. Please check!';
const ERROR_CODE_CALLBACK_INVALID = 10801003;
const ERROR_MSG_CALLBACK_INVALID = 'The parameter callback invalid. Please check!';

interface LeakWatcherConfig {
  objectWatcher: string;
  objectUniqueIDs: Array<number>;
  checkInterval: number;
  retainedVisibleThreshold: number;
  retainedInvisibleThreshold: number;
  maxStoredHeapDumps: number;
  dumpHeapWaitTimeMs: number;
  whiteList: Array<string>;
}

let leakWatcherConfig: LeakWatcherConfig = {
  objectWatcher: '',
  objectUniqueIDs: [],
  checkInterval: 30000,
  retainedVisibleThreshold: 5,
  retainedInvisibleThreshold: 1,
  maxStoredHeapDumps: 10,
  dumpHeapWaitTimeMs: 5000,
  whiteList: []
};

const stateForeground = 1;
const stateBackground = 3;

interface AppStateInformation {
  applicationContext: Context;
  bundleFlags: number;
  bundleName: string;
  processInformation: any[];
  applicationState: number;
  exists: any[];
  currentLeakList: any[];
  isConfigObj: boolean;
  isGC: boolean;
  stateForeground: number;
  stateBackground: number;
}

let appState: AppStateInformation = {
  applicationContext: getApplicationContext(),
  bundleFlags: bundleManager.BundleFlag.GET_BUNDLE_INFO_DEFAULT,
  bundleName: '',
  processInformation: [],
  applicationState: 2,
  exists: [],
  currentLeakList: [],
  isConfigObj: false,
  isGC: true,
  stateForeground: 1,
  stateBackground: 3
};

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
let maxFileNum = 20;

function getApplicationContext(): Context | undefined {
  try {
    let applicationContext = application.getApplicationContext();
    return applicationContext;
  } catch (error) {
    console.error(`getApplicationContext failed`);
    return undefined;
  }
}

function getProcessName(): void {
  try {
    let data = bundleManager.getBundleInfoForSelfSync(appState.bundleFlags);
    appState.bundleName = data.name;
  } catch(error) {
    let message = (error as BusinessError).message;
    console.error(`getBundleInfoForSelf error: ${JSON.stringify(message)}`);
    return;
  }
}

function setGcDelayAndDumpDelay(configs): void {
  if (configs.checkInterval && configs.checkInterval > 0 &&
      configs.dumpHeapWaitTimeMs && configs.dumpHeapWaitTimeMs < configs.checkInterval) {
    jsLeakWatcherNative.setGcDelay(configs.checkInterval);
    jsLeakWatcherNative.setDumpDelay(configs.dumpHeapWaitTimeMs);
  } else {
    jsLeakWatcherNative.setGcDelay(leakWatcherConfig.checkInterval);
    jsLeakWatcherNative.setDumpDelay(leakWatcherConfig.dumpHeapWaitTimeMs);
  }
}

function setDumpFileSaveAmount(configs): void {
  if (!configs.maxStoredHeapDumps || configs.maxStoredHeapDumps <= 0) {
    maxFileNum = leakWatcherConfig.maxStoredHeapDumps * 2;
  } else {
    maxFileNum = configs.maxStoredHeapDumps * 2;
  }
}

function setMonitoredIDAndObjectType(configs): void {
  leakWatcherConfig.objectUniqueIDs = [...configs.objectUniqueIDs];
  if (leakWatcherConfig.objectUniqueIDs && leakWatcherConfig.objectUniqueIDs.length > 0) {
    leakWatcherConfig.objectWatcher = 'CustomComponent';
  } else {
    leakWatcherConfig.objectWatcher = configs.objectWatcher;
  }
}

function setWhiteList(configs): void {
  leakWatcherConfig.whiteList = configs.whiteList;
}

function setForegroundAndBackgroundThreshold(configs): void {
  leakWatcherConfig.retainedVisibleThreshold = configs.retainedVisibleThreshold;
  leakWatcherConfig.retainedInvisibleThreshold = configs.retainedInvisibleThreshold;
}

function getCustomAttribute(configs): void {
  if (!Array.isArray(configs)) {
    appState.isConfigObj = true;
    setLeakWatcherConfig(configs);
  }
}


function setLeakWatcherConfig(configs): void {
  setWhiteList(configs);
  setMonitoredIDAndObjectType(configs);
  setGcDelayAndDumpDelay(configs);
  setForegroundAndBackgroundThreshold(configs);
  setDumpFileSaveAmount(configs);
  getProcessName();
}

function monitorLeakIDandWhitelist(obj): boolean {
  if (leakWatcherConfig.objectUniqueIDs.length > 0 &&
      !leakWatcherConfig.objectUniqueIDs.includes(obj.__nativeId__Internal)) {
      console.log(`The ID of the monitored object does not exist.`);
      return true;
  }
  if (leakWatcherConfig.whiteList.some(item => item.toLowerCase() === obj.constructor.name.toLowerCase()) &&
      !leakWatcherConfig.objectUniqueIDs.includes(obj.__nativeId__Internal)) {
    console.log(`Whitelist detection: ${leakWatcherConfig.whiteList}`);
    return true;
  }
  return false;
}

function startGCtask(context): void {
  appState.currentLeakList = getLeakList();
  context.getRunningProcessInformation().then((data) => {
    appState.processInformation = data;
    appState.exists = appState.processInformation.find(item => item.processName === appState.bundleName);
    if (appState.processInformation && appState.processInformation.length > 0 && appState.exists) {
      appState.applicationState = appState.exists.state;
      if (appState.currentLeakList.length < leakWatcherConfig.retainedVisibleThreshold &&
          appState.applicationState === appState.stateForeground) {
        appState.isGC = false;
        console.log(`The number of startGCtask foreground leaks: ${(appState.currentLeakList.length)}` +
                    `is less than the threshold.`);
        return;
      }

      if (appState.currentLeakList.length < leakWatcherConfig.retainedInvisibleThreshold &&
          appState.applicationState === appState.stateBackground) {
        appState.isGC = false;
        console.log(`The number of startGCtask background leaks: ${(appState.currentLeakList.length)}` +
                    `is less than the threshold.`);
        return;
      }
      ArkTools.forceFullGC();
      appState.isGC = true;
    }
  }).catch((error: BusinessError) => {
    console.error(`startGCtask error: ${JSON.stringify(error)}`);
    appState.isGC = false;
    return;
  });
}

function startDumptask(filePath, callback): void {
  if (!appState.isGC) {
    console.log('startDumptask is not executed.');
    return;
  }

  const intersection = getLeakList().filter(item1 =>
    appState.currentLeakList.some(item2 => item2.hash === item1.hash)
  );

  if (appState.processInformation && appState.processInformation.length > 0 && appState.exists) {
    if (intersection.length < leakWatcherConfig.retainedVisibleThreshold &&
      appState.applicationState === appState.stateForeground) {
      console.log(`The number of startDumptask foreground leaks: ${intersection.length}` +
                  `is less than the threshold.`);
      return;
    }

    if (intersection.length < leakWatcherConfig.retainedInvisibleThreshold &&
        appState.applicationState === appState.stateBackground) {
      console.log(`The number of startDumptask background leaks: ${intersection.length}` +
                  `is less than the threshold.`);
      return;
    }
    dumpInner(filePath, true, true, callback);
  }
  return;
}

function getLeakList() {
  return Array.from(watchObjMap.values());
}

function createHeapDumpFile(fileName, filePath, isRawHeap, isSync, dumpCallback = undefined): void {
  let suffix = isRawHeap ? '.rawheap' : '.heapsnapshot';
  let heapDumpFileName = fileName + suffix;
  let desFilePath = filePath + '/' + heapDumpFileName;
  if (isRawHeap) {
    if (!isSync) {
      jsLeakWatcherNative.dumpRawHeap(desFilePath, dumpCallback);
    } else {
      jsLeakWatcherNative.dumpRawHeapSync(desFilePath, dumpCallback);
    }
  } else {
    hidebug.dumpJsHeapData(fileName);
    fs.moveFileSync('/data/storage/el2/base/files/' + heapDumpFileName, desFilePath, 0);
  }
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
  if (files.length > maxFileNum) {
    const regex = /(\d+)\.(heapsnapshot|jsleaklist|rawheap)/;
    files.sort((a, b) => {
      const matchA = a.match(regex);
      const matchB = b.match(regex);
      const timeStampA = matchA ? parseInt(matchA[1]) : 0;
      const timeStampB = matchB ? parseInt(matchB[1]) : 0;
      return timeStampA - timeStampB;
    });
    for (let i = 0; i < files.length - maxFileNum; i++) {
      fs.unlinkSync(filePath + '/' + files[i]);
      console.log(`File: ${files[i]} is deleted.`);
    }
  }  
}

function registerObject(obj, msg) {
  if (!obj) {
    return;
  }
  if (appState.isConfigObj && monitorLeakIDandWhitelist(obj)) {
    return;
  }
  let objMsg = { hash: util.getHash(obj), name: obj.constructor.name, msg: msg };
  watchObjMap.set(objMsg.hash, objMsg);
  registry.register(obj, objMsg.hash);
}

let lifecycleId;
function registerAbilityLifecycleCallback() {
  let abilityLifecycleCallback = {
    onAbilityDestroy(ability) {
      registerObject(ability, '');
    }
  }
  if (appState.applicationContext === undefined) {
    console.error(`registerAbilityLifecycleCallback getApplicationContext failed`);
    return;
  }
  let applicationContext = appState.applicationContext;
  lifecycleId = applicationContext.registerAbilityLifecycleCallback(abilityLifecycleCallback);
}

function unregisterAbilityLifecycleCallback() {
  if (appState.applicationContext === undefined) {
    console.error(`registerAbilityLifecycleCallback getApplicationContext failed`);
    return;
  }
  let applicationContext = appState.applicationContext;
  applicationContext.unregisterAbilityLifecycleCallback(lifecycleId, (error, data) => {
    console.log('unregisterAbilityLifecycleCallback success! err:' + JSON.stringify(error));
  });
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
  if (config.includes('Ability')) {
    registerAbilityLifecycleCallback();
  }
}

function dumpInnerSync(filePath, needSandBox, isRawHeap) {
  if (!enabled) {
    return [];
  }
  if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
    throw new BusinessError(ERROR_CODE_INVALID_PARAM);
  }
  const fileTimeStamp = new Date().getTime().toString();
  try {
    const heapDumpSHA256 = createHeapDumpFile(fileTimeStamp, filePath, isRawHeap, true);
    let file = fs.openSync(filePath + '/' + fileTimeStamp + '.jsleaklist', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
    let leakObjList = getLeakList();
    if (isRawHeap) {
      let result = { version: '2.0.0', snapshot_hash: heapDumpSHA256, leakObjList: leakObjList };
      fs.writeSync(file.fd, JSON.stringify(result));
    } else {
      let result = { snapshot_hash: heapDumpSHA256, leakObjList: leakObjList };
      fs.writeSync(file.fd, JSON.stringify(result));
    }
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

function dumpInner(filePath, needSandBox, isRawHeap, jsCallback: Callback<Array<string>> = undefined) {
  if (!enabled) {
    return [];
  }
  if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
    throw new BusinessError(ERROR_CODE_INVALID_PARAM);
  }
  const fileTimeStamp = new Date().getTime().toString();
  try {
    createHeapDumpFile(fileTimeStamp, filePath, isRawHeap, false, (code) => {
      console.log('createHeapDumpFile begin!');
      let file = fs.openSync(filePath + '/' + fileTimeStamp + '.jsleaklist', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
      let leakObjList = getLeakList();
      let suffix = isRawHeap ? '.rawheap' : '.heapsnapshot';
      let heapDumpFileName = fileTimeStamp + suffix;
      let desFilePath = filePath + '/' + heapDumpFileName;
      const heapDumpSHA256 = getHeapDumpSHA256(desFilePath);

      if (isRawHeap) {
        let result = { version: '2.0.0', snapshot_hash: heapDumpSHA256, leakObjList: leakObjList };
        fs.writeSync(file.fd, JSON.stringify(result));
      } else {
        let result = { snapshot_hash: heapDumpSHA256, leakObjList: leakObjList };
        fs.writeSync(file.fd, JSON.stringify(result));
      }
      fs.closeSync(file);

      try {
        deleteOldFile(filePath);
      } catch (e) {
        console.log('Delete old files failed! ' + e);
        return [];
      }
      
      let fileList: string[] = [];
      if (needSandBox) {
        fileList = [filePath + '/' + fileTimeStamp + '.jsleaklist', filePath + '/' + fileTimeStamp + '.rawheap'];
      } else {
        fileList = [fileTimeStamp + '.jsleaklist', fileTimeStamp + '.heapsnapshot'];
      }

      if (jsCallback) {
        jsCallback(fileList);
      }
      return [];
    });
  } catch (error) {
    console.log('Dump heaoSnapShot or LeakList failed! ' + error);
    return [];
  }
}

function shutdownJsLeakWatcher() {
  jsLeakWatcherNative.removeTask();
  jsLeakWatcherNative.unregisterArkUIObjectLifeCycleCallback();
  jsLeakWatcherNative.unregisterWindowLifeCycleCallback();
  unregisterArkUIObjectLifeCycleCallback();
  unregisterAbilityLifecycleCallback();
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
    return dumpInnerSync(filePath, false, false);
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
  enableLeakWatcher: (isEnabled: boolean, configs: Array<string> | LeakWatcherConfig, callback: Callback<Array<string>>) => {
    if (isEnabled === undefined || isEnabled === null) {
      throw new BusinessError(ERROR_CODE_ENABLE_INVALID);
    }
    if (configs === undefined || configs === null) {
      throw new BusinessError(ERROR_CODE_CONFIG_INVALID);
    }
    if (callback === undefined || callback === null) {
      throw new BusinessError(ERROR_CODE_CALLBACK_INVALID);
    }
    if (isEnabled === enabled) {
      console.log('JsLeakWatcher is already started or stopped.');
      return;
    }
    enabled = isEnabled;
    if (!isEnabled) {
      shutdownJsLeakWatcher();
      return;
    }
    getCustomAttribute(configs);

    const validConfig = ['CustomComponent', 'Window', 'NodeContainer', 'XComponent', 'Ability'];
    let configArray: string[] = Array.isArray(configs) ?
        configs : leakWatcherConfig.objectWatcher ?
        [leakWatcherConfig.objectWatcher] : [];

    for (let i = 0; i < configArray.length; i++) {
      if (!validConfig.includes(configArray[i])) {
        throw new BusinessError(ERROR_CODE_CONFIG_INVALID);
      }
    }
    if (configArray.length === 0) {
      configArray = validConfig;
    }
    
    if (appState.applicationContext === undefined) {
      console.log('enableLeakWatcher applicationContext is undefined');
      return;
    }
    const context: Context = appState.applicationContext;
    const filePath : string = `${context.filesDir}/${JSLEAK_ROOT_DIR_NAME}`;
    if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
      fs.mkdirSync(filePath);
    }

    jsLeakWatcherNative.handleGCTask(() => {
      if (appState.isConfigObj) {
        startGCtask(context);
      } else {
        ArkTools.forceFullGC();
      }
    });
    jsLeakWatcherNative.handleDumpTask(() => {
      if (appState.isConfigObj) {
        startDumptask(filePath, callback);
      } else {
        if (watchObjMap.size === 0) {
          console.log('No js leak detected, no need to dump.');
          return;
        }
        dumpInner(filePath, true, true, callback);
      }
    });
    jsLeakWatcherNative.handleShutdownTask(() => {
      enabled = false;
      shutdownJsLeakWatcher();
    });
    executeRegister(configArray);
  }
};

export default jsLeakWatcher;
