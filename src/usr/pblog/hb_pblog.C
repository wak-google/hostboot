/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/pblog/hb_pblog.C $                                    */
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

#include "hb_pblog.H"
#include "hb_flash.H"

#include <pblog/common.h>
#include <pblog/nvram.h>
#include <pblog/record.h>
#include <pblog/pblog.h>
#include <nanopb/pb_decode.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <errl/errlud.H>
#include <errl/errludtarget.H>
#include <errl/errlmanager.H>
#include <targeting/common/targetservice.H>

#include <initservice/taskargs.H>
#include <usr/pblog/pblogif.H>
#include <usr/pnor/pnorif.H>

#ifdef CONFIG_BMC_EC
#include <ec/platform.H>
#include <ec/rtcif.H>
#endif
#include <console/consoleif.H>

trace_desc_t* g_trac_pblog = NULL;
TRAC_INIT(&g_trac_pblog, "PBLOG", 4*KILOBYTE, TRACE::BUFFER_SLOW);

#ifdef CONFIG_PBLOG_PRINT
// callback function to print out pblog debug/trace statements
int pblog_printf(int severity, const char *format, ...)
{
    char str[4096];
    if (severity == 0)  // skip debug traces
    {
        return 0;
    }

    va_list args;
    va_start( args, format );
    int length = vsprintf( str, format, args ) + sizeof('\0');
    va_end( args );

    // Remove trailing newline character.
    str[strlen( str ) - 1] = ' ';
    TRACFCOMP( g_trac_pblog, "%s", str );
    return length;
}
#endif

// This is the task entry point.  Log a power on event and print out the
// recent events.
static void pblogEntryPoint(errlHndl_t &io_taskRetErrl)
{
    io_taskRetErrl = PBLOG::logPowerOnEvent(pblog_TYPE_POWER_ON);

    Singleton<PBLOG::Pblog>::instance().printRecentEvents();
}
TASK_ENTRY_MACRO( pblogEntryPoint );

extern char hbi_Version[];


namespace NVRAM {

int lookup(const char *key, char **out_data)
{
    return Singleton<PBLOG::Pblog>::instance().nvramLookup(key, out_data);
}

}  // namespace NVRAM

