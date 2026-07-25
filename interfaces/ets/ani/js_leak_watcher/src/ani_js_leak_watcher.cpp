/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
 *
*/

#include "ani_js_leak_watcher.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <atomic>
#include <chrono>
#include <cstring>

#include "ani_util.h"
#include "hilog/log.h"
#include "sys_param.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD003D00
#undef LOG_TAG
#define LOG_TAG "JSLEAK_WATCHER_ANI"

#define JSLEAK_WATCHER_NAME_LEN 256

namespace OHOS {
namespace HiviewDFX {
namespace {
auto g_runner = OHOS::AppExecFwk::EventRunner::Current();
auto g_handler = std::make_shared<LeakWatcherEventHandler>(g_runner);
ani_ref g_arkUICallbackRef = nullptr;
ani_vm *g_aniVm = nullptr;

ani_env *GetAniEnv(ani_vm *vm)
{
    ani_env *env = nullptr;
    if (vm == nullptr || vm->GetEnv(ANI_VERSION_1, &env) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "GetEnv failed");
        return nullptr;
    }
    return env;
}

ani_env *AttachAniEnv(ani_vm *vm)
{
    ani_env *workerEnv = nullptr;
    ani_options aniArgs {0, nullptr};
    if (vm->AttachCurrentThread(&aniArgs, ANI_VERSION_1, &workerEnv) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "Attach Env failed");
        return nullptr;
    }
    return workerEnv;
}
} // namespace

// === LeakWatcherEventHandler 实现 ===
void LeakWatcherEventHandler::ProcessEvent(const OHOS::AppExecFwk::InnerEvent::Pointer &event)
{
    if (!isRunning_) {
        return;
    }
    auto eventId = event->GetInnerEventId();
    if (eventId == DUMP_EVENT_ID) {
        ExecuteJsFunc(dumpFuncRef_);
    } else if (eventId == GC_EVENT_ID) {
        ExecuteJsFunc(gcFuncRef_);
        SendEvent(GC_EVENT_ID, gcDelayTime_, Priority::IDLE);
        SendEvent(DUMP_EVENT_ID, dumpDelayTime_, Priority::IDLE);
    }
}

void LeakWatcherEventHandler::SetJsLeakWatcherStatus(bool isRunning)
{
    isRunning_ = isRunning;
    if (!isRunning) {
        Reset();
    }
}

void LeakWatcherEventHandler::Reset()
{
    ani_env *env = GetAniEnv(g_aniVm);
    if (env != nullptr && !JsLeakWatcherAniUtil::IsRefUndefined(env, dumpFuncRef_)) {
        env->GlobalReference_Delete(dumpFuncRef_);
        dumpFuncRef_ = nullptr;
    }
    if (env != nullptr && !JsLeakWatcherAniUtil::IsRefUndefined(env, gcFuncRef_)) {
        env->GlobalReference_Delete(gcFuncRef_);
        gcFuncRef_ = nullptr;
    }
    if (env != nullptr && !JsLeakWatcherAniUtil::IsRefUndefined(env, shutdownFuncRef_)) {
        env->GlobalReference_Delete(shutdownFuncRef_);
        shutdownFuncRef_ = nullptr;
    }
}

void LeakWatcherEventHandler::ExecuteJsFunc(ani_ref callbackRef)
{
    ani_env *env = GetAniEnv(g_aniVm);
    if (env == nullptr) {
        HILOG_ERROR(LOG_CORE, "ExecuteJsFunc env is null");
        return;
    }
    if (JsLeakWatcherAniUtil::IsRefUndefined(env, callbackRef)) {
        HILOG_ERROR(LOG_CORE, "ExecuteJsFunc callbackRef is undefined");
        return;
    }
    ani_size nrRefs = 16;
    if (env->CreateLocalScope(nrRefs) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "ExecuteJsFunc CreateLocalScope failed");
        return;
    }
    ani_ref ret {};
    if (env->FunctionalObject_Call(reinterpret_cast<ani_fn_object>(callbackRef), 0, nullptr, &ret) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "ExecuteJsFunc FunctionalObject_Call failed");
    }
    env->DestroyLocalScope();
}

std::shared_ptr<LeakWatcherEventHandler> GetLeakWatcherHandler()
{
    return g_handler;
}

bool CreateFile(const std::string &filePath)
{
    if (access(filePath.c_str(), F_OK) == 0) {
        return access(filePath.c_str(), W_OK) == 0;
    }
    const mode_t defaultMode = S_IRUSR | S_IWUSR | S_IRGRP; // -rw-r-----
    int fd = creat(filePath.c_str(), defaultMode);
    fdsan_exchange_owner_tag(fd, 0, FDTAG);
    if (fd == -1) {
        return false;
    }
    fdsan_close_with_tag(fd, FDTAG);
    return true;
}

