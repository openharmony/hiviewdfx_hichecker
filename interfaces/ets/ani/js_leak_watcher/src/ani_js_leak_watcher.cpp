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

#include "ani_js_leak_watcher.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>

#include "ani_util.h"
#include "hilog/log.h"
#include "sys_param.h"
#include "dfx_jsnapi.h"

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
    if (fd == -1) {
        return false;
    }
    fdsan_exchange_owner_tag(fd, 0, FDTAG);
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

static void CloseFileWithError(FILE *file, const char *name)
{
    if (fclose(file) != 0) {
        HILOG_ERROR(LOG_CORE, "AppendMetaData fclose %{public}s failed", name);
    }
}

static bool WriteMetaData(FILE *targetFile, FILE *metaDataFile)
{
    constexpr auto buffSize = 1024;
    char buff[buffSize] = {0};
    size_t readSize = 0;
    while ((readSize = fread(buff, 1, buffSize, metaDataFile)) > 0) {
        if (fwrite(buff, 1, readSize, targetFile) != readSize) {
            return false;
        }
    }
    return true;
}

static bool WriteFileSize(FILE *targetFile, uint32_t size)
{
    return fwrite(&size, sizeof(size), 1, targetFile) == 1;
}

static void CloseAllFiles(FILE *targetFile, FILE *metaDataFile)
{
    if (targetFile != nullptr) {
        CloseFileWithError(targetFile, "targetFile");
    }
    if (metaDataFile != nullptr) {
        CloseFileWithError(metaDataFile, "metaDataFile");
    }
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
        CloseFileWithError(targetFile, "targetFile");
        return false;
    }
    if (!WriteMetaData(targetFile, metaDataFile)) {
        CloseAllFiles(targetFile, metaDataFile);
        return false;
    }
    if (!WriteFileSize(targetFile, rawHeapFileSize)) {
        CloseAllFiles(targetFile, metaDataFile);
        return false;
    }
    if (!WriteFileSize(targetFile, metaDataFileSize)) {
        CloseAllFiles(targetFile, metaDataFile);
        return false;
    }
    CloseAllFiles(targetFile, metaDataFile);
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
    g_handler->SetShutdownFuncRef(gRef);
}

static void RemoveTask(ani_env *env)
{
    g_handler->SetJsLeakWatcherStatus(false);
}

static void ExecuteDumpCallback(ani_ref gCallback, uint8_t retcode)
{
    ani_env *workerEnv = AttachAniEnv(g_aniVm);
    if (workerEnv == nullptr) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap callback AttachAniEnv failed");
        ani_env *fallbackEnv = GetAniEnv(g_aniVm);
        if (fallbackEnv != nullptr && !JsLeakWatcherAniUtil::IsRefUndefined(fallbackEnv, gCallback)) {
            fallbackEnv->GlobalReference_Delete(gCallback);
        }
        return;
    }
    ani_size nrRefs = 16;
    if (workerEnv->CreateLocalScope(nrRefs) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap callback CreateLocalScope failed");
        if (!JsLeakWatcherAniUtil::IsRefUndefined(workerEnv, gCallback)) {
            workerEnv->GlobalReference_Delete(gCallback);
        }
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
}

static void DumpRawHeapCallback(ani_ref gCallback, const std::string &dynamicPath, uint8_t retcode)
{
    HILOG_INFO(LOG_CORE, "DumpRawHeap callback get retcode: %{public}d", retcode);
    AppendMetaData(dynamicPath);
    auto handler = GetLeakWatcherHandler();
    if (handler == nullptr) {
        ani_env *env = GetAniEnv(g_aniVm);
        if (env != nullptr && !JsLeakWatcherAniUtil::IsRefUndefined(env, gCallback)) {
            env->GlobalReference_Delete(gCallback);
        }
        return;
    }
    bool postOk = handler->PostTask([gCallback, retcode]() {
        ExecuteDumpCallback(gCallback, retcode);
        }, "DumpRawHeapCallback", 0, OHOS::AppExecFwk::EventQueue::Priority::IMMEDIATE, {});
    if (!postOk) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap callback PostTask failed");
        ani_env *env = GetAniEnv(g_aniVm);
        if (env != nullptr && !JsLeakWatcherAniUtil::IsRefUndefined(env, gCallback)) {
            env->GlobalReference_Delete(gCallback);
        }
    }
}

