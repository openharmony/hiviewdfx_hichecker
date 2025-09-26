/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "ani_hichecker.h"

#include <map>

#include "hichecker.h"
#include "hilog/log_c.h"
#include "hilog/log_cpp.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D0B
#undef LOG_TAG
#define LOG_TAG "HiChecker_ANI"
namespace OHOS {
namespace HiviewDFX {
namespace {
const char NAMESPACE_NAME_HICHECKER[] = "@ohos.hichecker.hichecker";
const char CLASS_NAME_BUSINESSERROR[] = "@ohos.base.BusinessError";
const char CLASS_NAME_BIGINT[] = "escompat.BigInt";
const char FUNC_NAME_GETLONG[] = "getLong";
const char BIGINT_CTOR_MANGLING[] = "C{std.core.String}:";
constexpr uint64_t GET_RULE_PARAM_FAIL = 0;
constexpr int ERR_PARAM = 401;
}

bool MatchValueType(ani_env *env, ani_object value, const char* targetType)
{
    ani_class cls {};
    ani_boolean isBigInt = ANI_FALSE;
    if (ANI_OK != env->FindClass(targetType, &cls)) {
        return false;
    }
    if (ANI_OK != env->Object_InstanceOf(value, cls, &isBigInt)) {
        return false;
    }
    return isBigInt;
}

uint64_t GetRuleParam(ani_env *env, ani_object rule)
{
    if (!MatchValueType(env, rule, CLASS_NAME_BIGINT)) {
        HILOG_ERROR(LOG_CORE, "Type error, should be bigint type!");
        return GET_RULE_PARAM_FAIL;
    }
    ani_long value = 0;
    if (ANI_OK != env->Object_CallMethodByName_Long(rule, FUNC_NAME_GETLONG, ":l", &value)) {
        HILOG_ERROR(LOG_CORE, "Get bigint value failed.");
        return GET_RULE_PARAM_FAIL;
    }
    if (value == static_cast<ani_long>(GET_RULE_PARAM_FAIL)) {
        HILOG_ERROR(LOG_CORE, "Invalid input, please check!");
    }
    return static_cast<uint64_t>(value);
}

ani_object BuildBigintResult(ani_env *env, uint64_t rule)
{
    ani_object result {};
    ani_class cls {};
    if (ANI_OK != env->FindClass(CLASS_NAME_BIGINT, &cls)) {
        HILOG_ERROR(LOG_CORE, "Find class %{public}s failed.", CLASS_NAME_BIGINT);
        return result;
    }
    ani_method bigIntCtor;
    if (ANI_OK != env->Class_FindMethod(cls, "<ctor>", BIGINT_CTOR_MANGLING, &bigIntCtor)) {
        HILOG_ERROR(LOG_CORE, "Find method BigInt <ctor> failed.");
        return result;
    }
    std::string bigUintStr = std::to_string(rule);
    ani_string strBigUintValue;
    if (ANI_OK != env->String_NewUTF8(bigUintStr.c_str(), bigUintStr.size(), &strBigUintValue)) {
        HILOG_ERROR(LOG_CORE, "New string object failed.");
        return result;
    }
    if (ANI_OK != env->Object_New(cls, bigIntCtor, &result, strBigUintValue)) {
        HILOG_ERROR(LOG_CORE, "New %{public}s object failed.", CLASS_NAME_BIGINT);
        return result;
    }
    return result;
}

void ThrowError(ani_env *env, int32_t errCode)
{
    std::map<int32_t, std::string> errMap = {
        {ERR_PARAM, "Invalid input parameter! only one bigint type parameter is needed"},
    };
    if (errMap.find(errCode) == errMap.end()) {
        return;
    }
    ani_class cls{};
    if (ANI_OK != env->FindClass(CLASS_NAME_BUSINESSERROR, &cls)) {;
        return;
    }
    ani_method ctor {};
    if (ANI_OK != env->Class_FindMethod(cls, "<ctor>", ":", &ctor)) {
        return;
    }
    ani_object error {};
    if (ANI_OK != env->Object_New(cls, ctor, &error)) {
        return;
    }
    if (ANI_OK != env->Object_SetPropertyByName_Double(error, "code", static_cast<ani_double>(errCode))) {
        return;
    }
    std::string message = errMap[errCode];
    ani_string messageRef {};
    if (ANI_OK != env->String_NewUTF8(message.c_str(), message.size(), &messageRef)) {
        return;
    }
    if (ANI_OK != env->Object_SetPropertyByName_Ref(error, "message", static_cast<ani_ref>(messageRef))) {
        return;
    }
    if (ANI_OK != env->ThrowError(static_cast<ani_error>(error))) {
        HILOG_ERROR(LOG_CORE, "Throw ani error failed");
    }
    return;
}


static void AddRule(ani_env *env, ani_object rule)
{
    uint64_t ruleVal = GetRuleParam(env, rule);
    if (ruleVal != GET_RULE_PARAM_FAIL) {
        HiChecker::AddRule(ruleVal);
    }
    return;
}

void RemoveRule(ani_env *env, ani_object rule)
{
    uint64_t ruleVal = GetRuleParam(env, rule);
    if (ruleVal != GET_RULE_PARAM_FAIL) {
        HiChecker::RemoveRule(ruleVal);
    }
    return;
}

static ani_object GetRule(ani_env *env)
{
    uint64_t ruleVal = HiChecker::GetRule();
    return BuildBigintResult(env, ruleVal);
}

ani_boolean Contains(ani_env *env, ani_object rule)
{
    uint64_t ruleVal = GetRuleParam(env, rule);
    return HiChecker::Contains(ruleVal);
}

void AddCheckRule(ani_env *env, ani_object rule)
{
    uint64_t ruleVal = GetRuleParam(env, rule);
    if (ruleVal != GET_RULE_PARAM_FAIL) {
        HiChecker::AddRule(ruleVal);
    } else {
        ThrowError(env, ERR_PARAM);
    }
    return;
}

void RemoveCheckRule(ani_env *env, ani_object rule)
{
    uint64_t ruleVal = GetRuleParam(env, rule);
    if (ruleVal != GET_RULE_PARAM_FAIL) {
        HiChecker::RemoveRule(ruleVal);
    } else {
        ThrowError(env, ERR_PARAM);
    }
    return;
}

ani_boolean ContainsCheckRule(ani_env *env, ani_object rule)
{
    uint64_t ruleVal = GetRuleParam(env, rule);
    if (ruleVal == GET_RULE_PARAM_FAIL) {
        ThrowError(env, ERR_PARAM);
    }
    return HiChecker::Contains(ruleVal);
}
}
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    ani_env *env = nullptr;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        HILOG_ERROR(LOG_CORE, "Unsupported ANI_VERSION_1");
        return ANI_ERROR;
    }

    ani_namespace ns {};
    if (ANI_OK != env->FindNamespace(OHOS::HiviewDFX::NAMESPACE_NAME_HICHECKER, &ns)) {
        return ANI_ERROR;
    }

    std::array methods = {
        ani_native_function{"addRule", nullptr,
                            reinterpret_cast<void *>(OHOS::HiviewDFX::AddRule)},
        ani_native_function{"removeRule", nullptr,
                            reinterpret_cast<void *>(OHOS::HiviewDFX::RemoveRule)},
        ani_native_function{"getRule", nullptr,
                            reinterpret_cast<void *>(OHOS::HiviewDFX::GetRule)},
        ani_native_function{"contains", nullptr,
                            reinterpret_cast<void *>(OHOS::HiviewDFX::Contains)},
        ani_native_function{"addCheckRule", nullptr,
                            reinterpret_cast<void *>(OHOS::HiviewDFX::AddCheckRule)},
        ani_native_function{"removeCheckRule", nullptr,
                            reinterpret_cast<void *>(OHOS::HiviewDFX::RemoveCheckRule)},
        ani_native_function{"containsCheckRule", nullptr,
                            reinterpret_cast<void *>(OHOS::HiviewDFX::ContainsCheckRule)},
    };

    if (ANI_OK != env->Namespace_BindNativeFunctions(ns, methods.data(), methods.size())) {
        return ANI_ERROR;
    };

    *result = ANI_VERSION_1;
    return ANI_OK;
}
