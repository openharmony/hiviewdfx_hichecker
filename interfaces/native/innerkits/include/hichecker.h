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

#ifndef HIVIEWDFX_HICHECKER_H
#define HIVIEWDFX_HICHECKER_H

#include <mutex>
#include <string>

#include "caution.h"

namespace OHOS {
namespace HiviewDFX {
namespace Rule {
const uint64_t RULE_CAUTION_PRINT_LOG = 1ULL << 63;
const uint64_t RULE_CAUTION_TRIGGER_CRASH = 1ULL << 62;
const uint64_t RULE_THREAD_CHECK_SLOW_PROCESS = 1ULL;
const uint64_t RULE_CHECK_SLOW_EVENT = 1ULL << 32;
const uint64_t RULE_CHECK_ABILITY_CONNECTION_LEAK = 1ULL << 33;
const uint64_t ALL_RULES = RULE_CAUTION_PRINT_LOG | RULE_CAUTION_TRIGGER_CRASH
            | RULE_THREAD_CHECK_SLOW_PROCESS | RULE_CHECK_SLOW_EVENT | RULE_CHECK_ABILITY_CONNECTION_LEAK;
const uint64_t ALL_PROCESS_RULES = RULE_CHECK_SLOW_EVENT | RULE_CHECK_ABILITY_CONNECTION_LEAK;
const uint64_t ALL_THREAD_RULES = RULE_THREAD_CHECK_SLOW_PROCESS;
const uint64_t ALL_CAUTION_RULES = RULE_CAUTION_PRINT_LOG | RULE_CAUTION_TRIGGER_CRASH;
};

class CautionDetail {
public:
    CautionDetail(const Caution& caution, uint64_t rules) : caution_(caution), rules_(rules) {}
    CautionDetail(const CautionDetail&) = delete;
    CautionDetail& operator = (CautionDetail&) = delete;
    bool CautionEnable(uint64_t rule) const
    {
        return rule == (rules_ & rule);
    }

    const Caution caution_;
    uint64_t rules_;
};

class HiChecker {
public:
    HiChecker() = delete;
    HiChecker(const HiChecker&) = delete;
    HiChecker& operator = (HiChecker&) = delete;
    static void NotifySlowProcess(const std::string& tag);
    static void NotifySlowEvent(const std::string& tag);
    static void NotifyAbilityConnectionLeak(const Caution& caution);
    static bool NeedCheckSlowEvent();

    static void AddRule(uint64_t rule);
    static void RemoveRule(uint64_t rule);
    static uint64_t GetRule();
    static bool Contains(uint64_t rule);
private:
    static void HandleCaution(const Caution& caution);
    static void OnThreadCautionFound(CautionDetail& cautionDetail);
    static void OnProcessCautionFound(CautionDetail& cautionDetail);
    static void PrintLog(const CautionDetail& cautionDetail);
    static void TriggerCrash(const CautionDetail& cautionDetail);
    static bool HasCautionRule(uint64_t rules);
    static void DumpStackTrace(std::string& msg);
    static bool CheckRule(uint64_t rule);

    static std::mutex mutexLock_;
    static volatile bool checkMode_;
    static volatile uint64_t processRules_;
    static thread_local uint64_t threadLocalRules_;
};
} // HiviewDFX
} // OHOS
#endif // HIVIEWDFX_HICHECKER_H