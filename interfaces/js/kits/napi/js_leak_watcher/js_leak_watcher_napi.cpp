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
 */

#include <string>
#include <unistd.h>
#include "hilog/log.h"
#include "js_leak_watcher_napi.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD003D00
#undef LOG_TAG
#define LOG_TAG "JSLEAK_WATCHER_C"

using namespace OHOS::Ace;
using namespace OHOS::AppExecFwk;
using namespace OHOS::Rosen;
using ArkUIRuntimeCallInfo = panda::JsiRuntimeCallInfo;

struct TsfnContext {
    napi_threadsafe_function tsfn;
};

auto g_runner = EventRunner::Current();
auto g_handler = std::make_shared<LeakWatcherEventHandler>(g_runner);
auto g_listener = OHOS::sptr<WindowLifeCycleListener>::MakeSptr();
napi_ref g_callbackRef = nullptr;

static bool CreateFile(const std::string& filePath)
{
    if (access(filePath.c_str(), F_OK) == 0) {
        return access(filePath.c_str(), W_OK) == 0;
    }
    const mode_t defaultMode = S_IRUSR | S_IWUSR | S_IRGRP; // -rw-r-----
    int fd = creat(filePath.c_str(), defaultMode);
    if (fd == -1) {
        return false;
    }
    close(fd);
    return true;
}

static bool GetNapiStringValue(napi_env env, napi_value value, std::string& str)
{
    size_t bufLen = 0;
    napi_status status = napi_get_value_string_utf8(env, value, nullptr, 0, &bufLen);
    if (status != napi_ok) {
        return false;
    }
    str.reserve(bufLen + 1);
    str.resize(bufLen);
    status = napi_get_value_string_utf8(env, value, str.data(), bufLen + 1, &bufLen);
    if (status != napi_ok) {
        return false;
    }
    return true;
}

static uint64_t GetFileSize(const std::string& filePath)
{
    struct stat st;
    if (stat(filePath.c_str(), &st) != 0) {
        return 0;
    }
    return st.st_size;
}

static bool AppendMetaData(const std::string& filePath)
{
    const char* metaDataPath = "/system/lib64/module/arkcompiler/metadata.json";
    auto rawHeapFileSize = static_cast<uint32_t>(GetFileSize(filePath));
    auto metaDataFileSize = static_cast<uint32_t>(GetFileSize(metaDataPath));
    FILE* targetFile = fopen(filePath.c_str(), "ab");
    if (targetFile == nullptr) {
        return false;
    }
    FILE* metaDataFile = fopen(metaDataPath, "rb");
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

static napi_value CreateUndefined(napi_env env)
{
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

static bool GetCallbackRef(napi_env env, napi_callback_info info, napi_ref* ref)
{
    napi_handle_scope scope = nullptr;
    napi_open_handle_scope(env, &scope);
    size_t argc = ONE_VALUE_LIMIT;
    napi_value argv[ONE_VALUE_LIMIT] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc != ONE_VALUE_LIMIT) {
        napi_close_handle_scope(env, scope);
        return false;
    }
    napi_create_reference(env, argv[0], 1, ref);
    napi_close_handle_scope(env, scope);
    return true;
}

static void MainThreadExec(napi_env env, napi_value jscb, void* context, void* data)
{
    HILOG_INFO(LOG_CORE, "main thread callback starts");
    uint8_t* pData = static_cast<uint8_t*>(data);
    if (!pData) {
        HILOG_ERROR(LOG_CORE, "MainThreadExec pData is nulltr!");
        return;
    }
    uint8_t value = *pData;

    napi_value argv[1];
    napi_value global = nullptr;
    if (napi_get_global(env, &global) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "MainThreadExec napi_get_global failed");
        delete pData;
        return;
    }
    if (napi_create_uint32(env, value, &argv[0]) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "MainThreadExec napi_create_uint32 failed");
        delete pData;
        return;
    }
    napi_call_function(env, global, jscb, 1, argv, nullptr);

    if (context == nullptr) {
        HILOG_ERROR(LOG_CORE, "MainThreadExec context is nullptr");
    } else {
        auto tsfnContext = static_cast<TsfnContext*>(context);
        if (napi_release_threadsafe_function(tsfnContext->tsfn, napi_tsfn_release) != napi_status::napi_ok) {
            HILOG_ERROR(LOG_CORE, "MainThreadExec release_threadsafe_function failed");
        }
    }
    delete pData;
}

static napi_value RegisterArkUIObjectLifeCycleCallback(napi_env env, napi_callback_info info)
{
    if (!GetCallbackRef(env, info, &g_callbackRef)) {
        return nullptr;
    }

    RefPtr<Kit::UIContext> uiContext = Kit::UIContext::Current();
    if (uiContext == nullptr) {
        return nullptr;
    }
    uiContext->RegisterArkUIObjectLifecycleCallback([env](void* obj) {
        napi_handle_scope scope = nullptr;
        napi_open_handle_scope(env, &scope);
        ArkUIRuntimeCallInfo* arkUIRuntimeCallInfo = reinterpret_cast<ArkUIRuntimeCallInfo*>(obj);
        panda::Local<panda::JSValueRef> firstArg = arkUIRuntimeCallInfo->GetCallArgRef(0);
        napi_value param = reinterpret_cast<napi_value>(*firstArg);
        napi_value global = nullptr;
        napi_get_global(env, &global);
        napi_value callback = nullptr;
        napi_get_reference_value(env, g_callbackRef, &callback);
        napi_value argv[1] = {param};
        napi_call_function(env, global, callback, 1, argv, nullptr);
        napi_close_handle_scope(env, scope);
    });

    return CreateUndefined(env);
}

