/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/include/usr/pblog/pblogif.H $                             */
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

#ifndef __PBLOG_PBLOGIF_H
#define __PBLOG_PBLOGIF_H

extern "C" {
#include <pblog/pblog.pb.h>
}

#include <errl/errlentry.H>
#include <pnor/pnorif.H>

namespace NVRAM
{

// Some common/static NVRAM key names.
static const char kNvramMcsGrouping[] = "hb,mcs-grouping";
static const char kNvramSmt[] = "sbe,smt";

// If key is found, allocates *out_data and returns number of bytes of data.
// Caller owns allocated *out_data and must free() it.
// Returns <0 if not found or error.
int lookup(const char *key, char **out_data);

};

namespace PBLOG
{
    // TODO: base this on reading the erase size?
    static const int PBLOG_REGION_SIZE = 64 * KILOBYTE;
    static const PNOR::SectionId PBLOG_PNOR_SECTION = PNOR::PBLOG;

    errlHndl_t logPowerOnEvent(pblog_event_type event_type);
    errlHndl_t logBootEvent();
    void logErrlEvent(const errlHndl_t &errl);
    uint32_t getBootnum();
};

#endif
