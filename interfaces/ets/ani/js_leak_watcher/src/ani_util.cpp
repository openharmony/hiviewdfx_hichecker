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

#include "ani_util.h"

#include "hilog/log.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD003D00
#undef LOG_TAG
#define LOG_TAG "JSLEAK_WATCHER_ANI_UTIL"

namespace OHOS {
namespace HiviewDFX {

void JsLeakWatcherAniUtil::ThrowErrorMessage(ani_env *env, const std::string &msg, int32_t errCode)
{
    if (env == nullptr) {
        return;
    }
    ani_class cls {};
    if (env->FindClass(CLASS_NAME_BUSINESSERROR, &cls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_BUSINESSERROR);
        return;
    }
    ani_method ctor {};
    if (env->Class_FindMethod(cls, "<ctor>", ":", &ctor) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find method BusinessError constructor failed");
        return;
    }
    ani_object error {};
    if (env->Object_New(cls, ctor, &error) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "new object %{public}s failed", CLASS_NAME_BUSINESSERROR);
        return;
    }
    if (env->Object_SetPropertyByName_Int(error, "code_", static_cast<ani_int>(errCode)) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "set property BusinessError.code_ failed");
        return;
    }
    ani_string messageRef {};
    if (env->String_NewUTF8(msg.c_str(), msg.size(), &messageRef) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "new message string failed");
        return;
    }
    if (env->Object_SetPropertyByName_Ref(error, "message", static_cast<ani_ref>(messageRef)) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "set property BusinessError.message failed");
        return;
    }
    if (env->ThrowError(static_cast<ani_error>(error)) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "throwError ani_error object failed");
    }
}

ani_object JsLeakWatcherAniUtil::CreateUndefined(ani_env *env)
{
    ani_ref undefinedRef = nullptr;
    if (env->GetUndefined(&undefinedRef) != ANI_OK) {
        return nullptr;
    }
    return static_cast<ani_object>(undefinedRef);
}

ani_status JsLeakWatcherAniUtil::ParseAniString(ani_env *env, ani_string aniStr, std::string &str)
{
    ani_size srcSize = 0;
    ani_status status = env->String_GetUTF8Size(aniStr, &srcSize);
    if (status != ANI_OK) {
        return status;
    }
    std::vector<char> buffer(srcSize + 1);
    ani_size dstSize = 0;
    status = env->String_GetUTF8SubString(aniStr, 0, srcSize, buffer.data(), buffer.size(), &dstSize);
    if (status != ANI_OK || srcSize != dstSize) {
        return status;
    }
    str.assign(buffer.data(), dstSize);
    return ANI_OK;
}

ani_ref JsLeakWatcherAniUtil::CreateGlobalReference(ani_env *env, ani_ref ref)
{
    ani_ref gRef {};
    if (env == nullptr) {
        return gRef;
    }
    if (env->GlobalReference_Create(ref, &gRef) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "failed to create global reference");
    }
    return gRef;
}

bool JsLeakWatcherAniUtil::IsRefUndefined(ani_env *env, ani_ref ref)
{
    ani_boolean isUndefined = ANI_FALSE;
    if (env != nullptr) {
        env->Reference_IsUndefined(ref, &isUndefined);
    }
    return isUndefined;
}

ani_object JsLeakWatcherAniUtil::CreateInt(ani_env *env, int32_t num)
{
    ani_class cls {};
    ani_object obj {};
    if (env == nullptr) {
        return obj;
    }
    if (env->FindClass(CLASS_NAME_INT, &cls) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "find class %{public}s failed", CLASS_NAME_INT);
        return obj;
    }
    ani_method ctor {};
    if (env->Class_FindMethod(cls, "<ctor>", "i:", &ctor) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "create int failed, Class_FindMethod failed");
        return obj;
    }
    if (env->Object_New(cls, ctor, &obj, static_cast<ani_int>(num)) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "create int failed, Object_New failed");
    }
    return obj;
}

ani_string JsLeakWatcherAniUtil::CreateAniString(ani_env *env, const std::string &str)
{
    ani_string result {};
    if (env == nullptr) {
        return result;
    }
    if (env->String_NewUTF8(str.c_str(), str.size(), &result) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "new ani string failed");
    }
    return result;
}

ani_vm *JsLeakWatcherAniUtil::GetAniVm(ani_env *env)
{
    ani_vm *vm = nullptr;
    if (env->GetVM(&vm) != ANI_OK) {
        HILOG_ERROR(LOG_CORE, "Failed get vm");
    }
    return vm;
}

bool JsLeakWatcherAniUtil::DumpHeapSnapshot(const std::string &filePath)
{
    return DumpHeapSnapshotImpl(filePath);
}

void JsLeakWatcherAniUtil::DumpHeapSnapshot(const std::string &filePath,
    const std::function<void(uint8_t)> &callback)
{
    DumpHeapSnapshotImplAsync(filePath, callback);
}
} // namespace HiviewDFX
} // namespace OHOS
