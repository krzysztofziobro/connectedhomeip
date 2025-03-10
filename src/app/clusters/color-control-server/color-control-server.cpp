/**
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

/**
 *
 *    Copyright (c) 2020 Silicon Labs
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
/****************************************************************************
 * @file
 * @brief Routines for the Color Control Server
 *plugin.
 *******************************************************************************
 ******************************************************************************/
#include "color-control-server.h"

#include <app/util/af.h>

#include <app-common/zap-generated/af-structs.h>
#include <app-common/zap-generated/attribute-id.h>
#include <app-common/zap-generated/attribute-type.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/cluster-id.h>
#include <app/CommandHandler.h>
#include <app/reporting/reporting.h>
#include <app/util/af-event.h>
#include <app/util/attribute-storage.h>

using namespace chip;
using namespace app::Clusters::ColorControl;

#define COLOR_TEMP_CONTROL emberAfPluginColorControlServerTempTransitionEventControl
#define COLOR_XY_CONTROL emberAfPluginColorControlServerXyTransitionEventControl
#define COLOR_HSV_CONTROL emberAfPluginColorControlServerHueSatTransitionEventControl

// move mode
enum
{
    MOVE_MODE_STOP = 0x00,
    MOVE_MODE_UP   = 0x01,
    MOVE_MODE_DOWN = 0x03
};

enum
{
    COLOR_MODE_HSV         = 0x00,
    COLOR_MODE_CIE_XY      = 0x01,
    COLOR_MODE_TEMPERATURE = 0x02
};

enum
{
    HSV_TO_HSV                 = 0x00,
    HSV_TO_CIE_XY              = 0x01,
    HSV_TO_TEMPERATURE         = 0x02,
    CIE_XY_TO_HSV              = 0x10,
    CIE_XY_TO_CIE_XY           = 0x11,
    CIE_XY_TO_TEMPERATURE      = 0x12,
    TEMPERATURE_TO_HSV         = 0x20,
    TEMPERATURE_TO_CIE_XY      = 0x21,
    TEMPERATURE_TO_TEMPERATURE = 0x22
};

EmberEventControl emberAfPluginColorControlServerTempTransitionEventControl;
EmberEventControl emberAfPluginColorControlServerXyTransitionEventControl;
EmberEventControl emberAfPluginColorControlServerHueSatTransitionEventControl;

#define UPDATE_TIME_MS 100
#define TRANSITION_TIME_1S 10
#define MIN_CIE_XY_VALUE 0
// this value comes directly from the ZCL specification table 5.3
#define MAX_CIE_XY_VALUE 0xfeff
#define MIN_TEMPERATURE_VALUE 0
#define MAX_TEMPERATURE_VALUE 0xfeff
#define MIN_HUE_VALUE 0
#define MAX_HUE_VALUE 254
#define MIN_SATURATION_VALUE 0
#define MAX_SATURATION_VALUE 254
#define HALF_MAX_UINT8T 127
#define HALF_MAX_UINT16T 0x7FFF

#define MAX_ENHANCED_HUE_VALUE 0xFFFF

#define MIN_CURRENT_LEVEL 0x01
#define MAX_CURRENT_LEVEL 0xFE

#define REPORT_FAILED 0xFF

typedef struct
{
    uint8_t initialHue;
    uint8_t currentHue;
    uint8_t finalHue;
    uint16_t stepsRemaining;
    uint16_t stepsTotal;
    uint16_t initialEnhancedHue;
    uint16_t currentEnhancedHue;
    uint16_t finalEnhancedHue;
    EndpointId endpoint;
    bool up;
    bool repeat;
    bool isEnhancedHue;
} ColorHueTransitionState;

static ColorHueTransitionState colorHueTransitionState;

typedef struct
{
    uint16_t initialValue;
    uint16_t currentValue;
    uint16_t finalValue;
    uint16_t stepsRemaining;
    uint16_t stepsTotal;
    uint16_t lowLimit;
    uint16_t highLimit;
    EndpointId endpoint;
} Color16uTransitionState;

static Color16uTransitionState colorXTransitionState;
static Color16uTransitionState colorYTransitionState;

static Color16uTransitionState colorTempTransitionState;

static Color16uTransitionState colorSaturationTransitionState;

// Forward declarations:
static bool computeNewColor16uValue(Color16uTransitionState * p);
static void stopAllColorTransitions(void);
static void handleModeSwitch(EndpointId endpoint, uint8_t newColorMode);
static bool shouldExecuteIfOff(EndpointId endpoint, uint8_t optionMask, uint8_t optionOverride);

#ifdef EMBER_AF_PLUGIN_COLOR_CONTROL_SERVER_HSV
static uint8_t addHue(uint8_t hue1, uint8_t hue2);
static uint8_t subtractHue(uint8_t hue1, uint8_t hue2);
static uint8_t addSaturation(uint8_t saturation1, uint8_t saturation2);
static uint8_t subtractSaturation(uint8_t saturation1, uint8_t saturation2);
static void initHueSat(EndpointId endpoint);
static uint8_t readHue(EndpointId endpoint);
static uint8_t readSaturation(EndpointId endpoint);
static uint16_t addEnhancedHue(uint16_t hue1, uint16_t hue2);
static uint16_t subtractEnhancedHue(uint16_t hue1, uint16_t hue2);
static uint16_t readEnhancedHue(EndpointId endpoint);

static bool moveHue(uint8_t moveMode, uint16_t rate, uint8_t optionsMask, uint8_t optionsOverride, bool isEnhanced);
static bool moveToHue(uint16_t hue, uint8_t hueMoveMode, uint16_t transitionTime, uint8_t optionsMask, uint8_t optionsOverride,
                      bool isEnhanced);
static bool moveToHueAndSaturation(uint16_t hue, uint8_t saturation, uint16_t transitionTime, uint8_t optionsMask,
                                   uint8_t optionsOverride, bool isEnhanced);
static bool stepHue(uint8_t stepMode, uint16_t stepSize, uint16_t transitionTime, uint8_t optionsMask, uint8_t optionsOverride,
                    bool isEnhanced);
#endif

#ifdef EMBER_AF_PLUGIN_COLOR_CONTROL_SERVER_XY
static uint16_t findNewColorValueFromStep(uint16_t oldValue, int16_t step);
static uint16_t readColorX(EndpointId endpoint);
static uint16_t readColorY(EndpointId endpoint);
#endif

static uint16_t computeTransitionTimeFromStateAndRate(Color16uTransitionState * p, uint16_t rate);

// convenient token handling functions
static uint8_t readColorMode(EndpointId endpoint)
{
    uint8_t colorMode;
    emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID,
                               reinterpret_cast<uint8_t *>(&colorMode), sizeof(uint8_t));
    return colorMode;
}

static uint16_t readColorTemperature(EndpointId endpoint)
{
    uint16_t colorTemperature;
    emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                               reinterpret_cast<uint8_t *>(&colorTemperature), sizeof(uint16_t));

    return colorTemperature;
}

static uint16_t readColorTemperatureMin(EndpointId endpoint)
{
    uint16_t colorTemperatureMin;
    EmberAfStatus status;

    status =
        emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_ATTRIBUTE_ID,
                                   reinterpret_cast<uint8_t *>(&colorTemperatureMin), sizeof(uint16_t));

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        colorTemperatureMin = MIN_TEMPERATURE_VALUE;
    }

    return colorTemperatureMin;
}

static uint16_t readColorTemperatureMax(EndpointId endpoint)
{
    uint16_t colorTemperatureMax;
    EmberAfStatus status;

    status =
        emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_ATTRIBUTE_ID,
                                   reinterpret_cast<uint8_t *>(&colorTemperatureMax), sizeof(uint16_t));

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        colorTemperatureMax = MAX_TEMPERATURE_VALUE;
    }

    return colorTemperatureMax;
}

static uint16_t readColorTemperatureCoupleToLevelMin(EndpointId endpoint)
{
    uint16_t colorTemperatureCoupleToLevelMin;
    EmberAfStatus status;

    status = emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID,
                                        ZCL_COLOR_CONTROL_TEMPERATURE_LEVEL_MIN_MIREDS_ATTRIBUTE_ID,
                                        reinterpret_cast<uint8_t *>(&colorTemperatureCoupleToLevelMin), sizeof(uint16_t));

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        // Not less than the physical min.
        colorTemperatureCoupleToLevelMin = readColorTemperatureMin(endpoint);
    }

    return colorTemperatureCoupleToLevelMin;
}

static uint8_t readLevelControlCurrentLevel(EndpointId endpoint)
{
    uint8_t currentLevel;
    EmberAfStatus status;

    status = emberAfReadServerAttribute(endpoint, ZCL_LEVEL_CONTROL_CLUSTER_ID, ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                        reinterpret_cast<uint8_t *>(&currentLevel), sizeof(uint8_t));

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        currentLevel = 0x7F; // midpoint of range 0x01-0xFE
    }

    return currentLevel;
}

static void writeRemainingTime(EndpointId endpoint, uint16_t remainingTime)
{
    emberAfWriteServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_REMAINING_TIME_ATTRIBUTE_ID,
                                reinterpret_cast<uint8_t *>(&remainingTime), ZCL_INT16U_ATTRIBUTE_TYPE);
}

