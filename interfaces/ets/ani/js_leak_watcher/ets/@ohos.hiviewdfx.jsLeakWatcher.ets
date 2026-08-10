/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

import { BusinessError, RecordData, Callback } from '@ohos.base';
import fileIo, { ListFileOptions, Filter, ReadOptions } from '@ohos.file.fs';
import util from '@ohos.util';
import hidebug from '@ohos.hidebug';
import cryptoFramework from '@ohos.security.cryptoFramework';
import bundleManager from '@ohos.bundle.bundleManager';
import application from '@ohos.app.ability.application';
import appManager from '@ohos.app.ability.appManager';
import AbilityLifecycleCallback from '@ohos.app.ability.AbilityLifecycleCallback';
import UIAbility from '@ohos.app.ability.UIAbility';
import { ExtendableComponent } from '@ohos.arkui.component';

declare class ArkTools {
  static forceFullGC(): void;
}

export default namespace jsLeakWatcher {
  loadLibraryWithPermissionCheck("jsleakwatcher_ani", "@ohos.hiviewdfx.jsLeakWatcher");

  const SANDBOX_PATH: string = '/data/storage/el2/base/files/';
  const JSLEAK_ROOT_DIR_NAME: string = 'jsleak';
  const ERROR_CODE_INVALID_PARAM: int = 401;
  const ERROR_MSG_INVALID_PARAM: string = 'Parameter error. Please check!';
  const ERROR_CODE_ENABLE_INVALID: int = 10801001;
  const ERROR_MSG_ENABLE_INVALID: string = 'The parameter isEnabled invalid. Please check!';
  const ERROR_CODE_CONFIG_INVALID: int = 10801002;
  const ERROR_MSG_CONFIG_INVALID: string = 'The parameter config invalid. Please check!';
  const ERROR_CODE_CALLBACK_INVALID: int = 10801003;
  const ERROR_MSG_CALLBACK_INVALID: string = 'The parameter callback invalid. Please check!';
  const MIN_CHECK_INTERVAL: int = 90000;
  const ATTEMPT_COUNT: int = 2;
  const MAX_FILE_NUM_DEFAULT: int = 20;
  const STATE_FOREGROUND: int = 1;
  const STATE_BACKGROUND: int = 3;


  export enum MonitorObjectType {
    ALL = -1,
    CUSTOM_COMPONENT = 1 << 0,
    WINDOW = 1 << 1,
    NODE_CONTAINER = 1 << 2,
    X_COMPONENT = 1 << 3,
    ABILITY = 1 << 4
  }

  export interface LeakWatcherConfig {
    monitorObjectTypes: MonitorObjectType;
    objectUniqueIDs?: Array<int>;
    checkInterval?: int;
    fgLeakCountThreshold?: int;
    bgLeakCountThreshold?: int;
    maxStoredHeapDumps?: int;
    dumpHeapWaitTimeMs?: int;
    exclusionList?: Array<string>;
  }

  interface ObjMsg {
    hash: number;
    name: string;
    msg: string;
  }

  interface LeakJsonResult {
    version?: string;
    snapshot_hash: string;
    leakObjList: ObjMsg[];
  }

  native function getPid(): int;
  native function getTid(): int;
  native function getDumpStatus(): boolean;
  native function setGcDelay(delay: int): void;
  native function setDumpDelay(delay: int): void;
  native function dumpRawHeap(dynamicPath: string, staticPath: string, callback: (code: int) => void): void;
  native function dumpRawHeapSync(dynamicPath: string, staticPath: string): void;
  native function handleDumpTask(callback: () => void): void;
  native function handleGCTask(callback: () => void): void;
  native function handleShutdownTask(callback: () => void): void;
  native function removeTask(): void;
  // native function 用于 NodeContainer/XComponent 的生命周期回调(C++ 实现,通过 UIContext 注册)。
  // declare function 用于 CustomComponent 的生命周期回调(ArkUI 框架全局函数,运行时由 arkoala.abc 提供)。
  // 两者是独立机制,需分别注册/注销。
  native function registerArkUIObjectLifeCycleCallbackNative(
    callback: (weakRef: WeakRef<object>, msg: string) => void): boolean;
  native function unregisterArkUIObjectLifeCycleCallbackNative(): void;

