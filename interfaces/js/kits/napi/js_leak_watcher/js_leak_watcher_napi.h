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

#ifndef JS_LEAK_WATCHER_NAPI_H
#define JS_LEAK_WATCHER_NAPI_H
#include "event_handler.h"
#include "event_runner.h"
#include "native_engine/native_engine.h"
#include "ui/view/ui_context.h"
#include "window_manager.h"
#include "ecmascript/napi/include/dfx_jsnapi.h"
#include <string>
#include <cstdint>
#include <memory>

constexpr uint32_t DUMP_EVENT_ID = 0;
constexpr uint32_t GC_EVENT_ID = 1;
constexpr uint32_t ONE_VALUE_LIMIT = 1;
constexpr uint32_t TWO_LIMIT = 2;

class LeakWatcherEventHandler : public OHOS::AppExecFwk::EventHandler {
public:
    explicit LeakWatcherEventHandler(
        const std::shared_ptr<OHOS::AppExecFwk::EventRunner> &runner) : EventHandler(runner) {}

    void ProcessEvent(const OHOS::AppExecFwk::InnerEvent::Pointer &event) override
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
    
    void SetDumpDelayTime(uint32_t delay)
    {
        dumpDelayTime_ = delay;
    }

    void SetGcDelayTime(uint32_t delay)
    {
        gcDelayTime_ = delay;
    }
    uint32_t GetGcDelayTime() const
    {
        return gcDelayTime_;
    }
    uint32_t GetDumpDelayTime() const
    {
        return dumpDelayTime_;
    }
    bool GetJsLeakWatcherStatus() const
    {
        return isRunning_;
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
        if (!isRunning) {
            Reset();
        }
    }

private:
    void ExecuteJsFunc(napi_ref callbackRef)
    {
        napi_handle_scope scope = nullptr;
        napi_open_handle_scope(env_, &scope);
        napi_value global = nullptr;
        napi_get_global(env_, &global);
        napi_value callback = nullptr;
        napi_get_reference_value(env_, callbackRef, &callback);
        napi_value argv[1] = {nullptr};
        napi_call_function(env_, global, callback, 1, argv, nullptr);
        napi_close_handle_scope(env_, scope);
    }
    void Reset()
    {
        napi_delete_reference(env_, dumpFuncRef_);
        napi_delete_reference(env_, gcFuncRef_);
        napi_delete_reference(env_, shutdownFuncRef_);
        dumpFuncRef_ = nullptr;
        gcFuncRef_ = nullptr;
        shutdownFuncRef_ = nullptr;
    }
    napi_env env_ = nullptr;
    napi_ref dumpFuncRef_ = nullptr;
    napi_ref gcFuncRef_ = nullptr;
    napi_ref shutdownFuncRef_ = nullptr;
    bool isRunning_ = false;
    uint32_t dumpDelayTime_ = 30000; // 30s
    uint32_t gcDelayTime_ = 27000; // 27s
};

class WindowLifeCycleListener : public OHOS::Rosen::IWindowLifeCycleListener {
public:
    void OnWindowDestroyed(const OHOS::Rosen::WindowLifeCycleInfo& info, void* jsWindowNapiValue) override
    {
        napi_handle_scope scope = nullptr;
        napi_open_handle_scope(env_, &scope);
        napi_value global = nullptr;
        napi_get_global(env_, &global);
        napi_value callback = nullptr;
        napi_get_reference_value(env_, callbackRef_, &callback);
        napi_value param = reinterpret_cast<napi_value>(jsWindowNapiValue);
        napi_value argv[1] = {param};
        napi_call_function(env_, global, callback, 1, argv, nullptr);
        napi_close_handle_scope(env_, scope);
    }

    WindowLifeCycleListener() {}
    void SetEnvAndCallback(napi_env env, napi_ref callbackRef)
    {
        env_ = env;
        callbackRef_ = callbackRef;
    }
    void Reset()
    {
        napi_delete_reference(env_, callbackRef_);
        env_ = nullptr;
        callbackRef_ = nullptr;
    }
private:
    napi_env env_ = nullptr;
    napi_ref callbackRef_ = nullptr;
};

std::shared_ptr<LeakWatcherEventHandler> GetTestHandler();
bool TestCreateFile(const std::string& filePath);
uint64_t TestGetFileSize(const std::string& filePath);
bool TestAppendMetaData(const std::string& filePath);

#endif // JS_LEAK_WATCHER_NAPI_H
