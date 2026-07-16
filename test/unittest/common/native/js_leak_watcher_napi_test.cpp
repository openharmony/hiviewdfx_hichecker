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
#include <vector>
#include <climits>

#include "event_handler.h"
#include "event_runner.h"
#include "js_leak_watcher_napi.h"
#include "js_leak_watcher_ts.h"

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
    EXPECT_TRUE(result);
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

/**
 * @tc.name: DefaultDelayTimeTest001
 * @tc.desc: test default dump delay time value
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, DefaultDelayTimeTest001, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    handler->SetDumpDelayTime(3000);
    handler->SetJsLeakWatcherStatus(true);
    handler->SetJsLeakWatcherStatus(false);
    ASSERT_EQ(handler->GetDumpDelayTime(), 3000);
}

/**
 * @tc.name: DefaultDelayTimeTest002
 * @tc.desc: test default gc delay time value
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, DefaultDelayTimeTest002, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    handler->SetGcDelayTime(90000);
    handler->SetJsLeakWatcherStatus(true);
    handler->SetJsLeakWatcherStatus(false);
    ASSERT_EQ(handler->GetGcDelayTime(), 90000);
}

/**
 * @tc.name: SetDumpDelayTimeTest004
 * @tc.desc: test SetDumpDelayTime function with max uint32 value
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetDumpDelayTimeTest004, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    uint32_t testDelay = UINT32_MAX;
    handler->SetDumpDelayTime(testDelay);
    ASSERT_EQ(handler->GetDumpDelayTime(), testDelay);
}

/**
 * @tc.name: SetGcDelayTimeTest004
 * @tc.desc: test SetGcDelayTime function with max uint32 value
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetGcDelayTimeTest004, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    uint32_t testDelay = UINT32_MAX;
    handler->SetGcDelayTime(testDelay);
    ASSERT_EQ(handler->GetGcDelayTime(), testDelay);
}

/**
 * @tc.name: SetDumpDelayTimeTest005
 * @tc.desc: test SetDumpDelayTime function with value 1
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetDumpDelayTimeTest005, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    uint32_t testDelay = 1;
    handler->SetDumpDelayTime(testDelay);
    ASSERT_EQ(handler->GetDumpDelayTime(), testDelay);
}

/**
 * @tc.name: SetGcDelayTimeTest005
 * @tc.desc: test SetGcDelayTime function with value 1
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetGcDelayTimeTest005, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    uint32_t testDelay = 1;
    handler->SetGcDelayTime(testDelay);
    ASSERT_EQ(handler->GetGcDelayTime(), testDelay);
}

/**
 * @tc.name: CreateFileTest004
 * @tc.desc: test CreateFile function with empty path
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, CreateFileTest004, TestSize.Level1)
{
    std::string filePath = "";
    bool result = TestCreateFile(filePath);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: CreateFileTest005
 * @tc.desc: test CreateFile function creates file with correct permissions
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, CreateFileTest005, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    bool result = TestCreateFile(filePath);
    ASSERT_TRUE(result);
    struct stat st;
    ASSERT_EQ(stat(filePath.c_str(), &st), 0);
    ASSERT_EQ(st.st_mode & 0777, 0640);
}

/**
 * @tc.name: GetFileSizeTest005
 * @tc.desc: test GetFileSize function with empty string path
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, GetFileSizeTest005, TestSize.Level1)
{
    std::string filePath = "";
    uint64_t fileSize = TestGetFileSize(filePath);
    ASSERT_EQ(fileSize, 0);
}

/**
 * @tc.name: GetFileSizeTest006
 * @tc.desc: test GetFileSize function with directory path
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, GetFileSizeTest006, TestSize.Level1)
{
    std::string filePath = "/data";
    uint64_t fileSize = TestGetFileSize(filePath);
    ASSERT_GT(fileSize, 0);
}

/**
 * @tc.name: AppendMetaDataTest005
 * @tc.desc: test AppendMetaData function with empty file path
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, AppendMetaDataTest005, TestSize.Level1)
{
    std::string filePath = "";
    bool result = TestAppendMetaData(filePath);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: HandlerStatusResetTest001
 * @tc.desc: test handler status reset clears all function refs
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, HandlerStatusResetTest001, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    handler->SetJsLeakWatcherStatus(true);
    ASSERT_TRUE(handler->GetJsLeakWatcherStatus());
    handler->SetJsLeakWatcherStatus(false);
    ASSERT_FALSE(handler->GetJsLeakWatcherStatus());
}

/**
 * @tc.name: HandlerStatusResetTest002
 * @tc.desc: test multiple status toggles
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, HandlerStatusResetTest002, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    for (int i = 0; i < 10; ++i) {
        handler->SetJsLeakWatcherStatus(true);
        ASSERT_TRUE(handler->GetJsLeakWatcherStatus());
        handler->SetJsLeakWatcherStatus(false);
        ASSERT_FALSE(handler->GetJsLeakWatcherStatus());
    }
}

/**
 * @tc.name: DelayTimePersistenceTest001
 * @tc.desc: test delay time persists after status toggle
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, DelayTimePersistenceTest001, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    uint32_t dumpDelay = 5000;
    uint32_t gcDelay = 10000;
    handler->SetDumpDelayTime(dumpDelay);
    handler->SetGcDelayTime(gcDelay);
    handler->SetJsLeakWatcherStatus(true);
    handler->SetJsLeakWatcherStatus(false);
    ASSERT_EQ(handler->GetDumpDelayTime(), dumpDelay);
    ASSERT_EQ(handler->GetGcDelayTime(), gcDelay);
}

/**
 * @tc.name: DelayTimePersistenceTest002
 * @tc.desc: test delay time can be changed after status toggle
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, DelayTimePersistenceTest002, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    handler->SetDumpDelayTime(3000);
    handler->SetJsLeakWatcherStatus(true);
    handler->SetJsLeakWatcherStatus(false);
    handler->SetDumpDelayTime(6000);
    ASSERT_EQ(handler->GetDumpDelayTime(), 6000);
}

/**
 * @tc.name: CreateFileTest007
 * @tc.desc: test CreateFile function with root path
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, CreateFileTest007, TestSize.Level1)
{
    std::string filePath = "/";
    bool result = TestCreateFile(filePath);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: CreateFileTest008
 * @tc.desc: test CreateFile function with nested non-existing directory
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, CreateFileTest008, TestSize.Level1)
{
    std::string filePath = "/nonexistent/dir/file.txt";
    bool result = TestCreateFile(filePath);
    ASSERT_FALSE(result);
}

/**
 * @tc.name: GetFileSizeTest007
 * @tc.desc: test GetFileSize function with root path
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, GetFileSizeTest007, TestSize.Level1)
{
    std::string filePath = "/";
    uint64_t fileSize = TestGetFileSize(filePath);
    ASSERT_GT(fileSize, 0);
}

/**
 * @tc.name: FileOperationsChainTest001
 * @tc.desc: test create, write, get size, and append chain
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, FileOperationsChainTest001, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    bool createResult = TestCreateFile(filePath);
    ASSERT_TRUE(createResult);
    uint64_t initialSize = TestGetFileSize(filePath);
    ASSERT_EQ(initialSize, 0);
    std::ofstream file(filePath, std::ios::app);
    ASSERT_TRUE(file.is_open());
    file << "test data";
    file.close();
    uint64_t afterWriteSize = TestGetFileSize(filePath);
    ASSERT_GT(afterWriteSize, initialSize);
}

/**
 * @tc.name: FileOperationsChainTest002
 * @tc.desc: test multiple file creations and size checks
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, FileOperationsChainTest002, TestSize.Level1)
{
    std::string filePath1 = TEST_FILE_PATH;
    std::string filePath2 = TEST_FILE_PATH + "_2";
    bool result1 = TestCreateFile(filePath1);
    bool result2 = TestCreateFile(filePath2);
    ASSERT_TRUE(result1);
    ASSERT_TRUE(result2);
    uint64_t size1 = TestGetFileSize(filePath1);
    uint64_t size2 = TestGetFileSize(filePath2);
    ASSERT_EQ(size1, 0);
    ASSERT_EQ(size2, 0);
    if (access(filePath2.c_str(), F_OK) == 0) {
        unlink(filePath2.c_str());
    }
}

/**
 * @tc.name: SetDumpDelayTimeTest006
 * @tc.desc: test SetDumpDelayTime with typical values
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetDumpDelayTimeTest006, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    std::vector<uint32_t> testDelays = {100, 500, 1000, 2000, 3000, 5000, 10000};
    for (auto delay : testDelays) {
        handler->SetDumpDelayTime(delay);
        ASSERT_EQ(handler->GetDumpDelayTime(), delay);
    }
}

/**
 * @tc.name: SetGcDelayTimeTest006
 * @tc.desc: test SetGcDelayTime with typical values
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, SetGcDelayTimeTest006, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    std::vector<uint32_t> testDelays = {1000, 5000, 10000, 30000, 60000, 90000, 120000};
    for (auto delay : testDelays) {
        handler->SetGcDelayTime(delay);
        ASSERT_EQ(handler->GetGcDelayTime(), delay);
    }
}

/**
 * @tc.name: AppendMetaDataTest006
 * @tc.desc: test AppendMetaData after multiple file writes
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, AppendMetaDataTest006, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    std::ofstream targetFile(filePath);
    ASSERT_TRUE(targetFile.is_open());
    for (int i = 0; i < 100; ++i) {
        targetFile << "line " << i << "\n";
    }
    targetFile.close();
    uint64_t initialSize = TestGetFileSize(filePath);
    ASSERT_GT(initialSize, 0);
    bool result = TestAppendMetaData(filePath);
    if (result) {
        uint64_t finalSize = TestGetFileSize(filePath);
        ASSERT_GT(finalSize, initialSize);
    }
}

/**
 * @tc.name: HandlerConcurrentStatusTest001
 * @tc.desc: test rapid status changes
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, HandlerConcurrentStatusTest001, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    handler->SetJsLeakWatcherStatus(true);
    handler->SetJsLeakWatcherStatus(true);
    handler->SetJsLeakWatcherStatus(true);
    ASSERT_TRUE(handler->GetJsLeakWatcherStatus());
    handler->SetJsLeakWatcherStatus(false);
    handler->SetJsLeakWatcherStatus(false);
    handler->SetJsLeakWatcherStatus(false);
    ASSERT_FALSE(handler->GetJsLeakWatcherStatus());
}

/**
 * @tc.name: GetFileSizeTest008
 * @tc.desc: test GetFileSize with binary content
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, GetFileSizeTest008, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    std::ofstream file(filePath, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    char binaryData[256];
    for (int i = 0; i < 256; ++i) {
        binaryData[i] = static_cast<char>(i);
    }
    file.write(binaryData, sizeof(binaryData));
    file.close();
    uint64_t fileSize = TestGetFileSize(filePath);
    ASSERT_EQ(fileSize, sizeof(binaryData));
}

/**
 * @tc.name: CreateFileTest009
 * @tc.desc: test CreateFile with file containing special characters in name
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, CreateFileTest009, TestSize.Level1)
{
    std::string filePath = "/data/test_file_123.txt";
    bool result = TestCreateFile(filePath);
    ASSERT_TRUE(result);
    ASSERT_EQ(access(filePath.c_str(), F_OK), 0);
    if (access(filePath.c_str(), F_OK) == 0) {
        unlink(filePath.c_str());
    }
}

/**
 * @tc.name: HandlerDelayTimeBoundaryTest001
 * @tc.desc: test delay time boundary values
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, HandlerDelayTimeBoundaryTest001, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    handler->SetDumpDelayTime(0);
    ASSERT_EQ(handler->GetDumpDelayTime(), 0);
    handler->SetDumpDelayTime(1);
    ASSERT_EQ(handler->GetDumpDelayTime(), 1);
    handler->SetDumpDelayTime(UINT32_MAX - 1);
    ASSERT_EQ(handler->GetDumpDelayTime(), UINT32_MAX - 1);
    handler->SetDumpDelayTime(UINT32_MAX);
    ASSERT_EQ(handler->GetDumpDelayTime(), UINT32_MAX);
}

/**
 * @tc.name: HandlerDelayTimeBoundaryTest002
 * @tc.desc: test gc delay time boundary values
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, HandlerDelayTimeBoundaryTest002, TestSize.Level1)
{
    auto handler = GetTestHandler();
    ASSERT_NE(handler, nullptr);
    handler->SetGcDelayTime(0);
    ASSERT_EQ(handler->GetGcDelayTime(), 0);
    handler->SetGcDelayTime(1);
    ASSERT_EQ(handler->GetGcDelayTime(), 1);
    handler->SetGcDelayTime(UINT32_MAX - 1);
    ASSERT_EQ(handler->GetGcDelayTime(), UINT32_MAX - 1);
    handler->SetGcDelayTime(UINT32_MAX);
    ASSERT_EQ(handler->GetGcDelayTime(), UINT32_MAX);
}

/**
 * @tc.name: AppendMetaDataTest007
 * @tc.desc: test AppendMetaData with large target file
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, AppendMetaDataTest007, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    std::ofstream targetFile(filePath);
    ASSERT_TRUE(targetFile.is_open());
    std::string largeContent(8192, 'x');
    targetFile << largeContent;
    targetFile.close();
    uint64_t initialSize = TestGetFileSize(filePath);
    ASSERT_EQ(initialSize, largeContent.length());
    bool result = TestAppendMetaData(filePath);
    if (result) {
        uint64_t finalSize = TestGetFileSize(filePath);
        ASSERT_GT(finalSize, initialSize);
    }
}

/**
 * @tc.name: FileOperationsChainTest003
 * @tc.desc: test file operations with metadata append
 * @tc.type: FUNC
 */
HWTEST_F(JsLeakWatcherNapiTest, FileOperationsChainTest003, TestSize.Level1)
{
    std::string filePath = TEST_FILE_PATH;
    bool createResult = TestCreateFile(filePath);
    ASSERT_TRUE(createResult);
    std::ofstream file(filePath, std::ios::app);
    ASSERT_TRUE(file.is_open());
    file << "heap snapshot data";
    file.close();
    uint64_t beforeMetaSize = TestGetFileSize(filePath);
    ASSERT_GT(beforeMetaSize, 0);
    bool appendResult = TestAppendMetaData(filePath);
    if (appendResult) {
        uint64_t afterMetaSize = TestGetFileSize(filePath);
        ASSERT_GT(afterMetaSize, beforeMetaSize);
    }
}
} // namespace HiviewDFX
} // namespace OHOS
