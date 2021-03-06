/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/secureboot/base/securerom.C $                         */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2013,2017                        */
/* [+] International Business Machines Corp.                              */
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
#include <secureboot/service.H>
#include <secureboot/secure_reasoncodes.H>
#include <sys/mmio.h>
#include <kernel/pagemgr.H>
#include <limits.h>
#include <targeting/common/commontargeting.H>
#include <targeting/common/targetservice.H>
#include <devicefw/driverif.H>
#include <errl/errlentry.H>
#include <errl/errlmanager.H>
#include "../common/securetrace.H"

#include "securerom.H"
#include <secureboot/settings.H>

// Quick change for unit testing
//#define TRACUCOMP(args...)  TRACFCOMP(args)
#define TRACUCOMP(args...)


// Hardcode define for Secure ROM code (bootrom.bin) in system
// Secure ROM has 16KB reserved address space
#define SECUREROM_MEMORY_SIZE (16 * KILOBYTE)
// 4 pages * (PAGESIZE=4K) = 16K
#define SECUREROM_NUM_PAGES (SECUREROM_MEMORY_SIZE / PAGESIZE)

namespace SECUREBOOT
{


/**
 * @brief Initialize Secure Rom by loading it into memory and
 *        retrieving Hash Keys
 */
errlHndl_t initializeSecureROM(void)
{
    return Singleton<SecureROM>::instance().initialize();
}


// TODO securebootp9 - the method signature below was brought in from
// p8. There are many more changes need to this file however, in order to
// be considered up-to-date.
/**
 * @brief Verify Signed Container
 */
errlHndl_t verifyContainer(void * i_container, const sha2_hash_t* i_hwKeyHash)
{
    TRACUCOMP(g_trac_secure, "verifyContainer(): i_container=%p, size=0x%x",
              i_container, i_size);

    return Singleton<SecureROM>::instance().verifyContainer(i_container,
                                                            i_hwKeyHash);
}

/**
 * @brief Hash Signed Blob
 *
 */
errlHndl_t hashBlob(const void * i_blob, size_t i_size, SHA512_t io_buf)
{
    return Singleton<SecureROM>::instance().hashBlob(i_blob, i_size, io_buf);
}

/**
 * @brief Hash concatenation of 2 Blobs
 *
 */
errlHndl_t hashConcatBlobs(const blobPair_t &i_blobs, SHA512_t o_buf)
{
    return Singleton<SecureROM>::instance().hashConcatBlobs(i_blobs, o_buf);
}

/*
 * @brief  Externally available hardware keys' hash retrieval function
 */
void getHwKeyHash(sha2_hash_t o_hash)
{
    return Singleton<SecureROM>::instance().getHwKeyHash(o_hash);
}

}; //end SECUREBOOT namespace

/********************
 Public Methods
 ********************/

// allow external methods to access g_trac_secure
using namespace SECUREBOOT;

/**
 * @brief Initialize Secure Rom by loading it into memory and
 *        getting Hash Keys
 */
