/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/include/usr/initservice/istepdispatcherif.H $             */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2012,2016                        */
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
#ifndef __INITSERVICE_ISTEPDISPATCHERIF_H
#define __INITSERVICE_ISTEPDISPATCHERIF_H

namespace INITSERVICE
{


/**
 * @brief This function will hold off callers until a sync point message is
 *      received from an outside source.
 *
 * @return NONE.
 */
void waitForSyncPoint ( void );


/**
 * @brief This function is to be used by external code to initiate a sync
 *      point reached message to be sent to the Fsp.
 *
 * @return errlHndl_t - NULL if successful, otherwise a pointer to the error
 *      log.
 */
errlHndl_t sendSyncPoint ( void );


/**
 * @brief This function is to be used by external code to initiate a Istep
 *      complete message to be sent to the Fsp.
 *
 *      NOTE: This function should ONLY be called by the start_payload istep.
 *      Any other use is unsupported and could cause unknown side effects.
 *
 * @return errlHndl_t - NULL if successful, otherwise a pointer to the error
 *      log.
 */
errlHndl_t sendIstepCompleteMsg ( void );

/**
 * @brief This function is to be used by external code to check whether
 *      a shutdown has been requested
 *
 * @return bool - true if shutdown requested, false if not
 */
bool isShutdownRequested ( void );

/**
 * @brief This function is to be used by external code to tell
 *      this instance of istepdispatcher to stop executing steps
 *
 * @return Nothing
 */
void stopIpl( void );

#ifdef CONFIG_BMC_IPMI
/**
 * @brief This function is to be used by external code to
 *        initate a system reboot via IPMI commands
 *
 * @return Nothing
 */
void requestReboot( void );
#endif

/**
 * @brief This function is to be used by external code to tell
 *      this instance of istepdispatcher whether it should
 *      accept istep messages or not
 *
 * @param[in] i_accept - true if istep messages are to be accepted, false if not
 *
 * @return Nothing
 */
void setAcceptIstepMessages ( bool i_accept );

/**
 * @brief This function will set a member class variable to state new gard
 * records have been committed on this boot attempt.
 *
 * Gard records being created are an indication of forward progress.  Therefore
 * if a gard record has been created we can issue a reboot that does not count
 * towards the reboot count limit.
 *
 * @return None.
 */
void setNewGardRecord ( void );

}

#endif
