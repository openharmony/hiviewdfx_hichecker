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
let process = requireNapi('process');
let cryptoFramework = requireNapi('security.cryptoFramework');
let jsLeakWatcherNative = requireNapi('hiviewdfx.jsleakwatchernative');
let application = requireNapi('app.ability.application');
let bundleManager = requireNapi('bundle.bundleManager');

const SANDBOX_PATH = '/data/storage/el2/base/files/';
const JSLEAK_ROOT_DIR_NAME = 'jsleak';
const ERROR_CODE_INVALID_PARAM = 401;
const ERROR_MSG_INVALID_PARAM = 'Parameter error. Please check!';
const ERROR_CODE_ENABLE_INVALID = 10801001;
const ERROR_MSG_ENABLE_INVALID = 'The parameter isEnabled invalid. Please check!';
const ERROR_CODE_CONFIG_INVALID = 10801002;
const ERROR_MSG_CONFIG_INVALID = 'The parameter config invalid. Please check!';
const ERROR_CODE_CALLBACK_INVALID = 10801003;
const ERROR_MSG_CALLBACK_INVALID = 'The parameter callback invalid. Please check!';

enum MonitorObjectType {
  ALL = -1,
  CUSTOM_COMPONENT = 1 << 0,
  WINDOW = 1 << 1,
  NODE_CONTAINER = 1 << 2,
  X_COMPONENT = 1 << 3,
  ABILITY = 1 << 4
}

interface LeakWatcherConfig {
  monitorObjectTypes: MonitorObjectType;
  objectUniqueIDs: Array<number>;
  checkInterval: number;
  fgLeakCountThreshold: number;
  bgLeakCountThreshold: number;
  maxStoredHeapDumps: number;
  dumpHeapWaitTimeMs: number;
  exclusionList: Array<string>;
}

let leakWatcherConfig: LeakWatcherConfig = {
  monitorObjectTypes: MonitorObjectType.ALL,
  objectUniqueIDs: [],
  checkInterval: 30000,
  fgLeakCountThreshold: 5,
  bgLeakCountThreshold: 1,
  maxStoredHeapDumps: 10,
  dumpHeapWaitTimeMs: 5000,
  exclusionList: []
};

let dumpStatus: boolean = jsLeakWatcherNative.getDumpStatus();

let retryMonitorObjectTypes: MonitorObjectType | undefined = undefined;
const ATTEMPT_COUNT = 2;
let currentAttempt = 1;

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
  duplicateLeakList: any[];
  intersection: any[];
  leakListPath: string[];
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
  duplicateLeakList: [],
  intersection: [],
  leakListPath: [],
  isConfigObj: false,
  isGC: true,
  stateForeground: 1,
  stateBackground: 3
};

interface ReportRawHeap {
  pid: number;
  happenTime: string;
  module: string;
  leakList: string;
  dynamicRawheapPath: string;
  staticRawheapPath: string;
  leakListPath: string;
  leakObjectCount: number;
}

let report: ReportRawHeap = {
  pid: process.pid,
  happenTime: new Date().getTime().toString(),
  module: '',
  leakList: '',
  dynamicRawheapPath: '',
  staticRawheapPath: '',
  leakListPath: '',
  leakObjectCount: 0
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
let firstDump = true;
let curTimeStamp = '';

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

function convertToMask(configArray: string[]): number {
  if (!configArray || configArray.length === 0) {
      return MonitorObjectType.ALL;
  }

  const typeMap = {
      'CustomComponent': MonitorObjectType.CUSTOM_COMPONENT,
      'Window':          MonitorObjectType.WINDOW,
      'NodeContainer':   MonitorObjectType.NODE_CONTAINER,
      'XComponent':      MonitorObjectType.X_COMPONENT,
      'Ability':         MonitorObjectType.ABILITY
  };

  let mask = 0;
  configArray.forEach(item => {
      const bitValue = typeMap[item] || 0; 
      mask |= bitValue;
  });

  return mask;
}

function getJsleaklistFile(filePath, needSandBox, isRawHeap, jsCallback) {
  if (!dumpStatus) {
    if (firstDump) {
      firstDump = false;
      deleteUnMatchDumpFile(filePath);
    } else {
      deleteLastOldFile(filePath);
    }
  }
  let file = dumpStatus ? fs.openSync(filePath + '/' + getHeapBaseName(false) + '.jsleaklist', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE) :
    fs.openSync(filePath + '/' + getHeapBaseName(true) + '.jsleaklist', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE)
  let leakObjList = appState.intersection;;
  let suffix = isRawHeap ? '.rawheap' : '.heapsnapshot';
  let heapDumpFileName = getHeapBaseName(false) + suffix;
  let desFilePath = filePath + '/' + heapDumpFileName;
  const heapDumpSHA256 = dumpStatus ? getHeapDumpSHA256(desFilePath) : '';

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
    fileList = [filePath + '/' + getHeapBaseName(false) + '.jsleaklist', dumpStatus ? filePath + '/' + getHeapBaseName(false) + '.rawheap' : ''];
  } else {
    fileList = [getHeapBaseName(false) + '.jsleaklist', getHeapBaseName(false) + '.heapsnapshot'];
  }

  appState.leakListPath = fileList;
  if (jsCallback) {
    jsCallback(fileList);
  }

  setReportData();
  jsLeakWatcherNative.reportRawHeap(report);
  return [];
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
  if (configs.monitorObjectTypes & MonitorObjectType.CUSTOM_COMPONENT) {
    leakWatcherConfig.objectUniqueIDs =
      Array.isArray(configs.objectUniqueIDs) ? [...configs.objectUniqueIDs] : [];
  }
  leakWatcherConfig.monitorObjectTypes = configs.monitorObjectTypes;
}