  declare function registerArkUIObjectLifeCycleCallback(
    callback: (weakRef: WeakRef<object>, msg: string) => void): void;
  declare function unregisterArkUIObjectLifeCycleCallback(): void;

  class LeakWatcherAbilityCallback extends AbilityLifecycleCallback {
    onAbilityDestroy(ability: UIAbility): void {
      if (appState.isConfigObj) {
        let exclList: string[] = leakWatcherConfig.exclusionList ?? [];
        let name: string = ability.context.abilityInfo.name;
        let skip: boolean = exclList.some((item: string) => item.toLowerCase() === name.toLowerCase());
        if (skip) { return; }
      }
      registerObject(ability, '');
    }
  }

  let enabled: boolean = false;
  let firstDump: boolean = true;
  let curTimeStamp: string = '';
  let maxFileNum: int = MAX_FILE_NUM_DEFAULT;
  let dumpStatus: boolean = true;
  let retryMask: int = 0;
  let currentAttempt: int = 1;
  let lifecycleId: int = -1;
  let bundleName: string = '';

  let watchObjMap: Map<number, ObjMsg> = new Map<number, ObjMsg>();

  const registry: FinalizationRegistry<number> = new FinalizationRegistry<number>((hash: number) => {
    if (watchObjMap.has(hash)) {
      watchObjMap.delete(hash);
    }
  });

  let leakWatcherConfig: LeakWatcherConfig = {
    monitorObjectTypes: MonitorObjectType.ALL,
    objectUniqueIDs: [],
    checkInterval: MIN_CHECK_INTERVAL,
    fgLeakCountThreshold: 5,
    bgLeakCountThreshold: 1,
    maxStoredHeapDumps: 10,
    dumpHeapWaitTimeMs: 5000,
    exclusionList: []
  };

  interface AppStateInformation {
    applicationContext: Object;
    bundleName: string;
    processInformation: appManager.ProcessInformation[];
    applicationState: int;
    exists: appManager.ProcessInformation | undefined;
    currentLeakList: ObjMsg[];
    duplicateLeakList: ObjMsg[];
    intersection: ObjMsg[];
    leakListPath: string[];
    isConfigObj: boolean;
    isGC: boolean;
  }

  let appState: AppStateInformation = {
    applicationContext: application.getApplicationContextInstance(),
    bundleName: '',
    processInformation: [],
    applicationState: 2,
    exists: undefined,
    currentLeakList: [],
    duplicateLeakList: [],
    intersection: [],
    leakListPath: [],
    isConfigObj: false,
    isGC: true
  };

  function throwBusinessError(code: int): void {
    let msg: string = ERROR_MSG_INVALID_PARAM;
    if (code === ERROR_CODE_ENABLE_INVALID) {
      msg = ERROR_MSG_ENABLE_INVALID;
    } else if (code === ERROR_CODE_CONFIG_INVALID) {
      msg = ERROR_MSG_CONFIG_INVALID;
    } else if (code === ERROR_CODE_CALLBACK_INVALID) {
      msg = ERROR_MSG_CALLBACK_INVALID;
    }
    throw new BusinessError<void>(code, msg);
  }

  function toInt(v: int | undefined, def: int): int {
    if (v === undefined || v === null) {
      return def;
    }
    return v as int;
  }

  // Class.of(obj).getName() 返回含命名空间的完整类名(如 "module.MyComponent"),
  // 对齐 NAPI obj.constructor.name 只返回类名(如 "MyComponent")。
  function getShortClassName(obj: object): string {
    let fullName: string = Class.of(obj).getName();
    let dotIdx: int = fullName.lastIndexOf('.');
    if (dotIdx >= 0 && dotIdx < fullName.length - 1) {
      return fullName.substring(dotIdx + 1);
    }
    return fullName;
  }