uint64_t GetFileSize(const std::string &filePath)
{
    struct stat st;
    if (stat(filePath.c_str(), &st) != 0) {
        return 0;
    }
    return st.st_size;
}

bool AppendMetaData(const std::string &filePath)
{
#ifdef __aarch64__
    const char *metaDataPath = "/system/lib64/module/arkcompiler/metadata.json";
#else
    const char *metaDataPath = "/system/lib/module/arkcompiler/metadata.json";
#endif
    auto rawHeapFileSize = static_cast<uint32_t>(GetFileSize(filePath));
    auto metaDataFileSize = static_cast<uint32_t>(GetFileSize(metaDataPath));
    FILE *targetFile = fopen(filePath.c_str(), "ab");
    if (targetFile == nullptr) {
        return false;
    }
    FILE *metaDataFile = fopen(metaDataPath, "rb");
    if (metaDataFile == nullptr) {
        fclose(targetFile);
        return false;
    }
    constexpr auto buffSize = 1024;
    char buff[buffSize] = {0};
    size_t readSize = 0;
    while ((readSize = fread(buff, 1, buffSize, metaDataFile)) > 0) {
        if (fwrite(buff, 1, readSize, targetFile) != readSize) {
            fclose(targetFile);
            fclose(metaDataFile);
            return false;
        }
    }
    if (fwrite(&rawHeapFileSize, sizeof(rawHeapFileSize), 1, targetFile) != 1) {
        fclose(targetFile);
        fclose(metaDataFile);
        return false;
    }
    if (fwrite(&metaDataFileSize, sizeof(metaDataFileSize), 1, targetFile) != 1) {
        fclose(targetFile);
        fclose(metaDataFile);
        return false;
    }
    fclose(targetFile);
    fclose(metaDataFile);
    return true;
}

static bool GetCallbackRef(ani_env *env, ani_ref funcRef, ani_ref *gRef)
{
    if (JsLeakWatcherAniUtil::IsRefUndefined(env, funcRef)) {
        return false;
    }
    *gRef = JsLeakWatcherAniUtil::CreateGlobalReference(env, funcRef);
    return !JsLeakWatcherAniUtil::IsRefUndefined(env, *gRef);
}

static ani_boolean GetDumpStatus(ani_env *env)
{
    char paraName[JSLEAK_WATCHER_NAME_LEN] = "hiviewdfx.hichecker.jsleakwatcher.dump";
    CachedHandle appEnableHandle = CachedParameterCreate(paraName, "true");
    if (appEnableHandle == nullptr) {
        return ANI_TRUE;
    }
    const char *paramValue = CachedParameterGet(appEnableHandle);
    ani_boolean result = ANI_TRUE;
    if (paramValue != nullptr && strlen(paramValue) != 0 && strcmp(paramValue, "false") == 0) {
        result = ANI_FALSE;
    }
    CachedParameterDestroy(appEnableHandle);
    return result;
}

static void SetDumpDelay(ani_env *env, ani_int delay)
{
    g_handler->SetDumpDelayTime(static_cast<uint32_t>(delay));
}

static void SetGcDelay(ani_env *env, ani_int delay)
{
    g_handler->SetGcDelayTime(static_cast<uint32_t>(delay));
}

static void HandleDumpTask(ani_env *env, ani_ref funcRef)
{
    ani_ref gRef = nullptr;
    if (!GetCallbackRef(env, funcRef, &gRef)) {
        HILOG_ERROR(LOG_CORE, "HandleDumpTask GetCallbackRef failed");
        return;
    }
    g_handler->SetAniEnv(env);
    g_handler->SetDumpFuncRef(gRef);
    g_handler->SetJsLeakWatcherStatus(true);
}

static void HandleGCTask(ani_env *env, ani_ref funcRef)
{
    ani_ref gRef = nullptr;
    if (!GetCallbackRef(env, funcRef, &gRef)) {
        HILOG_ERROR(LOG_CORE, "HandleGCTask GetCallbackRef failed");
        return;
    }
    g_handler->SetAniEnv(env);
    g_handler->SetGcFuncRef(gRef);
    g_handler->SendEvent(GC_EVENT_ID, g_handler->GetGcDelayTime(), LeakWatcherEventHandler::Priority::IDLE);
}

