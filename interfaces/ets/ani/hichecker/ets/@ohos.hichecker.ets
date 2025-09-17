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

export default namespace hichecker {
  loadLibraryWithPermissionCheck("hichecker_ani", "@ohos.hichecker");
  const RULE_CAUTION_PRINT_LOG: bigint = 9223372036854775808n; // 1 << 63
  const RULE_CAUTION_TRIGGER_CRASH: bigint = 4611686018427387904n; // 1 << 62
  const RULE_THREAD_CHECK_SLOW_PROCESS: bigint = 1n;
  const RULE_CHECK_SLOW_EVENT: bigint = 4294967296n; // 1 << 32
  const RULE_CHECK_ABILITY_CONNECTION_LEAK: bigint = 8589934592n; // 1 << 33
  const RULE_CHECK_ARKUI_PERFORMANCE: bigint = 17179869184n; // 1 << 34

  native function addRule(rule: bigint): void;
  native function removeRule(rule: bigint): void;
  native function getRule(): bigint;
  native function contains(rule: bigint): boolean;
  native function addCheckRule(rule: bigint): void;
  native function removeCheckRule(rule: bigint): void;
  native function containsCheckRule(rule: bigint): boolean;
}
