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

#ifndef JS_LEAK_WATCHER_ANI_UTIL_H
#define JS_LEAK_WATCHER_ANI_UTIL_H

#include <cstdint>
#include <functional>
#include <string>

#include "ani.h"

namespace OHOS {
namespace HiviewDFX {

constexpr char CLASS_NAME_BUSINESSERROR[] = "@ohos.base.BusinessError";
constexpr char CLASS_NAME_INT[] = "std.core.Int";
constexpr char CLASS_NAME_STRING[] = "std.core.String";

class JsLeakWatcherAniUtil {
public:
    static void ThrowErrorMessage(ani_env *env, const std::string &msg, int32_t errCode);
    static ani_object CreateUndefined(ani_env *env);
    static ani_status ParseAniString(ani_env *env, ani_string aniStr, std::string &str);
    static ani_ref CreateGlobalReference(ani_env *env, ani_ref ref);
    static bool IsRefUndefined(ani_env *env, ani_ref ref);
    static ani_object CreateInt(ani_env *env, int32_t num);
    static ani_string CreateAniString(ani_env *env, const std::string &str);
    static ani_vm *GetAniVm(ani_env *env);
};
} // namespace HiviewDFX
} // namespace OHOS

#endif // JS_LEAK_WATCHER_ANI_UTIL_H
