/*
 * SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
 * Copyright (c) 2026 HMS Industrial Networks AB
 *
 * This file is part of the HMS Anybus ONE SDK and must only be used with
 * HMS products. See the LICENSE file in the root of the SDK for full
 * license terms.
 *
 * File:        abo_spi_channel.c
 * Description: Channel status and the close echo
 */

#include "abo_spi_channel.h"
#include "abo_spi_frame.h"

/*******************************************************************************
** Internal data
*******************************************************************************/

const uint8_t abo_spi_channelCount = (uint8_t)ABO_SPI_CHANNEL_COUNT;

/*******************************************************************************
** Private data
*******************************************************************************/

#define ABO_SPI_DIRECTION_NONE   0u
#define ABO_SPI_DIRECTION_OUTPUT 1u
#define ABO_SPI_DIRECTION_INPUT  2u

/*
** A channel with no data in either direction is neither, as the device sees it.
*/
#define ABO_SPI_DIRECTION_ENTRY(name, group, tohStatus, frhStatus, tohData, tohSize, frhData, frhSize) \
   (uint8_t)(((tohSize) > 0u)   ? ABO_SPI_DIRECTION_OUTPUT                                           \
             : ((frhSize) > 0u) ? ABO_SPI_DIRECTION_INPUT                                            \
                                : ABO_SPI_DIRECTION_NONE),

static const uint8_t abo_spi_channelDirection[ABO_SPI_CHANNEL_ENTRIES] = {
#if ABO_SPI_CHANNEL_COUNT > 0u
   ABO_SPI_CHANNEL_LIST(ABO_SPI_DIRECTION_ENTRY)
#else
   /*
   ** Unused: C99 has no empty initialiser.
   */
   ABO_SPI_DIRECTION_NONE
#endif
};

/*******************************************************************************
** Private services
*******************************************************************************/

/*------------------------------------------------------------------------------
** The state of a channel the configuration carries, or NULL for anything else.
**------------------------------------------------------------------------------
*/
static const ABO_SPI_ChannelState_t* ConstStateOf(const ABO_SPI_Handle_t* handle, uint8_t index)
{
   if ((handle == NULL) || ((unsigned int)index >= abo_spi_channelCount))
   {
      return NULL;
   }

   return &handle->channels[index];
}

/*------------------------------------------------------------------------------
** The same, for a caller that writes. One bounds check, not two: casting the
** const away is well defined where the object itself is not const, and the
** caller's handle is not.
**------------------------------------------------------------------------------
*/
static ABO_SPI_ChannelState_t* StateOf(ABO_SPI_Handle_t* handle, uint8_t index)
{
   return (ABO_SPI_ChannelState_t*)ConstStateOf(handle, index);
}

/*------------------------------------------------------------------------------
** A bit of the device's last reported status, false while it is not reporting.
**------------------------------------------------------------------------------
*/
static bool DeviceStatusBit(const ABO_SPI_Handle_t* handle, uint8_t index, uint8_t bit)
{
   const ABO_SPI_ChannelState_t* state = ConstStateOf(handle, index);

   if ((state == NULL) || !handle->accepted || handle->deviceLost)
   {
      return false;
   }

   return (state->deviceStatus & bit) != 0u;
}

/*------------------------------------------------------------------------------
** Whether the index is a configured channel of that direction.
**------------------------------------------------------------------------------
*/
static bool HasDirection(uint8_t index, uint8_t direction)
{
   return ((unsigned int)index < abo_spi_channelCount) && (abo_spi_channelDirection[index] == direction);
}

/*------------------------------------------------------------------------------
** Set or clear one of the host's good bits, leaving the echoed marker alone.
**------------------------------------------------------------------------------
*/
static void SetOwnStatusBit(ABO_SPI_Handle_t* handle, uint8_t index, uint8_t bit, bool good)
{
   ABO_SPI_ChannelState_t* state = StateOf(handle, index);

   if (state == NULL)
   {
      return;
   }

   if (good)
   {
      state->ownStatus = (uint8_t)(state->ownStatus | bit);
   }
   else
   {
      state->ownStatus = (uint8_t)(state->ownStatus & (uint8_t)~bit);
   }
}

