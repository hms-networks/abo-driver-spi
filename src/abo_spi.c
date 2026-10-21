/*
 * SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
 * Copyright (c) 2026 HMS Industrial Networks AB
 *
 * This file is part of the HMS Anybus ONE SDK and must only be used with
 * HMS products. See the LICENSE file in the root of the SDK for full
 * license terms.
 *
 * File:        abo_spi.c
 * Description: The cycle: build, exchange, check, publish
 */

#include <string.h>

#include "abo_spi.h"
#include "abo_spi_channel.h"
#include "abo_spi_frame.h"

/*******************************************************************************
** Constants
*******************************************************************************/

/*
** This driver clocks the bus. The host as slave is not implemented.
*/
#if !ABO_SPI_DEVICE_IS_SLAVE
#error "this configuration makes the host the SPI slave, which this driver does not implement"
#endif

/*
** A master's transfer is over in the frame's clocking time; a cycle is a bound only a broken port reaches.
*/
#define ABO_SPI_TRANSFER_TIMEOUT_US (ABO_SPI_CYCLE_TIME_US)

/*
** Silence before the device is lost: the device's own host-loss timeout.
*/
#define ABO_SPI_LOSS_WINDOW_US ((uint32_t)(ABO_SPI_CYCLE_TIME_US) * (uint32_t)(ABO_SPI_HOST_LOSS_CYCLES))

#if (ABO_SPI_HOST_LOSS_CYCLES) < 1
#error "abo_spi_config.h states a host-loss timeout of zero cycles; the device would be lost on every cycle."
#endif

/*
** Half the 32-bit clock is the most a difference can tell apart.
*/
#if ((ABO_SPI_CYCLE_TIME_US) * (ABO_SPI_HOST_LOSS_CYCLES)) >= 0x80000000
#error "abo_spi_config.h states a host-loss timeout too long to time on a 32-bit microsecond clock."
#endif

/*******************************************************************************
** Types
*******************************************************************************/

/** @brief Where one channel's two status bytes sit. */
typedef struct abo_spi_ChannelPlacement
{
   uint16_t tohStatus;
   uint16_t frhStatus;
} abo_spi_ChannelPlacement_t;

/*******************************************************************************
** Private data
*******************************************************************************/

#define ABO_SPI_PLACEMENT_ENTRY(name, group, tohStatus, frhStatus, tohData, tohSize, frhData, frhSize) \
   { (uint16_t)(tohStatus), (uint16_t)(frhStatus) },

/*
** The driver takes the status offsets from the list here, and the direction in
** abo_spi_channel.c; the accessors carry the rest. At least one entry, so that
** a configuration with no channels compiles.
*/
static const abo_spi_ChannelPlacement_t abo_spi_channelPlacement[ABO_SPI_CHANNEL_ENTRIES] = {
#if ABO_SPI_CHANNEL_COUNT > 0u
   ABO_SPI_CHANNEL_LIST(ABO_SPI_PLACEMENT_ENTRY)
#else
   /*
   ** Unused: C99 has no empty initialiser.
   */
   { 0u, 0u }
#endif
};

/*
** Every channel's status and data bytes lie between the frame header and the
** CRC. The array length goes negative for an entry where they do not.
*/
#define ABO_SPI_PAYLOAD_END ((ABO_SPI_FRAME_LENGTH) - ((ABO_SPI_CRC_ENABLED) ? ABO_SPI_CRC_SIZE : 0u))
#define ABO_SPI_IN_PAYLOAD(offset, size)                                                                               \
   (((offset) >= ABO_SPI_HEADER_SIZE) && (((offset) + (size)) <= ABO_SPI_PAYLOAD_END))
#define ABO_SPI_DATA_IN_PAYLOAD(offset, size) (((size) == 0u) || ABO_SPI_IN_PAYLOAD((offset), (size)))

#define ABO_SPI_CHECK_ENTRY(name, group, tohStatus, frhStatus, tohData, tohSize, frhData, frhSize)                    \
   typedef char abo_spi_##name##_lies_in_the_frame                                                                     \
      [(ABO_SPI_IN_PAYLOAD((tohStatus), 1u) && ABO_SPI_IN_PAYLOAD((frhStatus), 1u)                                     \
        && ABO_SPI_DATA_IN_PAYLOAD((tohData), (tohSize)) && ABO_SPI_DATA_IN_PAYLOAD((frhData), (frhSize)))           \
          ? 1                                                                                                          \
          : -1];

#if ABO_SPI_CHANNEL_COUNT > 0u
ABO_SPI_CHANNEL_LIST(ABO_SPI_CHECK_ENTRY)
#endif

/*******************************************************************************
** Private services
*******************************************************************************/