static void HandleShutdownTask(ani_env *env, ani_ref funcRef)
{
    ani_ref gRef = nullptr;
    if (!GetCallbackRef(env, funcRef, &gRef)) {
        HILOG_ERROR(LOG_CORE, "HandleShutdownTask GetCallbackRef failed");
        return;
    }
    g_handler->SetAniEnv(env);
    g_handler->SetShutdownFuncRef(gRef);
}

static void RemoveTask(ani_env *env)
{
    g_handler->SetJsLeakWatcherStatus(false);
}

static void DumpRawHeap(ani_env *env, ani_string filePathAni, ani_ref callbackRef)
{
    HILOG_INFO(LOG_CORE, "DumpRawHeap begin!");
    std::string filePath;
    if (JsLeakWatcherAniUtil::ParseAniString(env, filePathAni, filePath) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap ParseAniString failed");
        return;
    }
    if (!CreateFile(filePath)) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap CreateFile failed");
        return;
    }
        ani_ref gCallback = JsLeakWatcherAniUtil::CreateGlobalReference(env, callbackRef);
    if (g_aniVm == nullptr) {
        g_aniVm = JsLeakWatcherAniUtil::GetAniVm(env);
    }
        auto callback = [gCallback, filePath](uint8_t retcode) {
        HILOG_INFO(LOG_CORE, "DumpRawHeap callback get retcode: %{public}d", retcode);
        AppendMetaData(filePath);
                auto handler = GetLeakWatcherHandler();
        if (handler == nullptr) {
            return;
        }
        handler->PostTask([gCallback, retcode]() {
            ani_env *workerEnv = AttachAniEnv(g_aniVm);
            if (workerEnv == nullptr) {
                HILOG_ERROR(LOG_CORE, "DumpRawHeap callback AttachAniEnv failed");
                return;
            }
            ani_size nrRefs = 16;
            if (workerEnv->CreateLocalScope(nrRefs) != ANI_OK) {
                HILOG_ERROR(LOG_CORE, "DumpRawHeap callback CreateLocalScope failed");
                return;
            }
            if (JsLeakWatcherAniUtil::IsRefUndefined(workerEnv, gCallback)) {
                HILOG_ERROR(LOG_CORE, "DumpRawHeap callback gCallback undefined");
                workerEnv->DestroyLocalScope();
                return;
            }
                        ani_object retCodeObj = JsLeakWatcherAniUtil::CreateInt(workerEnv, retcode);
            ani_ref args[1] = {static_cast<ani_ref>(retCodeObj)};
            ani_ref ret {};
            if (workerEnv->FunctionalObject_Call(reinterpret_cast<ani_fn_object>(gCallback), 1, args, &ret)
                != ANI_OK) {
                HILOG_ERROR(LOG_CORE, "DumpRawHeap callback FunctionalObject_Call failed");
            }
            workerEnv->DestroyLocalScope();
                        workerEnv->GlobalReference_Delete(gCallback);
        }, "DumpRawHeapCallback", 0, OHOS::AppExecFwk::EventQueue::Priority::IMMEDIATE, {});
    };
    DumpHeapSnapshotImplAsync(filePath, callback);
}

static void DumpRawHeapSync(ani_env *env, ani_string filePathAni)
{
    HILOG_INFO(LOG_CORE, "DumpRawHeapSync begin!");
    std::string filePath;
    if (JsLeakWatcherAniUtil::ParseAniString(env, filePathAni, filePath) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeapSync ParseAniString failed");
        return;
    }
    if (!CreateFile(filePath)) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeapSync CreateFile failed");
        return;
    }
    if (!DumpHeapSnapshotImpl(filePath)) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeapSync DumpHeapSnapshotImpl failed");
        return;
    }
    AppendMetaData(filePath);
}

static ani_boolean RegisterArkUIObjectLifeCycleCallback(ani_env *env, ani_ref funcRef)
{
    if (JsLeakWatcherAniUtil::IsRefUndefined(env, funcRef)) {
        return ANI_FALSE;
    }
    g_arkUICallbackRef = JsLeakWatcherAniUtil::CreateGlobalReference(env, funcRef);
    if (g_aniVm == nullptr) {
        g_aniVm = JsLeakWatcherAniUtil::GetAniVm(env);
    }
    return ANI_TRUE;
}

static void UnregisterArkUIObjectLifeCycleCallback(ani_env *env)
{
    ani_env *aniEnv = GetAniEnv(g_aniVm);
    if (aniEnv != nullptr && !JsLeakWatcherAniUtil::IsRefUndefined(aniEnv, g_arkUICallbackRef)) {
        aniEnv->GlobalReference_Delete(g_arkUICallbackRef);
        g_arkUICallbackRef = nullptr;
    }
}