errlHndl_t SecureROM::initialize()
{
    TRACDCOMP(g_trac_secure,ENTER_MRK"SecureROM::initialize()");

    errlHndl_t l_errl = NULL;
#if (0)
    bool l_cleanup = false;
    uint32_t l_rc = 0;

    do{

        // Check to see if ROM has already been initialized
        if (iv_device_ptr != NULL)
        {
            // The Secure ROM has already been initialized
            TRACUCOMP(g_trac_secure,"SecureROM::initialize(): Already "
                      "Loaded: iv_device_ptr=%p", iv_device_ptr);

            // Can skip the rest of this function
            break;
        }


        /*********************************************************************/
        /*  Find base address of Secure ROM via TBROM_BASE_REG scom register */
        /*********************************************************************/

        const uint32_t tbrom_reg_addr = 0x02020017;
        uint64_t tbrom_reg_data;
        size_t op_size = sizeof(uint64_t);

        l_errl = deviceRead( TARGETING::MASTER_PROCESSOR_CHIP_TARGET_SENTINEL,
                             &(tbrom_reg_data),
                             op_size,
                             DEVICE_SCOM_ADDRESS(tbrom_reg_addr) );

        if (l_errl != NULL)
        {
            TRACFCOMP(g_trac_secure,ERR_MRK"SecureROM::initialize():"
            " Fail SCOM Read of tbrom_reg_addr (0x%x)", tbrom_reg_addr);

            break;
        }


        TRACUCOMP(g_trac_secure,INFO_MRK"SecureROM::initialize(): "
                  "tbrom_reg_data = 0x%016llx", tbrom_reg_data);


        // This register contains the starting address of the bootrom device
        void * l_rom_baseAddr = reinterpret_cast<void*>(tbrom_reg_data);


        /*******************************************************************/
        /*  Map the bootrom code into virtual memory                       */
        /*******************************************************************/
        void * l_rom_virtAddr = mmio_dev_map(l_rom_baseAddr, THIRTYTWO_GB);

        if (l_rom_virtAddr == NULL)
        {
            TRACFCOMP(g_trac_secure,ERR_MRK"SecureROM::initialize():"
            " mmio_dev_map failed: l_rom_virtAddr=%p, l_rom_baseAddr=%p",
            l_rom_virtAddr, l_rom_baseAddr);

            /*@
             * @errortype
             * @moduleid     SECUREBOOT::MOD_SECURE_ROM_INIT
             * @reasoncode   SECUREBOOT::RC_DEV_MAP_FAIL
             * @userdata1    TBROM Register Address
             * @userdata2    TBROM Register Data
             * @devdesc      mmio_dev_map() failed for Secure ROM
             * @custdesc     A problem occurred during the IPL of the system.
             */
            l_errl = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                            SECUREBOOT::MOD_SECURE_ROM_INIT,
                                            SECUREBOOT::RC_DEV_MAP_FAIL,
                                            TO_UINT64(tbrom_reg_addr),
                                            tbrom_reg_data,
                                            true /*Add HB Software Callout*/ );

            l_errl->collectTrace(SECURE_COMP_NAME,256);
            break;

        }

        /**********************************************************************/
        /*  Allocate Memory: Request full SECUREROM_MEMORY_SIZE               */
        /**********************************************************************/

        // Using malloc() rather than allocatePage because malloc() will
        // handle error path
        iv_device_ptr = malloc(SECUREROM_MEMORY_SIZE);

        // Pages are now allocated, so free below if necessary
        l_cleanup = true;

        /***************************************************************/
        /*  Copy and setup ROM code in allocated memory                */
        /***************************************************************/

        //  memcpy from mapped device to allocated pages
        memcpy( iv_device_ptr, l_rom_virtAddr, SECUREROM_MEMORY_SIZE );

        // invalidate icache to make sure that bootrom code in memory is used
        size_t l_icache_invalid_size = (SECUREROM_MEMORY_SIZE /
                                        sizeof(uint64_t));

        mm_icache_invalidate( iv_device_ptr, l_icache_invalid_size);

        // Make this address space executable
        uint64_t l_access_type = EXECUTABLE;
        l_rc = mm_set_permission( iv_device_ptr,
                                  SECUREROM_MEMORY_SIZE,
                                  l_access_type);


        if (l_rc != 0)
        {
            TRACFCOMP(g_trac_secure,EXIT_MRK"SecureROM::initialize():"
            " Fail from mm_set_permission(EXECUTABLE): l_rc=0x%x, ptr=%p, "
            "size=0x%x, access=0x%x", l_rc, iv_device_ptr,
            SECUREROM_MEMORY_SIZE, l_access_type);

            /*@
             * @errortype
             * @moduleid     SECUREBOOT::MOD_SECURE_ROM_INIT
             * @reasoncode   SECUREBOOT::RC_SET_PERMISSION_FAIL_EXE
             * @userdata1    l_rc
             * @userdata2    iv_device_ptr
             * @devdesc      mm_set_permission(EXECUTABLE) failed for Secure ROM
             * @custdesc     A problem occurred during the IPL of the system.
             */
            l_errl = new ERRORLOG::ErrlEntry(
                                   ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                   SECUREBOOT::MOD_SECURE_ROM_INIT,
                                   SECUREBOOT::RC_SET_PERMISSION_FAIL_EXE,
                                   TO_UINT64(l_rc),
                                   reinterpret_cast<uint64_t>(iv_device_ptr),
                                   true /*Add HB Software Callout*/ );

            l_errl->collectTrace(SECURE_COMP_NAME,256);
            break;

        }


        /***************************************************************/
        /*  Retrieve HW Hash Keys From The System                      */
        /***************************************************************/

        // @todo RTC:RTC:34080 - Support for SecureROM::getHwKeyHash()
        l_errl = SecureROM::getHwKeyHash();

        if (l_errl != NULL)
        {
            TRACFCOMP(g_trac_secure,ERR_MRK"SecureROM::initialize():"
            " SecureROM::getHwKeyHash() returned an error");

            l_errl->collectTrace(SECURE_COMP_NAME,256);
            break;

        }


        /***************************************************************/
        /*  Secure ROM successfully initialized                        */
        /***************************************************************/
        // If we've made it this far without an error, than Secure ROM
        //  is properly initialized and pages shouldn't be de-allocated
        l_cleanup = false;
        TRACFCOMP(g_trac_secure,INFO_MRK"SecureROM::initialize(): SUCCESSFUL:"
        " iv_device_ptr=%p", iv_device_ptr);


    }while(0);

    // Check to see if we should free pages
    if (l_cleanup == true)
    {
        SecureROM::_cleanup();
    }

    TRACDCOMP(g_trac_secure,EXIT_MRK"SecureROM::initialize() - %s",
              ((NULL == l_errl) ? "No Error" : "With Error") );
