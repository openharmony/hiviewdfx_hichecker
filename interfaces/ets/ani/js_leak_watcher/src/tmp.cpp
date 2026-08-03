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

#include "ani_util.h"

#include "dfx_jsnapi.h"
#include "hilog/log.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD003D00
#undef LOG_TAG
#define LOG_TAG "JSLEAK_WATCHER_TMP"

// 临时占位实现:ark::ets::interop::js::IsHybridMode() 和 GetEcmaVM() 定义在
// runtime_core 的 internal_api.cpp 中,但该文件在 OHOS 标准构建中未被编译进
// libarkruntime.so / libark_jsruntime.so,导致链接时 undefined symbol。
// 等待第三方提供 ANI 友好的 DumpHeapSnapshot 接口后,删除本文件即可。
namespace ark::ets::interop::js {
bool IsHybridMode()
{
    // 占位:始终返回 false,DumpHeapSnapshotImpl 会因此直接报错返回,
    // 不影响 so 加载和其他 native 函数的注册。
    return false;
}

const void *GetEcmaVM()
{
    // 占位:返回 nullptr,调用方会检查并报错返回。
    return nullptr;
}
}

namespace OHOS {
namespace HiviewDFX {

// 同步版本 - 对应 NAPI DumpRawHeapSync
// NAPI: engine->DumpHeapSnapshot(filePath, true, DumpFormat::BINARY, false, true, true);
bool DumpHeapSnapshotImpl(const std::string &filePath)
{
    HILOG_INFO(LOG_CORE, "DumpHeapSnapshotImpl sync called, filePath: %{public}s", filePath.c_str());
    if (!ark::ets::interop::js::IsHybridMode()) {
        HILOG_ERROR(LOG_CORE, "DumpHeapSnapshotImpl: hybrid interop not initialized, cannot dump");
        return false;
    }
    const void *raw = ark::ets::interop::js::GetEcmaVM();
    if (raw == nullptr) {
        HILOG_ERROR(LOG_CORE, "DumpHeapSnapshotImpl: GetEcmaVM returned null");
        return false;
    }
    auto *vm = static_cast<const panda::ecmascript::EcmaVM *>(raw);
    panda::ecmascript::DumpSnapShotOption opt;
    opt.isSync = true;
    opt.dumpFormat = panda::ecmascript::DumpFormat::BINARY;
    opt.isFullGC = false;
    opt.isVmMode = true;
    opt.isBeforeFill = false;
    opt.isJSLeakWatcher = true;
    opt.languageEnv = panda::ecmascript::LanguageEnv::HYBRID;
    panda::DFXJSNApi::DumpHeapSnapshot(vm, filePath, opt);
    return true;
}

// 异步版本 - 对应 NAPI DumpRawHeapImpl (带 callback)
// NAPI: DFXJSNApi::DumpHeapSnapshot(vm, filePath, dumpOption, [tsfnContext](uint8_t retcode){...})
void DumpHeapSnapshotImplAsync(const std::string &filePath,
    const std::function<void(uint8_t)> &callback)
{
    HILOG_INFO(LOG_CORE, "DumpHeapSnapshotImplAsync async called, filePath: %{public}s", filePath.c_str());
    if (!ark::ets::interop::js::IsHybridMode()) {
        HILOG_ERROR(LOG_CORE, "DumpHeapSnapshotImplAsync: hybrid interop not initialized, cannot dump");
        if (callback) {
            callback(static_cast<uint8_t>(1)); // FORK_FAILED
        }
        return;
    }
    const void *raw = ark::ets::interop::js::GetEcmaVM();
    if (raw == nullptr) {
        HILOG_ERROR(LOG_CORE, "DumpHeapSnapshotImplAsync: GetEcmaVM returned null");
        if (callback) {
            callback(static_cast<uint8_t>(1)); // FORK_FAILED
        }
        return;
    }
    auto *vm = static_cast<const panda::ecmascript::EcmaVM *>(raw);
    panda::ecmascript::DumpSnapShotOption opt;
    opt.isSync = false;
    opt.dumpFormat = panda::ecmascript::DumpFormat::BINARY;
    opt.isFullGC = false;
    opt.isVmMode = true;
    opt.isBeforeFill = false;
    opt.isJSLeakWatcher = true;
    opt.languageEnv = panda::ecmascript::LanguageEnv::HYBRID;
    panda::DFXJSNApi::DumpHeapSnapshot(vm, filePath, opt, callback);
    panda::DFXJSNApi::DestroyHeapProfiler(vm);
}
} // namespace HiviewDFX
} // namespace OHOS