static void DumpRawHeap(ani_env *env, ani_string dynamicPathAni, ani_string staticPathAni, ani_ref callbackRef)
{
    std::string dynamicPath;
    std::string staticPath;
    if (JsLeakWatcherAniUtil::ParseAniString(env, dynamicPathAni, dynamicPath) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap ParseAniString dynamicPath failed");
        return;
    }
    if (JsLeakWatcherAniUtil::ParseAniString(env, staticPathAni, staticPath) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap ParseAniString staticPath failed");
        return;
    }
    if (!CreateFile(dynamicPath) || !CreateFile(staticPath)) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap CreateFile failed");
        return;
    }
    ani_ref gCallback = JsLeakWatcherAniUtil::CreateGlobalReference(env, callbackRef);
    if (g_aniVm == nullptr) {
        g_aniVm = JsLeakWatcherAniUtil::GetAniVm(env);
    }
    auto callback = [gCallback, dynamicPath](uint8_t retcode) {
        DumpRawHeapCallback(gCallback, dynamicPath, retcode);
    };
    panda::DumpSnapShotOption dumpOption;
    dumpOption.dumpFormat = panda::DumpFormat::BINARY;
    dumpOption.isVmMode = true;
    dumpOption.isJSLeakWatcher = true;
    dumpOption.isFullGC = false;
    dumpOption.isBeforeFill = false;
    dumpOption.isSync = false;
    dumpOption.languageEnv = panda::ecmascript::LanguageEnv::HYBRID;
    if (!panda::DFXJSNApi::DumpHybridRawHeapSnapshot(dynamicPath, staticPath, dumpOption, callback)) {
        HILOG_ERROR(LOG_CORE, "DumpHybridRawHeapSnapshot failed.");
        callback(static_cast<uint8_t>(1));
    }
}

static void DumpRawHeapSync(ani_env *env, ani_string dynamicPathAni, ani_string staticPathAni)
{
    HILOG_INFO(LOG_CORE, "DumpRawHeapSync begin!");
    std::string dynamicPath;
    std::string staticPath;
    if (JsLeakWatcherAniUtil::ParseAniString(env, dynamicPathAni, dynamicPath) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeapSync ParseAniString dynamicPath failed");
        return;
    }
    if (JsLeakWatcherAniUtil::ParseAniString(env, staticPathAni, staticPath) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeapSync ParseAniString staticPath failed");
        return;
    }
    if (!CreateFile(dynamicPath)) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeapSync CreateFile dynamicPath failed");
        return;
    }
    if (!CreateFile(staticPath)) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeapSync CreateFile staticPath failed");
        return;
    }
    panda::DumpSnapShotOption dumpOption;
    dumpOption.dumpFormat = panda::DumpFormat::BINARY;
    dumpOption.isVmMode = true;
    dumpOption.isJSLeakWatcher = true;
    dumpOption.isFullGC = false;
    dumpOption.isBeforeFill = false;
    dumpOption.isSync = true;
    dumpOption.languageEnv = panda::ecmascript::LanguageEnv::HYBRID;
    auto dummyCallback = []([[maybe_unused]] uint8_t code) {};
    if (!panda::DFXJSNApi::DumpHybridRawHeapSnapshot(dynamicPath, staticPath, dumpOption, dummyCallback)) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeapSync DumpHybridRawHeapSnapshot failed");
        return;
    }
    AppendMetaData(dynamicPath);
}

