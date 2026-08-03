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

#ifndef ANI_JS_LEAK_WATCHER_H
#define ANI_JS_LEAK_WATCHER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "ani.h"
#include "event_handler.h"
#include "event_runner.h"
#include "ui/view/ui_context.h"

namespace OHOS {
namespace HiviewDFX {

// ANI 侧 ArkUI 生命周期回调数据结构,与 ace_engine 中
// common_module.cpp 的 ArkUIObjectLifecycleData 保持一致。
// 当 ArkUI 触发生命周期回调时,void* data 指向此结构体。
struct ArkUIObjectLifecycleData {
    ani_object weakRef;
    ani_string className;
    ani_string nodeType;
    ani_long nodePtr;
};

constexpr uint32_t DUMP_EVENT_ID = 0;
constexpr uint32_t GC_EVENT_ID = 1;
constexpr uint32_t DEFAULT_DUMP_DELAY = 3000;
constexpr uint32_t DEFAULT_GC_DELAY = 90000;
const uint64_t FDTAG = 0xD002D0B;

class LeakWatcherEventHandler : public OHOS::AppExecFwk::EventHandler {
public:
    explicit LeakWatcherEventHandler(const std::shared_ptr<OHOS::AppExecFwk::EventRunner> &runner)
        : EventHandler(runner) {}

    void ProcessEvent(const OHOS::AppExecFwk::InnerEvent::Pointer &event) override;
    void SetDumpDelayTime(uint32_t delay) { dumpDelayTime_ = delay; }
    void SetGcDelayTime(uint32_t delay) { gcDelayTime_ = delay; }
    uint32_t GetGcDelayTime() const { return gcDelayTime_; }
    void SetAniEnv(ani_env *env) { env_ = env; }
    void SetDumpFuncRef(ani_ref ref) { dumpFuncRef_ = ref; }
    void SetGcFuncRef(ani_ref ref) { gcFuncRef_ = ref; }
    void SetShutdownFuncRef(ani_ref ref) { shutdownFuncRef_ = ref; }
    void SetJsLeakWatcherStatus(bool isRunning);
    void Reset();

private:
    void ExecuteJsFunc(ani_ref callbackRef);
    ani_env *env_ = nullptr;
    ani_ref dumpFuncRef_ = nullptr;
    ani_ref gcFuncRef_ = nullptr;
    ani_ref shutdownFuncRef_ = nullptr;
    bool isRunning_ = false;
    uint32_t dumpDelayTime_ = DEFAULT_DUMP_DELAY;
    uint32_t gcDelayTime_ = DEFAULT_GC_DELAY;
};

std::shared_ptr<LeakWatcherEventHandler> GetLeakWatcherHandler();
bool CreateFile(const std::string &filePath);
uint64_t GetFileSize(const std::string &filePath);
bool AppendMetaData(const std::string &filePath);
ani_status ANI_ConstructorImpl(ani_vm *vm, uint32_t *result);
} // namespace HiviewDFX
} // namespace OHOS

#endif // ANI_JS_LEAK_WATCHER_H
