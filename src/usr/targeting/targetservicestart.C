/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/targeting/targetservicestart.C $                      */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2012,2017                        */
/* [+] Google Inc.                                                        */
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

/**
 *  @file targeting/targetservicestart.C
 *
 *  @brief Hostboot entry point for target service
 */

//******************************************************************************
// Includes
//******************************************************************************

// STD
#include <stdio.h>
#include <stdlib.h>

// Other components
#include <sys/misc.h>
#include <sys/task.h>
#include <targeting/common/trace.H>
#include <targeting/adapters/assertadapter.H>
#include <initservice/taskargs.H>
#include <util/utilmbox_scratch.H>

// This component
#include <targeting/common/targetservice.H>
#include <targeting/attrrp.H>

// Others
#include <errl/errlentry.H>
#include <errl/errlmanager.H>
#include <devicefw/userif.H>
#include <config.h>


using namespace INITSERVICE::SPLESS;
//******************************************************************************
// targetService
//******************************************************************************

namespace TARGETING
{

#define TARG_NAMESPACE "TARGETING::"

#define TARG_LOC TARG_NAMESPACE TARG_CLASS TARG_FN ": "

//******************************************************************************
// _start
//******************************************************************************

#define TARG_CLASS ""

/*
 * @brief Initialize any attributes that need to be set early on
 */
static void initializeAttributes(TargetService& i_targetService,
                                 bool i_isMpipl,
                                 bool i_istepMode,
                                 ATTR_MASTER_MBOX_SCRATCH_type& i_masterScratch);

/**
 *  @brief Check that at least one processor of our cpu type is being targeted
 */
static void checkProcessorTargeting(TargetService& i_targetService);

/**
 *  @brief Entry point for initialization service to initialize the targeting
 *      code
 *
 *  @param[in] io_pError
 *      Error log handle; returns NULL on success, !NULL otherwise
 *
 *  @note: Link register is configured to automatically invoke task_end() when
 *      this routine returns
 */
static void initTargeting(errlHndl_t& io_pError)
{
    #define TARG_FN "initTargeting(errlHndl_t& io_pError)"

    TARG_ENTER();

    //Need to stash away the master mbox regs as they will
    //be overwritten
    bool l_isMpipl = false;
    bool l_isIstepMode = false;
    ATTR_MASTER_MBOX_SCRATCH_type l_scratch = {0,0,0,0,0,0,0,0};

    for(size_t i=0; i< sizeof(l_scratch)/sizeof(l_scratch[0]); i++)
    {
        l_scratch[i] =
        Util::readScratchReg(MBOX_SCRATCH_REG1+i);
    }

    // Check mbox scratch reg 3 for IPL boot options
    // Specifically istep mode (bit 0) and MPIPL (bit 2)
    INITSERVICE::SPLESS::MboxScratch3_t l_scratch3;
    l_scratch3.data32 = l_scratch[SCRATCH_3];

    if(l_scratch3.isMpipl)
    {
        TARG_INF("We are running MPIPL mode");
        l_isMpipl = true;
    }
    if(l_scratch3.istepMode)
    {
        l_isIstepMode = true;
    }

    AttrRP::init(io_pError, l_isMpipl);

    if (io_pError == NULL)
    {
        TargetService& l_targetService = targetService();
        (void)l_targetService.init();

        initializeAttributes(l_targetService, l_isMpipl, l_isIstepMode, l_scratch);
        checkProcessorTargeting(l_targetService);

        // Print out top-level model value from loaded targeting values.
        // @TODO RTC:88056 Make the model printed more meaniful
        Target* l_pTopLevel = NULL;
        l_targetService.getTopLevelTarget(l_pTopLevel);
        ATTR_MODEL_type l_model = MODEL_NA;
        if (l_pTopLevel->tryGetAttr<ATTR_MODEL>(l_model)) {
            TARG_INF("Initialized targeting for model: %s",
                     l_pTopLevel->getAttrAsString<ATTR_MODEL>());
        }

// No error module loaded in VPO to save load time
#ifndef CONFIG_VPO_COMPILE
        // call ErrlManager function - tell him that TARG is ready!
        ERRORLOG::ErrlManager::errlResourceReady(ERRORLOG::TARG);
#endif
    }

    TARG_EXIT();

#undef TARG_FN
}

/**
 *  @brief Create _start entry point using task entry macro and vector to
 *      initTargeting function
 */
TASK_ENTRY_MACRO(initTargeting);


/**
 *  @brief Check that at least one processor of our cpu type is being targeted
 */
static void checkProcessorTargeting(TargetService& i_targetService)
{
    #define TARG_FN "checkProcessorTargeting()"
    TARG_ENTER();

    PredicateCTM l_procChip(CLASS_CHIP,TYPE_PROC);
    ProcessorCoreType l_coreType = cpu_core_type();
    bool l_haveOneCorrectProcessor = false;
    TargetRangeFilter l_filter(
        i_targetService.begin(),
        i_targetService.end(),
        &l_procChip);

    for(;l_filter && (l_haveOneCorrectProcessor != true);++l_filter)
    {
        switch(l_filter->getAttr<ATTR_MODEL>())
        {
            case MODEL_VENICE:
                if(l_coreType == CORE_POWER8_VENICE)
                {
                    l_haveOneCorrectProcessor = true;
                }
                break;

            case MODEL_MURANO:
                if(l_coreType == CORE_POWER8_MURANO)
                {
                    l_haveOneCorrectProcessor = true;
                }
                break;

            case MODEL_NAPLES:
                if(l_coreType == CORE_POWER8_NAPLES)
                {
                    l_haveOneCorrectProcessor = true;
                }
                break;
            case MODEL_NIMBUS:
                if(l_coreType == CORE_POWER9_NIMBUS)
                {
                    l_haveOneCorrectProcessor = true;
                }
                break;
            case MODEL_CUMULUS:
                if(l_coreType == CORE_POWER9_CUMULUS)
                {
                    l_haveOneCorrectProcessor = true;
                }
                break;

            default:
                break;
        };
    }

    TARG_ASSERT((l_haveOneCorrectProcessor == true), TARG_ERR_LOC "FATAL: No "
                "targeted processors are of the correct type");

    TARG_EXIT();

    #undef TARG_FN
}

/*
 * @brief Initialize any attributes that need to be set early on
 */
static void initializeAttributes(TargetService& i_targetService,
                                 bool i_isMpipl,
                                 bool i_istepMode,
                                 ATTR_MASTER_MBOX_SCRATCH_type& i_masterScratch)
{
    #define TARG_FN "initializeAttributes()...)"
    TARG_ENTER();

    Target* l_pTopLevel = NULL;

    i_targetService.getTopLevelTarget(l_pTopLevel);
    if(l_pTopLevel)
    {
        Target* l_pMasterProcChip = NULL;
        i_targetService.masterProcChipTargetHandle(l_pMasterProcChip);

        if(l_pMasterProcChip)
        {
            // Master uses xscom by default, needs to be set before
            // doing any other scom accesses
            ScomSwitches l_switches =
              l_pMasterProcChip->getAttr<ATTR_SCOM_SWITCHES>();
            l_switches.useXscom = 1;
            l_switches.useFsiScom = 0;
            l_switches.useSbeScom = 0;
            l_pMasterProcChip->setAttr<ATTR_SCOM_SWITCHES>(l_switches);


            // Master can only use Host I2C so needs to be set before
            // doing any I2C accesses
            I2cSwitches l_i2c_switches =
              l_pMasterProcChip->getAttr<ATTR_I2C_SWITCHES>();
            l_i2c_switches.useHostI2C = 1;
            l_i2c_switches.useFsiI2C  = 0;
            l_pMasterProcChip->setAttr<ATTR_I2C_SWITCHES>(l_i2c_switches);


            // Master has SBE started
            l_pMasterProcChip->setAttr<ATTR_SBE_IS_STARTED>(1);


            l_pTopLevel->setAttr<ATTR_MASTER_MBOX_SCRATCH>(i_masterScratch);

            // Targeting data defaults to non istep, only turn "on" if bit
            // is set so we don't tromp default setting
            if (i_istepMode)
            {
                l_pTopLevel->setAttr<ATTR_ISTEP_MODE>(1);
            }

            //Set the RISK_LEVEL ATTR based off of master Scratch regs
            INITSERVICE::SPLESS::MboxScratch5_t l_scratch5;
            l_scratch5.data32 = i_masterScratch[INITSERVICE::SPLESS::SCRATCH_5];
            if(l_scratch5.riskLevel)
            {
                l_pTopLevel->setAttr<ATTR_RISK_LEVEL>(1);
            }
        }

        if(i_isMpipl)
        {
            l_pTopLevel->setAttr<ATTR_IS_MPIPL_HB>(1);
        }
        else
        {
            l_pTopLevel->setAttr<ATTR_IS_MPIPL_HB>(0);
        }
    }
    else // top level is NULL - never expected
    {
        TARG_INF("Top level target is NULL");
    }

    TARG_EXIT();
    #undef TARG_FN
}

#undef TARG_CLASS

#undef TARG_NAMESPACE

} // End namespace TARGETING