  function convertToMask(configArray: string[]): int {
    if (configArray.length === 0) {
      return -1;
    }
    let mask: int = 0;
    for (let item of configArray) {
      if (item === 'CustomComponent') { mask |= 1; }
      else if (item === 'Window') { mask |= 2; }
      else if (item === 'NodeContainer') { mask |= 4; }
      else if (item === 'XComponent') { mask |= 8; }
      else if (item === 'Ability') { mask |= 16; }
    }
    return mask;
  }

  function getLeakList(): ObjMsg[] {
    let result: ObjMsg[] = [];
    watchObjMap.forEach((v: ObjMsg) => { result.push(v); });
    return result;
  }

  function getTimestampByFileName(fileName: string): int {
    let regex: RegExp = new RegExp('(\\d+)\\.(heapsnapshot|jsleaklist|rawheap)');
    let match: RegExpMatchArray | null = fileName.match(regex);
    if (match !== null && match.length > 1) {
      return parseInt(match[1] as string).toInt();
    }
    return 0;
  }

  function getHeapPrefix(): string {
    return `jsleakwatcher-${getPid()}-${getTid()}-`;
  }

  function getStaticHeapPrefix(): string {
    return `jsleakwatcher-static_jsheap-${getPid()}-`;
  }

  function getHeapBaseName(update: boolean): string {
    if (update) {
      curTimeStamp = Date.now().toString();
    }
    return `${getHeapPrefix()}${curTimeStamp}`;
  }

  function getStaticHeapBaseName(): string {
    return `${getStaticHeapPrefix()}${curTimeStamp}`;
  }

  function deleteLastOldFile(filePath: string): void {
    let opt: ListFileOptions = {
      recursion: false, listNum: 0,
      filter: { suffix: ['.jsleaklist'] } as Filter
    };
    let files: string[];
    try {
      files = fileIo.listFileSync(filePath, opt);
    } catch (e) {
      console.log('List files failed! ' + e);
      return;
    }
    if (files.length <= 0) { return; }
    files.sort((a: string, b: string) => getTimestampByFileName(b) - getTimestampByFileName(a));
    let prefix: string = getHeapPrefix();
    if (!files[0].startsWith(prefix)) { return; }
    let base: string = prefix + getTimestampByFileName(files[0]).toString();
    try {
      let lp: string = filePath + '/' + base + '.jsleaklist';
      if (fileIo.accessSync(lp)) { fileIo.unlinkSync(lp); }
      let rp: string = filePath + '/' + base + '.rawheap';
      if (fileIo.accessSync(rp)) { fileIo.unlinkSync(rp); }
      else { fileIo.unlinkSync(filePath + '/' + base + '.heapsnapshot'); }
    } catch (e) {
      console.log('Delete files failed! ' + e);
    }
    console.log(`delete same process heap files ${base}`);
  }

  function deleteUnMatchDumpFile(filePath: string): void {
    let opt: ListFileOptions = {
      recursion: false, listNum: 0,
      filter: { suffix: ['.rawheap'] } as Filter
    };
    let files: string[];
    try {
      files = fileIo.listFileSync(filePath, opt);
    } catch (e) {
      console.log('List files failed! ' + e);
      return;
    }
    for (let i: int = 0; i < files.length; i++) {
      let off: int = files[i].lastIndexOf('.rawheap');
      if (off <= 0) { continue; }
      let lp: string = filePath + '/' + files[i].substring(0, off) + '.jsleaklist';
      if (!fileIo.accessSync(lp)) {
        try { fileIo.unlinkSync(filePath + '/' + files[i]); }
        catch (e) { console.log('Delete files failed! ' + e); return; }
        console.warn(`delete ${files[i]} which no match jsleaklist`);
      }
    }
  }

