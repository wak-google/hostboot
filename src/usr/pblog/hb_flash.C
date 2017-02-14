/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/pblog/hb_flash.C $                                    */
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

#include "hb_flash.H"
#include <pblog/common.h>
#include <pblog/flash.h>

#include <devicefw/userif.H>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errl/errlmanager.H>

static int pnor_read(pblog_flash_ops *ops, int offset, size_t len, void *data)
{
    TARGETING::Target* pnor_target =
        TARGETING::MASTER_PROCESSOR_CHIP_TARGET_SENTINEL;

    errlHndl_t l_errhdl = DeviceFW::deviceRead( pnor_target,
                                                data,
                                                len,
                                                DEVICE_PNOR_ADDRESS(0, offset) );
    if( l_errhdl != NULL )
    {
        delete l_errhdl;  // avoid recursive errl
        return PBLOG_ERR_IO;
    }

    return len;
}

static int pnor_write(pblog_flash_ops *ops, int offset, size_t len,
                      const void *data)
{
    TARGETING::Target* pnor_target =
        TARGETING::MASTER_PROCESSOR_CHIP_TARGET_SENTINEL;

    errlHndl_t l_errhdl = DeviceFW::deviceWrite( pnor_target,
                                                 const_cast<void*>( data ),
                                                 len,
                                                 DEVICE_PNOR_ADDRESS(0, offset) );
    if( l_errhdl != NULL )
    {
        delete l_errhdl;  // avoid recursive errl
        return PBLOG_ERR_IO;
    }

    return len;
}

static int pnor_erase(pblog_flash_ops *ops, int offset, size_t len)
{
    TARGETING::Target* pnor_target =
        TARGETING::MASTER_PROCESSOR_CHIP_TARGET_SENTINEL;

    // As an optimization, check if the if the region is already erased.
    // Reading is faster than erasing.
    uint8_t *buf = static_cast<uint8_t*>( malloc( len ) );
    if( buf )
    {
        memset( buf, 0, len );
        pnor_read( ops, offset, len, buf );
        bool erased = true;
        for ( size_t i = 0; i < len; ++i )
        {
            if( buf[i] != 0xff )
            {
                erased = false;
                break;
            }
        }
        if( erased )
        {
            return PBLOG_SUCCESS;
        }
    }

    // Write NULL to indicate erase op with the PnorDD API.
    errlHndl_t l_errhdl = DeviceFW::deviceWrite( pnor_target,
                                                 NULL,
                                                 len,
                                                 DEVICE_PNOR_ADDRESS(0, offset) );
    if( l_errhdl != NULL )
    {
        delete l_errhdl;  // avoid recursive errl
        return PBLOG_ERR_IO;
    }

    return PBLOG_SUCCESS;
}

struct pblog_flash_ops pnor_ops = {
    pnor_read,
    pnor_write,
    pnor_erase
};
