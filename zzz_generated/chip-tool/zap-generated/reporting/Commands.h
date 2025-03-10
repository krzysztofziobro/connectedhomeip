/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

// THIS FILE IS GENERATED BY ZAP

#pragma once

#include <commands/reporting/ReportingCommand.h>

typedef void (*UnsupportedAttributeCallback)(void * context);

class Listen : public ReportingCommand
{
public:
    Listen() : ReportingCommand("listen") {}

    ~Listen()
    {
        delete onReportBinaryInputBasicPresentValueCallback;
        delete onReportBinaryInputBasicStatusFlagsCallback;
        delete onReportColorControlCurrentHueCallback;
        delete onReportColorControlCurrentSaturationCallback;
        delete onReportColorControlCurrentXCallback;
        delete onReportColorControlCurrentYCallback;
        delete onReportColorControlColorTemperatureCallback;
        delete onReportDoorLockLockStateCallback;
        delete onReportLevelControlCurrentLevelCallback;
        delete onReportOccupancySensingOccupancyCallback;
        delete onReportOnOffOnOffCallback;
        delete onReportPressureMeasurementMeasuredValueCallback;
        delete onReportPumpConfigurationAndControlCapacityCallback;
        delete onReportRelativeHumidityMeasurementMeasuredValueCallback;
        delete onReportSwitchCurrentPositionCallback;
        delete onReportTemperatureMeasurementMeasuredValueCallback;
        delete onReportThermostatLocalTemperatureCallback;
        delete onReportWindowCoveringCurrentPositionLiftPercentageCallback;
        delete onReportWindowCoveringCurrentPositionTiltPercentageCallback;
        delete onReportWindowCoveringOperationalStatusCallback;
        delete onReportWindowCoveringTargetPositionLiftPercent100thsCallback;
        delete onReportWindowCoveringTargetPositionTiltPercent100thsCallback;
        delete onReportWindowCoveringCurrentPositionLiftPercent100thsCallback;
        delete onReportWindowCoveringCurrentPositionTiltPercent100thsCallback;
        delete onReportWindowCoveringSafetyStatusCallback;
    }

    void AddReportCallbacks(uint8_t endpointId) override
    {
        chip::app::CHIPDeviceCallbacksMgr & callbacksMgr = chip::app::CHIPDeviceCallbacksMgr::GetInstance();
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x000F, 0x0055,
                                       onReportBinaryInputBasicPresentValueCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x000F, 0x006F,
                                       onReportBinaryInputBasicStatusFlagsCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0300, 0x0000,
                                       onReportColorControlCurrentHueCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0300, 0x0001,
                                       onReportColorControlCurrentSaturationCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0300, 0x0003,
                                       onReportColorControlCurrentXCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0300, 0x0004,
                                       onReportColorControlCurrentYCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0300, 0x0007,
                                       onReportColorControlColorTemperatureCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0101, 0x0000,
                                       onReportDoorLockLockStateCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0008, 0x0000,
                                       onReportLevelControlCurrentLevelCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0406, 0x0000,
                                       onReportOccupancySensingOccupancyCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0006, 0x0000,
                                       onReportOnOffOnOffCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0403, 0x0000,
                                       onReportPressureMeasurementMeasuredValueCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0200, 0x0013,
                                       onReportPumpConfigurationAndControlCapacityCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0405, 0x0000,
                                       onReportRelativeHumidityMeasurementMeasuredValueCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x003B, 0x0001,
                                       onReportSwitchCurrentPositionCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0402, 0x0000,
                                       onReportTemperatureMeasurementMeasuredValueCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0201, 0x0000,
                                       onReportThermostatLocalTemperatureCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0102, 0x0008,
                                       onReportWindowCoveringCurrentPositionLiftPercentageCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0102, 0x0009,
                                       onReportWindowCoveringCurrentPositionTiltPercentageCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0102, 0x000A,
                                       onReportWindowCoveringOperationalStatusCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0102, 0x000B,
                                       onReportWindowCoveringTargetPositionLiftPercent100thsCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0102, 0x000C,
                                       onReportWindowCoveringTargetPositionTiltPercent100thsCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0102, 0x000E,
                                       onReportWindowCoveringCurrentPositionLiftPercent100thsCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0102, 0x000F,
                                       onReportWindowCoveringCurrentPositionTiltPercent100thsCallback->Cancel());
        callbacksMgr.AddReportCallback(GetExecContext()->storage->GetRemoteNodeId(), endpointId, 0x0102, 0x001A,
                                       onReportWindowCoveringSafetyStatusCallback->Cancel());
    }

    static void OnDefaultSuccessResponse(void * context) { ChipLogProgress(chipTool, "Default Success Response"); }

    static void OnDefaultFailureResponse(void * context, uint8_t status)
    {
        ChipLogProgress(chipTool, "Default Failure Response: 0x%02x", status);
    }

    static void OnUnsupportedAttributeResponse(void * context)
    {
        ChipLogError(chipTool, "Unsupported attribute Response. This should never happen !");
    }

    static void OnBooleanAttributeResponse(void * context, bool value)
    {
        ChipLogProgress(chipTool, "Boolean attribute Response: %d", value);
    }

    static void OnInt8uAttributeResponse(void * context, uint8_t value)
    {
        ChipLogProgress(chipTool, "Int8u attribute Response: %" PRIu8, value);
    }

    static void OnInt16uAttributeResponse(void * context, uint16_t value)
    {
        ChipLogProgress(chipTool, "Int16u attribute Response: %" PRIu16, value);
    }

    static void OnInt16sAttributeResponse(void * context, int16_t value)
    {
        ChipLogProgress(chipTool, "Int16s attribute Response: %" PRId16, value);
    }

