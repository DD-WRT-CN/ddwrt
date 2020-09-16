/*
 * @file        IxOsalOsSymbols.c 
 * @author 	Intel Corporation
 * @date        25-10-2005
 *
 * @brief       description goes here
 *
 * @par
 * IXP400 SW Release Crypto version 2.3
 * 
 * -- Copyright Notice --
 * 
 * @par
 * Copyright (c) 2001-2005, Intel Corporation.
 * All rights reserved.
 * 
 * @par
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * 
 * @par
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * 
 * @par
 * -- End of Copyright Notice --
 */

#include <linux/module.h>
#include "IxOsal.h"

EXPORT_SYMBOL (ixOsalIrqBind);
EXPORT_SYMBOL (ixOsalIrqUnbind);
EXPORT_SYMBOL (ixOsalIrqLock);
EXPORT_SYMBOL (ixOsalIrqUnlock);
EXPORT_SYMBOL (ixOsalIrqLevelSet);
EXPORT_SYMBOL (ixOsalIrqEnable);
EXPORT_SYMBOL (ixOsalIrqDisable);

EXPORT_SYMBOL (ixOsalCacheDmaMalloc);
EXPORT_SYMBOL (ixOsalCacheDmaFree);

EXPORT_SYMBOL (ixOsalTimestampGet);
EXPORT_SYMBOL (ixOsalTimestampResolutionGet);
EXPORT_SYMBOL (ixOsalSysClockRateGet);

#ifdef __ixpTolapai
EXPORT_SYMBOL (ixOsalPciDeviceFind);
EXPORT_SYMBOL (ixOsalPciSlotAddress);
EXPORT_SYMBOL (ixOsalPciConfigReadByte);
EXPORT_SYMBOL (ixOsalPciConfigReadShort);
EXPORT_SYMBOL (ixOsalPciConfigReadLong);
EXPORT_SYMBOL (ixOsalPciConfigWriteByte);
EXPORT_SYMBOL (ixOsalPciConfigWriteShort);
EXPORT_SYMBOL (ixOsalPciConfigWriteLong);
EXPORT_SYMBOL (ixOsalPciDeviceFree);
#endif /* __ixpTolapai */
