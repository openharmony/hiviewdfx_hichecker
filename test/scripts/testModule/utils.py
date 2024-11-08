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

import os
import zipfile
import subprocess
import re
import time
import json

OUTPUT_PATH = "testModule"

def GetPathByAttribute(tree, key, value):
    attributes = tree['attributes']
    if attributes is None:
        print("tree contains no attributes")
        return None
    path = []
    if attributes.get(key) == value:
        return path
    for index, child in enumerate(tree['children']):
        child_path = path + [index]
        result = GetPathByAttribute(child, key, value)
        if result is not None:
            return child_path + result
    return None

def GetElementByPath(tree, path):
    if len(path) == 1:
        return tree['children'][path[0]]
    return GetElementByPath(tree['children'][path[0]], path[1:])

def GetLocationByText(tree, text):
    path = GetPathByAttribute(tree, "text", text)
    if path is None or len(path) == 0:
        print(f"text not find in layout file")
    element = GetElementByPath(tree, path)
    locations = element['attributes']['bounds'].replace('[', '').replace(']', ' ').replace(',',' ').strip().split()
    return int((int(locations[0]) + int(locations[2])) / 2), int((int(locations[1]) + int(locations[3])) / 2)

def GetLayoutTree():
    output = subprocess.check_output("hdc shell uitest dumpLayout", text=True)
    path = output.strip().split(":")[-1]
    output = subprocess.check_output(f"hdc file recv {path} {OUTPUT_PATH}/layout.json")
    with open(f"{OUTPUT_PATH}/layout.json", encoding="utf-8") as f:
        tree = json.load(f)
    return tree

def TouchButtonByText(text):
    layoutTree = GetLayoutTree()
    location = GetLocationByText(layoutTree, text)
    output = subprocess.check_output(f"hdc shell uitest uiInput click {location[0]} {location[1]}")