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

#include "time-synchronization-server.h"

#if OPENTHREAD_CONFIG_TIME_SYNC_ENABLE
#include <openthread/network_time.h>
#endif

#include <app/util/af.h>

#include <app/common/gen/attribute-id.h>
#include <app/common/gen/attribute-type.h>
#include <app/common/gen/cluster-id.h>
#include <app/util/af-event.h>
#include <app/util/attribute-storage.h>

#include <support/logging/CHIPLogging.h>

using namespace chip;
using namespace chip::app;

#ifndef emberAfTimeSyncClusterPrintln
#define emberAfTimeSyncClusterPrintln(...) ChipLogProgress(Zcl, __VA_ARGS__);
#endif

bool emberAfTimeSyncClusterSetUtcTimeCallback(chip::EndpointId endpoint, chip::app::CommandSender * commandObj, uint64_t UtcTime,
                                              uint8_t Granularity, uint8_t TimeSource)
{
    return EMBER_ZCL_STATUS_FAILURE;
}

EmberAfStatus emberAfTimeSyncClusterGetUtcTime(chip::EndpointId endpoint, uint64_t * timeUTC)
{
    return emberAfReadServerAttribute(endpoint, ZCL_TIMESYNC_CLUSTER_ID, ZCL_TIME_SYNC_UTC_TIME_ATTRIBUTE_ID,
                                       (uint8_t *) timeUTC, sizeof(*timeUTC));
}
