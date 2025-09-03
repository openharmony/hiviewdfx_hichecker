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


#include "hichecker_fuzzer.h"
#include <fuzzer/FuzzedDataProvider.h>

namespace HicheckerFuzz {
constexpr uint8_t MAX_STR_LENGTH = 128;

void HicheckerFuzzTest(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    uint64_t rule = provider.ConsumeIntegral<uint64_t>();
    if (rule & OHOS::HiviewDFX::Rule::RULE_CAUTION_TRIGGER_CRASH) {
        return;
    }
    OHOS::HiviewDFX::HiChecker::AddRule(rule);
    std::string str = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    OHOS::HiviewDFX::HiChecker::NotifySlowProcess(str);
    OHOS::HiviewDFX::HiChecker::NotifySlowEvent(str);
    std::string msg = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    OHOS::HiviewDFX::Caution caution(rule, str, msg);
    OHOS::HiviewDFX::HiChecker::NotifyAbilityConnectionLeak(caution);
    std::string tag = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    OHOS::HiviewDFX::HiChecker::NotifyCaution(rule, tag, caution);
    OHOS::HiviewDFX::HiChecker::NeedCheckSlowEvent();
    OHOS::HiviewDFX::HiChecker::InitHicheckerParam(str.c_str());
    OHOS::HiviewDFX::HiChecker::Contains(rule);
    OHOS::HiviewDFX::HiChecker::GetRule();
    OHOS::HiviewDFX::HiChecker::RemoveRule(rule);
}
} // namespace HicheckerFuzz

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }
    HicheckerFuzz::HicheckerFuzzTest(data, size);
    return 0;
}