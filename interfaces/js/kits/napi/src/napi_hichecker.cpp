/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "napi_hichecker.h"

#include <map>

#include "hichecker.h"
#include "hilog/log_c.h"
#include "hilog/log_cpp.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr HiLogLabel LABEL = { LOG_CORE, 0xD002D0B, "HiChecker_NAPI" };
constexpr int ONE_VALUE_LIMIT = 1;
constexpr int ARRAY_INDEX_FIRST = 0;
constexpr uint64_t GET_RULE_PARAM_FAIL = 0;
constexpr int ERR_PARAM = 401;
}

napi_value AddRule(napi_env env, napi_callback_info info)
{
    uint64_t rule = GetRuleParam(env, info);
    if (rule != GET_RULE_PARAM_FAIL) {
        HiChecker::AddRule(rule);
    }
    return CreateUndefined(env);
}

napi_value RemoveRule(napi_env env, napi_callback_info info)
{
    uint64_t rule = GetRuleParam(env, info);
    if (rule != GET_RULE_PARAM_FAIL) {
        HiChecker::RemoveRule(rule);
    }
    return CreateUndefined(env);
}

napi_value GetRule(napi_env env, napi_callback_info info)
{
    uint64_t rule = HiChecker::GetRule();
    return ToUInt64Value(env, rule);
}

napi_value Contains(napi_env env, napi_callback_info info)
{
    uint64_t rule = GetRuleParam(env, info);
    napi_value result = nullptr;
    napi_get_boolean(env, HiChecker::Contains(rule), &result);
    return result;
}

napi_value AddCheckRule(napi_env env, napi_callback_info info)
{
    uint64_t rule = GetRuleParam(env, info);
    if (rule != GET_RULE_PARAM_FAIL) {
        HiChecker::AddRule(rule);
    } else {
        ThrowError(env, ERR_PARAM);
    }
    return CreateUndefined(env);
}

napi_value RemoveCheckRule(napi_env env, napi_callback_info info)
{
    uint64_t rule = GetRuleParam(env, info);
    if (rule != GET_RULE_PARAM_FAIL) {
        HiChecker::RemoveRule(rule);
    } else {
        ThrowError(env, ERR_PARAM);
    }
    return CreateUndefined(env);
}

napi_value ContainsCheckRule(napi_env env, napi_callback_info info)
{
    napi_value result = nullptr;
    uint64_t rule = GetRuleParam(env, info);
    if (rule == GET_RULE_PARAM_FAIL) {
        ThrowError(env, ERR_PARAM);
    }
    napi_get_boolean(env, HiChecker::Contains(rule), &result);
    return result;
}

napi_value DeclareHiCheckerInterface(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("addRule", AddRule),
        DECLARE_NAPI_FUNCTION("removeRule", RemoveRule),
        DECLARE_NAPI_FUNCTION("getRule", GetRule),
        DECLARE_NAPI_FUNCTION("contains", Contains),
        DECLARE_NAPI_FUNCTION("addCheckRule", AddCheckRule),
        DECLARE_NAPI_FUNCTION("removeCheckRule", RemoveCheckRule),
        DECLARE_NAPI_FUNCTION("containsCheckRule", ContainsCheckRule),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    DeclareHiCheckerRuleEnum(env, exports);
    return exports;
}

napi_value DeclareHiCheckerRuleEnum(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_STATIC_PROPERTY("RULE_CAUTION_PRINT_LOG", ToUInt64Value(env, Rule::RULE_CAUTION_PRINT_LOG)),
        DECLARE_NAPI_STATIC_PROPERTY("RULE_CAUTION_TRIGGER_CRASH",
            ToUInt64Value(env, Rule::RULE_CAUTION_TRIGGER_CRASH)),
        DECLARE_NAPI_STATIC_PROPERTY("RULE_THREAD_CHECK_SLOW_PROCESS",
            ToUInt64Value(env, Rule::RULE_THREAD_CHECK_SLOW_PROCESS)),
        DECLARE_NAPI_STATIC_PROPERTY("RULE_CHECK_SLOW_EVENT", ToUInt64Value(env, Rule::RULE_CHECK_SLOW_EVENT)),
        DECLARE_NAPI_STATIC_PROPERTY("RULE_CHECK_ABILITY_CONNECTION_LEAK",
            ToUInt64Value(env, Rule::RULE_CHECK_ABILITY_CONNECTION_LEAK)),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}

napi_value ToUInt64Value(napi_env env, uint64_t value)
{
    napi_value staticValue = nullptr;
    napi_create_bigint_uint64(env, value, &staticValue);
    return staticValue;
}

napi_value CreateUndefined(napi_env env)
{
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

void ThrowError(napi_env env, int errCode)
{
    std::map<int, std::string> errMap = {
        { ERR_PARAM, "Invalid input parameter!" },
    };
    if (errMap.find(errCode) != errMap.end()) {
        napi_throw_error(env, std::to_string(errCode).c_str(), errMap[errCode].c_str());
    }
    return;
}

uint64_t GetRuleParam(napi_env env, napi_callback_info info)
{
    size_t argc = ONE_VALUE_LIMIT;
    napi_value argv[ONE_VALUE_LIMIT] = { nullptr };
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);
    if (argc != ONE_VALUE_LIMIT) {
        HiLog::Error(LABEL, "invalid number=%{public}d of params.", ONE_VALUE_LIMIT);
        return GET_RULE_PARAM_FAIL;
    }
    if (!MatchValueType(env, argv[ARRAY_INDEX_FIRST], napi_bigint)) {
        HiLog::Error(LABEL, "Type error, should be bigint type!");
        return GET_RULE_PARAM_FAIL;
    }
    uint64_t rule = GET_RULE_PARAM_FAIL;
    bool lossless = true;
    napi_get_value_bigint_uint64(env, argv[ARRAY_INDEX_FIRST], &rule, &lossless);
    if (!lossless) {
        HiLog::Error(LABEL, "Type error, bigint should be 64!");
        return GET_RULE_PARAM_FAIL;
    }
    if (rule == GET_RULE_PARAM_FAIL) {
        HiLog::Error(LABEL, "invalid input, please check!");
    }
    return rule;
}

bool MatchValueType(napi_env env, napi_value value, napi_valuetype targetType)
{
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, value, &valueType);
    return valueType == targetType;
}

static napi_module g_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = DeclareHiCheckerInterface,
    .nm_modname = "hichecker",
    .nm_priv = ((void *)0),
    .reserved = {0}
};

extern "C" __attribute__((constructor)) void HiCheckerRegisterModule(void)
{
    napi_module_register(&g_module);
}
} // HiviewDFX
} // OHOS