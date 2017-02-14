/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/pblog/errl.C $                                        */
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

// This file implements support for mapping ERRL-generated hostboot errors
// to PBLOG protobuf style events.

#include "hb_pblog.H"

#include <pblog/pblog.h>
#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <errl/errlud.H>
#include <errl/errludtarget.H>
#include <errl/errlmanager.H>
#include <errl/errlreasoncodes.H>
#include <fapi2.H>
#include <targeting/common/targetservice.H>

#include <errldisplay/errldisplay.H>

#include <console/consoleif.H>

// This is #included by errldisplay.C but it is not safe to include twice (ugh).
// #include <plugins/fapiPlatHwpErrParser.H>
namespace fapi {
extern void parseHwpRc(uint32_t i_rc, char **o_name, char **o_desc);
extern void parseFfdcId(uint32_t i_id, char **o_name);
}

static bool string_decoder(pb_istream_t *stream,
                           const pb_field_t *field,
                           void **arg)
{
    size_t strsize = stream->bytes_left;
    char* str = (char*) malloc( strsize + 1 );
    if( str == NULL )
    {
        return false;
    }

    if (!pb_read( stream, (uint8_t*) str, strsize ))
    {
        free(str);
        return false;
    }
    str[strsize] = '\0';
    if( *arg )
        free( *arg );
    *arg = str;
    return true;
}

static bool string_encoder(pb_ostream_t *stream,
                           const pb_field_t *field,
                           void * const *arg)
{
    bool rc = false;
    char* str = *reinterpret_cast<char * const *>(arg);
    if( str == NULL )
    {
        return true;
    }
    if( !pb_encode_tag_for_field( stream, field ) )
    {
        return false;
    }
    rc = pb_encode_string( stream, (uint8_t*) str, strlen( str ) );
    return rc;
}

static bool device_locator_encoder(pb_ostream_t *stream,
                                        const pb_field_t *field,
                                        void * const *arg)
{
    pblog_DeviceLocator *device
        = static_cast<pblog_DeviceLocator*>(*arg);
    if( !pb_encode_tag_for_field( stream, field ) )
    {
        return false;
    }

    return pb_encode_submessage( stream,
                                 pblog_DeviceLocator_fields,
                                 device );
}

// Helper to set up a callback type 'string' or 'bytes' field in pblog from a
// character string input.  The passed-in 'str' is internally copied and
// eventually free'd after it is encoded.
static void add_string_cb(pb_callback_t *cb, const char *str)
{
    if( str == NULL )
        return;
    if( cb->arg )
        free( cb->arg );
    cb->arg = strdup( str );
    cb->funcs.encode = string_encoder;
}

static void add_target_locator(TARGETING::Target *target,
                               pblog_DeviceLocator *out_locator)
{
    if( target == NULL )
        return;

    // Use HUID value as integer ID.
    out_locator->has_id = true;
    out_locator->id = target->getAttr<TARGETING::ATTR_HUID>();

    // Use device locator string (preferred) or phys path as string ID.
    //TARGETING::ATTR_DEVICE_LOCATOR_type loc;
    TARGETING::ATTR_PHYS_PATH_type phys;
    //if( target->tryGetAttr<TARGETING::ATTR_DEVICE_LOCATOR>( loc ) &&
    //    strlen(loc) > 0)
    //{
    //    add_string_cb( &out_locator->path, loc );
    //}
    //else
    if( target->tryGetAttr<TARGETING::ATTR_PHYS_PATH>( phys ) )
    {
        char *tmp = phys.toString();
        add_string_cb( &out_locator->path, tmp );
        free( tmp );
    }
}

