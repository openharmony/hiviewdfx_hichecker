#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2024 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import time
import pytest
import re
import os
import subprocess
import colorama
from utils import *

class TestHicheckerjssdk:
    @pytest.mark.L0
    def test_hichecker_leakwatchersdk(self):
        subprocess.check_call("hdc shell rm -rf /data/app/el2/100/base/com.example.myapplication/haps/entry/files", shell=True)
        subprocess.check_call("hdc install testModule/resource/jsleakwatchr.hap", shell=True)
        time.sleep(3)
        subprocess.check_call("hdc shell aa start -a EntryAbility -b com.example.myapplication", shell=True)
        time.sleep(3)
        TouchButtonByText("dump")
        time.sleep(10)
        command = "ls -l /data/app/el2/100/base/com.example.myapplication/haps/entry/files|grep heapsnapshot"
        output = subprocess.check_output(f"hdc shell \"{command}\"", shell=True, text=True, encoding="utf-8")
        assert "heapsnapshot" in output
        filename = output.strip().split(" ")[-1]
        subprocess.check_call(f"hdc file recv /data/app/el2/100/base/com.example.myapplication/haps/entry/files/{filename}", shell=True)
        output = subprocess.check_output(f"certutil -hashfile {filename} SHA256")
        sha256_lower = output.splitlines()[1].decode('utf-8')
        sha256_upper = sha256_lower.upper()

        command = "ls -l /data/app/el2/100/base/com.example.myapplication/haps/entry/files|grep jsleaklist"
        output = subprocess.check_output(f"hdc shell \"{command}\"", shell=True, text=True, encoding="utf-8")
        assert "jsleaklist" in output
        filename = output.strip().split(" ")[-1]

        output = subprocess.check_output(f"hdc shell cat /data/app/el2/100/base/com.example.myapplication/haps/entry/files/{filename}", shell=True, text=True, encoding="utf-8")
        assert sha256_upper in output
        print(sha256_upper)
        subprocess.check_call("hdc uninstall com.example.myapplication", shell=True)