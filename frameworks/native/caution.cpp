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

#include "caution.h"

namespace OHOS {
namespace HiviewDFX {
uint64_t Caution::GetTriggerRule() const
{
    return triggerRule_;
}

std::string Caution::GetCautionMsg() const
{
    return cautionMsg_;
}

std::string Caution::GetStackTrace() const
{
    return stackTrace_;
}

void Caution::SetTriggerRule(uint64_t rule)
{
    triggerRule_ = rule;
}

void Caution::SetCautionMsg(const std::string& cautionMsg)
{
    cautionMsg_ = cautionMsg;
}

void Caution::SetStackTrace(const std::string& stackTrace)
{
    stackTrace_ = stackTrace;
}
} // HiviewDFX
} // OHOS