static void writeColorMode(EndpointId endpoint, uint8_t colorMode)
{
    emberAfWriteServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_ENHANCED_COLOR_MODE_ATTRIBUTE_ID,
                                reinterpret_cast<uint8_t *>(&colorMode), ZCL_INT8U_ATTRIBUTE_TYPE);

    emberAfWriteServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID,
                                reinterpret_cast<uint8_t *>(&colorMode), ZCL_INT8U_ATTRIBUTE_TYPE);
}

static void writeHue(EndpointId endpoint, uint8_t hue)
{
    emberAfWriteServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_HUE_ATTRIBUTE_ID,
                                reinterpret_cast<uint8_t *>(&hue), ZCL_INT8U_ATTRIBUTE_TYPE);
}

static void writeEnhancedHue(EndpointId endpoint, uint16_t hue)
{
    emberAfWriteServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_ENHANCED_CURRENT_HUE_ATTRIBUTE_ID,
                                reinterpret_cast<uint8_t *>(&hue), ZCL_INT16U_ATTRIBUTE_TYPE);

    writeHue(endpoint, static_cast<uint8_t>(hue >> 8));
}

static void writeSaturation(EndpointId endpoint, uint8_t saturation)
{
    emberAfWriteServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_SATURATION_ATTRIBUTE_ID,
                                reinterpret_cast<uint8_t *>(&saturation), ZCL_INT8U_ATTRIBUTE_TYPE);
}

static void writeColorX(EndpointId endpoint, uint16_t colorX)
{
    emberAfWriteServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID,
                                reinterpret_cast<uint8_t *>(&colorX), ZCL_INT16U_ATTRIBUTE_TYPE);
}

static void writeColorY(EndpointId endpoint, uint16_t colorY)
{
    emberAfWriteServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID,
                                reinterpret_cast<uint8_t *>(&colorY), ZCL_INT16U_ATTRIBUTE_TYPE);
}

// -------------------------------------------------------------------------
// ****** callback section *******

#ifdef EMBER_AF_PLUGIN_COLOR_CONTROL_SERVER_HSV
static bool moveToHueAndSaturation(uint16_t hue, uint8_t saturation, uint16_t transitionTime, uint8_t optionsMask,
                                   uint8_t optionsOverride, bool isEnhanced)
{
    // If isEnhanced is True this function was called by EnhancedMoveToHueAndSaturation command and the hue is a uint16
    // If isEnhanced is False this function was called by MoveToHueAndSaturation command and the hue is are a uint8

    EndpointId endpoint = emberAfCurrentEndpoint();
    uint16_t currentHue = isEnhanced ? readEnhancedHue(endpoint) : static_cast<uint16_t>(readHue(endpoint));
    bool moveUp;

    if (transitionTime == 0)
    {
        transitionTime++;
    }

    // limit checking:  hue and saturation are 0..254.  Spec dictates we ignore
    // this and report a malformed packet.
    if ((!isEnhanced && hue > MAX_HUE_VALUE) || saturation > MAX_SATURATION_VALUE)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_MALFORMED_COMMAND);
        return true;
    }

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    // compute shortest direction
    uint16_t halfWay = isEnhanced ? HALF_MAX_UINT16T : HALF_MAX_UINT8T;
    if (hue > currentHue)
    {
        moveUp = (hue - currentHue) < halfWay;
    }
    else
    {
        moveUp = (currentHue - hue) > halfWay;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_HSV);

    // now, kick off the state machine.
    initHueSat(endpoint);
    colorHueTransitionState.isEnhancedHue = isEnhanced;

    if (isEnhanced)
    {
        colorHueTransitionState.initialEnhancedHue = currentHue;
        colorHueTransitionState.currentEnhancedHue = currentHue;
        colorHueTransitionState.finalEnhancedHue   = hue;
    }
    else
    {
        colorHueTransitionState.initialHue = static_cast<uint8_t>(currentHue);
        colorHueTransitionState.currentHue = static_cast<uint8_t>(currentHue);
        colorHueTransitionState.finalHue   = static_cast<uint8_t>(hue);
    }

    colorHueTransitionState.stepsRemaining = transitionTime;
    colorHueTransitionState.stepsTotal     = transitionTime;
    colorHueTransitionState.endpoint       = endpoint;
    colorHueTransitionState.up             = moveUp;
    colorHueTransitionState.repeat         = false;

    colorSaturationTransitionState.initialValue   = readSaturation(endpoint);
    colorSaturationTransitionState.currentValue   = readSaturation(endpoint);
    colorSaturationTransitionState.finalValue     = saturation;
    colorSaturationTransitionState.stepsRemaining = transitionTime;
    colorSaturationTransitionState.stepsTotal     = transitionTime;
    colorSaturationTransitionState.endpoint       = endpoint;
    colorSaturationTransitionState.lowLimit       = MIN_SATURATION_VALUE;
    colorSaturationTransitionState.highLimit      = MAX_SATURATION_VALUE;

    writeRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_HSV_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

/** @brief Move To Hue And Saturation
 *
 *
 *
 * @param hue   Ver.: always
 * @param saturation   Ver.: always
 * @param transitionTime   Ver.: always
 */
bool emberAfColorControlClusterMoveToHueAndSaturationCallback(EndpointId endpoint, app::CommandHandler * commandObj, uint8_t hue,
                                                              uint8_t saturation, uint16_t transitionTime, uint8_t optionsMask,
                                                              uint8_t optionsOverride)
{
    return moveToHueAndSaturation(static_cast<uint16_t>(hue), saturation, transitionTime, optionsMask, optionsOverride, false);
}

static bool moveHue(uint8_t moveMode, uint16_t rate, uint8_t optionsMask, uint8_t optionsOverride, bool isEnhanced)
{
    // If isEnhanced is True this function was called by EnhancedMoveHue command and rate is a uint16 value
    // If isEnhanced is False this function was called by MoveHue command and rate is a uint8 value

    uint8_t currentHue          = 0;
    uint16_t currentEnhancedHue = 0;
    EndpointId endpoint         = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    if (moveMode == EMBER_ZCL_HUE_MOVE_MODE_STOP)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_HSV);

    // now, kick off the state machine.
    initHueSat(endpoint);
    colorHueTransitionState.isEnhancedHue = isEnhanced;

    if (isEnhanced)
    {
        currentEnhancedHue                         = readEnhancedHue(endpoint);
        colorHueTransitionState.initialEnhancedHue = currentEnhancedHue;
        colorHueTransitionState.currentEnhancedHue = currentEnhancedHue;
    }
    else
    {
        currentHue                         = readHue(endpoint);
        colorHueTransitionState.initialHue = currentHue;
        colorHueTransitionState.currentHue = currentHue;
    }

    if (moveMode == EMBER_ZCL_HUE_MOVE_MODE_UP)
    {
        if (isEnhanced)
        {
            colorHueTransitionState.finalEnhancedHue = addEnhancedHue(currentEnhancedHue, rate);
        }
        else
        {
            colorHueTransitionState.finalHue = addHue(currentHue, static_cast<uint8_t>(rate));
        }

        colorHueTransitionState.up = true;
    }
    else if (moveMode == EMBER_ZCL_HUE_MOVE_MODE_DOWN)
    {
        if (isEnhanced)
        {
            colorHueTransitionState.finalEnhancedHue = subtractEnhancedHue(currentEnhancedHue, rate);
        }
        else
        {
            colorHueTransitionState.finalHue = subtractHue(currentHue, static_cast<uint8_t>(rate));
        }

        colorHueTransitionState.up = false;
    }
    else
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_MALFORMED_COMMAND);
        return true;
    }
    colorHueTransitionState.stepsRemaining = TRANSITION_TIME_1S;
    colorHueTransitionState.stepsTotal     = TRANSITION_TIME_1S;
    colorHueTransitionState.endpoint       = endpoint;
    colorHueTransitionState.repeat         = true;
    // hue movement can last forever.  Indicate this with a remaining time of
    // maxint.
    writeRemainingTime(endpoint, MAX_INT16U_VALUE);

    colorSaturationTransitionState.stepsRemaining = 0;

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_HSV_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

bool emberAfColorControlClusterMoveHueCallback(EndpointId endpoint, app::CommandHandler * commandObj, uint8_t moveMode,
                                               uint8_t rate, uint8_t optionsMask, uint8_t optionsOverride)
{
    return moveHue(moveMode, static_cast<uint16_t>(rate), optionsMask, optionsOverride, false);
}

bool emberAfColorControlClusterMoveSaturationCallback(EndpointId aEndpoint, app::CommandHandler * commandObj, uint8_t moveMode,
                                                      uint8_t rate, uint8_t optionsMask, uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    uint16_t transitionTime;

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    if (moveMode == EMBER_ZCL_SATURATION_MOVE_MODE_STOP || rate == 0)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_HSV);

    // now, kick off the state machine.
    initHueSat(endpoint);

    colorHueTransitionState.stepsRemaining = 0;

    colorSaturationTransitionState.initialValue = readSaturation(endpoint);
    colorSaturationTransitionState.currentValue = readSaturation(endpoint);
    if (moveMode == EMBER_ZCL_SATURATION_MOVE_MODE_UP)
    {
        colorSaturationTransitionState.finalValue = MAX_SATURATION_VALUE;
    }
    else
    {
        colorSaturationTransitionState.finalValue = MIN_SATURATION_VALUE;
    }

    transitionTime = computeTransitionTimeFromStateAndRate(&colorSaturationTransitionState, rate);

    colorSaturationTransitionState.stepsRemaining = transitionTime;
    colorSaturationTransitionState.stepsTotal     = transitionTime;
    colorSaturationTransitionState.endpoint       = endpoint;
    colorSaturationTransitionState.lowLimit       = MIN_SATURATION_VALUE;
    colorSaturationTransitionState.highLimit      = MAX_SATURATION_VALUE;

    writeRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_HSV_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