static napi_value UnregisterArkUIObjectLifeCycleCallback(napi_env env, napi_callback_info info)
{
    RefPtr<Kit::UIContext> uiContext = Kit::UIContext::Current();
    if (uiContext == nullptr) {
        return nullptr;
    }
    uiContext->UnregisterArkUIObjectLifecycleCallback();
    napi_delete_reference(env, g_callbackRef);
    g_callbackRef = nullptr;
    return CreateUndefined(env);
}

static napi_value RegisterWindowLifeCycleCallback(napi_env env, napi_callback_info info)
{
    napi_ref ref = nullptr;
    if (!GetCallbackRef(env, info, &ref)) {
        return nullptr;
    }
    g_listener->SetEnvAndCallback(env, ref);
    WindowManager::GetInstance().RegisterWindowLifeCycleCallback(g_listener);
    return CreateUndefined(env);
}

static napi_value UnregisterWindowLifeCycleCallback(napi_env env, napi_callback_info info)
{
    WindowManager::GetInstance().UnregisterWindowLifeCycleCallback(g_listener);
    g_listener->Reset();
    return CreateUndefined(env);
}

static napi_value HandleDumpTask(napi_env env, napi_callback_info info)
{
    napi_ref ref = nullptr;
    if (!GetCallbackRef(env, info, &ref)) {
        return nullptr;
    }
    g_handler->SetEnv(env);
    g_handler->SetDumpFuncRef(ref);
    g_handler->SetJsLeakWatcherStatus(true);
    return CreateUndefined(env);
}

static napi_value SetDumpDelay(napi_env env, napi_callback_info info)
{
    napi_handle_scope scope = nullptr;
    napi_open_handle_scope(env, &scope);
    size_t argc = ONE_VALUE_LIMIT;
    napi_value argv[ONE_VALUE_LIMIT] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc != ONE_VALUE_LIMIT) {
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    uint32_t delay = 0;
    if (napi_get_value_uint32(env, argv[0], &delay) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "SetDumpDelay napi_get_value_uint32 failed");
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    g_handler->SetDumpDelayTime(delay);
    napi_close_handle_scope(env, scope);
    return CreateUndefined(env);
}

static napi_value SetGcDelay(napi_env env, napi_callback_info info)
{
    napi_handle_scope scope = nullptr;
    napi_open_handle_scope(env, &scope);
    size_t argc = ONE_VALUE_LIMIT;
    napi_value argv[ONE_VALUE_LIMIT] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc != ONE_VALUE_LIMIT) {
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    uint32_t delay = 0;
    if (napi_get_value_uint32(env, argv[0], &delay) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "SetGcDelay napi_get_value_uint32 failed");
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    g_handler->SetGcDelayTime(delay);
    napi_close_handle_scope(env, scope);
    return CreateUndefined(env);
}

static napi_value HandleGCTask(napi_env env, napi_callback_info info)
{
    napi_ref ref = nullptr;
    if (!GetCallbackRef(env, info, &ref)) {
        return nullptr;
    }
    g_handler->SetEnv(env);
    g_handler->SetGcFuncRef(ref);
    g_handler->SendEvent(GC_EVENT_ID, g_handler->GetGcDelayTime(), LeakWatcherEventHandler::Priority::IDLE);
    return CreateUndefined(env);
}

static napi_value HandleShutdownTask(napi_env env, napi_callback_info info)
{
    napi_ref ref = nullptr;
    if (!GetCallbackRef(env, info, &ref)) {
        return nullptr;
    }
    g_handler->SetEnv(env);
    g_handler->SetShutdownFuncRef(ref);
    return CreateUndefined(env);
}

static napi_value RemoveTask(napi_env env, napi_callback_info info)
{
    g_handler->SetJsLeakWatcherStatus(false);
    return CreateUndefined(env);
}

static void DumpRawHeapImpl(TsfnContext* tsfnContext, napi_callback_info info, std::string& filePath)
{
    panda::ecmascript::DumpSnapShotOption dumpOption;
    dumpOption.isVmMode = true;
    dumpOption.isJSLeakWatcher = true;
    dumpOption.isSync = false;
    dumpOption.dumpFormat = panda::ecmascript::DumpFormat::BINARY;
    auto cbinfo = reinterpret_cast<panda::JsiRuntimeCallInfo*>(info);
    panda::DFXJSNApi::DumpHeapSnapshot(cbinfo->GetVM(), filePath, dumpOption,
                                       [tsfnContext, filePath](uint8_t retcode) {
        HILOG_INFO(LOG_CORE, "DumpRawHeapImpl callback get retcode: %{public}d", retcode);
        uint8_t* pData = new uint8_t(retcode);
        if (tsfnContext == nullptr || tsfnContext->tsfn == nullptr) {
            HILOG_INFO(LOG_CORE, "DumpRawHeapImpl callback tsfnContext invalid!");
            delete pData;
        } else {
            napi_call_threadsafe_function(tsfnContext->tsfn, pData, napi_tsfn_nonblocking);
        }
        AppendMetaData(filePath);
    });
}