  function deleteOldFile(filePath: string): void {
    let opt: ListFileOptions = {
      recursion: false, listNum: 0,
      filter: { displayName: ['*.heapsnapshot', '*.jsleaklist', '*.rawheap'] } as Filter
    };
    let files: string[];
    try {
      files = fileIo.listFileSync(filePath, opt);
    } catch (e) {
      console.log('List files failed! ' + e);
      return;
    }
    if (files.length > maxFileNum) {
      files.sort((a: string, b: string) => getTimestampByFileName(a) - getTimestampByFileName(b));
      for (let i: int = 0; i < files.length - maxFileNum; i++) {
        fileIo.unlinkSync(filePath + '/' + files[i]);
        console.log(`File: ${files[i]} is deleted.`);
        let ts: string = getTimestampByFileName(files[i]).toString();
        if (i + 1 < files.length && files[i + 1].includes(ts)) {
          fileIo.unlinkSync(filePath + '/' + files[i + 1]);
          i++;
        }
      }
    }
  }

  function createHeapDumpFile(filePath: string, isRawHeap: boolean, isSync: boolean,
    dumpCallback?: (code: int) => void): void {
    if (firstDump) { firstDump = false; deleteUnMatchDumpFile(filePath); }
    else { deleteLastOldFile(filePath); }
    let fn: string = getHeapBaseName(true);
    let suf: string = isRawHeap ? '.rawheap' : '.heapsnapshot';
    let dp: string = filePath + '/' + fn + suf;
    let sp: string = filePath + '/' + getStaticHeapBaseName() + suf;
    if (isRawHeap) {
      if (!isSync && dumpCallback !== undefined) { dumpRawHeap(dp, sp, dumpCallback); }
      else { dumpRawHeapSync(dp, sp); }
    } else {
      hidebug.dumpJsHeapData(fn);
      fileIo.moveFileSync(SANDBOX_PATH + fn + suf, dp, 0);
    }
  }

  function getHeapDumpSHA256(filePath: string): string {
    let md: cryptoFramework.Md = cryptoFramework.createMd('SHA256');
    let f = fileIo.openSync(filePath, fileIo.OpenMode.READ_WRITE);
    let bs: int = 40960;
    let rs: int = 0;
    let buf: ArrayBuffer = new ArrayBuffer(bs);
    let rl: long = fileIo.readSync(f.fd, buf, { offset: rs, length: bs } as ReadOptions);
    while (rl > 0) {
      let blob: cryptoFramework.DataBlob = { data: new Uint8Array(buf.slice(0, rl)) } as cryptoFramework.DataBlob;
      md.updateSync(blob);
      rs += rl;
      rl = fileIo.readSync(f.fd, buf, { offset: rs, length: bs } as ReadOptions);
    }
    fileIo.closeSync(f);
    let dig: cryptoFramework.DataBlob = md.digestSync();
    let arr: string[] = [];
    for (let i: int = 0; i < dig.data.length; i++) {
      arr.push(('0' + (dig.data[i] as int).toString(16)).slice(-2));
    }
    return arr.join('').toUpperCase();
  }