static bool moveToHue(uint16_t hue, uint8_t hueMoveMode, uint16_t transitionTime, uint8_t optionsMask, uint8_t optionsOverride,
                      bool isEnhanced)
{
    // If isEnhanced is True this function was called by EnhancedMoveToHue and hue is a uint16 value
    // If isEnhanced is False this function was called by MoveToHue command and hue is are a uint8 value
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    uint16_t currentHue = isEnhanced ? readEnhancedHue(endpoint) : static_cast<uint16_t>(readHue(endpoint));
    uint8_t direction;

    if (transitionTime == 0)
    {
        transitionTime++;
    }

    // Standard Hue limit checking:  hue is 0..254.  Spec dictates we ignore
    // this and report a malformed packet.
    if (!isEnhanced && (hue > MAX_HUE_VALUE))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_MALFORMED_COMMAND);
        return true;
    }

    // For move to hue, the move modes are different from the other move commands.
    // Need to translate from the move to hue transitions to the internal
    // representation.
    switch (hueMoveMode)
    {
    case EMBER_ZCL_HUE_DIRECTION_SHORTEST_DISTANCE:
        if ((isEnhanced && (static_cast<uint16_t>(currentHue - hue) > HALF_MAX_UINT16T)) ||
            (!isEnhanced && (static_cast<uint8_t>(currentHue - hue) > HALF_MAX_UINT8T)))
        {
            direction = MOVE_MODE_UP;
        }
        else
        {
            direction = MOVE_MODE_DOWN;
        }
        break;
    case EMBER_ZCL_HUE_DIRECTION_LONGEST_DISTANCE:
        if ((isEnhanced && (static_cast<uint16_t>(currentHue - hue) > HALF_MAX_UINT16T)) ||
            (!isEnhanced && (static_cast<uint8_t>(currentHue - hue) > HALF_MAX_UINT8T)))
        {
            direction = MOVE_MODE_DOWN;
        }
        else
        {
            direction = MOVE_MODE_UP;
        }
        break;
    case EMBER_ZCL_HUE_DIRECTION_UP:
        direction = MOVE_MODE_UP;
        break;
    case EMBER_ZCL_HUE_DIRECTION_DOWN:
        direction = MOVE_MODE_DOWN;
        break;
    default:
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_MALFORMED_COMMAND);
        return true;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_HSV);

    // now, kick off the state machine.
    initHueSat(endpoint);
    colorHueTransitionState.isEnhancedHue = isEnhanced;

    if (isEnhanced)
    {
        colorHueTransitionState.initialEnhancedHue = readEnhancedHue(endpoint);
        colorHueTransitionState.currentEnhancedHue = readEnhancedHue(endpoint);
        colorHueTransitionState.finalEnhancedHue   = hue;
    }
    else
    {
        colorHueTransitionState.initialHue = readHue(endpoint);
        colorHueTransitionState.currentHue = readHue(endpoint);
        colorHueTransitionState.finalHue   = static_cast<uint8_t>(hue);
    }

    colorHueTransitionState.stepsRemaining = transitionTime;
    colorHueTransitionState.stepsTotal     = transitionTime;
    colorHueTransitionState.endpoint       = endpoint;
    colorHueTransitionState.up             = (direction == MOVE_MODE_UP);
    colorHueTransitionState.repeat         = false;

    colorSaturationTransitionState.stepsRemaining = 0;

    writeRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_HSV_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

bool emberAfColorControlClusterMoveToHueCallback(EndpointId endpoint, app::CommandHandler * commandObj, uint8_t hue,
                                                 uint8_t hueMoveMode, uint16_t transitionTime, uint8_t optionsMask,
                                                 uint8_t optionsOverride)
{
    return moveToHue(static_cast<uint16_t>(hue), hueMoveMode, transitionTime, optionsMask, optionsOverride, false);
}

bool emberAfColorControlClusterMoveToSaturationCallback(EndpointId aEndpoint, app::CommandHandler * commandObj, uint8_t saturation,
                                                        uint16_t transitionTime, uint8_t optionsMask, uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    if (transitionTime == 0)
    {
        transitionTime++;
    }

    // limit checking:  hue and saturation are 0..254.  Spec dictates we ignore
    // this and report a malformed packet.
    if (saturation > MAX_SATURATION_VALUE)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_MALFORMED_COMMAND);
        return true;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_HSV);

    // now, kick off the state machine.
    initHueSat(endpoint);

    colorHueTransitionState.stepsRemaining = 0;

    colorSaturationTransitionState.initialValue   = readSaturation(endpoint);
    colorSaturationTransitionState.currentValue   = readSaturation(endpoint);
    colorSaturationTransitionState.finalValue     = saturation;
    colorSaturationTransitionState.stepsRemaining = transitionTime;
    colorSaturationTransitionState.stepsTotal     = transitionTime;
    colorSaturationTransitionState.endpoint       = endpoint;
    colorSaturationTransitionState.lowLimit       = MIN_SATURATION_VALUE;
    colorSaturationTransitionState.highLimit      = MAX_SATURATION_VALUE;

    writeRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_HSV_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

static bool stepHue(uint8_t stepMode, uint16_t stepSize, uint16_t transitionTime, uint8_t optionsMask, uint8_t optionsOverride,
                    bool isEnhanced)
{
    // If isEnhanced is True this function was called by EnhancedStepHue and hue is a uint16 value
    // If isEnhanced is False this function was called by StepHue command and hue is are a uint8 value

    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    if (transitionTime == 0)
    {
        transitionTime++;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    if (stepMode == MOVE_MODE_STOP)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_HSV);

    // now, kick off the state machine.
    initHueSat(endpoint);
    colorHueTransitionState.isEnhancedHue = isEnhanced;

    if (isEnhanced)
    {
        colorHueTransitionState.initialEnhancedHue = colorHueTransitionState.currentEnhancedHue = readEnhancedHue(endpoint);
        if (stepMode == MOVE_MODE_UP)
        {
            colorHueTransitionState.finalEnhancedHue = addEnhancedHue(colorHueTransitionState.currentEnhancedHue, stepSize);
            colorHueTransitionState.up               = true;
        }
        else
        {
            colorHueTransitionState.finalEnhancedHue = subtractEnhancedHue(colorHueTransitionState.currentEnhancedHue, stepSize);
            colorHueTransitionState.up               = false;
        }
    }
    else
    {
        colorHueTransitionState.initialHue = colorHueTransitionState.currentHue = readHue(endpoint);
        if (stepMode == MOVE_MODE_UP)
        {
            colorHueTransitionState.finalHue = addHue(colorHueTransitionState.currentHue, static_cast<uint8_t>(stepSize));
            colorHueTransitionState.up       = true;
        }
        else
        {
            colorHueTransitionState.finalHue = subtractHue(colorHueTransitionState.currentHue, static_cast<uint8_t>(stepSize));
            colorHueTransitionState.up       = false;
        }
    }

    colorHueTransitionState.stepsRemaining = transitionTime;
    colorHueTransitionState.stepsTotal     = transitionTime;
    colorHueTransitionState.endpoint       = endpoint;
    colorHueTransitionState.repeat         = false;

    colorSaturationTransitionState.stepsRemaining = 0;

    writeRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_HSV_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

bool emberAfColorControlClusterStepHueCallback(EndpointId endpoint, app::CommandHandler * commandObj, uint8_t stepMode,
                                               uint8_t stepSize, uint8_t transitionTime, uint8_t optionsMask,
                                               uint8_t optionsOverride)
{
    return stepHue(stepMode, static_cast<uint16_t>(stepSize), static_cast<uint16_t>(transitionTime), optionsMask, optionsOverride,
                   false);
}

bool emberAfColorControlClusterStepSaturationCallback(EndpointId aEndpoint, app::CommandHandler * commandObj, uint8_t stepMode,
                                                      uint8_t stepSize, uint8_t transitionTime, uint8_t optionsMask,
                                                      uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    uint8_t currentSaturation = readSaturation(endpoint);

    if (transitionTime == 0)
    {
        transitionTime++;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    if (stepMode == MOVE_MODE_STOP)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_HSV);

    // now, kick off the state machine.
    initHueSat(endpoint);

    colorHueTransitionState.stepsRemaining = 0;

    colorSaturationTransitionState.initialValue = currentSaturation;
    colorSaturationTransitionState.currentValue = currentSaturation;

    if (stepMode == MOVE_MODE_UP)
    {
        colorSaturationTransitionState.finalValue = addSaturation(currentSaturation, stepSize);
    }
    else
    {
        colorSaturationTransitionState.finalValue = subtractSaturation(currentSaturation, stepSize);
    }
    colorSaturationTransitionState.stepsRemaining = transitionTime;
    colorSaturationTransitionState.stepsTotal     = transitionTime;
    colorSaturationTransitionState.endpoint       = endpoint;
    colorSaturationTransitionState.lowLimit       = MIN_SATURATION_VALUE;
    colorSaturationTransitionState.highLimit      = MAX_SATURATION_VALUE;

    writeRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_HSV_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

static uint8_t addSaturation(uint8_t saturation1, uint8_t saturation2)
{
    uint16_t saturation16;

    saturation16 = ((uint16_t) saturation1);
    saturation16 = static_cast<uint16_t>(saturation16 + static_cast<uint16_t>(saturation2));

    if (saturation16 > MAX_SATURATION_VALUE)
    {
        saturation16 = MAX_SATURATION_VALUE;
    }

    return ((uint8_t) saturation16);
}

static uint8_t subtractSaturation(uint8_t saturation1, uint8_t saturation2)
{
    if (saturation2 > saturation1)
    {
        return 0;
    }

    return static_cast<uint8_t>(saturation1 - saturation2);
}

// any time we call a hue or saturation transition, we need to assume certain
// things about the hue and saturation data structures.  This function will
// properly initialize them.
static void initHueSat(EndpointId endpoint)
{
    colorHueTransitionState.stepsRemaining = 0;
    colorHueTransitionState.currentHue     = readHue(endpoint);
    colorHueTransitionState.endpoint       = endpoint;

    colorHueTransitionState.currentEnhancedHue = readEnhancedHue(endpoint);
    colorHueTransitionState.isEnhancedHue      = false;

    colorSaturationTransitionState.stepsRemaining = 0;
    colorSaturationTransitionState.currentValue   = readSaturation(endpoint);
    colorSaturationTransitionState.endpoint       = endpoint;
}

static uint8_t readHue(EndpointId endpoint)
{
    uint8_t hue;
    emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_HUE_ATTRIBUTE_ID,
                               reinterpret_cast<uint8_t *>(&hue), sizeof(uint8_t));

    return hue;
}

static uint8_t readSaturation(EndpointId endpoint)
{
    uint8_t saturation;
    emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_SATURATION_ATTRIBUTE_ID,
                               reinterpret_cast<uint8_t *>(&saturation), sizeof(uint8_t));

    return saturation;
}
static uint16_t readEnhancedHue(EndpointId endpoint)
{
    uint16_t enhancedHue;
    emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_ENHANCED_CURRENT_HUE_ATTRIBUTE_ID,
                               reinterpret_cast<uint8_t *>(&enhancedHue), sizeof(uint16_t));

    return enhancedHue;
}

