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

#include <chrono>
#include <thread>

#include "event_handler.h"
#include "event_runner.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "native_engine/native_engine.h"
#include "ui/base/referenced.h"
#include "ui/view/ui_context.h"

using namespace OHOS::Ace;
using namespace OHOS::AppExecFwk;
using ArkUIRuntimeCallInfo = panda::JsiRuntimeCallInfo;

class LeakWatcherEventHandler : public EventHandler {
public:
    explicit LeakWatcherEventHandler(const std::shared_ptr<EventRunner> &runner) : EventHandler(runner) {}
};

static napi_value RegisterArkUIObjectLifeCycleCallback(napi_env env, napi_callback_info info);
static napi_value HandleIdleTask(napi_env env, napi_callback_info info);
static napi_value RemoveTask(napi_env env, napi_callback_info info);

napi_ref g_callbackRef = nullptr;

auto g_runner = EventRunner::Current();
auto g_handler = std::make_shared<LeakWatcherEventHandler>(g_runner);
constexpr int ONE_VALUE_LIMIT = 1;
constexpr int TWO_VALUE_LIMIT = 2;

static napi_value RegisterArkUIObjectLifeCycleCallback(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    napi_create_reference(env, argv[0], 1, &g_callbackRef);
    if (argc != ONE_VALUE_LIMIT) {
        return nullptr;
    }

    RefPtr<Kit::UIContext> uiContext = Kit::UIContext::Current();
    uiContext->RegisterArkUIObjectLifecycleCallback([env](void* obj) {
        ArkUIRuntimeCallInfo* arkUIRuntimeCallInfo = reinterpret_cast<ArkUIRuntimeCallInfo*>(obj);
        panda::Local<panda::JSValueRef> firstArg = arkUIRuntimeCallInfo->GetCallArgRef(0);
        napi_value param = reinterpret_cast<napi_value>(*firstArg);
        napi_value global;
        napi_get_global(env, &global);
        napi_value callback = nullptr;
        napi_get_reference_value(env, g_callbackRef, &callback);
        napi_value argv[1] = {nullptr};
        napi_call_function(env, global, callback, 1, argv, &param);
    });

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

static napi_value HandleIdleTask(napi_env env, napi_callback_info info)
{
    size_t argc = TWO_VALUE_LIMIT;
    napi_value argv[TWO_VALUE_LIMIT] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc != TWO_VALUE_LIMIT) {
        return nullptr;
    }
    int64_t delayTime = 0;
    napi_get_value_int64(env, argv[0], &delayTime);
    if (delayTime < 0) {
        return nullptr;
    }

    napi_ref callbackRef = nullptr;
    napi_create_reference(env, argv[1], 1, &callbackRef);
    auto task = [env, callbackRef]() {
        napi_value global;
        napi_get_global(env, &global);
        napi_value callback = nullptr;
        napi_get_reference_value(env, callbackRef, &callback);
        napi_value argv[1] = {nullptr};
        napi_call_function(env, global, callback, 1, argv, nullptr);
        napi_delete_reference(env, callbackRef);
    };
    const std::string taskName = "LeakWatcherTask";
    g_handler->PostIdleTask(task, taskName, delayTime);

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

static napi_value RemoveTask(napi_env env, napi_callback_info info)
{
    g_handler->RemoveTaskWithRet("LeakWatcherTask");
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value DeclareJsLeakWatcherInterface(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("registerArkUIObjectLifeCycleCallback", RegisterArkUIObjectLifeCycleCallback),
        DECLARE_NAPI_FUNCTION("handleIdleTask", HandleIdleTask),
        DECLARE_NAPI_FUNCTION("removeTask", RemoveTask),
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
