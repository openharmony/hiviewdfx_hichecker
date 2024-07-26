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

let util = requireNapi('util');
let fs = requireNapi('file.fs');
let hidebug = requireNapi('hidebug');
let cryptoFramework = requireNapi('security.cryptoFramework');

const ERROR_CODE_INVALID_PARAM = 401;
const ERROR_MSG_INVALID_PARAM = 'Parameter error. Please check!';

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

let watchObjMap = new Map();

const registry = new FinalizationRegistry((hash) => {
  gcObjList.push(hash);
});
const MAX_FILE_NUM = 20;

function flushLeakList() {
  let leakObjList = [];
  for (let [key, value] of watchObjMap) {
    if (!gcObjList.includes(key)) {
      leakObjList.push(value);
    }
  }
  return leakObjList;
}

function createHeapDumpFile(fileName, filePath) {
  hidebug.dumpJsHeapData(fileName);
  let heapDumpFileName = fileName + '.heapsnapshot';
  let desFilePath = filePath + '/' + heapDumpFileName;
  fs.moveFileSync('/data/storage/el2/base/files/' + heapDumpFileName, desFilePath, 0);
  return getHeapDumpSHA256(desFilePath);
}

function getHeapDumpSHA256(filePath) {
  let md = cryptoFramework.createMd('SHA256');
  let heapDumpFile = fs.openSync(filePath, fs.OpenMode.READ_WRITE);
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
  return Array.from(digestOutPut.data, byte => ('0' + byte.toString(16)).slice(-2)).join('').toUpperCase();
}

function deleteOldFile(filePath) {
  let listFileOption = {
    recursion: false,
    listNum: 0,
    filter: {
      suffix: ['.heapsnapshot', '.jsleaklist'],
      fileSizeOver: 0
    }
  };
  let files = fs.listFileSync(filePath, listFileOption);
  if (files.length > MAX_FILE_NUM) {
    const regex = /(\d+)\.(heapsnapshot|jsleaklist)/;
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

let jsLeakWatcher = {
  watch: (obj, msg) => {
    if (obj === undefined || obj === null || msg === undefined || msg === null) {
      throw new BusinessError(ERROR_CODE_INVALID_PARAM);
    }
    if (!enabled) {
      return;
    }
    let objMsg = {
      hash: util.getHash(obj),
      name: obj.constructor.name,
      msg: msg
    };
    watchObjMap.set(objMsg.hash, objMsg);
    registry.register(obj, objMsg.hash);
  },
  check: () => {
    if (!enabled) {
      return '';
    }
    let leakObjList = flushLeakList();
    return JSON.stringify(leakObjList);
  },
  dump: (filePath) => {
    if (filePath === undefined || filePath === null) {
      throw new BusinessError(ERROR_CODE_INVALID_PARAM);
    }
    let fileArray = new Array(2);
    if (!enabled) {
      return fileArray;
    }
    if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
      throw new BusinessError(ERROR_CODE_INVALID_PARAM);
    }

    const fileTimeStamp = new Date().getTime().toString();
    try {
      const heapDumpSHA256 = createHeapDumpFile(fileTimeStamp, filePath);

      let file = fs.openSync(filePath + '/' + fileTimeStamp + '.jsleaklist', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
      let leakObjList = flushLeakList();
      let result = {
        snapshot_hash: heapDumpSHA256,
        leakObjList: leakObjList
      };
      fs.writeSync(file.fd, JSON.stringify(result));
      fs.closeSync(file);
    } catch (error) {
      console.log('Dump heaoSnapShot or LeakList failed! ' + e);
      return fileArray;
    }

    try {
      deleteOldFile(filePath);
    } catch (e) {
      console.log('Delete old files failed! ' + e);
      return fileArray;
    }

    fileArray[0] = fileTimeStamp + '.jsleaklist';
    fileArray[1] = fileTimeStamp + '.heapsnapshot';
    return fileArray;
  },
  enable: (isEnable) => {
    if (isEnable === undefined || isEnable === null) {
      throw new BusinessError(ERROR_CODE_INVALID_PARAM);
    }
    enabled = isEnable;
    if (!isEnable) {
      gcObjList.length = 0;
      watchObjMap.clear();
    }
  }
};

export default jsLeakWatcher;