private:
    chip::Callback::Callback<BooleanAttributeCallback> * onReportBinaryInputBasicPresentValueCallback =
        new chip::Callback::Callback<BooleanAttributeCallback>(OnBooleanAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportBinaryInputBasicStatusFlagsCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportColorControlCurrentHueCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportColorControlCurrentSaturationCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<Int16uAttributeCallback> * onReportColorControlCurrentXCallback =
        new chip::Callback::Callback<Int16uAttributeCallback>(OnInt16uAttributeResponse, this);
    chip::Callback::Callback<Int16uAttributeCallback> * onReportColorControlCurrentYCallback =
        new chip::Callback::Callback<Int16uAttributeCallback>(OnInt16uAttributeResponse, this);
    chip::Callback::Callback<Int16uAttributeCallback> * onReportColorControlColorTemperatureCallback =
        new chip::Callback::Callback<Int16uAttributeCallback>(OnInt16uAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportDoorLockLockStateCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportLevelControlCurrentLevelCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportOccupancySensingOccupancyCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<BooleanAttributeCallback> * onReportOnOffOnOffCallback =
        new chip::Callback::Callback<BooleanAttributeCallback>(OnBooleanAttributeResponse, this);
    chip::Callback::Callback<Int16sAttributeCallback> * onReportPressureMeasurementMeasuredValueCallback =
        new chip::Callback::Callback<Int16sAttributeCallback>(OnInt16sAttributeResponse, this);
    chip::Callback::Callback<Int16sAttributeCallback> * onReportPumpConfigurationAndControlCapacityCallback =
        new chip::Callback::Callback<Int16sAttributeCallback>(OnInt16sAttributeResponse, this);
    chip::Callback::Callback<Int16uAttributeCallback> * onReportRelativeHumidityMeasurementMeasuredValueCallback =
        new chip::Callback::Callback<Int16uAttributeCallback>(OnInt16uAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportSwitchCurrentPositionCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<Int16sAttributeCallback> * onReportTemperatureMeasurementMeasuredValueCallback =
        new chip::Callback::Callback<Int16sAttributeCallback>(OnInt16sAttributeResponse, this);
    chip::Callback::Callback<Int16sAttributeCallback> * onReportThermostatLocalTemperatureCallback =
        new chip::Callback::Callback<Int16sAttributeCallback>(OnInt16sAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportWindowCoveringCurrentPositionLiftPercentageCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportWindowCoveringCurrentPositionTiltPercentageCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<Int8uAttributeCallback> * onReportWindowCoveringOperationalStatusCallback =
        new chip::Callback::Callback<Int8uAttributeCallback>(OnInt8uAttributeResponse, this);
    chip::Callback::Callback<Int16uAttributeCallback> * onReportWindowCoveringTargetPositionLiftPercent100thsCallback =
        new chip::Callback::Callback<Int16uAttributeCallback>(OnInt16uAttributeResponse, this);
    chip::Callback::Callback<Int16uAttributeCallback> * onReportWindowCoveringTargetPositionTiltPercent100thsCallback =
        new chip::Callback::Callback<Int16uAttributeCallback>(OnInt16uAttributeResponse, this);
    chip::Callback::Callback<Int16uAttributeCallback> * onReportWindowCoveringCurrentPositionLiftPercent100thsCallback =
        new chip::Callback::Callback<Int16uAttributeCallback>(OnInt16uAttributeResponse, this);
    chip::Callback::Callback<Int16uAttributeCallback> * onReportWindowCoveringCurrentPositionTiltPercent100thsCallback =
        new chip::Callback::Callback<Int16uAttributeCallback>(OnInt16uAttributeResponse, this);
    chip::Callback::Callback<Int16uAttributeCallback> * onReportWindowCoveringSafetyStatusCallback =
        new chip::Callback::Callback<Int16uAttributeCallback>(OnInt16uAttributeResponse, this);
};

void registerCommandsReporting(Commands & commands)
{
    const char * clusterName = "Reporting";

    commands_list clusterCommands = {
        make_unique<Listen>(),
    };

    commands.Register(clusterName, clusterCommands);
}