static void add_errl_target(const void * const data, size_t size,
                            pblog_DeviceLocator *out_locator)
{
    const char *char_buf = reinterpret_cast<const char*>( data );

    // The first part of the buffer is a TargetLabel_t.
    const ERRORLOG::TargetLabel_t *label =
        reinterpret_cast<const ERRORLOG::TargetLabel_t*>( char_buf );

    if( label->tag == 0xffffffff )
    {
        out_locator->has_id = false;
        add_string_cb( &out_locator->path, "master_sentinel" );
        return;
    }

    // The second part of the buffer contains the encoded target.
    // See Target::targetFFDC for encoding details.
    // We only care about the HUID and can look up the Target based on that.
    char_buf += sizeof(ERRORLOG::TargetLabel_t);
    const TARGETING::AttributeTraits<TARGETING::ATTR_HUID>::Type *huid =
        reinterpret_cast<const TARGETING::AttributeTraits<
            TARGETING::ATTR_HUID>::Type*>( char_buf );

    // Look up the HUID across all targets.
    TARGETING::Target *target = TARGETING::Target::getTargetFromHuid( *huid );
    add_target_locator(target, out_locator);
}

static fapi2::TargetType getFapiType(TARGETING::TargetHandle_t i_target)
{
    using namespace TARGETING;
    fapi::TargetType o_type;
    switch ( i_target->getAttr<ATTR_TYPE>() )
    {
    case TYPE_PROC:   o_type = fapi::TARGET_TYPE_PROC_CHIP;     break;
    case TYPE_EX:     o_type = fapi::TARGET_TYPE_EX_CHIPLET;    break;
    case TYPE_ABUS:   o_type = fapi::TARGET_TYPE_ABUS_ENDPOINT; break;
    case TYPE_XBUS:   o_type = fapi::TARGET_TYPE_XBUS_ENDPOINT; break;
    case TYPE_MCS:    o_type = fapi::TARGET_TYPE_MCS_CHIPLET;   break;
    case TYPE_MEMBUF: o_type = fapi::TARGET_TYPE_MEMBUF_CHIP;   break;
    case TYPE_MBA:    o_type = fapi::TARGET_TYPE_MBA_CHIPLET;   break;
    case TYPE_DIMM:   o_type = fapi::TARGET_TYPE_DIMM;          break;
    case TYPE_L4:     o_type = fapi::TARGET_TYPE_L4;            break;
    default:          o_type = fapi::TARGET_TYPE_NONE;          break;
    }

    return o_type;
}

static void add_errl_callout(const void * const data, size_t size,
                             pblog_OpenpowerFirmwareEvent *out_errl)
{
    // Parse encoded callout data.  See ErrlUserDetailsCallout class in
    // errludcallout.C for (undocumented) encoding details.
    if( size < sizeof(HWAS::callout_ud_t) )
        return;

    const HWAS::callout_ud_t* callout =
        reinterpret_cast<const HWAS::callout_ud_t*>( data );
    size_t l_curSize = sizeof(HWAS::callout_ud_t);

    switch ( callout->type )
    {
    case HWAS::HW_CALLOUT:
    {
        // Data is formatted as a callout_ud_t followed by an entity path.
        if( callout->deconfigState != HWAS::NO_DECONFIG )
        {
            // TODO flag deconfigured targets specially
        }
        if( size < (l_curSize + sizeof(uint8_t)) )
        {
            break;
        }
        callout++;

        if( *reinterpret_cast<const uint8_t*>( callout )
                == HWAS::TARGET_IS_SENTINEL )
        {
            pblog_DeviceLocator *loc =
                &out_errl->target[out_errl->target_count++];
            loc->has_id = false;
            add_string_cb( &loc->path, "master_sentinel");
            break;
        }
        // Need atleast 1 path element
        if( size < (l_curSize + sizeof(uint8_t) +
            sizeof(TARGETING::EntityPath::PathElement)) )
        {
            break;
        }
        const TARGETING::EntityPath *ep =
            reinterpret_cast<const TARGETING::EntityPath*>( callout );
        TARGETING::Target *target =
            TARGETING::targetService().toTarget( *ep );
        if( target )
        {
            pblog_DeviceLocator *loc =
                &out_errl->target[out_errl->target_count++];
            add_target_locator(target, loc);
        }
        break;
    }
    case HWAS::BUS_CALLOUT:
        break;
    case HWAS::CLOCK_CALLOUT:
        break;
    case HWAS::PROCEDURE_CALLOUT:
        break;
    }
}

