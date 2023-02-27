/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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

#include "bundle_mgr_proxy.h"
#include "bundle_mgr_interface.h"
#include "if_system_ability_manager.h"
#include "system_ability_definition.h"
#include "iservice_registry.h"
#include "refbase.h"
#include "string"

extern "C" {
bool GetAsanEnabled(const int userId, char *appspawnBundleName)
{
    OHOS::sptr<OHOS::ISystemAbilityManager> smgr = OHOS::SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    OHOS::sptr<OHOS::IRemoteObject> remoteObject = smgr->GetSystemAbility(OHOS::BUNDLE_MGR_SERVICE_SYS_ABILITY_ID);
    OHOS::sptr<OHOS::AppExecFwk::IBundleMgr> bundleMgrProxy(new OHOS::AppExecFwk::BundleMgrProxy(remoteObject));

    std::string bundleName = appspawnBundleName;
	
    OHOS::AppExecFwk::ApplicationInfo appInfo;
    bundleMgrProxy->GetApplicationInfo(bundleName,
                                       OHOS::AppExecFwk::ApplicationFlag::GET_BASIC_APPLICATION_INFO,
                                       userId,
                                       appInfo);
    if (appInfo.asanEnabled == true) {
        return true;
    }
    return false;
}
}
