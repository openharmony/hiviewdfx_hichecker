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

void AddRuleFuzz(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    uint64_t rule = provider.ConsumeIntegral<uint64_t>();
    OHOS::HiviewDFX::HiChecker::AddRule(rule);
}

void ContainsFuzz(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    uint64_t rule = provider.ConsumeIntegral<uint64_t>();
    OHOS::HiviewDFX::HiChecker::Contains(rule);
}

void RemoveRuleFuzz(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    uint64_t rule = provider.ConsumeIntegral<uint64_t>();
    OHOS::HiviewDFX::HiChecker::RemoveRule(rule);
}

void NotifySlowProcessFuzz(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    std::string str = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    OHOS::HiviewDFX::HiChecker::NotifySlowProcess(str);
}

void NotifySlowEventFuzz(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    std::string str = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    OHOS::HiviewDFX::HiChecker::NotifySlowEvent(str);
}

void NotifyAbilityConnectionLeakFuzz(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    uint64_t rule = provider.ConsumeIntegral<uint64_t>();
    if (rule & OHOS::HiviewDFX::Rule::RULE_CAUTION_TRIGGER_CRASH) {
        return;
    }
    std::string str = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    std::string msg = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    OHOS::HiviewDFX::Caution caution(rule, str, msg);
    OHOS::HiviewDFX::HiChecker::NotifyAbilityConnectionLeak(caution);
}

void NotifyCautionFuzz(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    uint64_t rule = provider.ConsumeIntegral<uint64_t>();
    if (rule & OHOS::HiviewDFX::Rule::RULE_CAUTION_TRIGGER_CRASH) {
        return;
    }
    OHOS::HiviewDFX::HiChecker::AddRule(rule);
    std::string str = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    std::string msg = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    OHOS::HiviewDFX::Caution caution(rule, str, msg);
    std::string tag = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    OHOS::HiviewDFX::HiChecker::NotifyCaution(rule, tag, caution);
}

void InitHicheckerParamFuzz(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    std::string str = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    OHOS::HiviewDFX::HiChecker::InitHicheckerParam(str.c_str());
}

void InitHicheckerParamWrapperFuzz(const uint8_t* data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    std::string str = provider.ConsumeRandomLengthString(MAX_STR_LENGTH);
    InitHicheckerParamWrapper(str.c_str());
}
} // namespace HicheckerFuzz

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }
    HicheckerFuzz::AddRuleFuzz(data, size);
    HicheckerFuzz::ContainsFuzz(data, size);
    HicheckerFuzz::RemoveRuleFuzz(data, size);
    HicheckerFuzz::NotifySlowProcessFuzz(data, size);
    HicheckerFuzz::NotifySlowEventFuzz(data, size);
    HicheckerFuzz::NotifyAbilityConnectionLeakFuzz(data, size);
    HicheckerFuzz::NotifyCautionFuzz(data, size);
    HicheckerFuzz::InitHicheckerParamFuzz(data, size);
    HicheckerFuzz::InitHicheckerParamWrapperFuzz(data, size);
    return 0;
}