/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/pblog/event_to_str.c $                                */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2017                             */
/* [+] Google Inc.                                                        */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pblog/pblog.pb.h>

// Helper to append 'sep' and 'appendval' to the input string.
static void join_str(char *str,
                     size_t len,
                     const void *appendval,
                     const char *sep) {
    if (appendval) {
        strncat(str, sep, len - strnlen(str, len));
        strncat(str, (const char *)appendval, len - strnlen(str, len));
    }
}

char *event_to_str(const pblog_Event *event, const char *sep)
{
    static const size_t kBufSize = 4096;
    size_t i = 0;
    char *buf = (char*)malloc(kBufSize);
    memset(buf, 0, kBufSize);

    strncat(buf, pblog_Event_Vendor_name(event->vendor), kBufSize);
    join_str(buf, kBufSize, pblog_event_type_name(event->type), sep);

    if (event->has_openpower_firmware_event)
    {
        const pblog_OpenpowerFirmwareEvent *opf =
            &event->openpower_firmware_event;
        join_str(buf, kBufSize, pblog_OpenpowerFirmwareEvent_Source_name(
            opf->source), sep);
        join_str(buf, kBufSize, pblog_OpenpowerFirmwareEvent_EpubSubsystem_name(
            opf->epub_subsystem), sep);
        join_str(buf, kBufSize, opf->module_name.arg, sep);
        join_str(buf, kBufSize, opf->reason_string.arg, sep);
        join_str(buf, kBufSize, opf->description.arg, sep);

        for (i = 0; i < opf->user_data_count; ++i)
        {
            char user_data_str[128];
            if (!opf->user_data[i].description.arg)
                continue;
            snprintf(user_data_str, sizeof(user_data_str), "%s=0x%016llX",
                     (const char *) opf->user_data[i].description.arg,
                     opf->user_data[i].data);
            join_str(buf, kBufSize, user_data_str, sep);
        }

        for (i = 0; i < opf->target_count; ++i)
        {
            char targetstr[128];
            if (!opf->target[i].path.arg)
                continue;
            snprintf(targetstr, sizeof(targetstr), "%08llX:%s",
                     opf->target[i].id,
                     (const char *) opf->target[i].path.arg);
            join_str(buf, kBufSize, targetstr, sep);
        }
    }

    for (i = 0; i < event->data_count; ++i)
    {
        strncat(buf, sep, kBufSize - strnlen(buf, kBufSize));
        strncat(buf, event->data[i].key.arg, kBufSize - strnlen(buf, kBufSize));
        strncat(buf, "=", kBufSize - strnlen(buf, kBufSize));
        strncat(buf, event->data[i].value.arg, kBufSize - strnlen(buf, kBufSize));
    }

    if (event->has_generic_memory_configuration_error)
    {
        join_str(buf, kBufSize,
            (char*) pblog_PbLogEventGenericMemoryConfigurationError_Type_name(
                event->generic_memory_configuration_error.type), sep);

        pblog_DeviceLocator *locator =
            event->generic_memory_configuration_error.device.arg;
        if(locator != NULL)
        {
            char devicestr[128];
            snprintf(devicestr, sizeof(devicestr), "%08X:%s",
                     locator->id,
                     locator->path.arg);
            join_str(buf, kBufSize, devicestr, sep);
        }
    }

    if (event->has_generic_shutdown)
    {
        join_str(buf, kBufSize,
            pblog_PbLogEventGenericShutdown_Type_name(
                event->generic_shutdown.type), sep);
    }
    if (event->has_generic_reset)
    {
        join_str(buf, kBufSize,
            pblog_PbLogEventGenericReset_Type_name(
            event->generic_reset.type), sep);
    }

    return buf;
}
