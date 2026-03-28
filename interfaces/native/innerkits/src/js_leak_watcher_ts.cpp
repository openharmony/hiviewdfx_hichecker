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

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD003D00

#undef LOG_TAG
#define LOG_TAG "JSLEAK_WATCHER_TS"

#define JSLEAK_WATCHER_NAME_LEN 256

bool CheckJsLeakWatcherParam(const char* bundleName)
{
    if (!bundleName) {
        return false;
    }
    char paraName[JSLEAK_WATCHER_NAME_LEN] = "hiviewdfx.hichecker.jsleakwatcher.";
    errno_t err = 0;
    err = strcat_s(paraName, sizeof(paraName), bundleName);
    if (err != EOK) {
        HILOG_INFO(LOG_CORE, "checker jsLeakwatcherparam strcat_s query name failed.");
        return false;
    }
    CachedHandle appEnableHandle = CachedParameterCreate(paraName, "false");
    if (appEnableHandle == nullptr) {
        return false;
    }
    const char *paramValue = CachedParameterGet(appEnableHandle);
    CachedParameterDestroy(appEnableHandle);
    if (paramValue != nullptr) {
        if (strcmp(paramValue, "true") == 0) {
            return true;
        }
    }
    return false;
}

inline napi_value CreateJsUndefined(napi_env env)
{
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value InternalCallback(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1];
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    
    return nullptr;
}

void CreateCallbackObject(napi_env env, napi_value* js_callback) {
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
    if (!CheckJsLeakWatcherParam(bundleName.c_str())) {
        return;
    }

    napi_handle_scope scope;
    napi_open_handle_scope(env, &scope);
    HILOG_INFO(LOG_CORE, "JSLeakWatcherEarlyInit %{public}s", bundleName.c_str());

    napi_value nvJsLeakWatcher = nullptr;

    napi_load_module(env, "@ohos.hiviewdfx.jsLeakWatcher", &nvJsLeakWatcher);

    napi_value jsFuncEnableLeakWatcher;
    napi_get_named_property(env, nvJsLeakWatcher, "enableLeakWatcher", &jsFuncEnableLeakWatcher);

    napi_value args[3];
    napi_get_boolean(env, true, &args[0]);
    
    napi_value configsObj;
    napi_create_object(env, &configsObj);

    napi_value value;
    napi_create_int32(env, -1, &value);
    napi_set_named_property(env, configsObj, "monitorObjectTypes", value);
    args[1] = configsObj;
    CreateCallbackObject(env, &args[2]);

    napi_value result;
    napi_call_function(env, nvJsLeakWatcher, jsFuncEnableLeakWatcher, 3, args, &result);
    napi_close_handle_scope(env, scope);
}