static napi_value DumpRawHeap(napi_env env, napi_callback_info info)
{
    HILOG_INFO(LOG_CORE, "DumpRawHeap begin!");
    napi_handle_scope scope = nullptr;
    napi_open_handle_scope(env, &scope);
    size_t argc = TWO_LIMIT;
    napi_value argv[TWO_LIMIT] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc != TWO_LIMIT) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap argc invalid");
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    std::string filePath = "";
    if (!GetNapiStringValue(env, argv[0], filePath)) {
        HILOG_ERROR(LOG_CORE, "DumpRawHeap GetNapiStringValue failed");
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    if (!CreateFile(filePath)) {
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    TsfnContext* tsfnContext = new TsfnContext();
    if (napi_create_threadsafe_function(env, argv[1], nullptr, CreateUndefined(env), 0, 1, nullptr,
                                        [](napi_env, void* finalizeData, void* context) {
                                            delete static_cast<TsfnContext*>(finalizeData);
                                        }, tsfnContext, MainThreadExec, &tsfnContext->tsfn)
        != napi_status::napi_ok) {
            HILOG_ERROR(LOG_CORE, "DumpRawHeap create_threadsafe func failed");
            delete tsfnContext;
            napi_close_handle_scope(env, scope);
            return CreateUndefined(env);
    }
    DumpRawHeapImpl(tsfnContext, info, filePath);
    napi_close_handle_scope(env, scope);
    return CreateUndefined(env);
}

static napi_value DumpRawHeapSync(napi_env env, napi_callback_info info)
{
    napi_handle_scope scope = nullptr;
    napi_open_handle_scope(env, &scope);
    size_t argc = ONE_VALUE_LIMIT;
    napi_value argv[ONE_VALUE_LIMIT] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc != ONE_VALUE_LIMIT) {
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    std::string filePath = "";
    if (!GetNapiStringValue(env, argv[0], filePath)) {
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    if (!CreateFile(filePath)) {
        napi_close_handle_scope(env, scope);
        return nullptr;
    }
    NativeEngine *engine = reinterpret_cast<NativeEngine*>(env);
    engine->DumpHeapSnapshot(filePath, true, DumpFormat::BINARY, false, true, true);
    AppendMetaData(filePath);
    napi_close_handle_scope(env, scope);
    return CreateUndefined(env);
}

napi_value DeclareJsLeakWatcherInterface(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("registerArkUIObjectLifeCycleCallback", RegisterArkUIObjectLifeCycleCallback),
        DECLARE_NAPI_FUNCTION("unregisterArkUIObjectLifeCycleCallback", UnregisterArkUIObjectLifeCycleCallback),
        DECLARE_NAPI_FUNCTION("registerWindowLifeCycleCallback", RegisterWindowLifeCycleCallback),
        DECLARE_NAPI_FUNCTION("unregisterWindowLifeCycleCallback", UnregisterWindowLifeCycleCallback),
        DECLARE_NAPI_FUNCTION("removeTask", RemoveTask),
        DECLARE_NAPI_FUNCTION("handleDumpTask", HandleDumpTask),
        DECLARE_NAPI_FUNCTION("handleGCTask", HandleGCTask),
        DECLARE_NAPI_FUNCTION("handleShutdownTask", HandleShutdownTask),
        DECLARE_NAPI_FUNCTION("dumpRawHeap", DumpRawHeap),
        DECLARE_NAPI_FUNCTION("dumpRawHeapSync", DumpRawHeapSync),
        DECLARE_NAPI_FUNCTION("setGcDelay", SetGcDelay),
        DECLARE_NAPI_FUNCTION("setDumpDelay", SetDumpDelay),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}

static napi_module _module = {
    .nm_version = 0,
    .nm_filename = nullptr,
    .nm_register_func = DeclareJsLeakWatcherInterface,
    .nm_modname = "jsLeakWatcherNative",
};
extern "C" __attribute__((constructor)) void NAPI_hiviewdfx_jsLeakWatcher_AutoRegister()
{
    napi_module_register(&_module);
}

//for test
std::shared_ptr<LeakWatcherEventHandler> GetTestHandler()
{
    return g_handler;
}

bool TestCreateFile(const std::string& filePath)
{
    return CreateFile(filePath);
}

uint64_t TestGetFileSize(const std::string& filePath)
{
    return GetFileSize(filePath);
}

bool TestAppendMetaData(const std::string& filePath)
{
    return AppendMetaData(filePath);
}