function setWhiteList(configs): void {
  leakWatcherConfig.exclusionList =
    Array.isArray(configs.exclusionList) ? configs.exclusionList : [];
}

function setForegroundAndBackgroundThreshold(configs): void {
  leakWatcherConfig.fgLeakCountThreshold = configs.fgLeakCountThreshold;
  leakWatcherConfig.bgLeakCountThreshold = configs.bgLeakCountThreshold;
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

function setReportData(): void {
  report.pid = process.pid;
  report.happenTime = new Date().getTime().toString();
  report.module = appState.bundleName;
  report.leakList = JSON.stringify(appState.intersection);
  report.dynamicRawheapPath = JSON.stringify(appState.leakListPath);
  report.staticRawheapPath = JSON.stringify(appState.leakListPath);
  report.leakListPath = JSON.stringify(appState.leakListPath);
  report.leakObjectCount = appState.intersection.length;
}

function monitorLeakIDandWhitelist(obj): boolean {
  if (leakWatcherConfig.objectUniqueIDs.length > 0 &&
      !leakWatcherConfig.objectUniqueIDs.includes(obj.__nativeId__Internal)) {
      console.log(`The ID of the monitored object does not exist.`);
      return true;
  }
  if (leakWatcherConfig.exclusionList.some(item => item.toLowerCase() === obj.constructor.name.toLowerCase()) &&
      !leakWatcherConfig.objectUniqueIDs.includes(obj.__nativeId__Internal)) {
    console.log(`Whitelist detection: ${leakWatcherConfig.exclusionList}`);
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
      if (appState.currentLeakList.length < leakWatcherConfig.fgLeakCountThreshold &&
          appState.applicationState === appState.stateForeground) {
        appState.isGC = false;
        console.log(`The number of startGCtask foreground leaks: ${(appState.currentLeakList.length)}` +
                    ` is less than the threshold.`);
        return;
      }

      if (appState.currentLeakList.length < leakWatcherConfig.bgLeakCountThreshold &&
          appState.applicationState === appState.stateBackground) {
        appState.isGC = false;
        console.log(`The number of startGCtask background leaks: ${(appState.currentLeakList.length)}` +
                    ` is less than the threshold.`);
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

  const isLengthEqual = appState.currentLeakList.length === appState.duplicateLeakList.length;
  const hashEqual = isLengthEqual &&
    appState.currentLeakList.every((item, index) => {
      const isDuplicateLeakList = appState.duplicateLeakList[index];
      if (!isDuplicateLeakList) {
        return false;
      }
      if (item.hash === null || isDuplicateLeakList.hash === null) {
        return false;
      }
      return item.hash === isDuplicateLeakList.hash;
    });
  if (hashEqual) {
    console.log("No new leakage objects were added."); 
    return;
  }
  appState.intersection = getLeakList().filter(item1 =>
    appState.currentLeakList.some(item2 => item2.hash === item1.hash)
  );
  appState.duplicateLeakList = appState.intersection;

  if (appState.processInformation && appState.processInformation.length > 0 && appState.exists) {
    if (appState.intersection.length < leakWatcherConfig.fgLeakCountThreshold &&
      appState.applicationState === appState.stateForeground) {
      console.log(`The number of startDumptask foreground leaks: ${appState.intersection.length}` +
                  ` is less than the threshold.`);
      return;
    }

    if (appState.intersection.length < leakWatcherConfig.bgLeakCountThreshold &&
        appState.applicationState === appState.stateBackground) {
      console.log(`The number of startDumptask background leaks: ${appState.intersection.length}` +
                  ` is less than the threshold.`);
      return;
    }
    dumpInner(filePath, true, true, callback);
  }
  return;
}

function getLeakList() {
  return Array.from(watchObjMap.values());
}

function getTimestampByFileName(fileName: string): number {
  const regex = /(\d+)\.(heapsnapshot|jsleaklist|rawheap)/;
  const fileMatch = fileName.match(regex);
  return fileMatch ? parseInt(fileMatch[1]) : 0;
}

function getHeapPrefix(): string {
  return `jsleakwatcher-${process.pid}-${process.tid}-`;
}

function getHeapBaseName(update: boolean): string {
  if (update) {
    curTimeStamp = new Date().getTime().toString();
  }
  return `${getHeapPrefix()}${curTimeStamp}`;
}

function deleteLastOldFile(filePath: string): void {
  let listFileOption = {
    recursion: false,
    listNum: 0,
    filter: {
      suffix: ['.jsleaklist'],
    }
  };
  let files = fs.listFileSync(filePath, listFileOption);
  if (files.length <= 0) {
    return;
  }
  files.sort((a, b) => {
    return getTimestampByFileName(b) - getTimestampByFileName(a);
  });
  let prefix = getHeapPrefix();
  if (!files[0].startsWith(prefix)) {
    return;
  }
  let fileBaseName = prefix + getTimestampByFileName(files[0]);
  try {
    fs.unlinkSync(filePath + '/' + fileBaseName + '.jsleaklist');
    fs.unlinkSync(filePath + '/' + fileBaseName + '.rawheap');
  } catch (e) {
    console.log('Delete files failed! ' + e);
    return;
  }
  console.log(`delete same process heap files ${fileBaseName}`);
}

function deleteUnMatchDumpFile(filePath: string): void {
  let listFileOption = {
    recursion: false,
    listNum: 0,
    filter: {
      suffix: ['.rawheap'],
    }
  };
  let files = fs.listFileSync(filePath, listFileOption);
  for (let i = 0; i < files.length; i++) {
    let suffixOffset = files[i].lastIndexOf('.rawheap');
    if (suffixOffset <= 0) {
      continue;
    }
    let listFilePath = filePath + '/' + files[i].substring(0, suffixOffset) + '.jsleaklist';
    if (!fs.accessSync(listFilePath, fs.AccessModeType.EXIST)) {
      try {
        fs.unlinkSync(filePath + '/' + files[i]);
      } catch (e) {
        console.log('Delete files failed! ' + e);
        return;
      }
      console.warn(`delete ${files[i]} which no match jsleaklist`);
    }
  }
}

function createHeapDumpFile(filePath, isRawHeap, isSync, dumpCallback = undefined): void {
  if (firstDump) {
    firstDump = false;
    deleteUnMatchDumpFile(filePath);
  } else {
    deleteLastOldFile(filePath);
  }
  let fileName = getHeapBaseName(true);
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
    fs.moveFileSync(SANDBOX_PATH + heapDumpFileName, desFilePath, 0);
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
    files.sort((a, b) => {
      return getTimestampByFileName(a) - getTimestampByFileName(b);
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

  let objMsg = { hash: util.getHash(obj), name: obj.constructor.name, msg: msg };
  watchObjMap.set(objMsg.hash, objMsg);
  registry.register(obj, objMsg.hash);
}

let lifecycleId;
function registerAbilityLifecycleCallback() {
  let abilityLifecycleCallback = {
    onAbilityDestroy(ability) {
      if (appState.isConfigObj &&
        leakWatcherConfig.exclusionList.some(item => item.toLowerCase() === ability.name.toLowerCase())) {
        console.log(`ability ${ability.name} in exclusionList`);
      } else {
        registerObject(ability, '');
      }
    }
  }
  if (appState.applicationContext === undefined) {
    console.error(`registerAbilityLifecycleCallback getApplicationContext failed`);
    return false;
  }
  let applicationContext = appState.applicationContext;
  lifecycleId = applicationContext.registerAbilityLifecycleCallback(abilityLifecycleCallback);
  return true;
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

function executeRegister(config: MonitorObjectType, currentAttempt = 0) {
  retryMonitorObjectTypes = undefined;
  if (config & MonitorObjectType.CUSTOM_COMPONENT) {
    registerArkUIObjectLifeCycleCallback((weakRef, msg) => {
      if (!weakRef) {
        return;
      }
      let obj = weakRef.deref();
      if (appState.isConfigObj && monitorLeakIDandWhitelist(obj)) {
        return;
      }
      registerObject(obj, msg);
    });
  }
  if (config & MonitorObjectType.WINDOW) {
    let ret = jsLeakWatcherNative.registerWindowLifeCycleCallback((obj) => {
      if (appState.isConfigObj && leakWatcherConfig.exclusionList.some(
        item => obj !== undefined && item.toLowerCase() === obj.getWindowProperties().name.toLowerCase())) {
        console.log(`window ${obj.getWindowProperties().name} in exclusionList`);
      } else {
        registerObject(obj, '');
      }
    });
    if (!ret) {
      retryMonitorObjectTypes = (retryMonitorObjectTypes === undefined) ? MonitorObjectType.WINDOW : retryMonitorObjectTypes | MonitorObjectType.WINDOW;
    }
  }
    if (config & MonitorObjectType.NODE_CONTAINER ||
        config & MonitorObjectType.X_COMPONENT) {
    let ret = jsLeakWatcherNative.registerArkUIObjectLifeCycleCallback((weakRef) => {
      if (!weakRef) {
        return;
      }
      let obj = weakRef.deref();
      registerObject(obj, '');
    });
    if (!ret) {
      retryMonitorObjectTypes = (retryMonitorObjectTypes === undefined) ? MonitorObjectType.NODE_CONTAINER : retryMonitorObjectTypes | MonitorObjectType.NODE_CONTAINER;
      retryMonitorObjectTypes = (retryMonitorObjectTypes === undefined) ? MonitorObjectType.WINDOW : retryMonitorObjectTypes | MonitorObjectType.X_COMPONENT;
    }
  }
  if (config & MonitorObjectType.ABILITY) {
    if (!registerAbilityLifecycleCallback()) {
      retryMonitorObjectTypes = (retryMonitorObjectTypes === undefined) ? MonitorObjectType.ABILITY : retryMonitorObjectTypes | MonitorObjectType.ABILITY;
    }
  }
}

function dumpInnerSync(filePath, needSandBox, isRawHeap) {
  if (!enabled) {
    return [];
  }
  if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
    throw new BusinessError(ERROR_CODE_INVALID_PARAM);
  }
  try {
    const heapDumpSHA256 = createHeapDumpFile(filePath, isRawHeap, true);
    let file = fs.openSync(filePath + '/' + getHeapBaseName(false) + '.jsleaklist', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
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
    return [filePath + '/' + getHeapBaseName(false) + '.jsleaklist', filePath + '/' + getHeapBaseName(false) + '.rawheap'];
  } else {
    return [getHeapBaseName(false) + '.jsleaklist', getHeapBaseName(false) + '.heapsnapshot'];
  }
}

function dumpInner(filePath, needSandBox, isRawHeap, jsCallback: Callback<Array<string>> = undefined) {
  dumpStatus = jsLeakWatcherNative.getDumpStatus();
  if (!enabled) {
    return [];
  }
  if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
    throw new BusinessError(ERROR_CODE_INVALID_PARAM);
  }
  if (!dumpStatus) {
    getJsleaklistFile(filePath, needSandBox, isRawHeap, jsCallback);
    return [];
  }
  try {
    createHeapDumpFile(filePath, isRawHeap, false, (code) => {
      console.log('createHeapDumpFile begin!');
      getJsleaklistFile(filePath, needSandBox, isRawHeap, jsCallback);
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
        configs : []

    for (let i = 0; i < configs.length; i++) {
      if (!validConfig.includes(configs[i])) {
        throw new BusinessError(ERROR_CODE_CONFIG_INVALID);
      }
    }
    if (configs.length === 0) {
      configs = validConfig;
    }
    
    if (appState.applicationContext === undefined) {
      console.log('enableLeakWatcher applicationContext is undefined');
      return;
    }
    let context : Context = getContext(this);
    let filePath : string = context ? context.filesDir : SANDBOX_PATH;
    if (!Array.isArray(configs)) {
      context = appState.applicationContext;
      filePath = context ? `${context.filesDir}/${JSLEAK_ROOT_DIR_NAME}` : SANDBOX_PATH;
    }
    if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
      fs.mkdirSync(filePath);
    }

    jsLeakWatcherNative.handleGCTask(() => {
      if (currentAttempt <= ATTEMPT_COUNT && retryMonitorObjectTypes !== undefined) {
        executeRegister(retryMonitorObjectTypes, currentAttempt);
        currentAttempt += 1;
      }
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
    if (Array.isArray(configs)) {
      leakWatcherConfig.monitorObjectTypes = convertToMask(configArray);
    }
    executeRegister(leakWatcherConfig.monitorObjectTypes);
  }
};

export default jsLeakWatcher;
