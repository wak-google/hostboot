/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/pblog/hb_pblog.H $                                    */
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

#ifndef __HB_PBLOG_PBLOG_H
#define __HB_PBLOG_PBLOG_H

#include <stddef.h>
#include <stdint.h>
#include <errl/errlentry.H>
#include <pblog/pblog.h>
#include <pblog/pblog.pb.h>

struct nvram;
struct pblog;
struct record_intf;

// From event_to_str.c.
extern "C" char *event_to_str(const pblog_Event *event, const char *sep);

namespace PBLOG
{

void errl_to_event(const errlHndl_t &errl, pblog_Event *event);
void errl_user_data_to_event(uint16_t sid, uint8_t ver, uint8_t sst,
                             compId_t compId,
                             const void * const data, size_t size,
                             pblog_Event *event);
void errl_decode_init(pblog_Event *event);
void errl_event_free(pblog_Event *event);
void populate_generic_memory_config_error( pblog_Event *event );

class Pblog {
public:
    Pblog();
    ~Pblog();

    // Initialize data structures.
    void initLog();

    void initNvram();

    // If key is found, allocates *out_data and returns number of bytes of data.
    // Caller owns allocated *out_data and must free() it.
    // Returns <0 if not found or error.
    int nvramLookup(const char *key, char **out_data);

    void addErrlEvent(const errlHndl_t &errl);

    // Adds the event to the persistent log.  No failure is returned to avoid
    // infinite error looping.
    void addEvent(pblog_Event *event);

#ifdef CONFIG_PBLOG_PRINT
    // Print the most recent 10 events logged as part of the boot process.
    void printRecentEvents();
#endif

    static uint32_t cv_bootnum;

private:
    static uint32_t getTimeNow(struct pblog *pblog);
    static uint32_t getBootnum(struct pblog *pblog);

#ifdef CONFIG_PBLOG_PRINT
    static pblog_status printEventCallback(int valid,
                                           const pblog_Event *event,
                                           void *priv);
#endif

    // Initializes the bootnum to the next highest value.
    void findNextBootnum();
    static pblog_status getHighestBootnumCallback(int valid,
                                                  const pblog_Event *event,
                                                  void *priv);

    struct record_intf *iv_records;
    struct pblog *iv_pblog;
    int iv_event_count;

    struct record_intf *iv_nvram_records;
    struct nvram *iv_nvram;
};

}  // namespace PBLOG

#endif  // __HB_PBLOG_PBLOG_H
