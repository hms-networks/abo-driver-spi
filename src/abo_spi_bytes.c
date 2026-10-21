/*
 * SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
 * Copyright (c) 2026 HMS Industrial Networks AB
 *
 * This file is part of the HMS Anybus ONE SDK and must only be used with
 * HMS products. See the LICENSE file in the root of the SDK for full
 * license terms.
 *
 * File:        abo_spi_bytes.c
 * Description: Frame value primitives, byte at a time
 */

#include <string.h>

#include "abo_spi.h"

/*******************************************************************************
** Private services
*******************************************************************************/

/*------------------------------------------------------------------------------
** Read a 1- to 8-byte unsigned value in the given byte order.
**------------------------------------------------------------------------------
*/
static uint64_t ReadUnsigned(const uint8_t* at, uint8_t size, bool bigEndian)
{
   uint64_t value = 0;
   uint8_t i;

   for (i = 0; i < size; i++)
   {
      uint8_t byte = bigEndian ? at[i] : at[size - 1u - i];
      value = (value << 8) | (uint64_t)byte;
   }

   return value;
}

/*------------------------------------------------------------------------------
** Write a 1- to 8-byte unsigned value in the given byte order.
**------------------------------------------------------------------------------
*/
static void WriteUnsigned(uint8_t* at, uint8_t size, bool bigEndian, uint64_t value)
{
   uint8_t i;

   for (i = 0; i < size; i++)
   {
      uint8_t byte = (uint8_t)(value >> (8u * i));

      if (bigEndian)
      {
         at[size - 1u - i] = byte;
      }
      else
      {
         at[i] = byte;
      }
   }
}

/*******************************************************************************
** Public services
*******************************************************************************/

uint8_t ABO_SPI_RdU8(const uint8_t* at)
{
   return at[0];
}

uint16_t ABO_SPI_RdU16Le(const uint8_t* at)
{
   return (uint16_t)ReadUnsigned(at, 2u, false);
}

uint16_t ABO_SPI_RdU16Be(const uint8_t* at)
{
   return (uint16_t)ReadUnsigned(at, 2u, true);
}

uint32_t ABO_SPI_RdU32Le(const uint8_t* at)
{
   return (uint32_t)ReadUnsigned(at, 4u, false);
}

uint32_t ABO_SPI_RdU32Be(const uint8_t* at)
{
   return (uint32_t)ReadUnsigned(at, 4u, true);
}

uint64_t ABO_SPI_RdU64Le(const uint8_t* at)
{
   return ReadUnsigned(at, 8u, false);
}

uint64_t ABO_SPI_RdU64Be(const uint8_t* at)
{
   return ReadUnsigned(at, 8u, true);
}

float ABO_SPI_RdF32Le(const uint8_t* at)
{
   uint32_t bits = ABO_SPI_RdU32Le(at);
   float value;

   /*
   ** Through memcpy rather than a cast: the two types may not alias.
   */
   memcpy(&value, &bits, sizeof value);

   return value;
}

float ABO_SPI_RdF32Be(const uint8_t* at)
{
   uint32_t bits = ABO_SPI_RdU32Be(at);
   float value;

   memcpy(&value, &bits, sizeof value);

   return value;
}

double ABO_SPI_RdF64Le(const uint8_t* at)
{
   uint64_t bits = ABO_SPI_RdU64Le(at);
   double value;

   memcpy(&value, &bits, sizeof value);

   return value;
}

double ABO_SPI_RdF64Be(const uint8_t* at)
{
   uint64_t bits = ABO_SPI_RdU64Be(at);
   double value;

   memcpy(&value, &bits, sizeof value);

   return value;
}

void ABO_SPI_WrU8(uint8_t* at, uint8_t value)
{
   at[0] = value;
}

void ABO_SPI_WrU16Le(uint8_t* at, uint16_t value)
{
   WriteUnsigned(at, 2u, false, value);
}

void ABO_SPI_WrU16Be(uint8_t* at, uint16_t value)
{
   WriteUnsigned(at, 2u, true, value);
}

void ABO_SPI_WrU32Le(uint8_t* at, uint32_t value)
{
   WriteUnsigned(at, 4u, false, value);
}

void ABO_SPI_WrU32Be(uint8_t* at, uint32_t value)
{
   WriteUnsigned(at, 4u, true, value);
}

void ABO_SPI_WrU64Le(uint8_t* at, uint64_t value)
{
   WriteUnsigned(at, 8u, false, value);
}

void ABO_SPI_WrU64Be(uint8_t* at, uint64_t value)
{
   WriteUnsigned(at, 8u, true, value);
}

void ABO_SPI_WrF32Le(uint8_t* at, float value)
{
   uint32_t bits;

   memcpy(&bits, &value, sizeof bits);
   ABO_SPI_WrU32Le(at, bits);
}

void ABO_SPI_WrF32Be(uint8_t* at, float value)
{
   uint32_t bits;

   memcpy(&bits, &value, sizeof bits);
   ABO_SPI_WrU32Be(at, bits);
}

void ABO_SPI_WrF64Le(uint8_t* at, double value)
{
   uint64_t bits;

   memcpy(&bits, &value, sizeof bits);
   ABO_SPI_WrU64Le(at, bits);
}

void ABO_SPI_WrF64Be(uint8_t* at, double value)
{
   uint64_t bits;

   memcpy(&bits, &value, sizeof bits);
   ABO_SPI_WrU64Be(at, bits);
}
