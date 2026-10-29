'''
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
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
'''

import os
import sys

def check_scons():
    print("checking scons...")
    scons = os.system("scons --version")
    if scons == 0:
        print("installed scons!")
    else:
        print("scons is not installed!")
        subprocess.run(["pip", "install", "../../../../../tools/thirdparty/SCons-4.4.0-py3-none-any.whl", "--ignore-installed"])

root = sys.argv[1]

if os.path.exists("../../../../../tools"):
    os.chdir("../../../../device/hisilicon/hispark_pegasus/sdk_liteos")
    os.system(r"..\..\..\..\..\tools\thirdparty\Git\bin\sh hm_build.sh " + root + " win")
    check_scons()
else:
    os.chdir("../../../../device/hisilicon/hispark_pegasus/sdk_liteos")
    os.system(r"sh hm_build.sh " + root + " win")