static void ExecuteArkUILifecycleCallback(void *data)
{
    ani_env *aniEnv = GetAniEnv(g_aniVm);
    if (aniEnv == nullptr) {
        HILOG_ERROR(LOG_CORE, "ArkUILifecycleCallback GetAniEnv failed");
        return;
    }
    if (JsLeakWatcherAniUtil::IsRefUndefined(aniEnv, g_arkUICallbackRef)) {
        HILOG_ERROR(LOG_CORE, "ArkUILifecycleCallback g_arkUICallbackRef undefined");
        return;
    }
    if (data == nullptr) {
        HILOG_ERROR(LOG_CORE, "ArkUILifecycleCallback data is null");
        return;
    }
    auto *lifecycleData = static_cast<ArkUIObjectLifecycleData *>(data);
    ani_size nrRefs = 16;
    if (aniEnv->CreateLocalScope(nrRefs) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "ArkUILifecycleCallback CreateLocalScope failed");
        return;
    }
    ani_ref args[1] = { static_cast<ani_ref>(lifecycleData->weakRef) };
    ani_ref ret {};
    if (aniEnv->FunctionalObject_Call(reinterpret_cast<ani_fn_object>(g_arkUICallbackRef),
        1, args, &ret) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "ArkUILifecycleCallback FunctionalObject_Call failed");
    }
    aniEnv->DestroyLocalScope();
}

static ani_boolean RegisterArkUIObjectLifeCycleCallback(ani_env *env, ani_ref funcRef)
{
    if (JsLeakWatcherAniUtil::IsRefUndefined(env, funcRef)) {
        return ANI_FALSE;
    }
    auto uiContext = OHOS::Ace::Kit::UIContext::Current();
    if (uiContext == nullptr) {
        HILOG_ERROR(LOG_CORE, "RegisterArkUIObjectLifeCycleCallback UIContext is null");
        return ANI_FALSE;
    }
    if (!JsLeakWatcherAniUtil::IsRefUndefined(env, g_arkUICallbackRef)) {
        env->GlobalReference_Delete(g_arkUICallbackRef);
        g_arkUICallbackRef = nullptr;
    }
    g_arkUICallbackRef = JsLeakWatcherAniUtil::CreateGlobalReference(env, funcRef);
    if (JsLeakWatcherAniUtil::IsRefUndefined(env, g_arkUICallbackRef)) {
        HILOG_ERROR(LOG_CORE, "RegisterArkUIObjectLifeCycleCallback CreateGlobalReference failed");
        return ANI_FALSE;
    }
    if (g_aniVm == nullptr) {
        g_aniVm = JsLeakWatcherAniUtil::GetAniVm(env);
    }
    uiContext->RegisterArkUIObjectLifecycleCallback([](void *data) {
        ExecuteArkUILifecycleCallback(data);
    });
    return ANI_TRUE;
}

static void UnregisterArkUIObjectLifeCycleCallback(ani_env *env)
{
    auto uiContext = OHOS::Ace::Kit::UIContext::Current();
    if (uiContext != nullptr) {
        uiContext->UnregisterArkUIObjectLifecycleCallback();
    }
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

ani_status ANI_ConstructorImpl(ani_vm *vm, uint32_t *result)
{
    ani_env *env = nullptr;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        HILOG_ERROR(LOG_CORE, "Unsupported ANI_VERSION_1");
        return ANI_ERROR;
    }
    g_aniVm = vm;
    ani_namespace ns {};
    if (ANI_OK != env->FindNamespace("@ohos.hiviewdfx.jsLeakWatcher.jsLeakWatcher", &ns)) {
        HILOG_ERROR(LOG_CORE, "FindNamespace @ohos.hiviewdfx.jsLeakWatcher.jsLeakWatcher failed");
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
        ani_native_function {"registerArkUIObjectLifeCycleCallbackNative", nullptr,
            reinterpret_cast<void *>(RegisterArkUIObjectLifeCycleCallback)},
        ani_native_function {"unregisterArkUIObjectLifeCycleCallbackNative", nullptr,
            reinterpret_cast<void *>(UnregisterArkUIObjectLifeCycleCallback)},
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
