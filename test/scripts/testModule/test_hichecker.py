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

class TestHicheckerjssdk:
    @pytest.mark.L0
    def test_hichecker_installhap(self):
        subprocess.check_call("hdc install testModule/resource/hichecker.hap", shell=True)
        time.sleep(2)
        subprocess.check_call("hdc shell aa start -a EntryAbility -b com.example.myapplication", shell=True)
        time.sleep(2)
        subprocess.check_call("hdc uninstall com.example.myapplication", shell=True)
        time.sleep(2)

    @pytest.mark.L0
    def test_hichecker_hicheckersdk(self):
        process = subprocess.Popen("hdc shell \"hilog | grep HICHECKER | grep NotifySlowProcess\"", stdout=subprocess.PIPE, shell=True, text=True)
        output = ""
        for _ in range(1):
            output += process.stdout.readline()
        assert "NotifySlowProcess" in output
        process.kill()