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

#include <cstring>

#include "hilog/log.h"
#include "js_leak_watcher_ts.h"
#include "hitrace_meter.h"
#include "securec.h"
#include "sys_param.h"
#include "parameters.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD003D00

#undef LOG_TAG
#define LOG_TAG "JSLEAK_WATCHER_TS"

#define JSLEAK_WATCHER_NAME_LEN 256

static bool g_enableStatus = false;

bool IsDebuggableHap()
{
    const char* debuggableEnv = getenv("HAP_DEBUGGABLE");
    return debuggableEnv != nullptr && strcmp(debuggableEnv, "true") == 0;
}

bool IsRootVersion()
{
    return OHOS::system::GetBoolParameter("const.debuggable", false);
}

bool CheckJsLeakWatcherParam(const char* bundleName)
{
    if (!bundleName) {
        return false;
    }
    char paraName[JSLEAK_WATCHER_NAME_LEN] = "hiviewdfx.hichecker.jsleakwatcher.leak.check";
    const std::string disable = "disable." + std::string(bundleName);
    CachedHandle appEnableHandle = CachedParameterCreate(paraName, disable.c_str());
    if (appEnableHandle == nullptr) {
        return false;
    }
    const char *paramValue = CachedParameterGet(appEnableHandle);
    if (paramValue != nullptr) {
        const std::string enable = "enable." + std::string(bundleName);
        if (strcmp(paramValue, enable.c_str()) == 0) {
            CachedParameterDestroy(appEnableHandle);
            return true;
        }
    }
    CachedParameterDestroy(appEnableHandle);
    return false;
}

void SetjsLeakWatcherEnableStatus(bool checkStatus)
{
    g_enableStatus = checkStatus;
}

bool GetjsLeakWatcherEnableStatus()
{
    return g_enableStatus;
}

inline napi_value CreateJsUndefined(napi_env env)
{
    napi_value result = nullptr;
    if (napi_get_undefined(env, &result) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "CreateJsUndefined napi_get_undefined failed");
    }
    return result;
}

napi_value InternalCallback(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1];
    if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "InternalCallback napi_get_cb_info failed");
    }
    return nullptr;
}

void CreateCallbackObject(napi_env env, napi_value* js_callback)
{
    napi_status status = napi_create_function(env,
        "myInternalCallback",
        NAPI_AUTO_LENGTH,
        InternalCallback,
        nullptr,
        js_callback);

    if (status != napi_ok) {
        HILOG_ERROR(LOG_CORE, "Failed to create callback function!");
    }
}

void JSLeakWatcherEarlyInit(napi_env env, std::string bundleName)
{
    HITRACE_METER_NAME(HITRACE_TAG_APP, __PRETTY_FUNCTION__);
    if (!IsRootVersion() && !IsDebuggableHap()) {
        HILOG_ERROR(LOG_CORE, "user mode release hap is not allow");
        return;
    }
    bool ret = CheckJsLeakWatcherParam(bundleName.c_str());
    SetjsLeakWatcherEnableStatus(ret);
    if (!ret) {
        return;
    }

    napi_handle_scope scope;
    if (napi_open_handle_scope(env, &scope) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSLeakWatcherEarlyInit napi_open_handle_scope failed");
        return;
    }
    HILOG_INFO(LOG_CORE, "JSLeakWatcherEarlyInit %{public}s", bundleName.c_str());

    napi_value nvJsLeakWatcher = nullptr;
    if (napi_load_module(env, "@ohos.hiviewdfx.jsLeakWatcher", &nvJsLeakWatcher) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSLeakWatcherEarlyInit napi_load_module failed");
        napi_close_handle_scope(env, scope);
        return;
    }

    napi_value jsFuncEnableLeakWatcher = nullptr;
    if (napi_get_named_property(env, nvJsLeakWatcher, "enableLeakWatcher", &jsFuncEnableLeakWatcher) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSLeakWatcherEarlyInit napi_get_named_property failed");
        napi_close_handle_scope(env, scope);
        return;
    }

    napi_value args[3];
    if (napi_get_boolean(env, true, &args[0]) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSLeakWatcherEarlyInit napi_get_boolean failed");
        napi_close_handle_scope(env, scope);
        return;
    }

    napi_value configsObj;
    if (napi_create_object(env, &configsObj) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSLeakWatcherEarlyInit napi_create_object failed");
        napi_close_handle_scope(env, scope);
        return;
    }

    napi_value value;
    if (napi_create_int32(env, -1, &value) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSLeakWatcherEarlyInit napi_create_int32 failed");
        napi_close_handle_scope(env, scope);
        return;
    }
    if (napi_set_named_property(env, configsObj, "monitorObjectTypes", value) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSLeakWatcherEarlyInit napi_set_named_property failed");
        napi_close_handle_scope(env, scope);
        return;
    }
    args[1] = configsObj;
    CreateCallbackObject(env, &args[2]);

    napi_value result;
    if (napi_call_function(env, nvJsLeakWatcher, jsFuncEnableLeakWatcher, 3, args, &result) != napi_ok) {
        HILOG_ERROR(LOG_CORE, "JSLeakWatcherEarlyInit napi_call_function failed");
    }
    napi_close_handle_scope(env, scope);
}

//for test
#ifdef JSLEAKWATHCER_UNITTEST
bool TestCheckJsLeakWatcherParam(const char* bundleName)
{
    return CheckJsLeakWatcherParam(bundleName);
}
#endif