#endif // #ifdef EMBER_AF_PLUGIN_COLOR_CONTROL_SERVER_HSV

#ifdef EMBER_AF_PLUGIN_COLOR_CONTROL_SERVER_XY

bool emberAfColorControlClusterMoveToColorCallback(EndpointId aEndpoint, app::CommandHandler * commandObj, uint16_t colorX,
                                                   uint16_t colorY, uint16_t transitionTime, uint8_t optionsMask,
                                                   uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    if (transitionTime == 0)
    {
        transitionTime++;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_CIE_XY);

    // now, kick off the state machine.
    colorXTransitionState.initialValue   = readColorX(endpoint);
    colorXTransitionState.currentValue   = readColorX(endpoint);
    colorXTransitionState.finalValue     = colorX;
    colorXTransitionState.stepsRemaining = transitionTime;
    colorXTransitionState.stepsTotal     = transitionTime;
    colorXTransitionState.endpoint       = endpoint;
    colorXTransitionState.lowLimit       = MIN_CIE_XY_VALUE;
    colorXTransitionState.highLimit      = MAX_CIE_XY_VALUE;

    colorYTransitionState.initialValue   = readColorY(endpoint);
    colorYTransitionState.currentValue   = readColorY(endpoint);
    colorYTransitionState.finalValue     = colorY;
    colorYTransitionState.stepsRemaining = transitionTime;
    colorYTransitionState.stepsTotal     = transitionTime;
    colorYTransitionState.endpoint       = endpoint;
    colorYTransitionState.lowLimit       = MIN_CIE_XY_VALUE;
    colorYTransitionState.highLimit      = MAX_CIE_XY_VALUE;

    writeRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_XY_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

bool emberAfColorControlClusterMoveColorCallback(EndpointId aEndpoint, app::CommandHandler * commandObj, int16_t rateX,
                                                 int16_t rateY, uint8_t optionsMask, uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    uint16_t transitionTimeX, transitionTimeY;
    uint16_t unsignedRate;

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    if (rateX == 0 && rateY == 0)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_CIE_XY);

    // now, kick off the state machine.
    colorXTransitionState.initialValue = readColorX(endpoint);
    colorXTransitionState.currentValue = colorXTransitionState.initialValue;
    if (rateX > 0)
    {
        colorXTransitionState.finalValue = MAX_CIE_XY_VALUE;
        unsignedRate                     = (uint16_t) rateX;
    }
    else
    {
        colorXTransitionState.finalValue = MIN_CIE_XY_VALUE;
        unsignedRate                     = (uint16_t)(rateX * -1);
    }
    transitionTimeX                      = computeTransitionTimeFromStateAndRate(&colorXTransitionState, unsignedRate);
    colorXTransitionState.stepsRemaining = transitionTimeX;
    colorXTransitionState.stepsTotal     = transitionTimeX;
    colorXTransitionState.endpoint       = endpoint;
    colorXTransitionState.lowLimit       = MIN_CIE_XY_VALUE;
    colorXTransitionState.highLimit      = MAX_CIE_XY_VALUE;

    colorYTransitionState.initialValue = readColorY(endpoint);
    colorYTransitionState.currentValue = colorYTransitionState.initialValue;
    if (rateY > 0)
    {
        colorYTransitionState.finalValue = MAX_CIE_XY_VALUE;
        unsignedRate                     = (uint16_t) rateY;
    }
    else
    {
        colorYTransitionState.finalValue = MIN_CIE_XY_VALUE;
        unsignedRate                     = (uint16_t)(rateY * -1);
    }
    transitionTimeY                      = computeTransitionTimeFromStateAndRate(&colorYTransitionState, unsignedRate);
    colorYTransitionState.stepsRemaining = transitionTimeY;
    colorYTransitionState.stepsTotal     = transitionTimeY;
    colorYTransitionState.endpoint       = endpoint;
    colorYTransitionState.lowLimit       = MIN_CIE_XY_VALUE;
    colorYTransitionState.highLimit      = MAX_CIE_XY_VALUE;

    if (transitionTimeX < transitionTimeY)
    {
        writeRemainingTime(endpoint, transitionTimeX);
    }
    else
    {
        writeRemainingTime(endpoint, transitionTimeY);
    }

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_XY_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

bool emberAfColorControlClusterStepColorCallback(EndpointId aEndpoint, app::CommandHandler * commandObj, int16_t stepX,
                                                 int16_t stepY, uint16_t transitionTime, uint8_t optionsMask,
                                                 uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    uint16_t colorX = findNewColorValueFromStep(readColorX(endpoint), stepX);
    uint16_t colorY = findNewColorValueFromStep(readColorY(endpoint), stepY);

    if (transitionTime == 0)
    {
        transitionTime++;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_CIE_XY);

    // now, kick off the state machine.
    colorXTransitionState.initialValue   = readColorX(endpoint);
    colorXTransitionState.currentValue   = readColorX(endpoint);
    colorXTransitionState.finalValue     = colorX;
    colorXTransitionState.stepsRemaining = transitionTime;
    colorXTransitionState.stepsTotal     = transitionTime;
    colorXTransitionState.endpoint       = endpoint;
    colorXTransitionState.lowLimit       = MIN_CIE_XY_VALUE;
    colorXTransitionState.highLimit      = MAX_CIE_XY_VALUE;

    colorYTransitionState.initialValue   = readColorY(endpoint);
    colorYTransitionState.currentValue   = readColorY(endpoint);
    colorYTransitionState.finalValue     = colorY;
    colorYTransitionState.stepsRemaining = transitionTime;
    colorYTransitionState.stepsTotal     = transitionTime;
    colorYTransitionState.endpoint       = endpoint;
    colorYTransitionState.lowLimit       = MIN_CIE_XY_VALUE;
    colorYTransitionState.highLimit      = MAX_CIE_XY_VALUE;

    writeRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_XY_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

static uint16_t findNewColorValueFromStep(uint16_t oldValue, int16_t step)
{
    uint16_t newValue;
    int32_t newValueSigned;

    newValueSigned = ((int32_t) oldValue) + ((int32_t) step);

    if (newValueSigned < 0)
    {
        newValue = 0;
    }
    else if (newValueSigned > MAX_CIE_XY_VALUE)
    {
        newValue = MAX_CIE_XY_VALUE;
    }
    else
    {
        newValue = (uint16_t) newValueSigned;
    }

    return newValue;
}

static uint16_t readColorX(EndpointId endpoint)
{
    uint16_t colorX;
    emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID,
                               reinterpret_cast<uint8_t *>(&colorX), sizeof(uint16_t));

    return colorX;
}

static uint16_t readColorY(EndpointId endpoint)
{
    uint16_t colorY;
    emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID,
                               reinterpret_cast<uint8_t *>(&colorY), sizeof(uint16_t));

    return colorY;
}

#endif //#ifdef EMBER_AF_PLUGIN_COLOR_CONTROL_SERVER_XY

#ifdef EMBER_AF_PLUGIN_COLOR_CONTROL_SERVER_TEMP