static ani_boolean GetDumpStatusAni(ani_env *env)
{
    return GetDumpStatus(env);
}

static ani_int GetPid(ani_env *env)
{
    return static_cast<ani_int>(getpid());
}

static ani_int GetTid(ani_env *env)
{
    return static_cast<ani_int>(gettid());
}

static ani_string GetClassName(ani_env *env, ani_object obj)
{
    ani_ref strRef {};
        if (env->Object_CallMethodByName_Ref(obj, "toString", ":C{std.core.String}", &strRef) != ANI_OK) {
        ani_string empty {};
        env->String_NewUTF8("", 0, &empty);
        return empty;
    }
    if (JsLeakWatcherAniUtil::IsRefUndefined(env, strRef)) {
        ani_string empty {};
        env->String_NewUTF8("", 0, &empty);
        return empty;
    }
    ani_string aniStr = static_cast<ani_string>(strRef);
    std::string str;
    if (JsLeakWatcherAniUtil::ParseAniString(env, aniStr, str) != ANI_OK) {
        ani_string empty {};
        env->String_NewUTF8("", 0, &empty);
        return empty;
    }
    // toString() 返回 "[object ClassName]" 格式, 提取类名
    // 也可能直接返回类名, 取实际值
    const std::string prefix = "[object ";
    const std::string suffix = "]";
    if (str.size() > prefix.size() + suffix.size() &&
        str.compare(0, prefix.size(), prefix) == 0 &&
        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0) {
        str = str.substr(prefix.size(), str.size() - prefix.size() - suffix.size());
    }
    ani_string result {};
    env->String_NewUTF8(str.c_str(), str.size(), &result);
    return result;
}

ani_status ANI_ConstructorImpl(ani_vm *vm, uint32_t *result)
{
    ani_env *env = nullptr;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        HILOG_ERROR(LOG_CORE, "Unsupported ANI_VERSION_1");
        return ANI_ERROR;
    }
    g_aniVm = vm;
    ani_namespace ns {};
    if (ANI_OK != env->FindNamespace("@ohos.hiviewdfx.jsLeakWatcher", &ns)) {
        HILOG_ERROR(LOG_CORE, "FindNamespace @ohos.hiviewdfx.jsLeakWatcher failed");
        return ANI_ERROR;
    }
    std::array methods = {
        ani_native_function {"getPid", nullptr, reinterpret_cast<void *>(GetPid)},
        ani_native_function {"getTid", nullptr, reinterpret_cast<void *>(GetTid)},
        ani_native_function {"getDumpStatus", nullptr, reinterpret_cast<void *>(GetDumpStatusAni)},
        ani_native_function {"setGcDelay", nullptr, reinterpret_cast<void *>(SetGcDelay)},
        ani_native_function {"setDumpDelay", nullptr, reinterpret_cast<void *>(SetDumpDelay)},
        ani_native_function {"dumpRawHeap", nullptr, reinterpret_cast<void *>(DumpRawHeap)},
        ani_native_function {"dumpRawHeapSync", nullptr, reinterpret_cast<void *>(DumpRawHeapSync)},
        ani_native_function {"handleDumpTask", nullptr, reinterpret_cast<void *>(HandleDumpTask)},
        ani_native_function {"handleGCTask", nullptr, reinterpret_cast<void *>(HandleGCTask)},
        ani_native_function {"handleShutdownTask", nullptr, reinterpret_cast<void *>(HandleShutdownTask)},
        ani_native_function {"removeTask", nullptr, reinterpret_cast<void *>(RemoveTask)},
        ani_native_function {"registerArkUIObjectLifeCycleCallback", nullptr,
            reinterpret_cast<void *>(RegisterArkUIObjectLifeCycleCallback)},
        ani_native_function {"unregisterArkUIObjectLifeCycleCallback", nullptr,
            reinterpret_cast<void *>(UnregisterArkUIObjectLifeCycleCallback)},
        ani_native_function {"getClassName", nullptr, reinterpret_cast<void *>(GetClassName)},
    };
    if (ANI_OK != env->Namespace_BindNativeFunctions(ns, methods.data(), methods.size())) {
        HILOG_ERROR(LOG_CORE, "Namespace_BindNativeFunctions failed");
        return ANI_ERROR;
    }
    *result = ANI_VERSION_1;
    return ANI_OK;
}
} // namespace HiviewDFX
} // namespace OHOS

ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    return OHOS::HiviewDFX::ANI_ConstructorImpl(vm, result);
}
