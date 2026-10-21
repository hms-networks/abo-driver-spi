/*
 * SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
 * Copyright (c) 2026 HMS Industrial Networks AB
 *
 * This file is part of the HMS Anybus ONE SDK and must only be used with
 * HMS products. See the LICENSE file in the root of the SDK for full
 * license terms.
 *
 * File:        abo_spi_frame.c
 * Description: Frame header and CRC
 */

#include "abo_spi_frame.h"

/*******************************************************************************
** Constants
*******************************************************************************/

/*
** CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF, no reflection
** and no final XOR. A nibble table: 32 bytes rather than 512, at two lookups
** per byte.
*/
static const uint16_t abo_spi_crcNibble[16] = { 0x0000u, 0x1021u, 0x2042u, 0x3063u, 0x4084u, 0x50A5u,
                                                0x60C6u, 0x70E7u, 0x8108u, 0x9129u, 0xA14Au, 0xB16Bu,
                                                0xC18Cu, 0xD1ADu, 0xE1CEu, 0xF1EFu };

/*******************************************************************************
** Public services
*******************************************************************************/

void abo_spi_FrameWriteHeader(uint8_t* frame, uint16_t layoutId, uint8_t sequence)
{
   frame[0] = (uint8_t)(layoutId & 0x00FFu);
   frame[1] = (uint8_t)(layoutId >> 8);
   frame[2] = sequence;
}

uint16_t abo_spi_FrameLayoutId(const uint8_t* frame)
{
   return (uint16_t)((uint16_t)frame[0] | ((uint16_t)frame[1] << 8));
}

uint8_t abo_spi_FrameSequence(const uint8_t* frame)
{
   return frame[2];
}

bool abo_spi_FrameIsLayoutIdIdle(uint16_t layoutId)
{
   return (layoutId == ABO_SPI_LAYOUT_ID_IDLE_LOW) || (layoutId == ABO_SPI_LAYOUT_ID_IDLE_HIGH);
}

uint16_t abo_spi_FrameCrc(const uint8_t* frame, uint16_t length)
{
   uint16_t covered = (uint16_t)(length - ABO_SPI_CRC_SIZE);
   uint16_t crc = 0xFFFFu;
   uint16_t i;

   for (i = 0u; i < covered; i++)
   {
      unsigned int byte = frame[i];
      unsigned int high = (((unsigned int)crc >> 12) ^ (byte >> 4)) & 0x0Fu;
      unsigned int low;

      crc = (uint16_t)((crc << 4) ^ abo_spi_crcNibble[high]);
      low = (((unsigned int)crc >> 12) ^ byte) & 0x0Fu;
      crc = (uint16_t)((crc << 4) ^ abo_spi_crcNibble[low]);
   }

   return crc;
}

void abo_spi_FrameAppendCrc(uint8_t* frame, uint16_t length)
{
   uint16_t crc = abo_spi_FrameCrc(frame, length);

   frame[length - ABO_SPI_CRC_SIZE] = (uint8_t)(crc & 0x00FFu);
   frame[length - 1u] = (uint8_t)(crc >> 8);
}

bool abo_spi_FrameIsCrcGood(const uint8_t* frame, uint16_t length)
{
   uint16_t carried = (uint16_t)((uint16_t)frame[length - ABO_SPI_CRC_SIZE]
                                 | ((uint16_t)frame[length - 1u] << 8));

   return carried == abo_spi_FrameCrc(frame, length);
}

bool abo_spi_FrameHasSequenceAdvanced(uint8_t received, uint8_t last, bool lastIsValid)
{
   return (!lastIsValid) || (received != last);
}