#endif
    return l_errl;

}


/**
 * @brief Verify Container against system hash keys
 */
errlHndl_t SecureROM::verifyContainer(void * i_container,
// TODO securebootp9 - this is dummy parameter added to aid in p9 port
// need to replace the method below with up-to-date version
                                      const sha2_hash_t* i_hwKeyHash)
{
    TRACDCOMP(g_trac_secure,ENTER_MRK"SecureROM::verifyContainer(): "
              "i_container=%p", i_container);


    errlHndl_t  l_errl = NULL;
    uint64_t    l_rc   = 0;

    do{

        // Check to see if ROM has already been initialized
        // This should have been done early in IPL so assert if this
        // is not the case as system is in a bad state
        assert(iv_device_ptr != NULL);


        // Declare local input struct
        ROM_hw_params l_hw_parms;

        // Clear/zero-out the struct since we want 0 ('zero') values for
        // struct elements my_ecid, entry_point and log
        memset(&l_hw_parms, 0, sizeof(ROM_hw_params));

        // Now set hw_key_hash, which is of type sha2_hash_t, to iv_key_hash
        memcpy (&l_hw_parms.hw_key_hash, &iv_key_hash, sizeof(sha2_hash_t));
        TRACFBIN(g_trac_secure,"SecureROM::verifyContainer(): hw_key_hash",
                 l_hw_parms.hw_key_hash, sizeof(sha2_hash_t));

        /*******************************************************************/
        /* Call ROM_verify() function via an assembly call                 */
        /*******************************************************************/

        // Set startAddr to ROM_verify() function at an offset of Secure ROM
        uint64_t l_rom_verify_startAddr = reinterpret_cast<uint64_t>(
                                                 iv_device_ptr)
                                               + ROM_VERIFY_FUNCTION_OFFSET;

        TRACUCOMP(g_trac_secure,"SecureROM::verifyContainer(): "
                  " Calling ROM_verify() via call_rom_verify: l_rc=0x%x, "
                  "l_hw_parms.log=0x%x (&l_hw_parms=%p) addr=%p (iv_d_p=%p)",
                  l_rc, l_hw_parms.log, &l_hw_parms, l_rom_verify_startAddr,
                 iv_device_ptr);


        ROM_container_raw* l_container = reinterpret_cast<ROM_container_raw*>(i_container);
        TRACFCOMP(g_trac_secure,"magic_number 0x%X", l_container->magic_number);
        TRACFCOMP(g_trac_secure,"version 0x%X", l_container->version);
        TRACFCOMP(g_trac_secure,"container_size 0x%X", l_container->container_size);
        TRACFCOMP(g_trac_secure,"target_hrmor 0x%X", l_container->target_hrmor);
        TRACFCOMP(g_trac_secure,"stack_pointer 0x%X", l_container->stack_pointer);
        TRACFBIN(g_trac_secure,"hw_pkey_a", l_container->hw_pkey_a, SHA512_DIGEST_LENGTH);
        TRACFBIN(g_trac_secure,"hw_pkey_b", l_container->hw_pkey_b, SHA512_DIGEST_LENGTH);
        TRACFBIN(g_trac_secure,"hw_pkey_c", l_container->hw_pkey_c, SHA512_DIGEST_LENGTH);
        TRACFCOMP(g_trac_secure,"prefix 0x%X", l_container->prefix);

        l_rc = call_rom_verify(reinterpret_cast<void*>
                               (l_rom_verify_startAddr),
                               l_container,
                               &l_hw_parms);


        TRACUCOMP(g_trac_secure,"SecureROM::verifyContainer(): "
                  "Back from ROM_verify() via call_rom_verify: l_rc=0x%x, "
                  "l_hw_parms.log=0x%x (&l_hw_parms=%p) addr=%p (iv_d_p=%p)",
                   l_rc, l_hw_parms.log, &l_hw_parms, l_rom_verify_startAddr,
                   iv_device_ptr);



        if (l_rc != 0)
        {
            TRACFCOMP(g_trac_secure,ERR_MRK"SecureROM::verifyContainer():"
            " ROM_verify() FAIL: l_rc=0x%x, l_hw_parms.log=0x%x "
            "addr=%p (iv_d_p=%p)", l_rc, l_hw_parms.log,
            l_rom_verify_startAddr, iv_device_ptr);

            /*@
             * @errortype
             * @severity     ERRL_SEV_UNRECOVERABLE
             * @moduleid     SECUREBOOT::MOD_SECURE_ROM_VERIFY
             * @reasoncode   SECUREBOOT::RC_ROM_VERIFY
             * @userdata1    l_rc
             * @userdata2    l_hw_parms.log
             * @devdesc      ROM_verify() Call Failed
             * @custdesc     Failure to verify authenticity of software.
             */
            l_errl = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                         SECUREBOOT::MOD_SECURE_ROM_VERIFY,
                                         SECUREBOOT::RC_ROM_VERIFY,
                                         l_rc,
                                         l_hw_parms.log,
                                         true /*Add HB Software Callout*/ );
            // Callout code to force a rewrite of the contents
            //@todo RTC:93870 - Define new callout for verification fail

            l_errl->collectTrace(SECURE_COMP_NAME,256);
            break;

        }

    }while(0);


    TRACDCOMP(g_trac_secure,EXIT_MRK"SecureROM::verifyContainer() - %s",
             ((NULL == l_errl) ? "No Error" : "With Error") );

    return l_errl;
}


