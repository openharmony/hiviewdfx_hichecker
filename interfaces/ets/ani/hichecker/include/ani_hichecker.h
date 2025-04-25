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

#ifndef ANI_HICHECKER_H
#define ANI_HICHECKER_H

#include <ani.h>
namespace OHOS {
namespace HiviewDFX {
static void AddRule(ani_env *env, ani_object rule);
static void RemoveRule(ani_env *env, ani_object rule);
static ani_object GetRule(ani_env *env);
static ani_boolean Contains(ani_env *env, ani_object rule);
static void AddCheckRule(ani_env *env, ani_object rule);
static void RemoveCheckRule(ani_env *env, ani_object rule);
static ani_boolean ContainsCheckRule(ani_env *env, ani_object rule);
static bool MatchValueType(ani_env *env, ani_object value, const char *targetType);
static uint64_t GetRuleParam(ani_env *env, ani_object rule);
static ani_object BuildBigintResult(ani_env *env, uint64_t rule);
static void ThrowError(ani_env *env, int32_t errCode);
} // HiviewDFX
} // OHOS
#endif // ANI_HICHECKER_H