/*------------------------------------------------------------------------------
** Microseconds between two readings. Unsigned arithmetic survives the wrap.
**------------------------------------------------------------------------------
*/
static uint32_t ElapsedUs(uint32_t now, uint32_t since)
{
   return now - since;
}

/*------------------------------------------------------------------------------
** Whether a moment has arrived. A difference in the upper half of the range
** means it is still ahead; compared moments are never far enough apart for
** that to be ambiguous.
**------------------------------------------------------------------------------
*/
static bool HasReached(uint32_t now, uint32_t when)
{
   return ElapsedUs(now, when) < 0x80000000u;
}

/*------------------------------------------------------------------------------
** Microseconds still to wait for a moment, and zero once it has arrived.
**------------------------------------------------------------------------------
*/
static uint32_t RemainingUs(uint32_t now, uint32_t when)
{
   if (HasReached(now, when))
   {
      return 0u;
   }

   return when - now;
}

/*------------------------------------------------------------------------------
** Count, saturating: a wrapped counter would read as nothing happening.
**------------------------------------------------------------------------------
*/
static void Count(ABO_SPI_Handle_t* handle, ABO_SPI_Counter_t which)
{
   if (handle->counters[which] != UINT32_MAX)
   {
      handle->counters[which]++;
   }
}

/*------------------------------------------------------------------------------
** Whether a whole loss window has passed since the last accepted frame.
**------------------------------------------------------------------------------
*/
static bool IsSilentTooLong(const ABO_SPI_Handle_t* handle, uint32_t now)
{
   return handle->started && (ElapsedUs(now, handle->lastAcceptedUs) >= ABO_SPI_LOSS_WINDOW_US);
}

/*------------------------------------------------------------------------------
** Lost now: declared by a cycle, or silent long enough that the next will.
**------------------------------------------------------------------------------
*/
static bool IsLostAt(const ABO_SPI_Handle_t* handle, uint32_t now)
{
   return handle->deviceLost || IsSilentTooLong(handle, now);
}

/*------------------------------------------------------------------------------
** Microseconds until the next cycle may start. The longer of two waits: the
** cycle time from the scheduled start, less the early margin, and the gap from
** the end of the last transfer, in full.
**------------------------------------------------------------------------------
*/
static uint32_t UsUntilDue(const ABO_SPI_Handle_t* handle, uint32_t now)
{
   uint32_t untilCycle;
   uint32_t untilGap;
   uint32_t wait;

   if (!handle->started)
   {
      return 0u;
   }

   untilCycle = RemainingUs(now, handle->cycleStartedUs + ABO_SPI_CYCLE_TIME_US - ABO_SPI_EARLY_MARGIN_US);
   untilGap = RemainingUs(now, handle->transferEndedUs + ABO_SPI_MIN_GAP_US);

   wait = (untilCycle > untilGap) ? untilCycle : untilGap;

   /*
   ** A longer wait means the caller was away longer than the clock can measure:
   ** the cycle is due now.
   */
   if (wait > (ABO_SPI_CYCLE_TIME_US + ABO_SPI_MIN_GAP_US))
   {
      wait = 0u;
   }

   return wait;
}

/*------------------------------------------------------------------------------
** Start both clocks on the first cycle.
**
** The schedule starts here. The silence is timed as though a frame had arrived
** a cycle ago, so a device that never answers at all is declared lost after
** the same number of cycles as one that stops answering.
**------------------------------------------------------------------------------
*/
static void StartTheClocks(ABO_SPI_Handle_t* handle, uint32_t now)
{
   handle->cycleStartedUs = now;
   handle->lastAcceptedUs = now - ABO_SPI_CYCLE_TIME_US;
   handle->started = true;
}

/*------------------------------------------------------------------------------
** Whether the schedule is a whole cycle or more behind, and not within the
** early margin ahead. Missed cycles are not made up in a burst.
**------------------------------------------------------------------------------
*/
static bool HasScheduleFallenBehind(const ABO_SPI_Handle_t* handle, uint32_t now)
{
   return ((uint32_t)(now - handle->cycleStartedUs) >= ABO_SPI_CYCLE_TIME_US) &&
          ((uint32_t)(handle->cycleStartedUs - now) > ABO_SPI_EARLY_MARGIN_US);
}

/*------------------------------------------------------------------------------
** Advance the schedule by one cycle rather than restarting it from now, so an
** early timer wake-up does not push the next cycle out. Once it has fallen a
** whole cycle behind it restarts from now, so missed cycles are not made up in
** a burst.
**
** The ordinary case leaves the new start at most an early margin ahead of now,
** or a little behind it where the caller woke late. Either keeps the schedule.
**------------------------------------------------------------------------------
*/
static void AdvanceSchedule(ABO_SPI_Handle_t* handle, uint32_t now)
{
   if (!handle->started)
   {
      StartTheClocks(handle, now);

      return;
   }

   handle->cycleStartedUs += ABO_SPI_CYCLE_TIME_US;

   if (HasScheduleFallenBehind(handle, now))
   {
      handle->cycleStartedUs = now;
   }
}