/**
 * @brief Hash Blob
 */
errlHndl_t SecureROM::hashBlob(const void * i_blob, size_t i_size, SHA512_t io_buf) const
{

    TRACDCOMP(g_trac_secure,INFO_MRK"SecureROM::hashBlob() NOT "
              "supported, but not returning error log");

    errlHndl_t  l_errl      =   NULL;

    do{
#ifdef CONFIG_ROM_CODE_PRESENT

        // Check to see if ROM has already been initialized
        // This should have been done early in IPL so assert if this
        // is not the case as system is in a bad state
        assert(iv_device_ptr != NULL);

        // Set startAddr to ROM_SHA512() function at an offset of Secure ROM
        uint64_t l_rom_SHA512_startAddr = reinterpret_cast<uint64_t>(
                                                 iv_device_ptr)
                                               + SHA512_HASH_FUNCTION_OFFSET;

        call_rom_SHA512(reinterpret_cast<void*>(l_rom_SHA512_startAddr),
                        reinterpret_cast<sha2_byte*>(i_blob),
                        i_size,
                        reinterpret_cast<sha2_hash_t*>(io_buf));

        TRACUCOMP(g_trac_secure,"SecureROM::hashBlob(): "
                  "call_rom_SHA512: blob=%p size=0x%X addr=%p (iv_d_p=%p)",
                   i_blob, i_size, l_rom_SHA512_startAddr,
                   iv_device_ptr);
#endif
    }while(0);


    TRACDCOMP(g_trac_secure,EXIT_MRK"SecureROM::hashBlob()");

    return l_errl;
}

/**
 * @brief Hash concatenation of N Blobs
 */
