/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "battery_module.h"
#include "battery_impl.h"

namespace OHOS {
namespace ACELite {
using namespace OHOS::ACELite;
void SuccessCallBack(const JSIValue thisVal, const JSIValue args, JSIValue jsiValue)
{
    if (JSI::ValueIsUndefined(args)) {
        return;
    }
    JSIValue success = JSI::GetNamedProperty(args, CB_SUCCESS);
    JSIValue complete = JSI::GetNamedProperty(args, CB_COMPLETE);
    if (!JSI::ValueIsUndefined(success)) {
        if (JSI::ValueIsUndefined(jsiValue)) {
            JSI::CallFunction(success, thisVal, nullptr, 0);
        } else {
            JSI::CallFunction(success, thisVal, &jsiValue, ARGC_ONE);
        }
    }
    if (!JSI::ValueIsUndefined(complete)) {
        JSI::CallFunction(complete, thisVal, nullptr, 0);
    }
    JSI::ReleaseValueList(success, complete, ARGS_END);
}

JSIValue BatteryModule::GetStatus(const JSIValue thisVal, const JSIValue* args, uint8_t argsNum)
{
    JSIValue undefValue = JSI::CreateUndefined();
    if ((args == nullptr) || (argsNum == 0) || JSI::ValueIsUndefined(args[0])) {
        return undefValue;
    }

    double level = 0;
    bool charging = false;
    (void)GetBatteryStatus(&charging, &level);
    JSIValue result = JSI::CreateObject();
    JSI::SetBooleanProperty(result, "charging", charging);
    JSI::SetNumberProperty(result, "level", level);
    SuccessCallBack(thisVal, args[0], result);
    JSI::ReleaseValue(result);
    return undefValue;
}

void InitBatteryModule(JSIValue exports)
{
    JSI::SetModuleAPI(exports, "getStatus", BatteryModule::GetStatus);
}
} // namespace ACELite
} // namespace OHOS
