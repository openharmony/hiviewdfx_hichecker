/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <cstdlib>
#include <ctime>
#include <gtest/gtest.h>

#include "caution.h"
#include "hichecker.h"

using namespace testing::ext;
using namespace OHOS::HiviewDFX;

namespace {
    const int64_t SEC_TO_NS = 1000000000;
    const int64_t MAX_CALL_DURATION_US = 1000; // 1ms
    const int LOOP_COUNT = 1000;
    const uint64_t RULE_ERROR0 = 0;
    const uint64_t RULE_ERROR1 = -1;
    const uint64_t RULE_ERROR2 = 999999999;
}

namespace OHOS {
namespace HiviewDFX {
class HiCheckerNativeTest : public testing::Test {
public:
    static void SerUpTestCase() {};
    static void TearDownTestCase() {};
    void SetUp();
    void TearDown();
};

void HiCheckerNativeTest::SetUp(void)
{
    HiChecker::RemoveRule(Rule::ALL_RULES);
}

void HiCheckerNativeTest::TearDown(void)
{
    HiChecker::RemoveRule(Rule::ALL_RULES);
}

static int64_t GetTimeNs()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * SEC_TO_NS + ts.tv_nsec;
}

/**
  * @tc.name: AddRule001
  * @tc.desc: add only one rule
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, AddRuleTest001, TestSize.Level1)
{
    uint64_t rule = Rule::RULE_THREAD_CHECK_SLOW_PROCESS;
    HiChecker::AddRule(Rule::RULE_THREAD_CHECK_SLOW_PROCESS);
    ASSERT_EQ(HiChecker::GetRule(), rule);
    rule |= Rule::RULE_CAUTION_PRINT_LOG;
    HiChecker::AddRule(Rule::RULE_CAUTION_PRINT_LOG);
    ASSERT_EQ(HiChecker::GetRule(), rule);
}

/**
  * @tc.name: AddRule002
  * @tc.desc: add two or more rules
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, AddRuleTest002, TestSize.Level1)
{
    uint64_t rule = Rule::RULE_THREAD_CHECK_SLOW_PROCESS |
        Rule::RULE_CHECK_SLOW_EVENT | Rule::RULE_CHECK_ABILITY_CONNECTION_LEAK;
    HiChecker::AddRule(Rule::RULE_THREAD_CHECK_SLOW_PROCESS |
        Rule::RULE_CHECK_SLOW_EVENT | Rule::RULE_CHECK_ABILITY_CONNECTION_LEAK);
    ASSERT_EQ(HiChecker::GetRule(), rule);
    rule |= (Rule::RULE_CAUTION_PRINT_LOG | Rule::RULE_CAUTION_TRIGGER_CRASH);
    HiChecker::AddRule(Rule::RULE_CAUTION_PRINT_LOG | Rule::RULE_CAUTION_TRIGGER_CRASH);
    ASSERT_EQ(HiChecker::GetRule(), rule);
}

/**
  * @tc.name: AddRule003
  * @tc.desc: add invaild rule
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, AddRuleTest003, TestSize.Level1)
{
    HiChecker::AddRule(RULE_ERROR0);
    ASSERT_EQ(HiChecker::GetRule(), 0);
    HiChecker::AddRule(RULE_ERROR1);
    ASSERT_EQ(HiChecker::GetRule(), 0);
    HiChecker::AddRule(RULE_ERROR2);
    ASSERT_EQ(HiChecker::GetRule(), 0);
}

/**
  * @tc.name: AddRulePerf
  * @tc.desc: test performance for AddRule
  * @tc.type: PERF
*/
HWTEST_F(HiCheckerNativeTest, AddRulePerfTest001, TestSize.Level2)
{
    int64_t total = 0;
    for (int i = 0; i < LOOP_COUNT; i++) {
        int64_t start = GetTimeNs();
        HiChecker::AddRule(Rule::RULE_CHECK_SLOW_EVENT);
        int64_t duration = GetTimeNs() - start;
        total += duration;
    }
    int64_t duration = (total / LOOP_COUNT);
    duration = duration / 1000;
    ASSERT_TRUE(duration < MAX_CALL_DURATION_US);
}

/**
  * @tc.name: RemoveRule001
  * @tc.desc: remove only one rule
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, RemoveRuleTest001, TestSize.Level1)
{
    HiChecker::AddRule(Rule::ALL_RULES);
    HiChecker::RemoveRule(Rule::RULE_CAUTION_TRIGGER_CRASH);
    ASSERT_FALSE(HiChecker::Contains(Rule::RULE_CAUTION_TRIGGER_CRASH));
    HiChecker::RemoveRule(Rule::RULE_CAUTION_PRINT_LOG);
    ASSERT_FALSE(HiChecker::Contains(Rule::RULE_CAUTION_PRINT_LOG));
    uint64_t rule = Rule::ALL_RULES ^ (Rule::RULE_CAUTION_PRINT_LOG | Rule::RULE_CAUTION_TRIGGER_CRASH);
    ASSERT_EQ(HiChecker::GetRule(), rule);
}

/**
  * @tc.name: RemoveRule002
  * @tc.desc: remove two or more rules
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, RemoveRuleTest002, TestSize.Level1)
{
    HiChecker::AddRule(Rule::ALL_RULES);
    HiChecker::RemoveRule(Rule::RULE_CAUTION_TRIGGER_CRASH | Rule::RULE_CAUTION_PRINT_LOG);
    ASSERT_FALSE(HiChecker::Contains(Rule::RULE_CAUTION_TRIGGER_CRASH));
    ASSERT_FALSE(HiChecker::Contains(Rule::RULE_CAUTION_PRINT_LOG));
    uint64_t rule = Rule::ALL_RULES ^ (Rule::RULE_CAUTION_PRINT_LOG | Rule::RULE_CAUTION_TRIGGER_CRASH);
    ASSERT_EQ(HiChecker::GetRule(), rule);
}

/**
  * @tc.name: RemoveRule003
  * @tc.desc: remove invaild rule
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, RemoveRuleTest003, TestSize.Level1)
{
    HiChecker::AddRule(Rule::ALL_RULES);
    HiChecker::RemoveRule(RULE_ERROR0);
    ASSERT_EQ(HiChecker::GetRule(), Rule::ALL_RULES);
    HiChecker::RemoveRule(RULE_ERROR1);
    ASSERT_EQ(HiChecker::GetRule(), Rule::ALL_RULES);
    HiChecker::RemoveRule(RULE_ERROR2);
    ASSERT_EQ(HiChecker::GetRule(), Rule::ALL_RULES);
}

/**
  * @tc.name: RemoveRulePerf
  * @tc.desc: test performance for RemoveRule
  * @tc.type: PERF
*/
HWTEST_F(HiCheckerNativeTest, RemoveRulePerfTest001, TestSize.Level2)
{
    int64_t total = 0;
    for (int i = 0; i < LOOP_COUNT; i++) {
        int64_t start = GetTimeNs();
        HiChecker::RemoveRule(Rule::RULE_CHECK_SLOW_EVENT);
        int64_t duration = GetTimeNs() - start;
        total += duration;
    }
    int64_t duration = (total / LOOP_COUNT);
    duration = duration / 1000;
    ASSERT_TRUE(duration < MAX_CALL_DURATION_US);
}

/**
  * @tc.name: Contains001
  * @tc.desc: test Contains
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, ContainsTest001, TestSize.Level1)
{
    HiChecker::AddRule(Rule::RULE_CAUTION_PRINT_LOG);
    ASSERT_TRUE(HiChecker::Contains(Rule::RULE_CAUTION_PRINT_LOG));
    ASSERT_FALSE(HiChecker::Contains(Rule::RULE_CAUTION_TRIGGER_CRASH));
    ASSERT_FALSE(HiChecker::Contains(Rule::RULE_CAUTION_PRINT_LOG | Rule::RULE_CAUTION_TRIGGER_CRASH));
}

/**
  * @tc.name: Contains002
  * @tc.desc: test Contains with invaild rule
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, ContainsTest002, TestSize.Level1)
{
    HiChecker::AddRule(Rule::ALL_RULES);
    ASSERT_FALSE(HiChecker::Contains(RULE_ERROR0));
    ASSERT_FALSE(HiChecker::Contains(RULE_ERROR1));
    ASSERT_FALSE(HiChecker::Contains(RULE_ERROR2));
}

/**
  * @tc.name: CautionTest001
  * @tc.desc: test Caution
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, CautionTest001, TestSize.Level1)
{
    Caution caution;
    caution.SetTriggerRule(Rule::RULE_CAUTION_PRINT_LOG);
    EXPECT_EQ(caution.GetTriggerRule(), Rule::RULE_CAUTION_PRINT_LOG);

    caution.SetStackTrace("stack_trace");
    EXPECT_EQ(caution.GetStackTrace(), "stack_trace");

    caution.SetCautionMsg("caution_msg");
    EXPECT_EQ(caution.GetCautionMsg(), "caution_msg");
}

/**
  * @tc.name: NotifySlowProcessTest001
  * @tc.desc: test NotifySlowProcess
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, NotifySlowProcessTest001, TestSize.Level1)
{
    HiChecker::AddRule(RULE_ERROR0);
    std::string eventTag = "NotifySlowProcessTest001";
    HiChecker::NotifySlowProcess(eventTag);
    HiChecker::AddRule(Rule::RULE_THREAD_CHECK_SLOW_PROCESS);
    ASSERT_EQ(HiChecker::GetRule(), Rule::RULE_THREAD_CHECK_SLOW_PROCESS);
    HiChecker::NotifySlowProcess(eventTag);
    HiChecker::RemoveRule(Rule::RULE_CHECK_SLOW_EVENT);
}

/**
  * @tc.name: NotifyAbilityConnectionLeakTest001
  * @tc.desc: test NotifyAbilityConnectionLeak
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, NotifyAbilityConnectionLeakTest001, TestSize.Level1)
{
    std::string cautionMsg = "NotifyAbilityConnectionLeakTest001";
    std::string stackTrace = "stackTrace";
    Caution cautionError(RULE_ERROR0, cautionMsg, "stackTrace");
    HiChecker::NotifyAbilityConnectionLeak(cautionError);
    Caution caution(Rule::RULE_CHECK_ABILITY_CONNECTION_LEAK, cautionMsg, stackTrace);
    HiChecker::NotifyAbilityConnectionLeak(caution);
    EXPECT_EQ(caution.GetStackTrace(), stackTrace);
    EXPECT_EQ(caution.GetCautionMsg(), cautionMsg);
}

/**
  * @tc.name: NotifySlowEventTest001
  * @tc.desc: test PrintLog
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, NotifySlowEventTest001, TestSize.Level1)
{
    HiChecker::AddRule(RULE_ERROR0);
    std::string eventTag = "NotifySlowEventTest001 time out";
    HiChecker::NotifySlowEvent(eventTag);
    HiChecker::AddRule(Rule::RULE_CHECK_SLOW_EVENT);
    ASSERT_TRUE(HiChecker::NeedCheckSlowEvent());
    HiChecker::NotifySlowEvent(eventTag);
    HiChecker::RemoveRule(Rule::RULE_CHECK_SLOW_EVENT);
}

/**
  * @tc.name: NotifyCautionTest001
  * @tc.desc: test NotifyCaution
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, NotifyCautionTest001, TestSize.Level1)
{
    HiChecker::AddRule(RULE_ERROR0);
    Caution caution;
    std::string tag = "error_tag";
    HiChecker::NotifyCaution(RULE_ERROR0, tag, caution);
    ASSERT_EQ(HiChecker::GetRule(), 0);
}

/**
  * @tc.name: NotifyCautionTest002
  * @tc.desc: test NotifyCaution
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, NotifyCautionTest002, TestSize.Level1)
{
    HiChecker::AddRule(Rule::RULE_CHECK_ARKUI_PERFORMANCE);
    std::string tag = "arkui_tag";
    Caution caution;
    caution.SetTriggerRule(Rule::RULE_CHECK_ARKUI_PERFORMANCE);
    HiChecker::NotifyCaution(Rule::RULE_CHECK_ARKUI_PERFORMANCE, tag, caution);
    HiChecker::RemoveRule(Rule::RULE_CHECK_ARKUI_PERFORMANCE);
    EXPECT_EQ(caution.GetCautionMsg(), "trigger:RULE_CHECK_ARKUI_PERFORMANCE,arkui_tag");
}

/**
  * @tc.name: NotifyCautionTest003
  * @tc.desc: test NotifyCaution
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, NotifyCautionTest003, TestSize.Level1)
{
    HiChecker::AddRule(Rule::RULE_THREAD_CHECK_SLOW_PROCESS);
    std::string tag = "slow_process_tag";
    Caution caution;
    caution.SetTriggerRule(Rule::RULE_THREAD_CHECK_SLOW_PROCESS);
    HiChecker::NotifyCaution(Rule::RULE_THREAD_CHECK_SLOW_PROCESS, tag, caution);
    HiChecker::RemoveRule(Rule::RULE_THREAD_CHECK_SLOW_PROCESS);
    EXPECT_EQ(caution.GetCautionMsg(), "trigger:RULE_THREAD_CHECK_SLOW_PROCESS,slow_process_tag");
}

/**
  * @tc.name: NotifyCautionTest004
  * @tc.desc: test NotifyCaution
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, NotifyCautionTest004, TestSize.Level1)
{
    HiChecker::AddRule(Rule::RULE_CHECK_SLOW_EVENT);
    std::string tag = "slow_event_tag";
    Caution caution;
    caution.SetTriggerRule(Rule::RULE_CHECK_SLOW_EVENT);
    HiChecker::NotifyCaution(Rule::RULE_CHECK_SLOW_EVENT, tag, caution);
    HiChecker::RemoveRule(Rule::RULE_CHECK_SLOW_EVENT);
    EXPECT_EQ(caution.GetCautionMsg(), "trigger:RULE_CHECK_SLOW_EVENT,slow_event_tag");
}

/**
  * @tc.name: InitHicheckerParamTest001
  * @tc.desc: test InitHicheckerParam
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, InitHicheckerParamTest001, TestSize.Level1)
{
    system("param set hiviewdfx.hichecker.checker_test 17179869184");
    const char *processName = "checker_test111";
    HiChecker::InitHicheckerParam(processName);
    ASSERT_FALSE(HiChecker::Contains(Rule::RULE_CHECK_ARKUI_PERFORMANCE));
}

/**
  * @tc.name: InitHicheckerParamTest002
  * @tc.desc: test InitHicheckerParam
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, InitHicheckerParamTest002, TestSize.Level1)
{
    system("param set hiviewdfx.hichecker.checker_test 17179869184");
    const char *processName = "checker_test";
    HiChecker::InitHicheckerParam(processName);
    ASSERT_TRUE(HiChecker::Contains(Rule::RULE_CHECK_ARKUI_PERFORMANCE));
}

/**
  * @tc.name: InitHicheckerParamTest003
  * @tc.desc: test InitHicheckerParam
  * @tc.type: FUNC
*/
HWTEST_F(HiCheckerNativeTest, InitHicheckerParamTest003, TestSize.Level1)
{
    const char *processName = "test.process.name.maxlength.greatthan.eighteen.aaaaaaaaaaaaaaaaaaaaaaaaaaa";
    HiChecker::InitHicheckerParam(processName);
    ASSERT_FALSE(HiChecker::Contains(Rule::RULE_CHECK_ARKUI_PERFORMANCE));
}
} // namespace HiviewDFX
} // namespace OHOS
