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

#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "js_leak_watcher_ts.h"

using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {

class JsLeakWatcherTsTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp()
    {
        SetjsLeakWatcherEnableStatus(false);
    }
    void TearDown()
    {
        SetjsLeakWatcherEnableStatus(false);
    }
};

/**
 * @tc.name: GetjsLeakWatcherEnableStatusTest001
 * @tc.desc: test GetjsLeakWatcherEnableStatus after SetjsLeakWatcherEnableStatus(false)
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, GetjsLeakWatcherEnableStatusTest001, TestSize.Level1)
{
    SetjsLeakWatcherEnableStatus(false);
    bool status = GetjsLeakWatcherEnableStatus();
    ASSERT_FALSE(status);
}

/**
 * @tc.name: GetjsLeakWatcherEnableStatusTest002
 * @tc.desc: test GetjsLeakWatcherEnableStatus after SetjsLeakWatcherEnableStatus(true)
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, GetjsLeakWatcherEnableStatusTest002, TestSize.Level1)
{
    SetjsLeakWatcherEnableStatus(true);
    bool status = GetjsLeakWatcherEnableStatus();
    ASSERT_TRUE(status);
}

/**
 * @tc.name: SetjsLeakWatcherEnableStatusTest001
 * @tc.desc: test SetjsLeakWatcherEnableStatus toggle from false to true
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, SetjsLeakWatcherEnableStatusTest001, TestSize.Level1)
{
    SetjsLeakWatcherEnableStatus(false);
    ASSERT_FALSE(GetjsLeakWatcherEnableStatus());
    SetjsLeakWatcherEnableStatus(true);
    ASSERT_TRUE(GetjsLeakWatcherEnableStatus());
}

/**
 * @tc.name: SetjsLeakWatcherEnableStatusTest002
 * @tc.desc: test SetjsLeakWatcherEnableStatus toggle from true to false
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, SetjsLeakWatcherEnableStatusTest002, TestSize.Level1)
{
    SetjsLeakWatcherEnableStatus(true);
    ASSERT_TRUE(GetjsLeakWatcherEnableStatus());
    SetjsLeakWatcherEnableStatus(false);
    ASSERT_FALSE(GetjsLeakWatcherEnableStatus());
}

/**
 * @tc.name: SetjsLeakWatcherEnableStatusTest003
 * @tc.desc: test SetjsLeakWatcherEnableStatus multiple times
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, SetjsLeakWatcherEnableStatusTest003, TestSize.Level1)
{
    SetjsLeakWatcherEnableStatus(true);
    ASSERT_TRUE(GetjsLeakWatcherEnableStatus());
    SetjsLeakWatcherEnableStatus(true);
    ASSERT_TRUE(GetjsLeakWatcherEnableStatus());
    SetjsLeakWatcherEnableStatus(false);
    ASSERT_FALSE(GetjsLeakWatcherEnableStatus());
    SetjsLeakWatcherEnableStatus(false);
    ASSERT_FALSE(GetjsLeakWatcherEnableStatus());
}

/**
 * @tc.name: CheckJsLeakWatcherParamTest001
 * @tc.desc: test CheckJsLeakWatcherParam with nullptr
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, CheckJsLeakWatcherParamTest001, TestSize.Level1)
{
    bool result = TestCheckJsLeakWatcherParam(nullptr);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: CheckJsLeakWatcherParamTest002
 * @tc.desc: test CheckJsLeakWatcherParam with empty string
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, CheckJsLeakWatcherParamTest002, TestSize.Level1)
{
    const char* emptyBundleName = "";
    bool result = TestCheckJsLeakWatcherParam(emptyBundleName);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: CheckJsLeakWatcherParamTest003
 * @tc.desc: test CheckJsLeakWatcherParam with valid bundle name (not enabled)
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, CheckJsLeakWatcherParamTest003, TestSize.Level1)
{
    const char* bundleName = "com.example.test";
    bool result = TestCheckJsLeakWatcherParam(bundleName);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: CheckJsLeakWatcherParamTest004
 * @tc.desc: test CheckJsLeakWatcherParam with long bundle name
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, CheckJsLeakWatcherParamTest004, TestSize.Level1)
{
    std::string longBundleName(300, 'a');
    bool result = TestCheckJsLeakWatcherParam(longBundleName.c_str());
    ASSERT_FALSE(result);
}

/**
 * @tc.name: CheckJsLeakWatcherParamTest005
 * @tc.desc: test CheckJsLeakWatcherParam with special characters in bundle name
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, CheckJsLeakWatcherParamTest005, TestSize.Level1)
{
    const char* bundleName = "com.example.test@123!#$%";
    bool result = TestCheckJsLeakWatcherParam(bundleName);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: CheckJsLeakWatcherParamTest006
 * @tc.desc: test CheckJsLeakWatcherParam with numeric bundle name
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, CheckJsLeakWatcherParamTest006, TestSize.Level1)
{
    const char* bundleName = "123456789";
    bool result = TestCheckJsLeakWatcherParam(bundleName);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: EnableStatusIntegrationTest001
 * @tc.desc: test integration of Set/Get enable status with CheckJsLeakWatcherParam
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, EnableStatusIntegrationTest001, TestSize.Level1)
{
    bool paramResult = TestCheckJsLeakWatcherParam("com.example.test");
    SetjsLeakWatcherEnableStatus(paramResult);
    ASSERT_FALSE(GetjsLeakWatcherEnableStatus());
}

/**
 * @tc.name: EnableStatusIntegrationTest002
 * @tc.desc: test integration with nullptr param and status set to true
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, EnableStatusIntegrationTest002, TestSize.Level1)
{
    bool paramResult = TestCheckJsLeakWatcherParam(nullptr);
    SetjsLeakWatcherEnableStatus(true);
    ASSERT_TRUE(GetjsLeakWatcherEnableStatus());
    ASSERT_FALSE(paramResult);
}

/**
 * @tc.name: EnableStatusIntegrationTest003
 * @tc.desc: test typical initialization flow
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherTsTest, EnableStatusIntegrationTest003, TestSize.Level1)
{
    SetjsLeakWatcherEnableStatus(false);
    const char* bundleName = "com.example.app";
    bool ret = TestCheckJsLeakWatcherParam(bundleName);
    SetjsLeakWatcherEnableStatus(ret);
    ASSERT_FALSE(GetjsLeakWatcherEnableStatus());
}

} // namespace HiviewDFX
} // namespace OHOS
