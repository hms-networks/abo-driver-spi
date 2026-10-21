/*
 * SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
 * Copyright (c) 2026 HMS Industrial Networks AB
 *
 * This file is part of the HMS Anybus ONE SDK and must only be used with
 * HMS products. See the LICENSE file in the root of the SDK for full
 * license terms.
 *
 * File:        abo_spi_channel.h
 * Description: Channel status. Internal to the driver.
 */

#ifndef ABO_SPI_CHANNEL_H
#define ABO_SPI_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

#include "abo_spi.h"

/**
 * @brief ABO_SPI_CHANNEL_COUNT as a value rather than a constant expression.
 *
 * With no channels configured, a loop or bounds check against the constant
 * compares with zero, which -Wtype-limits rejects as always true or false. The
 * loops are meant to be empty there and the checks to reject every index, not
 * to disappear.
 */
extern const uint8_t abo_spi_channelCount;

/** @brief Start every channel closed, with the host's producer status bad. */
void abo_spi_ChannelInit(ABO_SPI_Handle_t* handle);

/**
 * @brief Take one channel's status out of an accepted frame: latch a close if
 *        the marker moved, and echo the marker in the next frame.
 * @param firstFrame Adopt the marker rather than compare it.
 */
void abo_spi_ChannelTakeStatus(ABO_SPI_Handle_t* handle, uint8_t index, uint8_t deviceStatus,
                               bool firstFrame);

/** @brief Latch a close event on every channel, the device having been lost. */
void abo_spi_ChannelNoteDeviceLost(ABO_SPI_Handle_t* handle);

#endif /* ABO_SPI_CHANNEL_H */
