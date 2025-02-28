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

from hypium import UiDriver
from hypium import BY
from hypium import Point
from hypium.model import UiParam

driver = UiDriver.connect()

class TestHicheckerjssdk:
    @pytest.mark.L0
    def test_hichecker_leakwatchersdk(self):
        subprocess.check_call("hdc install testModule/resource/jsleakwatchr.hap", shell=True)
        time.sleep(3)
        subprocess.check_call("hdc shell aa start -a EntryAbility -b com.example.myapplication", shell=True)
        time.sleep(3)
        driver.touch(BY.text("Dump").type("Button"), mode=UiParam.DOUBLE)
        time.sleep(3)
        command = "ls -l /data/app/el2/100/base/com.example.myapplication/haps/entry/files|grep -e heapsnapshot -e jsleaklist"
        output = subprocess.check_output(f"hdc shell \"{command}\"", shell=True, text=True, encoding="utf-8")
        assert "heapsnapshot" in output
        assert "jsleaklist" in output
        subprocess.check_call("hdc uninstall com.example.myapplication", shell=True)