static void moveToColorTemp(EndpointId endpoint, uint16_t colorTemperature, uint16_t transitionTime)
{
    uint16_t temperatureMin = readColorTemperatureMin(endpoint);
    uint16_t temperatureMax = readColorTemperatureMax(endpoint);

    if (transitionTime == 0)
    {
        transitionTime++;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_TEMPERATURE);

    if (colorTemperature < temperatureMin)
    {
        colorTemperature = temperatureMin;
    }

    if (colorTemperature > temperatureMax)
    {
        colorTemperature = temperatureMax;
    }

    // now, kick off the state machine.
    colorTempTransitionState.initialValue   = readColorTemperature(endpoint);
    colorTempTransitionState.currentValue   = readColorTemperature(endpoint);
    colorTempTransitionState.finalValue     = colorTemperature;
    colorTempTransitionState.stepsRemaining = transitionTime;
    colorTempTransitionState.stepsTotal     = transitionTime;
    colorTempTransitionState.endpoint       = endpoint;
    colorTempTransitionState.lowLimit       = temperatureMin;
    colorTempTransitionState.highLimit      = temperatureMax;

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_TEMP_CONTROL, UPDATE_TIME_MS);
}

bool emberAfColorControlClusterMoveToColorTemperatureCallback(EndpointId aEndpoint, app::CommandHandler * commandObj,
                                                              uint16_t colorTemperature, uint16_t transitionTime,
                                                              uint8_t optionsMask, uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    moveToColorTemp(endpoint, colorTemperature, transitionTime);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

bool emberAfColorControlClusterMoveColorTemperatureCallback(EndpointId aEndpoint, app::CommandHandler * commandObj,
                                                            uint8_t moveMode, uint16_t rate, uint16_t colorTemperatureMinimum,
                                                            uint16_t colorTemperatureMaximum, uint8_t optionsMask,
                                                            uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    uint16_t tempPhysicalMin = MIN_TEMPERATURE_VALUE;
    Attributes::GetColorTempPhysicalMin(endpoint, &tempPhysicalMin);

    uint16_t tempPhysicalMax = MAX_TEMPERATURE_VALUE;
    Attributes::GetColorTempPhysicalMax(endpoint, &tempPhysicalMax);

    uint16_t transitionTime;

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    if (moveMode == MOVE_MODE_STOP)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    if (rate == 0)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_INVALID_FIELD);
        return true;
    }

    if (colorTemperatureMinimum < tempPhysicalMin)
    {
        colorTemperatureMinimum = tempPhysicalMin;
    }
    if (colorTemperatureMaximum > tempPhysicalMax)
    {
        colorTemperatureMaximum = tempPhysicalMax;
    }

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_TEMPERATURE);

    // now, kick off the state machine.
    colorTempTransitionState.initialValue = 0;
    Attributes::GetColorTemperature(endpoint, &colorTempTransitionState.initialValue);
    colorTempTransitionState.currentValue = colorTempTransitionState.initialValue;

    if (moveMode == MOVE_MODE_UP)
    {
        if (tempPhysicalMax > colorTemperatureMaximum)
        {
            colorTempTransitionState.finalValue = colorTemperatureMaximum;
        }
        else
        {
            colorTempTransitionState.finalValue = tempPhysicalMax;
        }
    }
    else
    {
        if (tempPhysicalMin < colorTemperatureMinimum)
        {
            colorTempTransitionState.finalValue = colorTemperatureMinimum;
        }
        else
        {
            colorTempTransitionState.finalValue = tempPhysicalMin;
        }
    }
    transitionTime                          = computeTransitionTimeFromStateAndRate(&colorTempTransitionState, rate);
    colorTempTransitionState.stepsRemaining = transitionTime;
    colorTempTransitionState.stepsTotal     = transitionTime;
    colorTempTransitionState.endpoint       = endpoint;
    colorTempTransitionState.lowLimit       = colorTemperatureMinimum;
    colorTempTransitionState.highLimit      = colorTemperatureMaximum;

    Attributes::SetRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_TEMP_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

