# IBM_PROLOG_BEGIN_TAG
# This is an automatically generated prolog.
#
# $Source: src/usr/htmgt/makefile $
#
# OpenPOWER HostBoot Project
#
# Contributors Listed Below - COPYRIGHT 2017
# [+] Google Inc.
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

# Pblog and nanopb build rules
# Requires libprotobuf-dev package for protoc and headers in
# /usr/include/google/protobuf

ROOTPATH = ../../..
MODULE = pblog

PBLOG_BUILD_STATIC = y
PBLOG_BUILD_SHARED = n
PBLOG_BUILD_MODULE_FILE = n

include $(PBLOG_DIR)/mk/pblog.mk

# The build system is weird and doesn't give us anything by default
# pblog depends on a basic stdinc
CFLAGS += -I${ROOTPATH}/src/include

# Build and include pblog
GEN_PASS_BODY += $(PBLOG_HEADERS) $(PBLOG_STATIC)
CLEAN_TARGETS += $(PBLOG_HEADERS)
EXTRAINCDIR += $(PBLOG_INCLUDE)

# Hostboot sources
OBJS = errl.o event_to_str.o hb_flash.o hb_pblog.o

include ${ROOTPATH}/config.mk

# Pblog static library
OBJECTS += $(PBLOG_STATIC)
