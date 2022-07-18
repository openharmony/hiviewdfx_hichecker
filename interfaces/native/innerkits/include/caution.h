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

#ifndef HIVIEWDFX_CAUTION_H
#define HIVIEWDFX_CAUTION_H

#include <string>

namespace OHOS {
namespace HiviewDFX {
class Caution {
public:
    Caution() : triggerRule_(0ULL) {}
    Caution(uint64_t triggerRule, const std::string& cautionMsg): triggerRule_(triggerRule),
        cautionMsg_(cautionMsg) {}
    Caution(uint64_t triggerRule, const std::string& cautionMsg, const std::string& stackTrace)
        : triggerRule_(triggerRule), cautionMsg_(cautionMsg), stackTrace_(stackTrace) {}
    ~Caution() {}
    Caution(const Caution&) = default;
    Caution& operator = (Caution&) = default;
    void SetTriggerRule(uint64_t rule);
    void SetCautionMsg(const std::string& cautionMsg);
    void SetStackTrace(const std::string& stackTrace);
    uint64_t GetTriggerRule() const;
    std::string GetCautionMsg() const;
    std::string GetStackTrace() const;
private:
    uint64_t triggerRule_;
    std::string cautionMsg_;
    std::string stackTrace_;
};
} // HiviewDFX
} // OHOS
#endif // HIVIEWDFX_CAUTION_H