bool emberAfColorControlClusterStepColorTemperatureCallback(EndpointId aEndpoint, app::CommandHandler * commandObj,
                                                            uint8_t stepMode, uint16_t stepSize, uint16_t transitionTime,
                                                            uint16_t colorTemperatureMinimum, uint16_t colorTemperatureMaximum,
                                                            uint8_t optionsMask, uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    uint16_t tempPhysicalMin = MIN_TEMPERATURE_VALUE;
    Attributes::GetColorTempPhysicalMin(endpoint, &tempPhysicalMin);

    uint16_t tempPhysicalMax = MAX_TEMPERATURE_VALUE;
    Attributes::GetColorTempPhysicalMax(endpoint, &tempPhysicalMax);

    if (transitionTime == 0)
    {
        transitionTime++;
    }

    // New command.  Need to stop any active transitions.
    stopAllColorTransitions();

    if (stepMode == MOVE_MODE_STOP)
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    if (colorTemperatureMinimum < tempPhysicalMin)
    {
        colorTemperatureMinimum = tempPhysicalMin;
    }
    if (colorTemperatureMaximum > tempPhysicalMax)
    {
        colorTemperatureMaximum = tempPhysicalMax;
    }

    // Handle color mode transition, if necessary.
    handleModeSwitch(endpoint, COLOR_MODE_TEMPERATURE);

    // now, kick off the state machine.
    colorTempTransitionState.initialValue = 0;
    Attributes::GetColorTemperature(endpoint, &colorTempTransitionState.initialValue);
    colorTempTransitionState.currentValue = colorTempTransitionState.initialValue;

    if (stepMode == MOVE_MODE_UP)
    {
        uint32_t finalValue32u = static_cast<uint32_t>(colorTempTransitionState.initialValue) + static_cast<uint32_t>(stepSize);
        if (finalValue32u > UINT16_MAX)
        {
            colorTempTransitionState.finalValue = UINT16_MAX;
        }
        else
        {
            colorTempTransitionState.finalValue = static_cast<uint16_t>(finalValue32u);
        }
    }
    else
    {
        uint32_t finalValue32u = static_cast<uint32_t>(colorTempTransitionState.initialValue) - static_cast<uint32_t>(stepSize);
        if (finalValue32u > UINT16_MAX)
        {
            colorTempTransitionState.finalValue = 0;
        }
        else
        {
            colorTempTransitionState.finalValue = static_cast<uint16_t>(finalValue32u);
        }
    }
    colorTempTransitionState.stepsRemaining = transitionTime;
    colorTempTransitionState.stepsTotal     = transitionTime;
    colorTempTransitionState.endpoint       = endpoint;
    colorTempTransitionState.lowLimit       = colorTemperatureMinimum;
    colorTempTransitionState.highLimit      = colorTemperatureMaximum;

    Attributes::SetRemainingTime(endpoint, transitionTime);

    // kick off the state machine:
    emberEventControlSetDelayMS(&COLOR_TEMP_CONTROL, UPDATE_TIME_MS);

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

void emberAfPluginLevelControlCoupledColorTempChangeCallback(EndpointId endpoint)
{
    // ZCL 5.2.2.1.1 Coupling color temperature to Level Control
    //
    // If the Level Control for Lighting cluster identifier 0x0008 is supported
    // on the same endpoint as the Color Control cluster and color temperature is
    // supported, it is possible to couple changes in the current level to the
    // color temperature.
    //
    // The CoupleColorTempToLevel bit of the Options attribute of the Level
    // Control cluster indicates whether the color temperature is to be linked
    // with the CurrentLevel attribute in the Level Control cluster.
    //
    // If the CoupleColorTempToLevel bit of the Options attribute of the Level
    // Control cluster is equal to 1 and the ColorMode or EnhancedColorMode
    // attribute is set to 0x02 (color temperature) then a change in the
    // CurrentLevel attribute SHALL affect the ColorTemperatureMireds attribute.
    // This relationship is manufacturer specific, with the qualification that
    // the maximum value of the CurrentLevel attribute SHALL correspond to a
    // ColorTemperatureMired attribute value equal to the
    // CoupleColorTempToLevelMinMireds attribute. This relationship is one-way so
    // a change to the ColorTemperatureMireds attribute SHALL NOT have any effect
    // on the CurrentLevel attribute.
    //
    // In order to simulate the behavior of an incandescent bulb, a low value of
    // the CurrentLevel attribute SHALL be associated with a high value of the
    // ColorTemperatureMireds attribute (i.e., a low value of color temperature
    // in kelvins).
    //
    // If the CoupleColorTempToLevel bit of the Options attribute of the Level
    // Control cluster is equal to 0, there SHALL be no link between color
    // temperature and current level.

    if (!emberAfContainsServer(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID))
    {
        return;
    }

    if (readColorMode(endpoint) == COLOR_MODE_TEMPERATURE)
    {
        uint16_t tempCoupleMin = readColorTemperatureCoupleToLevelMin(endpoint);
        uint16_t tempPhysMax   = readColorTemperatureMax(endpoint);
        uint8_t currentLevel   = readLevelControlCurrentLevel(endpoint);

        // Scale color temp setting between the coupling min and the physical max.
        // Note that mireds varies inversely with level: low level -> high mireds.
        // Peg min/MAX level to MAX/min mireds, otherwise interpolate.
        uint16_t newColorTemp;
        if (currentLevel <= MIN_CURRENT_LEVEL)
        {
            newColorTemp = tempPhysMax;
        }
        else if (currentLevel >= MAX_CURRENT_LEVEL)
        {
            newColorTemp = tempCoupleMin;
        }
        else
        {
            uint32_t tempDelta = (((uint32_t) tempPhysMax - (uint32_t) tempCoupleMin) * currentLevel) /
                (uint32_t)(MAX_CURRENT_LEVEL - MIN_CURRENT_LEVEL + 1);
            newColorTemp = (uint16_t)((uint32_t) tempPhysMax - tempDelta);
        }

        // Apply new color temp.
        moveToColorTemp(endpoint, newColorTemp, 0);
    }
}

#endif //#ifdef EMBER_AF_PLUGIN_COLOR_CONTROL_SERVER_TEMP

bool emberAfColorControlClusterStopMoveStepCallback(EndpointId aEndpoint, app::CommandHandler * commandObj, uint8_t optionsMask,
                                                    uint8_t optionsOverride)
{
    // Received a stop command.  This is all we need to do.
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        stopAllColorTransitions();
    }

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

static void startColorLoop(EndpointId endpoint, uint8_t startFromStartHue)
{
    uint8_t direction = 0;
    Attributes::GetColorLoopDirection(endpoint, &direction);

    uint16_t time = 0x0019;
    Attributes::GetColorLoopTime(endpoint, &time);

    uint16_t currentHue = 0;
    Attributes::GetEnhancedCurrentHue(endpoint, &currentHue);

    uint16_t startHue = 0x2300;
    if (startFromStartHue)
    {
        Attributes::GetColorLoopStartEnhancedHue(endpoint, &startHue);
    }
    else
    {
        startHue = currentHue;
    }

    Attributes::SetColorLoopStoredEnhancedHue(endpoint, currentHue);
    Attributes::SetColorLoopActive(endpoint, true);

    initHueSat(endpoint);

    colorHueTransitionState.isEnhancedHue = true;

    colorHueTransitionState.initialEnhancedHue = startHue;
    colorHueTransitionState.currentEnhancedHue = currentHue;

    if (direction)
    {
        colorHueTransitionState.finalEnhancedHue = static_cast<uint16_t>(startHue - 1);
    }
    else
    {
        colorHueTransitionState.finalEnhancedHue = static_cast<uint16_t>(startHue + 1);
    }

    colorHueTransitionState.up     = direction;
    colorHueTransitionState.repeat = true;

    colorHueTransitionState.stepsRemaining = static_cast<uint16_t>(time * TRANSITION_TIME_1S);
    colorHueTransitionState.stepsTotal     = static_cast<uint16_t>(time * TRANSITION_TIME_1S);
    colorHueTransitionState.endpoint       = endpoint;

    Attributes::SetRemainingTime(endpoint, MAX_INT16U_VALUE);
    emberEventControlSetDelayMS(&COLOR_HSV_CONTROL, UPDATE_TIME_MS);
}

bool emberAfColorControlClusterColorLoopSetCallback(chip::EndpointId aEndpoint, chip::app::CommandHandler * commandObj,
                                                    uint8_t updateFlags, uint8_t action, uint8_t direction, uint16_t time,
                                                    uint16_t startHue, uint8_t optionsMask, uint8_t optionsOverride)
{
    EndpointId endpoint = emberAfCurrentEndpoint();

    if (!shouldExecuteIfOff(endpoint, optionsMask, optionsOverride))
    {
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
        return true;
    }

    uint8_t isColorLoopActive = 0;
    Attributes::GetColorLoopActive(endpoint, &isColorLoopActive);

    uint8_t deactiveColorLoop =
        (updateFlags & EMBER_AF_COLOR_LOOP_UPDATE_FLAGS_UPDATE_ACTION) && (action == EMBER_ZCL_COLOR_LOOP_ACTION_DEACTIVATE);

    if (updateFlags & EMBER_AF_COLOR_LOOP_UPDATE_FLAGS_UPDATE_DIRECTION)
    {
        Attributes::SetColorLoopDirection(endpoint, direction);

        // Checks if color loop is active and stays active
        if (isColorLoopActive && !deactiveColorLoop)
        {
            colorHueTransitionState.up                 = direction;
            colorHueTransitionState.initialEnhancedHue = colorHueTransitionState.currentEnhancedHue;

            if (direction)
            {
                colorHueTransitionState.finalEnhancedHue = static_cast<uint16_t>(colorHueTransitionState.initialEnhancedHue - 1);
            }
            else
            {
                colorHueTransitionState.finalEnhancedHue = static_cast<uint16_t>(colorHueTransitionState.initialEnhancedHue + 1);
            }
            colorHueTransitionState.stepsRemaining = colorHueTransitionState.stepsTotal;
        }
    }

    if (updateFlags & EMBER_AF_COLOR_LOOP_UPDATE_FLAGS_UPDATE_TIME)
    {
        Attributes::SetColorLoopTime(endpoint, time);

        // Checks if color loop is active and stays active
        if (isColorLoopActive && !deactiveColorLoop)
        {
            colorHueTransitionState.stepsTotal         = static_cast<uint16_t>(time * TRANSITION_TIME_1S);
            colorHueTransitionState.initialEnhancedHue = colorHueTransitionState.currentEnhancedHue;

            if (colorHueTransitionState.up)
            {
                colorHueTransitionState.finalEnhancedHue = static_cast<uint16_t>(colorHueTransitionState.initialEnhancedHue - 1);
            }
            else
            {
                colorHueTransitionState.finalEnhancedHue = static_cast<uint16_t>(colorHueTransitionState.initialEnhancedHue + 1);
            }
            colorHueTransitionState.stepsRemaining = colorHueTransitionState.stepsTotal;
        }
    }

    if (updateFlags & EMBER_AF_COLOR_LOOP_UPDATE_FLAGS_UPDATE_START_HUE)
    {
        Attributes::SetColorLoopStartEnhancedHue(endpoint, startHue);
    }

    if (updateFlags & EMBER_AF_COLOR_LOOP_UPDATE_FLAGS_UPDATE_ACTION)
    {
        if (action == EMBER_ZCL_COLOR_LOOP_ACTION_DEACTIVATE)
        {
            if (isColorLoopActive)
            {
                stopAllColorTransitions();

                Attributes::SetColorLoopActive(endpoint, false);

                uint16_t storedEnhancedHue = 0;
                Attributes::GetColorLoopStoredEnhancedHue(endpoint, &storedEnhancedHue);
                Attributes::SetEnhancedCurrentHue(endpoint, storedEnhancedHue);
            }
            else
            {
                // Do Nothing since it's not on
            }
        }
        else if (action == EMBER_ZCL_COLOR_LOOP_ACTION_ACTIVATE_FROM_COLOR_LOOP_START_ENHANCED_HUE)
        {
            startColorLoop(endpoint, true);
        }
        else if (action == EMBER_ZCL_COLOR_LOOP_ACTION_ACTIVATE_FROM_ENHANCED_CURRENT_HUE)
        {
            startColorLoop(endpoint, false);
        }
        else
        {
            emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_MALFORMED_COMMAND);
            return true;
        }
    }

    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
    return true;
}

bool emberAfColorControlClusterEnhancedMoveHueCallback(EndpointId endpoint, app::CommandHandler * commandObj, uint8_t moveMode,
                                                       uint16_t rate, uint8_t optionsMask, uint8_t optionsOverride)
{
    return moveHue(moveMode, rate, optionsMask, optionsOverride, true);
}

bool emberAfColorControlClusterEnhancedMoveToHueCallback(EndpointId endpoint, app::CommandHandler * commandObj,
                                                         uint16_t enhancedHue, uint8_t direction, uint16_t transitionTime,
                                                         uint8_t optionsMask, uint8_t optionsOverride)
{
    return moveToHue(enhancedHue, direction, transitionTime, optionsMask, optionsOverride, true);
}

bool emberAfColorControlClusterEnhancedMoveToHueAndSaturationCallback(EndpointId endpoint, app::CommandHandler * commandObj,
                                                                      uint16_t enhancedHue, uint8_t saturation,
                                                                      uint16_t transitionTime, uint8_t optionsMask,
                                                                      uint8_t optionsOverride)
{
    return moveToHueAndSaturation(enhancedHue, saturation, transitionTime, optionsMask, optionsOverride, true);
}

bool emberAfColorControlClusterEnhancedStepHueCallback(EndpointId endpoint, app::CommandHandler * commandObj, uint8_t stepMode,
                                                       uint16_t stepSize, uint16_t transitionTime, uint8_t optionsMask,
                                                       uint8_t optionsOverride)
{
    return stepHue(stepMode, stepSize, transitionTime, optionsMask, optionsOverride, true);
}

// **************** transition state machines ***********

static void stopAllColorTransitions(void)
{
    emberEventControlSetInactive(&COLOR_TEMP_CONTROL);
    emberEventControlSetInactive(&COLOR_XY_CONTROL);
    emberEventControlSetInactive(&COLOR_HSV_CONTROL);
}

void emberAfPluginColorControlServerStopTransition(void)
{
    stopAllColorTransitions();
}

// The specification says that if we are transitioning from one color mode
// into another, we need to compute the new mode's attribute values from the
// old mode.  However, it also says that if the old mode doesn't translate into
// the new mode, this must be avoided.
// I am putting in this function to compute the new attributes based on the old
// color mode.
static void handleModeSwitch(EndpointId endpoint, uint8_t newColorMode)
{
    uint8_t oldColorMode = readColorMode(endpoint);
    uint8_t colorModeTransition;

    if (oldColorMode == newColorMode)
    {
        return;
    }
    else
    {
        writeColorMode(endpoint, newColorMode);
    }

    colorModeTransition = static_cast<uint8_t>((newColorMode << 4) + oldColorMode);

    // Note:  It may be OK to not do anything here.
    switch (colorModeTransition)
    {
    case HSV_TO_CIE_XY:
        emberAfPluginColorControlServerComputePwmFromXyCallback(endpoint);
        break;
    case TEMPERATURE_TO_CIE_XY:
        emberAfPluginColorControlServerComputePwmFromXyCallback(endpoint);
        break;
    case CIE_XY_TO_HSV:
        emberAfPluginColorControlServerComputePwmFromHsvCallback(endpoint);
        break;
    case TEMPERATURE_TO_HSV:
        emberAfPluginColorControlServerComputePwmFromHsvCallback(endpoint);
        break;
    case HSV_TO_TEMPERATURE:
        emberAfPluginColorControlServerComputePwmFromTempCallback(endpoint);
        break;
    case CIE_XY_TO_TEMPERATURE:
        emberAfPluginColorControlServerComputePwmFromTempCallback(endpoint);
        break;

    // for the following cases, there is no transition.
    case HSV_TO_HSV:
    case CIE_XY_TO_CIE_XY:
    case TEMPERATURE_TO_TEMPERATURE:
    default:
        return;
    }
}