errlHndl_t SecureROM::hashConcatBlobs(const blobPair_t &i_blobs,
                                      SHA512_t o_buf) const
{
    errlHndl_t pError = nullptr;
    std::vector<uint8_t> concatBuf;
    for (const auto &it : i_blobs)
    {
        assert(it.first != nullptr, "BUG! In SecureROM::hashConcatBlobs(), "
            "User passed in nullptr blob pointer");
        const uint8_t* const blob =  static_cast<const uint8_t*>(it.first);
        const auto blobSize = it.second;
        concatBuf.insert(concatBuf.end(), blob, blob + blobSize);
    }

    // Call hash blob on new concatenated buffer
    pError = hashBlob(concatBuf.data(),concatBuf.size(),o_buf);

    return pError;
}

/********************
 Internal Methods
 ********************/

/**
 * @brief  Constructor
 */
SecureROM::SecureROM()
:iv_device_ptr(NULL)
{
    TRACDCOMP(g_trac_secure, "SecureROM::SecureROM()>");

    // Clear out iv_key_hash, which is of type sha2_hash_t
    memset(&iv_key_hash, 0, sizeof(sha2_hash_t) );

}

/**
 * @brief  Destructor
 */
SecureROM::~SecureROM() { SecureROM::_cleanup(); };

void SecureROM::_cleanup()
{
    // deallocate pages
    if ( iv_device_ptr != NULL )
    {

        // Make this address space writable before sending it back
        //  to the Page Manager via free, otherwise PM will crash trying to
        //  update the previously-defined-as-excutable memory space
        uint64_t l_access_type = WRITABLE;
        uint64_t l_rc = mm_set_permission( iv_device_ptr,
                                           SECUREROM_MEMORY_SIZE,
                                           l_access_type );

        if (l_rc != 0)
        {
            TRACFCOMP(g_trac_secure,ERR_MRK"SecureROM:::_cleanup():"
            " Fail from mm_set_permission(WRITABLE): l_rc=0x%x, ptr=%p, "
            "size=0x%x, pages=%d, access=0x%x", l_rc, iv_device_ptr,
            SECUREROM_MEMORY_SIZE, SECUREROM_NUM_PAGES, l_access_type);

            /*@
             * @errortype
             * @moduleid     SECUREBOOT::MOD_SECURE_ROM_CLEANUP
             * @reasoncode   SECUREBOOT::RC_SET_PERMISSION_FAIL_WRITE
             * @userdata1    l_rc
             * @userdata2    iv_device_ptr
             * @devdesc      mm_set_permission(WRITABLE) failed for Secure ROM
             * @custdesc     A problem occurred during the IPL of the system.
             */
            errlHndl_t l_errl = new ERRORLOG::ErrlEntry(
                                    ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                    SECUREBOOT::MOD_SECURE_ROM_CLEANUP,
                                    SECUREBOOT::RC_SET_PERMISSION_FAIL_WRITE,
                                    TO_UINT64(l_rc),
                                    reinterpret_cast<uint64_t>(iv_device_ptr),
                                    true /*Add HB Software Callout*/ );

            l_errl->collectTrace(SECURE_COMP_NAME,256);

            // Commit here because function doesn't return error handle
            errlCommit(l_errl, SECURE_COMP_ID);

            // NOTE: Purposely not calling free() here -
            // prefer to have a memory leak than have another task crash
            // due to pages still being excutable or in a bad state

        }
        else
        {
            // Safe to free allocated pages
            free(iv_device_ptr);

            TRACDCOMP(g_trac_secure,INFO_MRK
                      "SecureROM::_cleanup(): pages set to "
                      "WRITABLE (rc=0x%x) and free called", l_rc);


            // Reset device ptr
            iv_device_ptr = NULL;
        }

    }
}


/**
 * @brief Retrieves HW keys' hash from the system
 */
errlHndl_t SecureROM::getHwKeyHash()
{

    errlHndl_t  l_errl      =   NULL;

    TRACFCOMP(g_trac_secure,INFO_MRK"SecureROM::getHwKeyHash() NOT supported");

    // @todo RTC:34080 - Add support for getting HW keys' hash from System

    return l_errl;
}

/**
 * @brief  Retrieve the internal hardware keys' hash from secure ROM object.
 */
void SecureROM::getHwKeyHash(sha2_hash_t o_hash)
{
    memcpy(o_hash, iv_key_hash, sizeof(sha2_hash_t));
}

/**
 * @brief Static instance function for testcase only
 */
SecureROM& SecureROM::getInstance()
{
    return Singleton<SecureROM>::instance();
}