// Reverse of fapiPlatHwpInvoker: fapiRcToErrl()
static void add_hwpf_ffdc(const void * const data, size_t size,
                          pblog_Event *event)
{
    if( size < sizeof(uint32_t) )
        return;
    uint32_t ffdcId = *reinterpret_cast<const uint32_t*>( data );

    char *ffdcName = NULL;
    fapi::parseFfdcId( ffdcId, &ffdcName );

    // Rest of the data is either a target, a ecmdbuffer, or a "basic"
    // type.  Unfortunately the encoded data doesn't tell us what
    // type it is =(  See ReturnCode::addErrorInfo() for details.
    const char *chardata =
        reinterpret_cast<const char*>( data ) + sizeof( uint32_t );

    // First try it as a target encoded as an ecmdstring.
    // To do that we try to convert ALL of the targets to ecmdstring and
    // compare since there is no other easy way to get "back" a target
    // from an ecmdstring.
    TARGETING::Target *target = NULL;
    for ( TARGETING::TargetIterator ti = TARGETING::targetService().begin();
        ti != TARGETING::targetService().end(); ++ti )
    {
        fapi::Target fapi_targ( getFapiType( *ti ), *ti );
        if( fapi_targ.getType() != fapi::TARGET_TYPE_NONE &&
            fapi_targ.getType() != fapi::TARGET_TYPE_SYSTEM &&
            fapi_targ.getType() != fapi::TARGET_TYPE_DIMM &&
            strcmp( chardata, fapi_targ.toEcmdString() ) == 0 )
        {
            target = *ti;
            break;
        }
    }
    if( target )
    {
        size_t ntargets = event->openpower_firmware_event.target_count;
        add_target_locator(target,
                           &event->openpower_firmware_event.target[ntargets]);
        event->openpower_firmware_event.target_count++;
    }
    else
    {
        // If not a target then just treat it as a bunch of hex bytes.
        char *byte_str = new char[size * 2 + 1];
        for ( size_t i = 0; i < size - sizeof(uint32_t); ++i )
            sprintf( byte_str + i * 2, "%02X", chardata[i] );
        event_add_kv_data( event, ffdcName ? ffdcName : "ffdc", byte_str );
        delete[] byte_str;
    }
    free( ffdcName );
}