static uint8_t addHue(uint8_t hue1, uint8_t hue2)
{
    uint16_t hue16;

    hue16 = ((uint16_t) hue1);
    hue16 = static_cast<uint16_t>(hue16 + static_cast<uint16_t>(hue2));

    if (hue16 > MAX_HUE_VALUE)
    {
        hue16 = static_cast<uint16_t>(hue16 - MAX_HUE_VALUE - 1);
    }

    return ((uint8_t) hue16);
}

static uint8_t subtractHue(uint8_t hue1, uint8_t hue2)
{
    uint16_t hue16;

    hue16 = ((uint16_t) hue1);
    if (hue2 > hue1)
    {
        hue16 = static_cast<uint16_t>(hue16 + MAX_HUE_VALUE + 1);
    }

    hue16 = static_cast<uint16_t>(hue16 - static_cast<uint16_t>(hue2));

    return ((uint8_t) hue16);
}

static bool computeNewHueValue(ColorHueTransitionState * p)
{
    uint32_t newHue32;
    uint16_t newHue;

    // exit with a false if hue is not currently moving
    if (p->stepsRemaining == 0)
    {
        return false;
    }

    (p->stepsRemaining)--;

    if (p->repeat == false)
    {
        writeRemainingTime(p->endpoint, p->stepsRemaining);
    }

    // are we going up or down?
    if ((p->isEnhancedHue && p->finalEnhancedHue == p->currentEnhancedHue) || (!p->isEnhancedHue && p->finalHue == p->currentHue))
    {
        // do nothing
    }
    else if (p->up)
    {
        newHue32 = static_cast<uint32_t>(p->isEnhancedHue ? subtractEnhancedHue(p->finalEnhancedHue, p->initialEnhancedHue)
                                                          : subtractHue(p->finalHue, p->initialHue));
        newHue32 *= static_cast<uint32_t>(p->stepsRemaining);
        newHue32 /= static_cast<uint32_t>(p->stepsTotal);

        if (p->isEnhancedHue)
        {
            p->currentEnhancedHue = subtractEnhancedHue(p->finalEnhancedHue, static_cast<uint16_t>(newHue32));
        }
        else
        {
            p->currentHue = subtractHue(p->finalHue, static_cast<uint8_t>(newHue32));
        }
    }
    else
    {
        newHue32 = static_cast<uint32_t>(p->isEnhancedHue ? subtractEnhancedHue(p->initialEnhancedHue, p->finalEnhancedHue)
                                                          : subtractHue(p->initialHue, p->finalHue));
        newHue32 *= static_cast<uint32_t>(p->stepsRemaining);
        newHue32 /= static_cast<uint32_t>(p->stepsTotal);

        if (p->isEnhancedHue)
        {
            p->currentEnhancedHue = addEnhancedHue(p->finalEnhancedHue, static_cast<uint16_t>(newHue32));
        }
        else
        {
            p->currentHue = addHue(p->finalHue, static_cast<uint8_t>(newHue32));
        }
    }

    if (p->stepsRemaining == 0)
    {
        if (p->repeat == false)
        {
            // we are performing a move to and not a move.
            return true;
        }
        else
        {
            // Check if we are in a color loop. If not, we are in a moveHue
            uint8_t isColorLoop = 0;
            Attributes::GetColorLoopActive(p->endpoint, &isColorLoop);

            if (isColorLoop)
            {
                p->currentEnhancedHue = p->initialEnhancedHue;
            }
            else
            {
                // we are performing a Hue move.  Need to compute the new values for the
                // next move period.
                if (p->up)
                {
                    if (p->isEnhancedHue)
                    {
                        newHue = subtractEnhancedHue(p->finalEnhancedHue, p->initialEnhancedHue);
                        newHue = addEnhancedHue(p->finalEnhancedHue, newHue);

                        p->initialEnhancedHue = p->finalEnhancedHue;
                        p->finalEnhancedHue   = newHue;
                    }
                    else
                    {
                        newHue = subtractHue(p->finalHue, p->initialHue);
                        newHue = addHue(p->finalHue, static_cast<uint8_t>(newHue));

                        p->initialHue = p->finalHue;
                        p->finalHue   = static_cast<uint8_t>(newHue);
                    }
                }
                else
                {
                    if (p->isEnhancedHue)
                    {
                        newHue = subtractEnhancedHue(p->initialEnhancedHue, p->finalEnhancedHue);
                        newHue = subtractEnhancedHue(p->finalEnhancedHue, newHue);

                        p->initialEnhancedHue = p->finalEnhancedHue;
                        p->finalEnhancedHue   = newHue;
                    }
                    else
                    {
                        newHue = subtractHue(p->initialHue, p->finalHue);
                        newHue = subtractHue(p->finalHue, static_cast<uint8_t>(newHue));

                        p->initialHue = p->finalHue;
                        p->finalHue   = static_cast<uint8_t>(newHue);
                    }
                }
            }

            p->stepsRemaining = p->stepsTotal;
        }
    }
    return false;
}

static uint16_t addEnhancedHue(uint16_t hue1, uint16_t hue2)
{
    return static_cast<uint16_t>(hue1 + hue2);
}

static uint16_t subtractEnhancedHue(uint16_t hue1, uint16_t hue2)
{
    return static_cast<uint16_t>(hue1 - hue2);
}

void emberAfPluginColorControlServerHueSatTransitionEventHandler(void)
{
    EndpointId endpoint = colorHueTransitionState.endpoint;
    bool limitReached1, limitReached2;

    limitReached1 = computeNewHueValue(&colorHueTransitionState);
    limitReached2 = computeNewColor16uValue(&colorSaturationTransitionState);

    if (limitReached1 || limitReached2)
    {
        stopAllColorTransitions();
    }
    else
    {
        emberEventControlSetDelayMS(&COLOR_HSV_CONTROL, UPDATE_TIME_MS);
    }

    if (colorHueTransitionState.isEnhancedHue)
    {
        writeEnhancedHue(colorHueTransitionState.endpoint, colorHueTransitionState.currentEnhancedHue);
    }
    else
    {
        writeHue(colorHueTransitionState.endpoint, colorHueTransitionState.currentHue);
    }

    writeSaturation(colorSaturationTransitionState.endpoint, (uint8_t) colorSaturationTransitionState.currentValue);
    if (colorHueTransitionState.isEnhancedHue)
    {
        emberAfColorControlClusterPrintln("Enhanced Hue %d Saturation %d endpoint %d", colorHueTransitionState.currentEnhancedHue,
                                          colorSaturationTransitionState.currentValue, endpoint);
    }
    else
    {
        emberAfColorControlClusterPrintln("Hue %d Saturation %d endpoint %d", colorHueTransitionState.currentHue,
                                          colorSaturationTransitionState.currentValue, endpoint);
    }
    emberAfPluginColorControlServerComputePwmFromHsvCallback(endpoint);
}

// Return value of true means we need to stop.
static bool computeNewColor16uValue(Color16uTransitionState * p)
{
    uint32_t newValue32u;

    if (p->stepsRemaining == 0)
    {
        return false;
    }

    (p->stepsRemaining)--;

    Attributes::SetRemainingTime(p->endpoint, p->stepsRemaining);

    // handle sign
    if (p->finalValue == p->currentValue)
    {
        // do nothing
    }
    else if (p->finalValue > p->initialValue)
    {
        newValue32u = ((uint32_t)(p->finalValue - p->initialValue));
        newValue32u *= ((uint32_t)(p->stepsRemaining));
        newValue32u /= ((uint32_t)(p->stepsTotal));
        p->currentValue = static_cast<uint16_t>(p->finalValue - static_cast<uint16_t>(newValue32u));

        if (static_cast<uint16_t>(newValue32u) > p->finalValue || p->currentValue > p->highLimit)
        {
            p->currentValue = p->highLimit;
        }
    }
    else
    {
        newValue32u = ((uint32_t)(p->initialValue - p->finalValue));
        newValue32u *= ((uint32_t)(p->stepsRemaining));
        newValue32u /= ((uint32_t)(p->stepsTotal));
        p->currentValue = static_cast<uint16_t>(p->finalValue + static_cast<uint16_t>(newValue32u));

        if (p->finalValue > UINT16_MAX - static_cast<uint16_t>(newValue32u) || p->currentValue < p->lowLimit)
        {
            p->currentValue = p->lowLimit;
        }
    }

    if (p->stepsRemaining == 0)
    {
        // we have completed our move.
        return true;
    }

    return false;
}