/*------------------------------------------------------------------------------
** Count a cycle that began a whole cycle time late: missed, not jittered.
**------------------------------------------------------------------------------
*/
static void NoteIfLate(ABO_SPI_Handle_t* handle, uint32_t now)
{
   if (handle->started && HasReached(now, handle->cycleStartedUs + (2u * (uint32_t)ABO_SPI_CYCLE_TIME_US)))
   {
      Count(handle, ABO_SPI_COUNTER_LATE);
   }
}

/*------------------------------------------------------------------------------
** Build the outgoing frame in the wire buffer, which nothing then writes until
** the transfer is over.
**------------------------------------------------------------------------------
*/
static void BuildTxFrame(ABO_SPI_Handle_t* handle)
{
   uint8_t index;

   memcpy(handle->wireTx, handle->fromHost, ABO_SPI_FRAME_LENGTH);

   handle->txSequence = (uint8_t)(handle->txSequence + 1u);
   abo_spi_FrameWriteHeader(handle->wireTx, ABO_SPI_LAYOUT_ID, handle->txSequence);

   for (index = 0u; index < abo_spi_channelCount; index++)
   {
      handle->wireTx[abo_spi_channelPlacement[index].frhStatus] = handle->channels[index].ownStatus;
   }

#if ABO_SPI_CRC_ENABLED
   abo_spi_FrameAppendCrc(handle->wireTx, ABO_SPI_FRAME_LENGTH);
#endif
}

/*------------------------------------------------------------------------------
** Check the received frame: idle, then CRC, then layout ID, then sequence. A
** CRC failure in a frame with another layout ID is reported as the layout.
**------------------------------------------------------------------------------
*/
static ABO_SPI_Result_t Validate(const ABO_SPI_Handle_t* handle)
{
   uint16_t layoutId = abo_spi_FrameLayoutId(handle->wireRx);

   if (abo_spi_FrameIsLayoutIdIdle(layoutId))
   {
      return ABO_SPI_ERR_IDLE;
   }

#if ABO_SPI_CRC_ENABLED
   if (!abo_spi_FrameIsCrcGood(handle->wireRx, ABO_SPI_FRAME_LENGTH))
   {
      return (layoutId != ABO_SPI_LAYOUT_ID) ? ABO_SPI_ERR_LAYOUT : ABO_SPI_ERR_CRC;
   }
#endif

   if (layoutId != ABO_SPI_LAYOUT_ID)
   {
      return ABO_SPI_ERR_LAYOUT;
   }

#if ABO_SPI_SEQUENCE_SUPERVISION
   if (!abo_spi_FrameHasSequenceAdvanced(abo_spi_FrameSequence(handle->wireRx), handle->rxSequence,
                                         handle->rxSequenceValid))
   {
      return ABO_SPI_REPEATED;
   }
#endif

   return ABO_SPI_OK;
}

/*------------------------------------------------------------------------------
** The counter a rejection belongs to.
**------------------------------------------------------------------------------
*/
static ABO_SPI_Counter_t CounterFor(ABO_SPI_Result_t result)
{
   ABO_SPI_Counter_t counter;

   switch (result)
   {
      case ABO_SPI_ERR_LAYOUT:
         counter = ABO_SPI_COUNTER_LAYOUT;
         break;

      case ABO_SPI_ERR_CRC:
         counter = ABO_SPI_COUNTER_CRC;
         break;

      case ABO_SPI_REPEATED:
         counter = ABO_SPI_COUNTER_REPEATED;
         break;

      case ABO_SPI_ERR_IDLE:
         counter = ABO_SPI_COUNTER_IDLE;
         break;

      case ABO_SPI_ERR_SHORT_FRAME:
         counter = ABO_SPI_COUNTER_SHORT_FRAME;
         break;

      case ABO_SPI_ERR_TRANSFER:
      default:
         counter = ABO_SPI_COUNTER_TRANSFER;
         break;
   }

   return counter;
}

/*------------------------------------------------------------------------------
** Declare the device lost once a transfer ends a loss window after the last
** accepted frame. Asked after every transfer, good ones too: a caller that
** stalled that long was away long enough to miss a close.
**------------------------------------------------------------------------------
*/
static void NoteSilence(ABO_SPI_Handle_t* handle)
{
   if (!handle->deviceLost && IsSilentTooLong(handle, handle->transferEndedUs))
   {
      handle->deviceLost = true;
      abo_spi_ChannelNoteDeviceLost(handle);
      Count(handle, ABO_SPI_COUNTER_DEVICE_LOST);
   }
}

