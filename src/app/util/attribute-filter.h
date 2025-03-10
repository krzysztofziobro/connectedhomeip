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

#pragma once

#include <app/util/af-enums.h>

#include <core/CHIPCallback.h>
#include <core/CHIPTLV.h>
#include <lib/support/FunctionTraits.h>

typedef void (*DefaultSuccessCallback)(void * context);
typedef void (*DefaultFailureCallback)(void * context, uint8_t status);

/**
 * BasicAttributeFilter accepts the actual type of onSuccess callback as template parameter.
 * It will check whether the type of the TLV data is expected by onSuccess callback.
 * If a non expected value received, onFailure callback will be called with EMBER_ZCL_STATUS_INVALID_VALUE.
 */
template <typename CallbackType>
void BasicAttributeFilter(chip::TLV::TLVReader * data, chip::Callback::Cancelable * onSuccess,
                          chip::Callback::Cancelable * onFailure)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    typename chip::FunctionTraits<CallbackType>::template ArgType<1> value;

    if ((err = data->Get(value)) == CHIP_NO_ERROR)
    {
        chip::Callback::Callback<CallbackType> * cb = chip::Callback::Callback<CallbackType>::FromCancelable(onSuccess);
        cb->mCall(cb->mContext, value);
    }
    else
    {
        ChipLogError(Zcl, "Failed to get value from TLV data for attribute reading response: %s", chip::ErrorStr(err));
        chip::Callback::Callback<DefaultFailureCallback> * cb =
            chip::Callback::Callback<DefaultFailureCallback>::FromCancelable(onFailure);
        cb->mCall(cb->mContext, EMBER_ZCL_STATUS_INVALID_VALUE);
    }
}