static uint16_t computeTransitionTimeFromStateAndRate(Color16uTransitionState * p, uint16_t rate)
{
    uint32_t transitionTime;
    uint16_t max, min;

    if (rate == 0)
    {
        return MAX_INT16U_VALUE;
    }

    if (p->currentValue > p->finalValue)
    {
        max = p->currentValue;
        min = p->finalValue;
    }
    else
    {
        max = p->finalValue;
        min = p->currentValue;
    }

    transitionTime = max - min;
    transitionTime *= 10;
    transitionTime /= rate;

    if (transitionTime > MAX_INT16U_VALUE)
    {
        return MAX_INT16U_VALUE;
    }

    return (uint16_t) transitionTime;
}

void emberAfPluginColorControlServerXyTransitionEventHandler(void)
{
    EndpointId endpoint = colorXTransitionState.endpoint;
    bool limitReachedX, limitReachedY;

    // compute new values for X and Y.
    limitReachedX = computeNewColor16uValue(&colorXTransitionState);

    limitReachedY = computeNewColor16uValue(&colorYTransitionState);

    if (limitReachedX || limitReachedY)
    {
        stopAllColorTransitions();
    }
    else
    {
        emberEventControlSetDelayMS(&COLOR_XY_CONTROL, UPDATE_TIME_MS);
    }

    // update the attributes
    writeColorX(colorXTransitionState.endpoint, colorXTransitionState.currentValue);
    writeColorY(colorXTransitionState.endpoint, colorYTransitionState.currentValue);

    emberAfColorControlClusterPrintln("Color X %d Color Y %d", colorXTransitionState.currentValue,
                                      colorYTransitionState.currentValue);

    emberAfPluginColorControlServerComputePwmFromXyCallback(endpoint);
}

void emberAfPluginColorControlServerTempTransitionEventHandler(void)
{
    EndpointId endpoint = colorTempTransitionState.endpoint;
    bool limitReached;

    limitReached = computeNewColor16uValue(&colorTempTransitionState);

    if (limitReached)
    {
        stopAllColorTransitions();
    }
    else
    {
        emberEventControlSetDelayMS(&COLOR_TEMP_CONTROL, UPDATE_TIME_MS);
    }

    Attributes::SetColorTemperature(colorTempTransitionState.endpoint, colorTempTransitionState.currentValue);

    emberAfColorControlClusterPrintln("Color Temperature %d", colorTempTransitionState.currentValue);

    emberAfPluginColorControlServerComputePwmFromTempCallback(endpoint);
}

static bool shouldExecuteIfOff(EndpointId endpoint, uint8_t optionMask, uint8_t optionOverride)
{
    // From 5.2.2.2.1.10 of ZCL7 document 14-0129-15f-zcl-ch-5-lighting.docx:
    //   "Command execution SHALL NOT continue beyond the Options processing if
    //    all of these criteria are true:
    //      - The On/Off cluster exists on the same endpoint as this cluster.
    //      - The OnOff attribute of the On/Off cluster, on this endpoint, is 0x00
    //        (FALSE).
    //      - The value of the ExecuteIfOff bit is 0."

    if (!emberAfContainsServer(endpoint, ZCL_ON_OFF_CLUSTER_ID))
    {
        return true;
    }

    uint8_t options;
    EmberAfStatus status =
        emberAfReadServerAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_OPTIONS_ATTRIBUTE_ID, &options, sizeof(options));
    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        emberAfColorControlClusterPrintln("Unable to read Options attribute: 0x%X", status);
        // If we can't read the attribute, then we should just assume that it has
        // its default value.
        options = 0x00;
    }

    bool on;
    status = emberAfReadServerAttribute(endpoint, ZCL_ON_OFF_CLUSTER_ID, ZCL_ON_OFF_ATTRIBUTE_ID, reinterpret_cast<uint8_t *>(&on),
                                        sizeof(on));
    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        emberAfColorControlClusterPrintln("Unable to read OnOff attribute: 0x%X", status);
        return true;
    }
    // The device is on - hence ExecuteIfOff does not matter
    if (on)
    {
        return true;
    }
    // The OptionsMask & OptionsOverride fields SHALL both be present or both
    // omitted in the command. A temporary Options bitmap SHALL be created from
    // the Options attribute, using the OptionsMask & OptionsOverride fields, if
    // present. Each bit of the temporary Options bitmap SHALL be determined as
    // follows:
    // Each bit in the Options attribute SHALL determine the corresponding bit in
    // the temporary Options bitmap, unless the OptionsMask field is present and
    // has the corresponding bit set to 1, in which case the corresponding bit in
    // the OptionsOverride field SHALL determine the corresponding bit in the
    // temporary Options bitmap.
    // The resulting temporary Options bitmap SHALL then be processed as defined
    // in section 5.2.2.2.1.10.

    // ---------- The following order is important in decision making -------
    // -----------more readable ----------
    //
    if (optionMask == 0xFF && optionOverride == 0xFF)
    {
        // 0xFF are the default values passed to the command handler when
        // the payload is not present - in that case there is use of option
        // attribute to decide execution of the command
        return READBITS(options, EMBER_ZCL_COLOR_CONTROL_OPTIONS_EXECUTE_IF_OFF);
    }
    // ---------- The above is to distinguish if the payload is present or not

    if (READBITS(optionMask, EMBER_ZCL_COLOR_CONTROL_OPTIONS_EXECUTE_IF_OFF))
    {
        // Mask is present and set in the command payload, this indicates
        // use the override as temporary option
        return READBITS(optionOverride, EMBER_ZCL_COLOR_CONTROL_OPTIONS_EXECUTE_IF_OFF);
    }
    // if we are here - use the option attribute bits
    return (READBITS(options, EMBER_ZCL_COLOR_CONTROL_OPTIONS_EXECUTE_IF_OFF));
}

void emberAfColorControlClusterServerInitCallback(EndpointId endpoint)
{
#ifdef EMBER_AF_PLUGIN_COLOR_CONTROL_SERVER_TEMP
    // 07-5123-07 (i.e. ZCL 7) 5.2.2.2.1.22 StartUpColorTemperatureMireds Attribute
    // The StartUpColorTemperatureMireds attribute SHALL define the desired startup color
    // temperature values a lamp SHAL use when it is supplied with power and this value SHALL
    // be reflected in the ColorTemperatureMireds attribute. In addition, the ColorMode and
    // EnhancedColorMode attributes SHALL be set to 0x02 (color temperature). The values of
    // the StartUpColorTemperatureMireds attribute are listed in the table below.
    // Value                Action on power up
    // 0x0000-0xffef        Set the ColorTemperatureMireds attribute to this value.
    // 0xffff               Set the ColorTemperatureMireds attribue to its previous value.

    // Initialize startUpColorTempMireds to "maintain previous value" value 0xFFFF
    uint16_t startUpColorTemp = 0xFFFF;
    EmberAfStatus status =
        emberAfReadAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_START_UP_COLOR_TEMPERATURE_MIREDS_ATTRIBUTE_ID,
                             CLUSTER_MASK_SERVER, reinterpret_cast<uint8_t *>(&startUpColorTemp), sizeof(startUpColorTemp), NULL);
    if (status == EMBER_ZCL_STATUS_SUCCESS)
    {
        uint16_t updatedColorTemp = MAX_TEMPERATURE_VALUE;
        status = emberAfReadAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                                      CLUSTER_MASK_SERVER, reinterpret_cast<uint8_t *>(&updatedColorTemp), sizeof(updatedColorTemp),
                                      NULL);
        if (status == EMBER_ZCL_STATUS_SUCCESS)
        {
            uint16_t tempPhysicalMin = readColorTemperatureMin(endpoint);
            uint16_t tempPhysicalMax = readColorTemperatureMax(endpoint);
            if (tempPhysicalMin <= startUpColorTemp && startUpColorTemp <= tempPhysicalMax)
            {
                // Apply valid startup color temp value that is within physical limits of device.
                // Otherwise, the startup value is outside the device's supported range, and the
                // existing setting of ColorTemp attribute will be left unchanged (i.e., treated as
                // if startup color temp was set to 0xFFFF).
                updatedColorTemp = startUpColorTemp;
                status           = emberAfWriteAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID,
                                               ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID, CLUSTER_MASK_SERVER,
                                               reinterpret_cast<uint8_t *>(&updatedColorTemp), ZCL_INT16U_ATTRIBUTE_TYPE);
                if (status == EMBER_ZCL_STATUS_SUCCESS)
                {
                    // Set ColorMode attributes to reflect ColorTemperature.
                    uint8_t updateColorMode = EMBER_ZCL_COLOR_MODE_COLOR_TEMPERATURE;
                    status =
                        emberAfWriteAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID,
                                              CLUSTER_MASK_SERVER, &updateColorMode, ZCL_ENUM8_ATTRIBUTE_TYPE);
                    updateColorMode = EMBER_ZCL_ENHANCED_COLOR_MODE_COLOR_TEMPERATURE;
                    status          = emberAfWriteAttribute(endpoint, ZCL_COLOR_CONTROL_CLUSTER_ID,
                                                   ZCL_COLOR_CONTROL_ENHANCED_COLOR_MODE_ATTRIBUTE_ID, CLUSTER_MASK_SERVER,
                                                   &updateColorMode, ZCL_ENUM8_ATTRIBUTE_TYPE);
                }
            }
        }
    }
#endif
}

void emberAfPluginColorControlServerComputePwmFromHsvCallback(EndpointId endpoint) {}

void emberAfPluginColorControlServerComputePwmFromTempCallback(EndpointId endpoint) {}

void emberAfPluginColorControlServerComputePwmFromXyCallback(EndpointId endpoint) {}
