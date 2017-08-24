# IBM_PROLOG_BEGIN_TAG
# This is an automatically generated prolog.
#
# $Source: src/import/chips/centaur/procedures/hwp/memory/p9c_mss_draminit_training_advanced.mk $
#
# OpenPOWER HostBoot Project
#
# Contributors Listed Below - COPYRIGHT 2016,2017
# [+] International Business Machines Corp.
#
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#
# IBM_PROLOG_END_TAG

# Include the macros and things for MSS procedures
-include 01common.mk
PROCEDURE=p9c_mss_draminit_training_advanced
$(eval $(call ADD_MEMORY_INCDIRS,$(PROCEDURE)))
OBJS+=p9c_mss_mcbist.o
OBJS+=p9c_mss_mcbist_common.o
OBJS+=p9c_mss_mcbist_address.o
lib${PROCEDURE}_DEPLIBS+=p9c_dimmBadDqBitmapFuncs
OBJS+=p9c_mss_termination_control.o
OBJS+=p9c_mss_generic_shmoo.o
lib${PROCEDURE}_DEPLIBS+=p9c_mss_unmask_errors
OBJS+=p9c_mss_mrs6_DDR4.o
OBJS+=p9c_mss_access_delay_reg.o
OBJS+=p9c_mss_funcs.o
$(call BUILD_PROCEDURE)
