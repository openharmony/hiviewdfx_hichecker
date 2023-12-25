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

#include "hichecker.h"

#include <csignal>
#include <cerrno>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <parameter.h>

#include "securec.h"

#include "dfx_dump_catcher.h"
#include "hilog/log_c.h"
#include "hilog/log_cpp.h"

namespace OHOS {
namespace HiviewDFX {
#define PARAM_BUF_LEN 128
#define QUERYNAME_LEN 80
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D0B
#undef LOG_TAG
#define LOG_TAG "HICHECKER"

std::mutex HiChecker::mutexLock_;
volatile bool HiChecker::checkMode_;
volatile uint64_t HiChecker::processRules_;
thread_local uint64_t HiChecker::threadLocalRules_;

void HiChecker::AddRule(uint64_t rule)
{
    std::lock_guard<std::mutex> lock(mutexLock_);
    if (!CheckRule(rule)) {
        return;
    }
    if ((Rule::RULE_CHECK_SLOW_EVENT & rule)) {
        checkMode_ = true;
    }
    threadLocalRules_ |= (Rule::ALL_THREAD_RULES | Rule::ALL_CAUTION_RULES) & rule;
    processRules_ |= (Rule::ALL_PROCESS_RULES | Rule::ALL_CAUTION_RULES) & rule;
}

void HiChecker::RemoveRule(uint64_t rule)
{
    std::lock_guard<std::mutex> lock(mutexLock_);
    if (!CheckRule(rule)) {
        return;
    }
    if ((Rule::RULE_CHECK_SLOW_EVENT & rule)) {
        checkMode_ = false;
    }
    threadLocalRules_ ^= threadLocalRules_ & rule;
    processRules_ ^= processRules_ & rule;
}

uint64_t HiChecker::GetRule()
{
    std::lock_guard<std::mutex> lock(mutexLock_);
    return (threadLocalRules_ | processRules_);
}

bool HiChecker::Contains(uint64_t rule)
{
    std::lock_guard<std::mutex> lock(mutexLock_);
    if (!CheckRule(rule)) {
        return false;
    }
    return rule == (rule & (threadLocalRules_ | processRules_));
}

void HiChecker::NotifySlowProcess(const std::string& tag)
{
    if ((threadLocalRules_ & Rule::RULE_THREAD_CHECK_SLOW_PROCESS) == 0) {
        return;
    }
    std::string stackTrace;
    DumpStackTrace(stackTrace);
    Caution caution(Rule::RULE_THREAD_CHECK_SLOW_PROCESS,
        "trigger:RULE_THREAD_CHECK_SLOW_PROCESS," + tag, stackTrace);
    HandleCaution(caution);
}

void HiChecker::NotifySlowEvent(const std::string& tag)
{
    if ((processRules_ & Rule::RULE_CHECK_SLOW_EVENT) == 0) {
        return;
    }
    std::string stackTrace;
    DumpStackTrace(stackTrace);
    Caution caution(Rule::RULE_CHECK_SLOW_EVENT,
        "trigger:RULE_CHECK_SLOW_EVENT," + tag, stackTrace);
    HandleCaution(caution);
}

void HiChecker::NotifyAbilityConnectionLeak(const Caution& caution)
{
    if ((processRules_ & Rule::RULE_CHECK_ABILITY_CONNECTION_LEAK) == 0) {
        return;
    }
    HandleCaution(caution);
}

void HiChecker::NotifyCaution(uint64_t rule, const std::string& tag, Caution& caution)
{
    if ((threadLocalRules_ & rule) == 0 && (processRules_ & rule) == 0) {
        return;
    }
    std::string msg;
    switch (rule) {
        case Rule::RULE_THREAD_CHECK_SLOW_PROCESS:
            msg = "trigger:RULE_THREAD_CHECK_SLOW_PROCESS," + tag;
            break;
        case Rule::RULE_CHECK_SLOW_EVENT:
            msg = "trigger:RULE_CHECK_SLOW_EVENT," + tag;
            break;
        case Rule::RULE_CHECK_ARKUI_PERFORMANCE:
            msg = "trigger:RULE_CHECK_ARKUI_PERFORMANCE," + tag;
            break;
        default:
            break;
    }
    if (Rule::RULE_CHECK_ABILITY_CONNECTION_LEAK != rule) {
        std::string stackTrace;
        DumpStackTrace(stackTrace);
        caution.SetCautionMsg(msg);
        caution.SetStackTrace(stackTrace);
    }
    HandleCaution(caution);
}

void HiChecker::HandleCaution(const Caution& caution)
{
    uint64_t triggerRule = caution.GetTriggerRule();
    if ((threadLocalRules_ & triggerRule)) {
        CautionDetail cautionDetail(caution, threadLocalRules_);
        OnThreadCautionFound(cautionDetail);
        return;
    }
    if ((processRules_ & triggerRule)) {
        CautionDetail cautionDetail(caution, processRules_);
        OnProcessCautionFound(cautionDetail);
        return;
    }
}

void HiChecker::OnThreadCautionFound(CautionDetail& cautionDetail)
{
    if ((cautionDetail.rules_ & Rule::ALL_CAUTION_RULES) == 0) {
        cautionDetail.rules_ |= Rule::RULE_CAUTION_PRINT_LOG;
    }
    if (cautionDetail.CautionEnable(Rule::RULE_CAUTION_PRINT_LOG)
        && !cautionDetail.CautionEnable(Rule::RULE_CAUTION_TRIGGER_CRASH)) {
        PrintLog(cautionDetail);
    }
    if (cautionDetail.CautionEnable(Rule::RULE_CAUTION_TRIGGER_CRASH)) {
        TriggerCrash(cautionDetail);
    }
}

void HiChecker::OnProcessCautionFound(CautionDetail& cautionDetail)
{
    OnThreadCautionFound(cautionDetail);
}

void HiChecker::PrintLog(const CautionDetail& cautionDetail)
{
    HILOG_INFO(LOG_CORE,
        "HiChecker caution with RULE_CAUTION_PRINT_LOG.\nCautionMsg:%{public}s\nStackTrace:\n%{public}s",
        cautionDetail.caution_.GetCautionMsg().c_str(),
        cautionDetail.caution_.GetStackTrace().c_str());
}

void HiChecker::TriggerCrash(const CautionDetail& cautionDetail)
{
    HILOG_INFO(LOG_CORE,
        "HiChecker caution with RULE_CAUTION_TRIGGER_CRASH; exit.\nCautionMsg:%{public}s\nStackTrace:\n%{public}s",
        cautionDetail.caution_.GetCautionMsg().c_str(),
        cautionDetail.caution_.GetStackTrace().c_str());
    kill(getpid(), SIGABRT);
}

bool HiChecker::NeedCheckSlowEvent()
{
    return checkMode_;
}

bool HiChecker::HasCautionRule(uint64_t rules)
{
    return (rules & Rule::ALL_CAUTION_RULES);
}

void HiChecker::DumpStackTrace(std::string& msg)
{
    DfxDumpCatcher dumplog;
    if (!dumplog.DumpCatch(getpid(), gettid(), msg)) {
        HILOG_INFO(LOG_CORE, "HiChecker DumpStackTrace fail.");
    }
}

bool HiChecker::CheckRule(uint64_t rule)
{
    if (rule <= 0 || Rule::ALL_RULES != (Rule::ALL_RULES | rule)) {
        HILOG_INFO(LOG_CORE, "input rule is not exist,please check.");
        return false;
    }
    return true;
}

void HiChecker::InitHicheckerParam(const char *processName)
{
    HILOG_INFO(LOG_CORE, "hichecker processName is %{public}s", processName);
    char checkerName[QUERYNAME_LEN] = "hiviewdfx.hichecker.";
    errno_t err = 0;
    err = strcat_s(checkerName, sizeof(checkerName), processName);
    if (err != EOK) {
        HILOG_INFO(LOG_CORE, "checker strcat_s query name failed.");
        return;
    }

    char paramOutBuf[PARAM_BUF_LEN] = { 0 };
    char defStrValue[PARAM_BUF_LEN] = { 0 };
    int retLen = GetParameter(checkerName, defStrValue, paramOutBuf, PARAM_BUF_LEN);
    if (retLen == 0 || retLen > PARAM_BUF_LEN - 1) {
        HILOG_INFO(LOG_CORE, "hichecker param is empty.");
        return;
    }
    paramOutBuf[retLen] = '\0';
    HILOG_INFO(LOG_CORE, "hichecker param value is %{public}s", paramOutBuf);
    std::string paramStr(paramOutBuf);
    uint64_t rule = std::stoull(paramStr);
    AddRule(rule);
    return;
}
} // HiviewDFX
} // OHOS