/*******************************************************************************
** Internal services
*******************************************************************************/

void abo_spi_ChannelInit(ABO_SPI_Handle_t* handle)
{
   uint8_t index;

   /*
   ** Every entry, including the one a configuration without channels has.
   */
   for (index = 0u; index < ABO_SPI_CHANNEL_ENTRIES; index++)
   {
      handle->channels[index].deviceStatus = 0u;
      handle->channels[index].ownStatus = (uint8_t)ABO_SPI_STATUS_CONSUMER_GOOD;
      handle->channels[index].closeEvent = false;
   }
}

void abo_spi_ChannelTakeStatus(ABO_SPI_Handle_t* handle, uint8_t index, uint8_t deviceStatus,
                               bool firstFrame)
{
   ABO_SPI_ChannelState_t* state = StateOf(handle, index);
   uint8_t marker;

   if (state == NULL)
   {
      return;
   }

   state->deviceStatus = (uint8_t)(deviceStatus & ABO_SPI_STATUS_DEFINED);
   marker = (uint8_t)(state->deviceStatus & ABO_SPI_STATUS_CLOSE);

   /*
   ** The first frame's marker predates this host, so it is adopted, not compared.
   */
   if (!firstFrame && (marker != (state->ownStatus & ABO_SPI_STATUS_CLOSE)))
   {
      state->closeEvent = true;
   }

   /*
   ** Echo the marker and only the marker: the good bits sent are the application's.
   */
   state->ownStatus = (uint8_t)((state->ownStatus & ~(uint8_t)ABO_SPI_STATUS_CLOSE) | marker);
}

void abo_spi_ChannelNoteDeviceLost(ABO_SPI_Handle_t* handle)
{
   uint8_t index;

   /*
   ** Before the first frame there was no connection to lose.
   */
   if (!handle->accepted)
   {
      return;
   }

   for (index = 0u; index < abo_spi_channelCount; index++)
   {
      handle->channels[index].closeEvent = true;
   }
}

/*******************************************************************************
** Public services
*******************************************************************************/

bool ABO_SPI_ChannelIsOpen(const struct ABO_SPI_Handle* handle, uint8_t index)
{
   return DeviceStatusBit(handle, index, (uint8_t)ABO_SPI_STATUS_OPEN);
}

bool ABO_SPI_ChannelIsDataValid(const struct ABO_SPI_Handle* handle, uint8_t index)
{
   return HasDirection(index, ABO_SPI_DIRECTION_OUTPUT) && ABO_SPI_ChannelIsOpen(handle, index) &&
          DeviceStatusBit(handle, index, (uint8_t)ABO_SPI_STATUS_PRODUCER_GOOD);
}

bool ABO_SPI_ChannelIsConsumerGood(const struct ABO_SPI_Handle* handle, uint8_t index)
{
   return HasDirection(index, ABO_SPI_DIRECTION_INPUT) &&
          DeviceStatusBit(handle, index, (uint8_t)ABO_SPI_STATUS_CONSUMER_GOOD);
}

bool ABO_SPI_ChannelTakeCloseEvent(struct ABO_SPI_Handle* handle, uint8_t index)
{
   ABO_SPI_ChannelState_t* state = StateOf(handle, index);
   bool event;

   if (state == NULL)
   {
      return false;
   }

   event = state->closeEvent;
   state->closeEvent = false;

   return event;
}

void ABO_SPI_ChannelSetProducerGood(struct ABO_SPI_Handle* handle, uint8_t index, bool good)
{
   if (HasDirection(index, ABO_SPI_DIRECTION_INPUT))
   {
      SetOwnStatusBit(handle, index, (uint8_t)ABO_SPI_STATUS_PRODUCER_GOOD, good);
   }
}

void ABO_SPI_ChannelSetConsumerGood(struct ABO_SPI_Handle* handle, uint8_t index, bool good)
{
   if (HasDirection(index, ABO_SPI_DIRECTION_OUTPUT))
   {
      SetOwnStatusBit(handle, index, (uint8_t)ABO_SPI_STATUS_CONSUMER_GOOD, good);
   }
}
