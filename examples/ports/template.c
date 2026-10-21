/*
 * SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
 * Copyright (c) 2026 HMS Industrial Networks AB
 *
 * This file is part of the HMS Anybus ONE SDK and must only be used with
 * HMS products. See the LICENSE file in the root of the SDK for full
 * license terms.
 *
 * File:        template.c
 * Description: The port you write. Both callbacks are left empty; each says
 *              what you owe and what you see until you write it.
 */

#include <string.h>

#include "port.h"

/*******************************************************************************
** Private services
*******************************************************************************/

/*------------------------------------------------------------------------------
** Exchange exactly length bytes, full duplex. You owe:
**
** - one chip-select assertion for the whole frame; the device ends a frame on
**   deassertion.
** - ABO_SPI_MODE, ABO_SPI_MSB_FIRST, and a clock between
**   ABO_SPI_MIN_FREQUENCY_HZ and ABO_SPI_FREQUENCY_HZ.
** - blocking until done or timeoutUs has passed; a DMA transfer waits here.
** - the number of bytes exchanged, or a negative value on failure.
**
** Until written: ABO_SPI_ERR_TRANSFER every cycle, then device loss, once NowUs
** is written.
**------------------------------------------------------------------------------
*/
static int32_t Transfer(void* ctx, const uint8_t* tx, uint8_t* rx, uint16_t length, uint32_t timeoutUs)
{
   (void)ctx;
   (void)tx;
   (void)rx;
   (void)length;
   (void)timeoutUs;

   return -1;
}

/*------------------------------------------------------------------------------
** A free-running microsecond counter. It may wrap and need not start at zero;
** a millisecond tick is too coarse. Read a few times per cycle, so keep it cheap.
**
** Until written: one transfer, then ABO_SPI_NOT_DUE for ever.
**------------------------------------------------------------------------------
*/
static uint32_t NowUs(void* ctx)
{
   (void)ctx;

   return 0u;
}

/*******************************************************************************
** Public services
*******************************************************************************/

bool Port_Open(ABO_SPI_Port_t* port)
{
   /*
   ** Zeroed first: see ABO_SPI_Port_t.
   */
   memset(port, 0, sizeof *port);

   port->transfer = Transfer;
   port->nowUs = NowUs;

   /*
   ** Your own context for the callbacks.
   */
   port->ctx = NULL;

   return true;
}

void Port_Close(void)
{
}
