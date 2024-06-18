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

let util = requireNapi('util')
let fs = requireNapi('file.fs')
let hidebug = requireNapi('hidebug')
let cryptoFramework = requireNapi('security.cryptoFramework')



const ERROR_CODE_INVALID_PARAM = 401;
const ERROR_MSG_INVALID_PARAM = 'Parameter error. The file path is invalid.';

let errMap = new Map();
errMap.set(ERROR_CODE_INVALID_PARAM, ERROR_MSG_INVALID_PARAM);

class BusinessError extends Error {
  constructor(code) {
    let msg = '';
    if (errMap.has(code)) {
      msg = errMap.get(code);
    } else {
      msg = ERROR_MSG_INNER_ERROR;
    }
    super(msg);
    this.code = code;
  }
}

let enabled = false;
let gcObjList = [];
let leakObjList = [];
let watchObjMap = new Map();

const MAX_FILE_NUM = 20;

let jsLeakWatcher = {
  watch: (obj, msg) => {
    if (!enabled) {
      return;
    }
    let objMsg = {
      hash: util.getHash(obj),
      name: obj.constructor.name,
      msg: msg
    };
    watchObjMap.set(objMsg.hash, objMsg);

    const registry = new FinalizationRegistry((hash) => {
      gcObjList.push(hash);
    });
    registry.register(obj, objMsg.hash);
  },
  check: () => {
    if (!enabled) {
      return;
    }
    ArkTools.forceFullGC();
    for (let [key, value] of watchObjMap) {
      if (!gcObjList.includes(key)) {
        leakObjList.push(value);
      }
    }
    return JSON.stringify(leakObjList);
  },
  dump: (filePath) => {
    if (!enabled) {
      return;
    }
    if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
      throw new BusinessError(ERROR_CODE_INVALID_PARAM);
    }

    const timeStamp = new Date().getTime().toString();
    try {
      hidebug.dumpJsHeapData(timeStamp);
      let heapDumpFileName = timeStamp + '.heapsnapshot';
      fs.moveFileSync('/data/storage/el2/base/files/' + heapDumpFileName, filePath + '/' + heapDumpFileName, 0);

      let file = fs.openSync(filePath + '/' + timeStamp + '.jsleakList', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
      let md = cryptoFramework.createMd('SHA256');
      let heapDumpFile = fs.openSync(filePath + '/' + heapDumpFileName, fs.OpenMode.READ_WRITE);
      let bufSize = 1024;
      let readSize = 0;
      let buf = new ArrayBuffer(bufSize);
      let readOptions = {
        offset: readSize,
        length: bufSize
      };
      let readLen = fs.readSync(heapDumpFile.fd, buf, readOptions);

      while (readLen > 0) {
        md.updateSync({ data: new Uint8Array(buf.slice(0, readLen)) });
        readSize += readLen;
        readOptions.offset = readSize;
        readLen = fs.readSync(heapDumpFile.fd, buf, readOptions);
      }
      fs.closeSync(heapDumpFile);

      let digestOutPut = md.digestSync();
      const hexString = Array.from(digestOutPut.data, byte => ('0' + byte.toString(16)).slice(-2))
                        .join('').toUpperCase();
      let result = {
        snapshot_hash: hexString,
        leakObjList: leakObjList
      };
      fs.writeSync(file.fd, JSON.stringify(result));
      fs.closeSync(file);
    } catch (error) {
      console.log('Dump heaoSnapShot or LeakList failed! ' + e);
      return null;
    }

    try {
      let listFileOption = {
        recursion: false,
        listNum: 0,
        filter: {
          suffix: [".heapsnapshot", ".jsleaklist"],
          fileSizeOver: 0
        }
      };
      let files = fs.listFileSync(filePath, listFileOption);
      if (files.length > MAX_FILE_NUM) {
        for (let i = 0; i < files.length - MAX_FILE_NUM; i++) {
          fs.unlinkSync(filePath + '/' + files[i]);
          console.log(`File: ${files[i]} is deleted.`);
        }
      }
    } catch (e) {
      console.log('Delete old files failed! ' + e);
      return null;
    }

    let fileArray = new Array(2);
    let leakListFileName = timeStamp + '.jsleaklist';
    let heapSnapShotFileName = timeStamp + '.heapsnapshot';
    fileArray[0] = leakListFileName;
    fileArray[1] = heapSnapShotFileName;
    return fileArray;
  },
  enable: (isEnable) => {
    enabled = isEnable;
    if (!isEnable) {
      gcObjList.length = 0;
      leakObjList.length = 0;
      watchObjMap.clear();
    }
  }
};

export default jsLeakWatcher;