/*------------------------------------------------------------------------------
** Count a cycle that published nothing, whether or not anything was wrong.
**------------------------------------------------------------------------------
*/
static ABO_SPI_Result_t CountUnpublished(ABO_SPI_Handle_t* handle, ABO_SPI_Result_t result)
{
   Count(handle, CounterFor(result));

   return result;
}

/*------------------------------------------------------------------------------
** Hand the accepted frame to the application, all of it at once.
**------------------------------------------------------------------------------
*/
static void Publish(ABO_SPI_Handle_t* handle)
{
   bool firstFrame = !handle->accepted;
   uint8_t index;

   memcpy(handle->toHost, handle->wireRx, ABO_SPI_FRAME_LENGTH);

   handle->rxSequence = abo_spi_FrameSequence(handle->wireRx);
   handle->rxSequenceValid = true;

   for (index = 0u; index < abo_spi_channelCount; index++)
   {
      abo_spi_ChannelTakeStatus(handle, index, handle->wireRx[abo_spi_channelPlacement[index].tohStatus],
                                firstFrame);
   }

   handle->accepted = true;
   handle->deviceLost = false;
   handle->lastAcceptedUs = handle->transferEndedUs;
   Count(handle, ABO_SPI_COUNTER_ACCEPTED);
}

/*******************************************************************************
** Public services
*******************************************************************************/

const uint8_t* ABO_SPI_ToHostImage(const struct ABO_SPI_Handle* handle)
{
   return handle->toHost;
}

uint8_t* ABO_SPI_FromHostImage(struct ABO_SPI_Handle* handle)
{
   return handle->fromHost;
}

ABO_SPI_Result_t ABO_SPI_Init(ABO_SPI_Handle_t* handle, const ABO_SPI_Port_t* port)
{
   if ((handle == NULL) || (port == NULL) || (port->transfer == NULL) || (port->nowUs == NULL))
   {
      return ABO_SPI_ERR_ARGUMENT;
   }

   memset(handle, 0, sizeof *handle);
   handle->port = *port;

   abo_spi_ChannelInit(handle);

   return ABO_SPI_OK;
}

ABO_SPI_Result_t ABO_SPI_RunCycle(ABO_SPI_Handle_t* handle)
{
   uint32_t now;
   int32_t exchanged;
   ABO_SPI_Result_t result;

   if ((handle == NULL) || (handle->port.transfer == NULL) || (handle->port.nowUs == NULL))
   {
      return ABO_SPI_ERR_ARGUMENT;
   }

   now = handle->port.nowUs(handle->port.ctx);

   if (UsUntilDue(handle, now) != 0u)
   {
      return ABO_SPI_NOT_DUE;
   }

   NoteIfLate(handle, now);

   BuildTxFrame(handle);

   AdvanceSchedule(handle, now);

   exchanged = handle->port.transfer(handle->port.ctx, handle->wireTx, handle->wireRx,
                                     (uint16_t)ABO_SPI_FRAME_LENGTH, ABO_SPI_TRANSFER_TIMEOUT_US);
   handle->transferEndedUs = handle->port.nowUs(handle->port.ctx);

   NoteSilence(handle);

   if (exchanged < 0)
   {
      return CountUnpublished(handle, ABO_SPI_ERR_TRANSFER);
   }

   if (exchanged != (int32_t)ABO_SPI_FRAME_LENGTH)
   {
      return CountUnpublished(handle, ABO_SPI_ERR_SHORT_FRAME);
   }

   result = Validate(handle);

   if (result != ABO_SPI_OK)
   {
      return CountUnpublished(handle, result);
   }

   Publish(handle);

   return ABO_SPI_OK;
}

uint32_t ABO_SPI_UsUntilDue(const ABO_SPI_Handle_t* handle)
{
   if ((handle == NULL) || (handle->port.nowUs == NULL))
   {
      return 0u;
   }

   return UsUntilDue(handle, handle->port.nowUs(handle->port.ctx));
}

bool ABO_SPI_IsOnline(const ABO_SPI_Handle_t* handle)
{
   if ((handle == NULL) || (handle->port.nowUs == NULL))
   {
      return false;
   }

   return handle->accepted && !IsLostAt(handle, handle->port.nowUs(handle->port.ctx));
}

uint32_t ABO_SPI_Counter(const ABO_SPI_Handle_t* handle, ABO_SPI_Counter_t which)
{
   if ((handle == NULL) || ((unsigned int)which >= ABO_SPI_COUNTER_COUNT))
   {
      return 0u;
   }

   return handle->counters[which];
}
