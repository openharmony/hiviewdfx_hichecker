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

#include "event_handler.h"
#include "event_runner.h"
#include "native_engine/native_engine.h"
#include "ui/view/ui_context.h"
#include "window_manager.h"

using namespace OHOS::Ace;
using namespace OHOS::AppExecFwk;
using namespace OHOS::Rosen;
using ArkUIRuntimeCallInfo = panda::JsiRuntimeCallInfo;

constexpr uint32_t DUMP_EVENT_ID = 0;
constexpr uint32_t GC_EVENT_ID = 1;
constexpr uint32_t MAX_EXECUTE_COUNT = 10;
constexpr uint32_t DUMP_DELAY_TIME = 30000; // 30s
constexpr uint32_t GC_DELAY_TIME = 27000; // 27s
constexpr uint32_t ONE_VALUE_LIMIT = 1;


class LeakWatcherEventHandler : public EventHandler {
public:
    explicit LeakWatcherEventHandler(const std::shared_ptr<EventRunner> &runner) : EventHandler(runner) {}

    void ProcessEvent(const InnerEvent::Pointer &event) override
    {
        if (!isRunning_) {
            return;
        }
        auto eventId = event->GetInnerEventId();
        if (eventId == DUMP_EVENT_ID) {
            ExecuteJsFunc(dumpFuncRef_);
            dumpCount_++;
            if (dumpCount_ < MAX_EXECUTE_COUNT) {
                SendEvent(DUMP_EVENT_ID, DUMP_DELAY_TIME, Priority::IDLE);
            }
        } else if (eventId == GC_EVENT_ID) {
            ExecuteJsFunc(gcFuncRef_);
            gcCount_++;
            if (gcCount_ < MAX_EXECUTE_COUNT) {
                SendEvent(GC_EVENT_ID, GC_DELAY_TIME, Priority::IDLE);
            }
        }
        if (dumpCount_ >= MAX_EXECUTE_COUNT && gcCount_ >= MAX_EXECUTE_COUNT) {
            ExecuteJsFunc(shutdownFuncRef_);
            SetJsLeakWatcherStatus(false);
        }
    }

    void SetEnv(napi_env env)
    {
        env_ = env;
    }
    void SetDumpFuncRef(napi_ref ref)
    {
        dumpFuncRef_ = ref;
    }
    void SetGcFuncRef(napi_ref ref)
    {
        gcFuncRef_ = ref;
    }
    void SetShutdownFuncRef(napi_ref ref)
    {
        shutdownFuncRef_ = ref;
    }
    void SetJsLeakWatcherStatus(bool isRunning)
    {
        isRunning_ = isRunning;
        dumpCount_ = 0;
        gcCount_ = 0;
    }

private:
    void ExecuteJsFunc(napi_ref callbackRef)
    {
        napi_value global = nullptr;
        napi_get_global(env_, &global);
        napi_value callback = nullptr;
        napi_get_reference_value(env_, callbackRef, &callback);
        napi_value argv[1] = {nullptr};
        napi_call_function(env_, global, callback, 1, argv, nullptr);
    }
    napi_env env_ = nullptr;
    napi_ref dumpFuncRef_ = nullptr;
    napi_ref gcFuncRef_ = nullptr;
    napi_ref shutdownFuncRef_ = nullptr;
    uint32_t dumpCount_ = 0;
    uint32_t gcCount_ = 0;
    bool isRunning_ = false;
};

class WindowLifeCycleListener : public IWindowLifeCycleListener {
public:
    void OnWindowDestroyed(const WindowLifeCycleInfo& info, void* jsWindowNapiValue) override
    {
        napi_value global = nullptr;
        napi_get_global(env_, &global);
        napi_value callback = nullptr;
        napi_get_reference_value(env_, callbackRef_, &callback);
        napi_value param = reinterpret_cast<napi_value>(jsWindowNapiValue);
        napi_value argv[1] = {param};
        napi_call_function(env_, global, callback, 1, argv, nullptr);
    }

    WindowLifeCycleListener() {}
    void SetEnvAndCallback(napi_env env, napi_ref callbackRef)
    {
        env_ = env;
        callbackRef_ = callbackRef;
    }
    void Reset()
    {
        env_ = nullptr;
        callbackRef_ = nullptr;
    }
private:
    napi_env env_ = nullptr;
    napi_ref callbackRef_ = nullptr;
};

static napi_value RegisterArkUIObjectLifeCycleCallback(napi_env env, napi_callback_info info);
static napi_value UnregisterArkUIObjectLifeCycleCallback(napi_env env, napi_callback_info info);
static napi_value RegisterWindowLifeCycleCallback(napi_env env, napi_callback_info info);
static napi_value UnregisterWindowLifeCycleCallback(napi_env env, napi_callback_info info);
static napi_value RemoveTask(napi_env env, napi_callback_info info);
static napi_value HandleDumpTask(napi_env env, napi_callback_info info);
static napi_value HandleGCTask(napi_env env, napi_callback_info info);
static napi_value HandleShutdownTask(napi_env env, napi_callback_info info);

auto g_runner = EventRunner::Current();
auto g_handler = std::make_shared<LeakWatcherEventHandler>(g_runner);
auto g_listener = OHOS::sptr<WindowLifeCycleListener>::MakeSptr();

static napi_value CreateUndefined(napi_env env)
{
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

static bool GetCallbackRef(napi_env env, napi_callback_info info, napi_ref* ref)
{
    size_t argc = ONE_VALUE_LIMIT;
    napi_value argv[ONE_VALUE_LIMIT] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc != ONE_VALUE_LIMIT) {
        return false;
    }
    napi_create_reference(env, argv[0], 1, ref);
    return true;
}

static napi_value RegisterArkUIObjectLifeCycleCallback(napi_env env, napi_callback_info info)
{
    napi_ref ref = nullptr;
    if (!GetCallbackRef(env, info, &ref)) {
        return nullptr;
    }

    RefPtr<Kit::UIContext> uiContext = Kit::UIContext::Current();
    uiContext->RegisterArkUIObjectLifecycleCallback([env, ref](void* obj) {
        ArkUIRuntimeCallInfo* arkUIRuntimeCallInfo = reinterpret_cast<ArkUIRuntimeCallInfo*>(obj);
        panda::Local<panda::JSValueRef> firstArg = arkUIRuntimeCallInfo->GetCallArgRef(0);
        napi_value param = reinterpret_cast<napi_value>(*firstArg);
        napi_value global = nullptr;
        napi_get_global(env, &global);
        napi_value callback = nullptr;
        napi_get_reference_value(env, ref, &callback);
        napi_value argv[1] = {param};
        napi_call_function(env, global, callback, 1, argv, nullptr);
    });

    return CreateUndefined(env);
}

static napi_value UnregisterArkUIObjectLifeCycleCallback(napi_env env, napi_callback_info info)
{
    RefPtr<Kit::UIContext> uiContext = Kit::UIContext::Current();
    uiContext->UnregisterArkUIObjectLifecycleCallback();
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
    g_handler->SendEvent(DUMP_EVENT_ID, DUMP_DELAY_TIME, LeakWatcherEventHandler::Priority::IDLE);
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
    g_handler->SendEvent(GC_EVENT_ID, GC_DELAY_TIME, LeakWatcherEventHandler::Priority::IDLE);
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
