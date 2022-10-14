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

#ifndef NAPI_HICHECKER_H
#define NAPI_HICHECKER_H

#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace HiviewDFX {
static napi_value AddRule(napi_env env, napi_callback_info info);
static napi_value RemoveRule(napi_env env, napi_callback_info info);
static napi_value GetRule(napi_env env, napi_callback_info info);
static napi_value Contains(napi_env env, napi_callback_info info);

static napi_value DeclareHiCheckerInterface(napi_env env, napi_value exports);
static napi_value DeclareHiCheckerRuleEnum(napi_env env, napi_value exports);
static napi_value CreateUndefined(napi_env env);
static napi_value ToUInt64Value(napi_env env, uint64_t value);
static bool MatchValueType(napi_env env, napi_value value, napi_valuetype targetType);
static uint64_t GetRuleParam(napi_env env, napi_callback_info info);
static void ThrowError(napi_env env, int errCode);
} // HiviewDFX
} // OHOS
#endif // NAPI_HICHECKER_H