namespace PBLOG
{

void errl_to_event(const errlHndl_t &errl,
                   pblog_Event *event)
{
    event->vendor = pblog_Event_VENDOR_IBM;

    event->has_type = true;
    event->type = pblog_TYPE_OPENPOWER_FIRMWARE_EVENT;

    event->has_openpower_firmware_event = true;

    pblog_OpenpowerFirmwareEvent *fw_event = &event->openpower_firmware_event;
    fw_event->has_source = true;
    fw_event->source =
        static_cast<pblog_OpenpowerFirmwareEvent_Source>(
            errl->reasonCode() & 0xFF00 );

    fw_event->has_epub_subsystem = true;
    fw_event->epub_subsystem =
        static_cast<pblog_OpenpowerFirmwareEvent_EpubSubsystem>(
            errl->subSys() );

    fw_event->has_module_id = true;
    fw_event->module_id = errl->moduleId();

    fw_event->has_reason_code = true;
    fw_event->reason_code = errl->reasonCode();

    // Special parsing support for FAPI/HWPF generated errors.
    if( fw_event->source == pblog_OpenpowerFirmwareEvent_SOURCE_HWPF &&
        fw_event->reason_code == fapi::RC_HWP_GENERATED_ERROR )
    {
        fw_event->has_reason_code = true;
        fw_event->reason_code = errl->getUserData1() & 0xffffffff;

        char *name = NULL, *desc = NULL;
        fapi::parseHwpRc( fw_event->reason_code, &name, &desc );
        add_string_cb( &fw_event->reason_string, name );
        add_string_cb( &fw_event->description, desc );
        free( desc );
        free( name );
    }
    // Special parsing support for PRDF generated errors.
    else if( fw_event->source == pblog_OpenpowerFirmwareEvent_SOURCE_PRDF )
    {
        uint32_t chipId = (errl->getUserData1() >> 32) & 0xffffffff;
        uint32_t signature = (errl->getUserData2() >> 32) & 0xffffffff;

        size_t ntargets = fw_event->target_count;
        // Look up the chip HUID across all targets.
        TARGETING::Target *target =
            TARGETING::Target::getTargetFromHuid( chipId );
        if( target )
        {
            add_target_locator(target, &fw_event->target[ntargets]);
            fw_event->target_count++;
        }

        fw_event->has_reason_code = true;
        fw_event->reason_code = signature;

        const ERRORLOGDISPLAY::ErrLogDisplay::prdfSignatureInfo *prdf =
            ERRORLOGDISPLAY::ErrLogDisplay::findPrdfSignatureInfo( signature );
        if( prdf )
        {
            add_string_cb( &fw_event->reason_string, prdf->name);
            add_string_cb( &fw_event->description, prdf->brief);
        }
    }
    else
    {
        // The rest of the events with just generic information.
        const ERRORLOGDISPLAY::ErrLogDisplay::errLogInfo *info =
            ERRORLOGDISPLAY::errLogDisplay().findErrLogInfo(
                errl->moduleId(), errl->reasonCode() );
        if( info->descriptString
            && strcmp( info->descriptString, "<none>" ) != 0 )
        {
            add_string_cb( &fw_event->description, info->descriptString );
        }
        if( info->moduleName && strcmp( info->moduleName, "unknown" ) != 0 )
        {
            add_string_cb( &fw_event->module_name, info->moduleName );
        }
        if( info->reasonString && strcmp( info->reasonString, "unknown" ) != 0 )
        {
            add_string_cb( &fw_event->reason_string, info->reasonString );
        }
        if( info->userData1String
            && strcmp( info->userData1String, "unknown" ) != 0
            && strcmp( info->userData1String, "<unused>" ) != 0 )
        {
            size_t c = fw_event->user_data_count;
            fw_event->user_data[c].data = errl->getUserData1();
            add_string_cb( &fw_event->user_data[c].description,
                           info->userData1String );
            fw_event->user_data_count++;
        }
        if( info->userData2String
            && strcmp( info->userData2String, "unknown" ) != 0
            && strcmp( info->userData1String, "<unused>" ) != 0 )
        {
            size_t c = fw_event->user_data_count;
            fw_event->user_data[c].data = errl->getUserData2();
            add_string_cb( &fw_event->user_data[c].description,
                           info->userData2String );
            fw_event->user_data_count++;
        }
    }
}

static bool is_memory_training_error(char* error_str)
{
    if( error_str == NULL )
    {
        return false;
    }
    // Check hb/src/usr/hwpf/hwp/dram_training/mss_draminit_training
    // /memory_mss_draminit_training.xml
    const size_t num_errors = 13;
    const char* training_errors[num_errors] = {
        "RC_MSS_DRAMINIT_TRAINING_DRAM_WIDTH_INPUT_ERROR_SETBBM",
        "RC_MSS_DRAMINIT_TRAINING_DRAM_WIDTH_INPUT_ERROR_GETBBM",
        "RC_MSS_DRAMINIT_TRAINING_DIMM_SPARE_INPUT_ERROR",
        "RC_MSS_DRAMINIT_TRAINING_C",
        "RC_MSS_DRAMINIT_TRAINING_WR_LVL_ERROR",
        "RC_MSS_DRAMINIT_TRAINING_DQS_ALIGNMENT_ERROR",
        "RC_MSS_DRAMINIT_TRAINING_RD_CLK_SYS_CLK_ALIGNMENT_ERROR",
        "RC_MSS_DRAMINIT_TRAINING_RD_CENTERING_ERROR",
        "RC_MSS_DRAMINIT_TRAINING_WR_CENTERING_ERROR",
        "RC_MSS_DRAMINIT_TRAINING_COURSE_RD_CENTERING_ERROR",
        "RC_MSS_DRAMINIT_TRAINING_CUSTOM_PATTERN_RD_CENTERING_ERROR",
        "RC_MSS_DRAMINIT_TRAINING_CUSTOM_PATTERN_WR_CENTERING_ERROR",
        "RC_MSS_DRAMINIT_TRAINING_DIGITAL_EYE_ERROR"
    };

    for( size_t i = 0; i < num_errors; i++ )
    {
        if( strcmp( training_errors[i], error_str ) == 0 )
        {
            return true;
        }
    }

    return false;
}

void populate_generic_memory_config_error(pblog_Event *event)
{
    // By now, event has an MBA target and a port position.
    // Check if it's got an OPF
    if( !event->has_openpower_firmware_event )
    {
        return;
    }

    // Check if the reason is a memory reason
    if( !is_memory_training_error(
            (char*) event->openpower_firmware_event.reason_string.arg ) )
    {
        return;
    }

    // Extract Target
    if( event->openpower_firmware_event.target_count <= 0 )
    {
        return;
    }
    // Get Target from device locator on the event
    pblog_DeviceLocator device_locator =
        event->openpower_firmware_event.target[0];
    TARGETING::TargetHandle_t target = NULL;
    for( TARGETING::TargetIterator ti = TARGETING::targetService().begin();
         ti != TARGETING::targetService().end();
         ti++ )
    {
        //TARGETING::ATTR_DEVICE_LOCATOR_type loc;
        TARGETING::ATTR_PHYS_PATH_type phys;
        //if( ti->tryGetAttr<TARGETING::ATTR_DEVICE_LOCATOR>( loc ) &&
        //    strcmp( loc, (char*)(device_locator.path.arg) ) == 0 )
        //{
        //    target = *ti;
        //    break;
        //}
        //else
        if( ti->tryGetAttr<TARGETING::ATTR_PHYS_PATH>( phys ) &&
                 strcmp( phys.toString(),
                         (char*)(device_locator.path.arg) ) == 0 )
        {
            target = *ti;
            break;
        }
    }
    // Ensure we got a target out
    if( target == NULL )
    {
        return;
    }

    // Check if the device locator is a MBA target
    if( target->getAttr<TARGETING::ATTR_TYPE>() != TARGETING::TYPE_MBA )
    {
        return;
    }

    // Extract PORT POSITION
    uint32_t port_position = 0;
    bool port_position_set = false;
    for( size_t i = 0; i < event->data_count; i++ )
    {
        if( strcmp( (char*)(event->data[i].key.arg), "PORT_POSITION" ) == 0 )
        {
            // Port position is stored in the value as a string.
            // Parse the multi-character string.
            // We know it's positive and zero-padded.
            char* position_str = (char*) event->data[i].value.arg;
            size_t length = strlen(position_str);
            for( size_t index = 0; index < length; index++ )
            {
                port_position *= 10;
                uint32_t digit = position_str[index] - '0';
                port_position += digit;
            }
            port_position_set = true;
            break;
        }
    }

    // Get DIMM child targets
    TARGETING::TargetHandleList dimms;
    TARGETING::targetService().getAssociated(
        dimms,
        target,
        TARGETING::TargetService::CHILD_BY_AFFINITY,
        TARGETING::TargetService::IMMEDIATE );

    // Make sure the port position could fit into the dimm list
    if( !port_position_set || dimms.size() <= port_position )
    {
        return;
    }

    // Grab a handle to the appropriate dimm target
    TARGETING::TargetHandle_t dimm_target = dimms[port_position];

    // Create a PbLogEventGenericMemoryConfigurationError
    event->has_generic_memory_configuration_error = true;
    pblog_PbLogEventGenericMemoryConfigurationError *mem_err =
        &event->generic_memory_configuration_error;

    // populate it
    mem_err->type
        = pblog_PbLogEventGenericMemoryConfigurationError_TYPE_TRAINING_FAILURE;

    mem_err->device.arg
        = (pblog_DeviceLocator*) malloc( sizeof( pblog_DeviceLocator ) );
    pblog_DeviceLocator *dimm_loc =
        (pblog_DeviceLocator*) mem_err->device.arg;
    mem_err->device.funcs.encode = device_locator_encoder;
    add_target_locator( dimm_target, dimm_loc );
    TARGETING::ATTR_POSITION_type position;
    if( dimm_target->tryGetAttr<TARGETING::ATTR_POSITION>( position ) )
    {
        dimm_loc->id = position;
    }
}

void errl_user_data_to_event(uint16_t sid, uint8_t ver, uint8_t sst,
                             compId_t compId,
                             const void * const data, size_t size,
                             pblog_Event *event)
{
    switch ( compId )
    {
    case HWPF_COMP_ID:
        switch ( sst )
        {
        case fapi::HWPF_UDT_HWP_FFDC:
            add_hwpf_ffdc( data, size, event );
            break;
        }
        break;
    case ERRL_COMP_ID:
        switch ( sst )
        {
        case ERRORLOG::ERRL_UDT_TARGET:
            add_errl_target(
                data, size,
                &event->openpower_firmware_event.target[
                    event->openpower_firmware_event.target_count++]);
            break;
        case ERRORLOG::ERRL_UDT_CALLOUT:
            add_errl_callout( data, size, &event->openpower_firmware_event );
            break;
        case ERRORLOG::ERRL_UDT_STRING:
        {
            char keyname[16];
            sprintf( keyname, "string_%d", sid );
            event_add_kv_data(
                event, keyname,
                reinterpret_cast<const char*>( data ) );
            break;
        }
        case ERRORLOG::ERRL_UDT_BACKTRACE:
            break;
        }
        break;
    }
}

void errl_decode_init(pblog_Event *event)
{
    event->openpower_firmware_event.description.funcs.decode = string_decoder;
    event->openpower_firmware_event.module_name.funcs.decode = string_decoder;
    event->openpower_firmware_event.reason_string.funcs.decode = string_decoder;
    for ( size_t i = 0;
        i < pb_arraysize( pblog_OpenpowerFirmwareEvent, user_data ); ++i )
    {
        event->openpower_firmware_event.user_data[i].description.funcs.decode =
            string_decoder;
    }
    for ( size_t i = 0;
        i < pb_arraysize( pblog_OpenpowerFirmwareEvent, target ); ++i )
    {
        event->openpower_firmware_event.target[i].path.funcs.decode =
            string_decoder;
    }
}

void errl_event_free(pblog_Event *event)
{
    free( event->openpower_firmware_event.description.arg );
    free( event->openpower_firmware_event.module_name.arg );
    free( event->openpower_firmware_event.reason_string.arg );
    for ( size_t i = 0;
        i < pb_arraysize( pblog_OpenpowerFirmwareEvent, user_data ); ++i )
    {
        free( event->openpower_firmware_event.user_data[i].description.arg );
    }
    for ( size_t i = 0;
        i < pb_arraysize( pblog_OpenpowerFirmwareEvent, target ); ++i )
    {
        free( event->openpower_firmware_event.target[i].path.arg );
    }
    free( event->generic_memory_configuration_error.device.arg );
}

}  // namespace PBLOG