namespace PBLOG {

errlHndl_t logPowerOnEvent(pblog_event_type event_type)
{
    pblog_Event event;
    event_init(&event);
    event.has_type = true;
    event.type = event_type;
    event_add_kv_data(&event, "hostboot", hbi_Version);
    Singleton<Pblog>::instance().addEvent( &event );
    event_free(&event);
    return NULL;
}

errlHndl_t logBootEvent()
{
    pblog_Event event;
    event_init(&event);
    event.has_type = true;
    event.type = pblog_TYPE_BOOT_UP;
    event_add_kv_data(&event, "sapphire", hbi_Version);
    Singleton<Pblog>::instance().addEvent( &event );
    event_free(&event);
    return NULL;
}

void logErrlEvent(const errlHndl_t &errl)
{
    Singleton<Pblog>::instance().addErrlEvent( errl );
}

uint32_t getBootnum()
{
    return Singleton<Pblog>::instance().cv_bootnum;
}

uint32_t Pblog::cv_bootnum = 0;

Pblog::Pblog()
    : iv_records( NULL ), iv_pblog( NULL ), iv_event_count( 0 ),
      iv_nvram_records( NULL), iv_nvram( NULL)
{
    initLog();
#ifdef CONFIG_NVRAM
    initNvram();
#endif  // CONFIG_NVRAM
}

Pblog::~Pblog()
{
    if( iv_pblog )
    {
        pblog_free( iv_pblog );
        delete iv_pblog;
        record_intf_free(iv_records);
        delete iv_records;
    }
#ifdef CONFIG_NVRAM
    if ( iv_nvram )
    {
        pblog_nvram_free( iv_nvram );
        delete iv_nvram;
        delete iv_records;
    }
#endif  // CONFIG_NVRAM
}

void Pblog::initLog()
{
    errlHndl_t err = NULL;

    PNOR::SectionInfo_t log_section;
    err = PNOR::getSectionInfo( PBLOG::PBLOG_PNOR_SECTION, log_section );
    if( err )
    {
        TRACFCOMP( g_trac_pblog, "PBLOG failure to find PNOR section" );
        delete err;
        return;
    }
    TRACFCOMP( g_trac_pblog, "PBLOG using flash section %s, %lld bytes @0x%llx",
               log_section.name, log_section.size, log_section.flashAddr );

    // Set up some regions to write event records.
    int num_regions = log_section.size / PBLOG::PBLOG_REGION_SIZE;
    assert(num_regions > 0);
    record_region *regions = new record_region[num_regions];
    for ( int i = 0; i < num_regions; ++i )
    {
        regions[i].offset = log_section.flashAddr + i * PBLOG::PBLOG_REGION_SIZE;
        regions[i].size = PBLOG::PBLOG_REGION_SIZE;
    }

    iv_records = new record_intf;
    int rc;
    rc = record_intf_init( iv_records, regions, num_regions, &pnor_ops );
    // regions has been copied by record_intf_init
    delete[] regions;
    if( rc != PBLOG_SUCCESS )
    {
        TRACFCOMP( g_trac_pblog, "record_intf_init failure! rc=%d", rc );
        return;
    }

    // Initialize the logging functionality.
    iv_pblog = new struct pblog;
    iv_pblog->get_current_bootnum = &Pblog::getBootnum;
    iv_pblog->get_time_now = &Pblog::getTimeNow;
    iv_event_count = pblog_init( iv_pblog, true, iv_records, NULL, 0 );
    if( iv_event_count < 0 )
    {
        TRACFCOMP( g_trac_pblog, "pblog_init failure! rc=%d", rc );
        delete iv_pblog;
        iv_pblog = NULL;
        return;
    }

    // Look through the log to figure out the current/next bootnum.
    findNextBootnum();

    TRACFCOMP( g_trac_pblog, "PBLOG initialized, found %d events",
               iv_event_count );

#ifdef CONFIG_PBLOG
    ERRORLOG::ErrlManager::errlResourceReady(ERRORLOG::PNOR);
#endif
}

void Pblog::initNvram()
{
    errlHndl_t err = NULL;

    PNOR::SectionInfo_t section;
    err = PNOR::getSectionInfo( PNOR::NVRAM, section );
    if( err )
    {
        TRACFCOMP( g_trac_pblog, "failure to find NVRAM PNOR section" );
        delete err;
        return;
    }
    TRACFCOMP( g_trac_pblog, "NVRAM using flash section %s, %lld bytes @0x%llx",
               section.name, section.size, section.flashAddr );

    // Set up regions.
    int num_regions = section.size / PBLOG::PBLOG_REGION_SIZE;
    assert(num_regions > 0);
    record_region *regions = new record_region[num_regions];
    for ( int i = 0; i < num_regions; ++i )
    {
        regions[i].offset = section.flashAddr + i * PBLOG::PBLOG_REGION_SIZE;
        regions[i].size = PBLOG::PBLOG_REGION_SIZE;
    }

    iv_nvram_records = new record_intf;
    int rc;
    rc = record_intf_init( iv_nvram_records, regions, num_regions, &pnor_ops );
    // regions has been copied by record_intf_init
    delete[] regions;
    if( rc != PBLOG_SUCCESS )
    {
        TRACFCOMP( g_trac_pblog, "record_intf_init failure! rc=%d", rc );
        return;
    }

    // Initialize the NVRAM functionality.
    iv_nvram = new struct nvram;
    pblog_nvram_init(iv_nvram, iv_nvram_records);
}

int Pblog::nvramLookup(const char *key, char **out_data)
{
    static const int kMaxNvramSize = 4096;
    *out_data = NULL;
    if( iv_nvram == NULL )
        return -1;

    *out_data = static_cast<char*>( malloc( kMaxNvramSize ) );
    memset( *out_data, 0, kMaxNvramSize );
    int rc = iv_nvram->lookup( iv_nvram, key, *out_data, kMaxNvramSize );
    if( rc <= 0 )
    {
        free( *out_data );
        *out_data = NULL;
        return -1;
    }
    TRACFCOMP( g_trac_pblog, "NVRAM found setting: %s=%s", key, *out_data);
    return rc;
}


void Pblog::addEvent(pblog_Event *event)
{
    if( iv_pblog == NULL )
    {
        return;
    }

    int tmpcount = 0;
    TRACFCOMP( g_trac_pblog, "PBLOG logging event:");
    printEventCallback(true, event, &tmpcount);

    int rc;
    rc = iv_pblog->add_event( iv_pblog, event );
    if( rc != 0 )
    {
        TRACFCOMP( g_trac_pblog, "add_event failure rc=%d", rc );
        return;
    }

    iv_event_count++;
}

void Pblog::addErrlEvent(const errlHndl_t &errl)
{
    if( iv_pblog == NULL )
    {
        return;
    }

    pblog_Event *event = new pblog_Event;
    event_init(event);
    errl_to_event(errl, event);
    // Loop through and parse all of the user data sections.
    for ( size_t i = 0; i < errl->iv_SectionVector.size(); ++i )
    {
        ERRORLOG::ErrlUD *user_data = errl->iv_SectionVector[i];
        errl_user_data_to_event( user_data->iv_header.iv_sid,
                                 user_data->iv_header.iv_ver,
                                 user_data->iv_header.iv_sst,
                                 user_data->iv_header.iv_compId,
                                 user_data->data(),
                                 user_data->dataSize(),
                                 event );
    }

    populate_generic_memory_config_error( event );

    addEvent( event );
    errl_event_free( event );
    event_free( event );
    delete event;
}

#ifdef CONFIG_PBLOG_PRINT
void Pblog::printRecentEvents()
{
    if( iv_pblog == NULL )
    {
        return;
    }

    TRACFCOMP( g_trac_pblog, "Most recent logged events:");

    int rc;
    int count = iv_event_count;
    pblog_Event *event = new pblog_Event;
    event_init(event);
    errl_decode_init(event);
    rc = iv_pblog->for_each_event( iv_pblog,
                                   &Pblog::printEventCallback,
                                   event,
                                   &count );
    errl_event_free(event);
    event_free(event);
    delete event;
    if( rc != 0 )
    {
        TRACFCOMP( g_trac_pblog, "for_each_event failure rc=%d", rc );
        return;
    }
}

pblog_status Pblog::printEventCallback(int valid,
                                       const pblog_Event *event,
                                       void *priv)
{
    int *count = static_cast<int*>(priv);
    if (*count < 10 && valid) {
        char str[4096];
#ifdef CONFIG_BMC_EC
        struct EC::RtcTime tm;
        EC::gmtime(event->timestamp, &tm);

        // Print out the counter, bootnum and timestamp of the event.
        sprintf(str, "#%d | %d | %02d:%02d:%02d %02d/%02d/%02d | ",
                *count,
                event->boot_number,
                tm.hour,
                tm.min,
                tm.sec,
                tm.mday,
                tm.mon,
                tm.year + 1900);
#endif

        // Add event-specific parameter string.
        char *event_string = event_to_str(event, " | ");
        strncat(str, event_string, sizeof(str));
        free(event_string);

        TRACFCOMP( g_trac_pblog, "%s", str );
    }

    *count -= 1;
    return PBLOG_SUCCESS;
}
#endif

void Pblog::findNextBootnum()
{
    if( iv_pblog == NULL )
    {
        return;
    }

    // Find the maximum bootnum recorded in the log.
    cv_bootnum = 0;
    pblog_Event *event = new pblog_Event;
    event_init(event);
    iv_pblog->for_each_event( iv_pblog,
                              &Pblog::getHighestBootnumCallback,
                              event,
                              &cv_bootnum );
    event_free(event);
    delete event;
    // Bump the boot number by 1 to represent this boot.
    cv_bootnum++;
}

pblog_status Pblog::getHighestBootnumCallback(int valid,
                                              const pblog_Event *event,
                                              void *priv)
{
    uint32_t *bootnum = static_cast<uint32_t*>(priv);
    if (valid && event->has_boot_number && event->boot_number > *bootnum)
    {
        *bootnum = event->boot_number;
    }
    return PBLOG_SUCCESS;
}

uint32_t Pblog::getTimeNow(struct pblog *pblog)
{
#ifdef CONFIG_BMC_EC
    // Read time from the EC, convert to time_t.
    errlHndl_t errl;
    EC::RtcTime time;
    errl = EC::rtcRead(&time);
    if (errl) {
        return 0;
    }

    return EC::mktime(&time);
#else
    return 0;
#endif
}

uint32_t Pblog::getBootnum(struct pblog *pblog)
{
    return cv_bootnum;
}

}  // namespace PBLOG
