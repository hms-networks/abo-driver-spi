/*
 * SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
 * Copyright (c) 2026 HMS Industrial Networks AB
 *
 * This file is part of the HMS Anybus ONE SDK and must only be used with
 * HMS products. See the LICENSE file in the root of the SDK for full
 * license terms.
 *
 * File:        abo_spi_frame.h
 * Description: Frame header, CRC and status bits. Internal to the driver.
 */

#ifndef ABO_SPI_FRAME_H
#define ABO_SPI_FRAME_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Bytes the header occupies: the layout ID and the sequence counter. */
#define ABO_SPI_HEADER_SIZE 3u

/** @brief Bytes the CRC occupies when one is configured. */
#define ABO_SPI_CRC_SIZE 2u

/** @brief What an idle bus reads as; neither is ever a layout ID. */
#define ABO_SPI_LAYOUT_ID_IDLE_LOW 0x0000u
/** @brief The other idle reading, a line held high. */
#define ABO_SPI_LAYOUT_ID_IDLE_HIGH 0xFFFFu

/*
** A channel's status byte. The configuration places the byte; the bits are fixed.
*/

/** @brief The producer of the channel's bytes is sound. */
#define ABO_SPI_STATUS_PRODUCER_GOOD 0x01u
/** @brief The consumer of the channel's bytes is taking them. */
#define ABO_SPI_STATUS_CONSUMER_GOOD 0x02u
/** @brief The channel has at least one connection. */
#define ABO_SPI_STATUS_OPEN 0x04u
/** @brief Toggles once for every connection that ends. */
#define ABO_SPI_STATUS_CLOSE 0x08u

/** @brief The bits a status byte is defined to carry. */
#define ABO_SPI_STATUS_DEFINED 0x0Fu

/** @brief Stamp the header: the layout ID little-endian, then the sequence counter. */
void abo_spi_FrameWriteHeader(uint8_t* frame, uint16_t layoutId, uint8_t sequence);

/** @brief The layout ID a frame leads with. */
uint16_t abo_spi_FrameLayoutId(const uint8_t* frame);

/** @brief The sequence counter a frame carries. */
uint8_t abo_spi_FrameSequence(const uint8_t* frame);

/**
 * @brief Whether a layout ID is one an idle bus reads as.
 *
 * The CRC is optional, so this is what catches a peer that is not driving the
 * bus.
 */
bool abo_spi_FrameIsLayoutIdIdle(uint16_t layoutId);

/**
 * @brief CRC-16/CCITT-FALSE over everything a frame carries ahead of its CRC.
 * @param frame  The frame.
 * @param length The whole frame's length, the CRC included.
 */
uint16_t abo_spi_FrameCrc(const uint8_t* frame, uint16_t length);

/** @brief Compute the CRC and write it, little-endian, into the last two bytes. */
void abo_spi_FrameAppendCrc(uint8_t* frame, uint16_t length);

/** @brief Whether the CRC in the last two bytes is the one the rest of the frame gives. */
bool abo_spi_FrameIsCrcGood(const uint8_t* frame, uint16_t length);

/** @brief Whether a received sequence counter moved. A skip is not a fault. */
bool abo_spi_FrameHasSequenceAdvanced(uint8_t received, uint8_t last, bool lastIsValid);

#endif /* ABO_SPI_FRAME_H */
