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
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "event_handler.h"
#include "event_runner.h"
#include "js_leak_watcher_napi.h"

using namespace testing::ext;
using namespace OHOS::AppExecFwk;

namespace {
    const std::string TEST_FILE_PATH = "/data/test_js_leak_watcher.txt";
    const std::string TEST_META_FILE_PATH = "/data/test_metadata.json";
    const std::string INVALID_FILE_PATH = "/invalid/path/test.txt";
}

namespace OHOS {
namespace HiviewDFX {

class JsLeakWatcherNapiTest : public testing::Test {
public:
    static void SetUpTestCase() {};
    static void TearDownTestCase() {};
    void SetUp();
    void TearDown();

private:
    void CleanupTestFiles();
};

void JsLeakWatcherNapiTest::SetUp(void)
{
    CleanupTestFiles();
}

void JsLeakWatcherNapiTest::TearDown(void)
{
    CleanupTestFiles();
}

void JsLeakWatcherNapiTest::CleanupTestFiles(void)
{
    if (access(TEST_FILE_PATH.c_str(), F_OK) == 0) {
        unlink(TEST_FILE_PATH.c_str());
    }
    if (access(TEST_META_FILE_PATH.c_str(), F_OK) == 0) {
        unlink(TEST_META_FILE_PATH.c_str());
    }
}

/**
 * @tc.name: SetDumpDelayTimeTest001
 * @tc.desc: test SetDumpDelayTime function with valid delay time
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetDumpDelayTimeTest001, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    
    uint32_t testDelay = 5000;
    handler->SetDumpDelayTime(testDelay);
    ASSERT_EQ(handler->GetDumpDelayTime(), testDelay);
}

/**
 * @tc.name: SetDumpDelayTimeTest002
 * @tc.desc: test SetDumpDelayTime function with zero delay time
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetDumpDelayTimeTest002, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    
    uint32_t testDelay = 0;
    handler->SetDumpDelayTime(testDelay);
    ASSERT_EQ(handler->GetDumpDelayTime(), testDelay);
}

/**
 * @tc.name: SetDumpDelayTimeTest003
 * @tc.desc: test SetDumpDelayTime function with large delay time
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetDumpDelayTimeTest003, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    
    uint32_t testDelay = 60000;
    handler->SetDumpDelayTime(testDelay);
    ASSERT_EQ(handler->GetDumpDelayTime(), testDelay);
}

/**
 * @tc.name: SetGcDelayTimeTest001
 * @tc.desc: test SetGcDelayTime function with valid delay time
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetGcDelayTimeTest001, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    
    uint32_t testDelay = 3000;
    handler->SetGcDelayTime(testDelay);
    ASSERT_EQ(handler->GetGcDelayTime(), testDelay);
}

/**
 * @tc.name: SetGcDelayTimeTest002
 * @tc.desc: test SetGcDelayTime function with zero delay time
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetGcDelayTimeTest002, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    
    uint32_t testDelay = 0;
    handler->SetGcDelayTime(testDelay);
    ASSERT_EQ(handler->GetGcDelayTime(), testDelay);
}

/**
 * @tc.name: SetGcDelayTimeTest003
 * @tc.desc: test SetGcDelayTime function with large delay time
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetGcDelayTimeTest003, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    
    uint32_t testDelay = 60000;
    handler->SetGcDelayTime(testDelay);
    ASSERT_EQ(handler->GetGcDelayTime(), testDelay);
}

/**
 * @tc.name: SetJsLeakWatcherStatusTest001
 * @tc.desc: test SetJsLeakWatcherStatus function with true
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetJsLeakWatcherStatusTest001, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    
    handler->SetJsLeakWatcherStatus(true);
    ASSERT_TRUE(handler->GetJsLeakWatcherStatus());
}

/**
 * @tc.name: SetJsLeakWatcherStatusTest002
 * @tc.desc: test SetJsLeakWatcherStatus function with false
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetJsLeakWatcherStatusTest002, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    handler->SetJsLeakWatcherStatus(false);
    ASSERT_FALSE(handler->GetJsLeakWatcherStatus());
}

/**
 * @tc.name: SetJsLeakWatcherStatusTest003
 * @tc.desc: test SetJsLeakWatcherStatus function toggle status
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetJsLeakWatcherStatusTest003, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    handler->SetJsLeakWatcherStatus(true);
    ASSERT_TRUE(handler->GetJsLeakWatcherStatus());
    handler->SetJsLeakWatcherStatus(false);
    ASSERT_FALSE(handler->GetJsLeakWatcherStatus());
    
    handler->SetJsLeakWatcherStatus(true);
    ASSERT_TRUE(handler->GetJsLeakWatcherStatus());
}

/**
 * @tc.name: CreateFileTest001
 * @tc.desc: test CreateFile function with valid path
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, CreateFileTest001, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    bool result = TestCreateFile(filePath);
    ASSERT_TRUE(result);
    ASSERT_EQ(access(filePath.c_str(), F_OK), 0);
}

/**
 * @tc.name: CreateFileTest002
 * @tc.desc: test CreateFile function with existing file
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, CreateFileTest002, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    bool result = TestCreateFile(filePath);
    ASSERT_TRUE(result);
    result = TestCreateFile(filePath);
    ASSERT_TRUE(result);
}

/**
 * @tc.name: CreateFileTest003
 * @tc.desc: test CreateFile function with invalid path
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, CreateFileTest003, TestSize.Level1)
{
    std::string filePath = INVALID_FILE_PATH;
    bool result = TestCreateFile(filePath);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: GetFileSizeTest001
 * @tc.desc: test GetFileSize function with existing file
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, GetFileSizeTest001, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    std::string testContent = "test content for file size";
    std::ofstream file(filePath);
    ASSERT_TRUE(file.is_open());
    file << testContent;
    file.close();
    uint64_t fileSize = TestGetFileSize(filePath);
    ASSERT_GE(fileSize, testContent.length());
}

/**
 * @tc.name: GetFileSizeTest002
 * @tc.desc: test GetFileSize function with empty file
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, GetFileSizeTest002, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    std::ofstream file(filePath);
    ASSERT_TRUE(file.is_open());
    file.close();
    uint64_t fileSize = TestGetFileSize(filePath);
    ASSERT_EQ(fileSize, 0);
}

/**
 * @tc.name: GetFileSizeTest003
 * @tc.desc: test GetFileSize function with non-existing file
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, GetFileSizeTest003, TestSize.Level1)
{
    std::string filePath = INVALID_FILE_PATH;
    uint64_t fileSize = TestGetFileSize(filePath);
    ASSERT_EQ(fileSize, 0);
}

/**
 * @tc.name: GetFileSizeTest004
 * @tc.desc: test GetFileSize function with large file
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, GetFileSizeTest004, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    std::string largeContent(4096, 'a');
    std::ofstream file(filePath);
    ASSERT_TRUE(file.is_open());
    file << largeContent;
    file.close();
    
    uint64_t fileSize = TestGetFileSize(filePath);
    ASSERT_GE(fileSize, largeContent.length());
}

/**
 * @tc.name: AppendMetaDataTest001
 * @tc.desc: test AppendMetaData function with valid files
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, AppendMetaDataTest001, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    std::ofstream targetFile(filePath);
    ASSERT_TRUE(targetFile.is_open());
    targetFile << "test heap dump content";
    targetFile.close();
    std::string metaDataPath = TEST_META_FILE_PATH;
    std::ofstream metaFile(metaDataPath);
    ASSERT_TRUE(metaFile.is_open());
    metaFile << "{\"test\": \"metadata\"}";
    metaFile.close();
    bool result = TestAppendMetaData(filePath);
    EXPECT_FALSE(result);
}

/**
 * @tc.name: AppendMetaDataTest002
 * @tc.desc: test AppendMetaData function with non-existing target file
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, AppendMetaDataTest002, TestSize.Level1)
{
    std::string filePath = INVALID_FILE_PATH;
    bool result = TestAppendMetaData(filePath);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: AppendMetaDataTest003
 * @tc.desc: test AppendMetaData function with existing target file
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, AppendMetaDataTest003, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    std::ofstream targetFile(filePath);
    ASSERT_TRUE(targetFile.is_open());
    targetFile << "test heap dump content";
    targetFile.close();
    uint64_t initialSize = TestGetFileSize(filePath);
    bool result = TestAppendMetaData(filePath);
    if (result) {
        uint64_t finalSize = TestGetFileSize(filePath);
        ASSERT_GT(finalSize, initialSize);
    } else {
        uint64_t finalSize = TestGetFileSize(filePath);
        ASSERT_EQ(finalSize, initialSize);
    }
}

/**
 * @tc.name: AppendMetaDataTest004
 * @tc.desc: test AppendMetaData function with empty file
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, AppendMetaDataTest004, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    std::ofstream targetFile(filePath);
    ASSERT_TRUE(targetFile.is_open());
    targetFile.close();
    uint64_t initialSize = TestGetFileSize(filePath);
    ASSERT_EQ(initialSize, 0);
    bool result = TestAppendMetaData(filePath);
    if (result) {
        uint64_t finalSize = TestGetFileSize(filePath);
        ASSERT_GT(finalSize, initialSize);
    }
}

} // namespace HiviewDFX
} // namespace OHOS