  function getProcessName(): void {
    try {
      let data: bundleManager.BundleInfo =
        bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_DEFAULT);
      appState.bundleName = data.name;
      bundleName = data.name;
    } catch (error) {
      console.error(`getBundleInfoForSelf error: ${(error as BusinessError).message}`);
    }
  }

  function getJsleaklistFile(filePath: string, needSandBox: boolean, isRawHeap: boolean,
    jsCallback?: Callback<Array<string>>): string[] {
    if (!dumpStatus) {
      if (firstDump) { firstDump = false; deleteUnMatchDumpFile(filePath); }
      else { deleteLastOldFile(filePath); }
    }
    let f = fileIo.openSync(filePath + '/' + getHeapBaseName(dumpStatus ? false : true) + '.jsleaklist',
      fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE);
    let lol: ObjMsg[] = getLeakList();
    let suf: string = isRawHeap ? '.rawheap' : '.heapsnapshot';
    let dp: string = filePath + '/' + getHeapBaseName(false) + suf;
    let sha: string = dumpStatus ? getHeapDumpSHA256(dp) : '';
    let r: LeakJsonResult = dumpStatus
      ? { version: '2.0.0', snapshot_hash: sha, leakObjList: lol }
      : { snapshot_hash: sha, leakObjList: lol };
    fileIo.writeSync(f.fd, JSON.stringify(r));
    fileIo.closeSync(f);
    try { deleteOldFile(filePath); }
    catch (e) { console.log('Delete old files failed! ' + e); return []; }
    let fl: string[] = [];
    if (needSandBox) {
      fl = [filePath + '/' + getHeapBaseName(false) + '.jsleaklist',
        dumpStatus ? filePath + '/' + getHeapBaseName(false) + '.rawheap' : '',
        dumpStatus ? filePath + '/' + getStaticHeapBaseName() + '.rawheap' : ''];
    } else {
      fl = [getHeapBaseName(false) + '.jsleaklist', getHeapBaseName(false) + '.heapsnapshot'];
    }
    appState.leakListPath = fl;
    if (jsCallback !== undefined) { jsCallback(fl); }
    return [];
  }

  function dumpInnerSync(filePath: string, needSandBox: boolean, isRawHeap: boolean): string[] {
    if (!enabled) { return []; }
    if (!fileIo.accessSync(filePath)) { throwBusinessError(ERROR_CODE_INVALID_PARAM); return []; }
    try {
      createHeapDumpFile(filePath, isRawHeap, true);
      let f = fileIo.openSync(filePath + '/' + getHeapBaseName(false) + '.jsleaklist',
        fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE);
      let lol: ObjMsg[] = getLeakList();
      let suf: string = isRawHeap ? '.rawheap' : '.heapsnapshot';
      let dp: string = filePath + '/' + getHeapBaseName(false) + suf;
      let sha: string = getHeapDumpSHA256(dp);
      let r: LeakJsonResult = isRawHeap
        ? { version: '2.0.0', snapshot_hash: sha, leakObjList: lol }
        : { snapshot_hash: sha, leakObjList: lol };
      fileIo.writeSync(f.fd, JSON.stringify(r));
      fileIo.closeSync(f);
    } catch (error) {
      console.log('Dump failed! ' + error);
      return [];
    }
    try { deleteOldFile(filePath); }
    catch (e) { console.log('Delete old files failed! ' + e); return []; }
    if (needSandBox) {
      return [filePath + '/' + getHeapBaseName(false) + '.jsleaklist',
              filePath + '/' + getHeapBaseName(false) + '.rawheap',
              filePath + '/' + getStaticHeapBaseName() + '.rawheap'];
    } else {
      return [getHeapBaseName(false) + '.jsleaklist', getHeapBaseName(false) + '.heapsnapshot'];
    }
  }

  function dumpInner(filePath: string, needSandBox: boolean, isRawHeap: boolean,
    jsCallback?: Callback<Array<string>>): string[] {
    dumpStatus = getDumpStatus();
    if (!enabled) { return []; }
    if (!fileIo.accessSync(filePath)) { throwBusinessError(ERROR_CODE_INVALID_PARAM); return []; }
    if (!dumpStatus) { getJsleaklistFile(filePath, needSandBox, isRawHeap, jsCallback); return []; }
    try {
      createHeapDumpFile(filePath, isRawHeap, false, (code: int) => {
        console.log('createHeapDumpFile begin!');
        getJsleaklistFile(filePath, needSandBox, isRawHeap, jsCallback);
      });
    } catch (error) {
      console.log('Dump failed! ' + error);
      return [];
    }
    return [];
  }

  function monitorLeakIDandWhitelist(obj: object, objName: string): boolean {
    let objId: int = (obj instanceof ExtendableComponent) ? obj.getUniqueId() : -1;
    let uniqueIds: int[] = leakWatcherConfig.objectUniqueIDs ?? [];
    if (uniqueIds.length > 0 && !uniqueIds.includes(objId)) {
      console.log(`The ID of the monitored object does not exist.`);
      return true;
    }
    let exclList: string[] = leakWatcherConfig.exclusionList ?? [];
    for (let i: int = 0; i < exclList.length; i++) {
      if (exclList[i].toLowerCase() === objName.toLowerCase() &&
          !uniqueIds.includes(objId)) {
        console.log(`Whitelist detection: ${exclList}`);
        return true;
      }
    }
    return false;
  }

  function registerObject(obj: object, msg: string): void {
    if (obj === null || obj === undefined) { return; }
    let hash: number = util.getHash(obj as RecordData);
    let objName: string = getShortClassName(obj);
    watchObjMap.set(hash, { hash: hash, name: objName, msg: msg });
    registry.register(obj, hash);
  }

  async function startGCtask(context: Object | undefined): Promise<void> {
    appState.currentLeakList = getLeakList();
    let data: appManager.ProcessInformation[] = await appManager.getRunningProcessInformation();
    try {
      appState.processInformation = data;
      appState.exists = undefined;
      for (let i: int = 0; i < data.length; i++) {
        if (data[i].processName === appState.bundleName) {
          appState.exists = data[i];
          break;
        }
      }
      if (appState.processInformation.length > 0 && appState.exists !== undefined) {
        let existsInfo: appManager.ProcessInformation = appState.exists!;
        appState.applicationState = existsInfo.state.valueOf() as int;
        if (appState.currentLeakList.length < (leakWatcherConfig.fgLeakCountThreshold ?? 5) &&
            appState.applicationState === STATE_FOREGROUND) {
          appState.isGC = false;
          console.log(`The number of startGCtask foreground leaks: ${appState.currentLeakList.length}` +
                      ` is less than the threshold.`);
          return;
        }
        if (appState.currentLeakList.length < (leakWatcherConfig.bgLeakCountThreshold ?? 1) &&
            appState.applicationState === STATE_BACKGROUND) {
          appState.isGC = false;
          console.log(`The number of startGCtask background leaks: ${appState.currentLeakList.length}` +
                      ` is less than the threshold.`);
          return;
        }
        ArkTools.forceFullGC();
        appState.isGC = true;
      }
    } catch (error) {
      console.error(`startGCtask error: ${JSON.stringify(error as BusinessError)}`);
      appState.isGC = false;
    }
  }

  function startDumptask(filePath: string, callback: Callback<Array<string>>): void {
    if (!appState.isGC) {
      console.log('startDumptask is not executed.');
      return;
    }
    const isLengthEqual: boolean = appState.currentLeakList.length === appState.duplicateLeakList.length;
    let hashEqual: boolean = isLengthEqual;
    if (hashEqual) {
      for (let i: int = 0; i < appState.currentLeakList.length; i++) {
        let item: ObjMsg = appState.currentLeakList[i];
        if (i >= appState.duplicateLeakList.length) { hashEqual = false; break; }
        let dup: ObjMsg = appState.duplicateLeakList[i];
        if (item.hash !== dup.hash) { hashEqual = false; break; }
      }
    }
    if (hashEqual) {
      console.log('No new leakage objects were added.');
      return;
    }
    let currentList: ObjMsg[] = getLeakList();
    appState.intersection = currentList.filter((item1: ObjMsg) =>
      appState.currentLeakList.some((item2: ObjMsg) => item2.hash === item1.hash));
    appState.duplicateLeakList = appState.intersection;
    if (appState.processInformation.length > 0 && appState.exists !== undefined) {
      if (appState.intersection.length < (leakWatcherConfig.fgLeakCountThreshold ?? 5) &&
          appState.applicationState === STATE_FOREGROUND) {
        console.log(`The number of startDumptask foreground leaks: ${appState.intersection.length}` +
                    ` is less than the threshold.`);
        return;
      }
      if (appState.intersection.length < (leakWatcherConfig.bgLeakCountThreshold ?? 1) &&
          appState.applicationState === STATE_BACKGROUND) {
        console.log(`The number of startDumptask background leaks: ${appState.intersection.length}` +
                    ` is less than the threshold.`);
        return;
      }
      dumpInner(filePath, true, true, callback);
    }
  }

  function shutdownJsLeakWatcher(): void {
    removeTask();
    unregisterArkUIObjectLifeCycleCallbackNative();
    unregisterArkUIObjectLifeCycleCallback();
    if (lifecycleId !== -1) {
      let appCtx = application.getApplicationContextInstance();
      if (appCtx !== undefined) {
        appCtx.offAbilityLifecycle(lifecycleId);
      }
      lifecycleId = -1;
    }
    watchObjMap.clear();
  }

  function executeRegister(mask: int): void {
    retryMask = 0;
    if ((mask & 1) !== 0) {
      registerArkUIObjectLifeCycleCallback((wr: WeakRef<object>, msg: string) => {
        if (wr === null || wr === undefined) { return; }
        let obj: object | undefined = wr.deref();
        if (obj === undefined) { return; }
        if (appState.isConfigObj) {
          let objName: string = getShortClassName(obj);
          if (monitorLeakIDandWhitelist(obj, objName)) { return; }
        }
        registerObject(obj, msg);
      });
    }
    if ((mask & 4) !== 0 || (mask & 8) !== 0) {
      let ret: boolean = registerArkUIObjectLifeCycleCallbackNative((wr: WeakRef<object>) => {
        if (wr === null || wr === undefined) { return; }
        let obj: object | undefined = wr.deref();
        if (obj === undefined) { return; }
        registerObject(obj, '');
      });
      if (!ret) { retryMask |= 4; retryMask |= 8; }
    }
    if ((mask & 16) !== 0) {
      let appCtx = application.getApplicationContextInstance();
      if (appCtx !== undefined) {
        appState.applicationContext = appCtx as Object;
        let cb: LeakWatcherAbilityCallback = new LeakWatcherAbilityCallback();
        lifecycleId = appCtx.onAbilityLifecycle(cb);
      } else {
        retryMask |= 16;
      }
    }
  }

  export function enable(isEnable: boolean): void {
    if (isEnable === undefined || isEnable === null) { throwBusinessError(ERROR_CODE_INVALID_PARAM); }
    enabled = isEnable;
    if (!isEnable) { watchObjMap.clear(); }
  }

  export function watch(obj: object, msg: string): void {
    if (obj === undefined || obj === null || msg === undefined || msg === null) {
      throwBusinessError(ERROR_CODE_INVALID_PARAM);
    }
    if (!enabled) { return; }
    registerObject(obj, msg);
  }

  export function check(): string {
    if (!enabled) { return ''; }
    return JSON.stringify(getLeakList());
  }

  export function dump(filePath: string): string[] {
    if (filePath === undefined || filePath === null) { throwBusinessError(ERROR_CODE_INVALID_PARAM); }
    return dumpInnerSync(filePath, false, false);
  }

  export function enableLeakWatcher(isEnabled: boolean,
    configs: Array<string>, callback: Callback<Array<string>>): void {
    if (isEnabled === undefined || isEnabled === null) { throwBusinessError(ERROR_CODE_ENABLE_INVALID); }
    if (configs === undefined || configs === null) { throwBusinessError(ERROR_CODE_CONFIG_INVALID); }
    if (callback === undefined || callback === null) { throwBusinessError(ERROR_CODE_CALLBACK_INVALID); }
    if (isEnabled === enabled) { console.log('JsLeakWatcher already started or stopped.'); return; }
    enabled = isEnabled;
    if (!isEnabled) { shutdownJsLeakWatcher(); return; }

    let configArray: string[] = configs;
    const valid: string[] = ['CustomComponent', 'Window', 'NodeContainer', 'XComponent', 'Ability'];
    for (let i: int = 0; i < configArray.length; i++) {
      if (!valid.includes(configArray[i])) { throwBusinessError(ERROR_CODE_CONFIG_INVALID); }
    }
    if (configArray.length === 0) { configArray = valid; }
    let appCtx = application.getApplicationContextInstance();
    if (appCtx === undefined) { console.log('enableLeakWatcher applicationContext undefined'); return; }
    appState.applicationContext = appCtx as Object;
    let filePath: string = `${appCtx.filesDir}/${JSLEAK_ROOT_DIR_NAME}`;
    if (!fileIo.accessSync(filePath)) { fileIo.mkdirSync(filePath); }
    getProcessName();
    let mask: int = convertToMask(configArray);
    registerTasksAndExecute(filePath, callback, mask);
  }

  export function enableLeakWatcher(isEnabled: boolean,
    configs: LeakWatcherConfig, callback: Callback<Array<string>>): void {
    if (isEnabled === undefined || isEnabled === null) { throwBusinessError(ERROR_CODE_ENABLE_INVALID); }
    if (configs === undefined || configs === null) { throwBusinessError(ERROR_CODE_CONFIG_INVALID); }
    if (callback === undefined || callback === null) { throwBusinessError(ERROR_CODE_CALLBACK_INVALID); }
    if (isEnabled === enabled) { console.log('JsLeakWatcher already started or stopped.'); return; }
    enabled = isEnabled;
    if (!isEnabled) { shutdownJsLeakWatcher(); return; }

    appState.isConfigObj = true;
    let cfg: LeakWatcherConfig = configs;
    if (cfg.exclusionList !== undefined) {
      let excl: string[] | undefined = cfg.exclusionList;
      if (excl !== undefined) { leakWatcherConfig.exclusionList = [...excl]; }
    }
    if (cfg.objectUniqueIDs !== undefined) {
      let ids: int[] | undefined = cfg.objectUniqueIDs;
      if (ids !== undefined) { leakWatcherConfig.objectUniqueIDs = [...ids]; }
    }
    let ci: int = toInt(cfg.checkInterval, MIN_CHECK_INTERVAL);
    let dh: int = toInt(cfg.dumpHeapWaitTimeMs, toInt(leakWatcherConfig.dumpHeapWaitTimeMs, 5000));
    if (ci > MIN_CHECK_INTERVAL && dh < ci) {
      setGcDelay(ci);
      setDumpDelay(dh);
    } else {
      setGcDelay(toInt(leakWatcherConfig.checkInterval, MIN_CHECK_INTERVAL));
      setDumpDelay(toInt(leakWatcherConfig.dumpHeapWaitTimeMs, 5000));
    }
    leakWatcherConfig.fgLeakCountThreshold = cfg.fgLeakCountThreshold ?? 5;
    leakWatcherConfig.bgLeakCountThreshold = cfg.bgLeakCountThreshold ?? 1;
    let m: int = toInt(cfg.maxStoredHeapDumps, 0);
    maxFileNum = m > 0 ? m * 2 : toInt(leakWatcherConfig.maxStoredHeapDumps, 10) * 2;
    leakWatcherConfig.monitorObjectTypes = cfg.monitorObjectTypes;

    let appCtx = application.getApplicationContextInstance();
    if (appCtx === undefined) { console.log('enableLeakWatcher applicationContext undefined'); return; }
    appState.applicationContext = appCtx as Object;
    let filePath: string = `${appCtx.filesDir}/${JSLEAK_ROOT_DIR_NAME}`;
    if (!fileIo.accessSync(filePath)) { fileIo.mkdirSync(filePath); }
    getProcessName();
    let mask: int = leakWatcherConfig.monitorObjectTypes.valueOf() as int;
    registerTasksAndExecute(filePath, callback, mask);
  }

  function registerTasksAndExecute(filePath: string,
    callback: Callback<Array<string>>, mask: int): void {
    handleGCTask(() => {
      if (currentAttempt <= ATTEMPT_COUNT && retryMask !== 0) {
        executeRegister(retryMask);
        currentAttempt += 1;
      }
      if (appState.isConfigObj) {
        startGCtask(appState.applicationContext);
      } else {
        ArkTools.forceFullGC();
      }
    });
    handleDumpTask(() => {
      if (appState.isConfigObj) {
        startDumptask(filePath, callback);
      } else {
        if (watchObjMap.size === 0) { console.log('No js leak detected.'); return; }
        dumpInner(filePath, true, true, callback);
      }
    });
    handleShutdownTask(() => {
      enabled = false;
      shutdownJsLeakWatcher();
    });
    executeRegister(mask);
  }

}
