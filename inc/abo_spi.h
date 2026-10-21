/*
 * SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
 * Copyright (c) 2026 HMS Industrial Networks AB
 *
 * This file is part of the HMS Anybus ONE SDK and must only be used with
 * HMS products. See the LICENSE file in the root of the SDK for full
 * license terms.
 *
 * File:        abo_spi.h
 * Description: Public interface
 */

#ifndef ABO_SPI_H
#define ABO_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
** Version
**
** The driver's own version; the generated header carries the configuration's.
** Below 1.0.0 the enumerations may be renumbered; from 1.0.0 they only grow at
** the end.
*******************************************************************************/

#define ABO_SPI_DRIVER_VERSION_MAJOR 0u
#define ABO_SPI_DRIVER_VERSION_MINOR 1u
#define ABO_SPI_DRIVER_VERSION_PATCH 0u

/** @brief The version as text. */
#define ABO_SPI_DRIVER_VERSION "0.1.0"

/*******************************************************************************
** What the generated accessors need
**
** Declared before the configuration header is included, and the handle defined
** after it, so that an application may include either header first.
*******************************************************************************/

/** @brief One link to one device. */
struct ABO_SPI_Handle;

/*
** These two, and the generated accessors that call them, require a handle.
** Every other call answers a NULL handle with ABO_SPI_ERR_ARGUMENT, false or
** zero.
*/

/**
 * @brief The last accepted frame from the device. Zeros until one is accepted.
 * @param handle Must not be NULL; see above.
 */
const uint8_t* ABO_SPI_ToHostImage(const struct ABO_SPI_Handle* handle);

/**
 * @brief The frame being built for the device, sent by the next cycle.
 * @param handle Must not be NULL; see above.
 */
uint8_t* ABO_SPI_FromHostImage(struct ABO_SPI_Handle* handle);

/**
 * @name Byte-at-a-time value access
 * Rd/Wr, then the type, then the byte order on the wire (Le/Be). Any alignment, any host byte order.
 * @{
 */
uint8_t ABO_SPI_RdU8(const uint8_t* at);
uint16_t ABO_SPI_RdU16Le(const uint8_t* at);
uint16_t ABO_SPI_RdU16Be(const uint8_t* at);
uint32_t ABO_SPI_RdU32Le(const uint8_t* at);
uint32_t ABO_SPI_RdU32Be(const uint8_t* at);
uint64_t ABO_SPI_RdU64Le(const uint8_t* at);
uint64_t ABO_SPI_RdU64Be(const uint8_t* at);
float ABO_SPI_RdF32Le(const uint8_t* at);
float ABO_SPI_RdF32Be(const uint8_t* at);
double ABO_SPI_RdF64Le(const uint8_t* at);
double ABO_SPI_RdF64Be(const uint8_t* at);

void ABO_SPI_WrU8(uint8_t* at, uint8_t value);
void ABO_SPI_WrU16Le(uint8_t* at, uint16_t value);
void ABO_SPI_WrU16Be(uint8_t* at, uint16_t value);
void ABO_SPI_WrU32Le(uint8_t* at, uint32_t value);
void ABO_SPI_WrU32Be(uint8_t* at, uint32_t value);
void ABO_SPI_WrU64Le(uint8_t* at, uint64_t value);
void ABO_SPI_WrU64Be(uint8_t* at, uint64_t value);
void ABO_SPI_WrF32Le(uint8_t* at, float value);
void ABO_SPI_WrF32Be(uint8_t* at, float value);
void ABO_SPI_WrF64Le(uint8_t* at, double value);
void ABO_SPI_WrF64Be(uint8_t* at, double value);
/** @} */

/*
** The three Is calls read false before the first accepted frame and after
** RunCycle has declared the device lost. A call for the other direction, or
** an index outside the configuration, reads false or does nothing.
*/

/** @brief Whether the channel has at least one connection. */
bool ABO_SPI_ChannelIsOpen(const struct ABO_SPI_Handle* handle, uint8_t index);

/**
 * @brief Whether an Output channel's bytes are produced values rather than the configured substitute.
 *
 * True while the channel is open and the network's producer status is good.
 * Otherwise the bytes are the channel's offline action, frozen or default, and
 * still a value to read: this answers "is it live", not "may I read it".
 */
bool ABO_SPI_ChannelIsDataValid(const struct ABO_SPI_Handle* handle, uint8_t index);

/** @brief Whether the network took an Input channel's data: its consumer status. */
bool ABO_SPI_ChannelIsConsumerGood(const struct ABO_SPI_Handle* handle, uint8_t index);

/**
 * @brief Take the latched close event: a connection ended since the last call.
 *
 * The close marker is one bit, so two closes with no frame accepted between them
 * read as none. That needs a host that was away, so the driver also latches a
 * close on every channel when it declares the device lost.
 */
bool ABO_SPI_ChannelTakeCloseEvent(struct ABO_SPI_Handle* handle, uint8_t index);

/**
 * @brief Report whether the host's data for an Input channel is valid: its producer status.
 *
 * Starts bad at ABO_SPI_Init. While it is bad the device does not use the
 * channel's data: the network receives the channel's offline action instead.
 */
void ABO_SPI_ChannelSetProducerGood(struct ABO_SPI_Handle* handle, uint8_t index, bool good);

/** @brief Report whether the host took an Output channel's data: its consumer status. Starts good. */
void ABO_SPI_ChannelSetConsumerGood(struct ABO_SPI_Handle* handle, uint8_t index, bool good);

/*******************************************************************************
** The configuration
*******************************************************************************/

#include "abo_spi_config.h"

#if ABO_SPI_HEADER_SCHEMA != 1
#error "abo_spi_config.h is a header schema this driver does not accept - update the driver."
#endif

#if ABO_SPI_FRAME_VERSION != 1
#error "abo_spi_config.h is for a frame protocol edition this driver does not implement - update the driver."
#endif

/*
** 3 and 2 are ABO_SPI_HEADER_SIZE and ABO_SPI_CRC_SIZE from the internal
** abo_spi_frame.h.
*/
#if ABO_SPI_FRAME_LENGTH < (3u + (ABO_SPI_CRC_ENABLED ? 2u : 0u))
#error "abo_spi_config.h states a frame too short to hold its own header and CRC."
#endif

#if ABO_SPI_CHANNEL_COUNT > 255u
#error "abo_spi_config.h states more channels than the driver indexes with a uint8_t."
#endif

/*
** Floats cross the wire as bit patterns, and C does not fix their width. The
** array length goes negative where the width is wrong, for a type this
** configuration uses.
*/
#if ABO_SPI_USES_FLOAT32
typedef char abo_spi_float_is_four_bytes[(sizeof(float) == 4u) ? 1 : -1];
#endif

#if ABO_SPI_USES_FLOAT64
typedef char abo_spi_double_is_eight_bytes[(sizeof(double) == 8u) ? 1 : -1];
#endif

/*******************************************************************************
** Constants
*******************************************************************************/

/** @brief Shortest gap between two transfers, in microseconds. A constant of the frame protocol. */
#ifndef ABO_SPI_MIN_GAP_US
#define ABO_SPI_MIN_GAP_US 50u
#endif

/**
 * @brief How early a cycle may start, in microseconds, so that a timer tick
 * that fires a little early still keeps the configured rate. ABO_SPI_MIN_GAP_US
 * is always kept in full.
 */
#define ABO_SPI_EARLY_MARGIN_US ((ABO_SPI_CYCLE_TIME_US) / 32u)

/** @brief Channel entries in the handle. At least one. */
#define ABO_SPI_CHANNEL_ENTRIES ((ABO_SPI_CHANNEL_COUNT) > 0u ? (ABO_SPI_CHANNEL_COUNT) : 1u)

/**
 * @brief Alignment of the two wire buffers, in bytes: a cache line.
 *
 * For a port that transfers by DMA on a host with a data cache. Cache
 * maintenance acts on whole lines, so a shared line would corrupt its neighbour.
 */
#ifndef ABO_SPI_WIRE_ALIGN_BYTES
#define ABO_SPI_WIRE_ALIGN_BYTES 32
#endif

/**
 * @brief The compiler's alignment attribute for the wire buffers.
 *
 * `__GNUC__` covers GCC, Clang and ARM Compiler 6, `__CC_ARM` ARM Compiler 5,
 * and `_Alignas` every C11 compiler. Any other compiler stops the build until
 * this macro is defined: as that compiler's alignment attribute for a port that
 * transfers by DMA on a host with a data cache, or empty for any other port,
 * which needs no alignment.
 */
#ifndef ABO_SPI_WIRE_ALIGN
#if defined(__GNUC__) || defined(__CC_ARM)
#define ABO_SPI_WIRE_ALIGN __attribute__((aligned(ABO_SPI_WIRE_ALIGN_BYTES)))
#elif defined(_MSC_VER)
#define ABO_SPI_WIRE_ALIGN __declspec(align(ABO_SPI_WIRE_ALIGN_BYTES))
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define ABO_SPI_WIRE_ALIGN _Alignas(ABO_SPI_WIRE_ALIGN_BYTES)
#else
#error "abo_spi.h cannot align the wire buffers on this compiler: define ABO_SPI_WIRE_ALIGN (empty without DMA)."
#endif
#endif

/*
** The frame and the gap after it must fit the cycle at the configured clock.
** The clocking time is rounded up, as in ABO_SPI_MIN_FREQUENCY_HZ below.
*/
#if (((((ABO_SPI_FRAME_LENGTH) * 8000000u) + (ABO_SPI_FREQUENCY_HZ) - 1u) / (ABO_SPI_FREQUENCY_HZ))                  \
     + (ABO_SPI_MIN_GAP_US))                                                                                         \
    > (ABO_SPI_CYCLE_TIME_US)
#error "abo_spi_config.h states a frame that cannot be clocked within its cycle time."
#endif

/** @brief The slowest SPI clock, in hertz, that still fits the frame and the gap in the cycle. */
/*
** Rounded up, not down: this is the slowest clock that still fits, and the
** integer division would otherwise name one a hair too slow to fit.
*/
#define ABO_SPI_MIN_FREQUENCY_HZ                                                                                     \
   ((uint32_t)((((uint64_t)(ABO_SPI_FRAME_LENGTH) * 8000000u)                                                        \
                + ((ABO_SPI_CYCLE_TIME_US) - (ABO_SPI_MIN_GAP_US)) - 1u) /                                           \
               ((ABO_SPI_CYCLE_TIME_US) - (ABO_SPI_MIN_GAP_US))))

/*******************************************************************************
** Public types
*******************************************************************************/

/**
 * @brief What one cycle did. Every reason a frame was not accepted is its own value.
 *
 * `ABO_SPI_OK`, `ABO_SPI_NOT_DUE` and `ABO_SPI_REPEATED` are not failures.
 * A repeat publishes nothing, there being nothing new to publish, and does not
 * count as having heard from the device: one that only ever repeats is still
 * declared lost once the host-loss timeout passes. That is deliberate. A device
 * answering with a frame it composed before the timeout began has said nothing
 * since.
 */
typedef enum ABO_SPI_Result
{
   ABO_SPI_OK = 0,             /**< A frame was accepted and published. */
   ABO_SPI_NOT_DUE = 1,        /**< The cycle time or the gap has not elapsed. Not an error. */
   ABO_SPI_ERR_TRANSFER = 2,   /**< The port reported failure. Nothing was exchanged. */
   ABO_SPI_ERR_SHORT_FRAME = 3,/**< The port exchanged a number of bytes that is not the frame length. */
   ABO_SPI_ERR_LAYOUT = 4,     /**< Layout ID mismatch: this header and the device's configuration differ. */
   ABO_SPI_ERR_CRC = 5,        /**< CRC mismatch. */
   ABO_SPI_REPEATED = 6,       /**< The device re-sent the frame it last composed. Not an error. */
   ABO_SPI_ERR_IDLE = 7,       /**< Layout ID read as 0x0000 or 0xFFFF: nobody is driving the bus. */
   ABO_SPI_ERR_ARGUMENT = 8    /**< A null handle, or a port without its two callbacks. */
} ABO_SPI_Result_t;

/** @brief The counters a support case is diagnosed from. */
typedef enum ABO_SPI_Counter
{
   ABO_SPI_COUNTER_ACCEPTED = 0,    /**< Frames accepted and published. */
   ABO_SPI_COUNTER_TRANSFER = 1,    /**< Transfers the port reported as failed. */
   ABO_SPI_COUNTER_SHORT_FRAME = 2, /**< Transfers that exchanged other than the frame length. */
   ABO_SPI_COUNTER_LAYOUT = 3,      /**< Frames discarded on the layout ID. */
   ABO_SPI_COUNTER_CRC = 4,         /**< Frames discarded on the CRC. */
   ABO_SPI_COUNTER_REPEATED = 5,    /**< Frames the device repeated, having composed no new one. */
   ABO_SPI_COUNTER_IDLE = 6,        /**< Frames discarded as an idle bus. */
   ABO_SPI_COUNTER_LATE = 7,        /**< Cycles that began a whole cycle time or more late. */
   ABO_SPI_COUNTER_DEVICE_LOST = 8  /**< Times the device was declared lost. */
} ABO_SPI_Counter_t;

/** @brief How many counters there are. */
#define ABO_SPI_COUNTER_COUNT 9u

/**
 * @brief The two callbacks the application supplies.
 *
 * Zero the whole structure before filling it in: a later release may add
 * members, and reads a zero one as absent.
 */
typedef struct ABO_SPI_Port
{
   /**
    * @brief Exchange exactly length bytes full duplex within one chip-select assertion.
    *
    * Blocks until done or timeoutUs has passed; a DMA or interrupt-driven
    * transfer waits for its completion inside this call.
    *
    * @return The number of bytes exchanged, or a negative value on failure.
    */
   int32_t (*transfer)(void* ctx, const uint8_t* tx, uint8_t* rx, uint16_t length, uint32_t timeoutUs);

   /** @brief A free-running microsecond counter. It may wrap. */
   uint32_t (*nowUs)(void* ctx);

   /** @brief Passed back to both callbacks. */
   void* ctx;
} ABO_SPI_Port_t;

/** @brief What the driver keeps for one channel. */
typedef struct ABO_SPI_ChannelState
{
   uint8_t deviceStatus; /**< The status byte of the last accepted frame. */
   uint8_t ownStatus;    /**< What goes out: the host's two good bits and the echoed close marker. */
   bool closeEvent;      /**< Latched when the close marker moved; cleared by taking it. */
} ABO_SPI_ChannelState_t;

/**
 * @brief One link to one device, in the application's storage.
 *
 * Its fields are the driver's and may change between releases.
 *
 * Declare it static or global, where the compiler aligns it. Allocated memory
 * must be aligned to ABO_SPI_WIRE_ALIGN_BYTES, or the wire buffers are not.
 *
 * One handle belongs to one context: the driver takes no lock, so a Set_ from
 * an interrupt or another task can land half in one frame and half in the next.
 * A single byte cannot tear.
 *
 * ABO_SPI_IsOnline only reads, but it reads several 32-bit fields in no
 * guaranteed order and none of them is volatile. Calling it from another
 * context is sound only where an aligned 32-bit read
 * cannot tear - which is the ordinary 32-bit host, and is not an 8- or 16-bit
 * one - and even there the answer may be a cycle old. Anywhere else, ask from
 * the context that calls RunCycle.
 */
struct ABO_SPI_Handle
{
   ABO_SPI_Port_t port;                /**< The callbacks, copied at Init. */
   uint32_t counters[ABO_SPI_COUNTER_COUNT];
   uint32_t transferEndedUs;             /**< nowUs when the last transfer ended, for the gap. */
   uint32_t cycleStartedUs;              /**< nowUs when the last transfer started, for the cycle time. */
   uint32_t lastAcceptedUs;              /**< nowUs when the last accepted transfer ended, for device loss. */
   uint8_t txSequence;                   /**< The counter this host stamps into each frame. */
   uint8_t rxSequence;                   /**< The counter the last accepted frame carried. */

   /*
   ** Kept across a device loss, so that a stalled device repeating one counter
   ** is not accepted again the moment it is declared lost.
   */
   bool rxSequenceValid;                 /**< Whether rxSequence holds anything yet. */
   bool started;                         /**< Whether the link has run a cycle, so the two timestamps mean something. */
   bool accepted;                        /**< Whether any frame has ever been accepted. */
   bool deviceLost;                      /**< Whether the device is currently declared lost. */
   ABO_SPI_ChannelState_t channels[ABO_SPI_CHANNEL_ENTRIES];

   uint8_t toHost[ABO_SPI_FRAME_LENGTH];   /**< The last accepted frame, which the application reads. */
   uint8_t fromHost[ABO_SPI_FRAME_LENGTH]; /**< What the application writes, sent by the next cycle. */

   /*
   ** Last, so that nothing shares wireRx's final cache line. Keep them last.
   */
   ABO_SPI_WIRE_ALIGN uint8_t wireTx[ABO_SPI_FRAME_LENGTH]; /**< The frame being sent; not written during a transfer. */
   ABO_SPI_WIRE_ALIGN uint8_t wireRx[ABO_SPI_FRAME_LENGTH]; /**< What the last transfer brought in, accepted or not. */
};

/** @brief One link to one device. */
typedef struct ABO_SPI_Handle ABO_SPI_Handle_t;

/*******************************************************************************
** Public services
*******************************************************************************/

/**
 * @brief Prepare a handle for use. Nothing is clocked.
 *
 * Both images are zeroed, every channel reads closed, and every channel's own
 * status starts with the producer bad and the consumer good.
 *
 * @param handle The application's handle.
 * @param port   The two callbacks. Copied.
 * @return ABO_SPI_OK, or ABO_SPI_ERR_ARGUMENT for a null handle or a port
 *         missing either callback.
 */
ABO_SPI_Result_t ABO_SPI_Init(ABO_SPI_Handle_t* handle, const ABO_SPI_Port_t* port);

/**
 * @brief Run one cycle of the frame protocol: build, exchange, check, publish.
 *
 * Returns ABO_SPI_NOT_DUE until the cycle time and the gap have elapsed, so it
 * may be called as often as convenient. Nothing of a rejected frame is
 * published; the last accepted frame stays as it was.
 *
 * @return ABO_SPI_OK when a frame was accepted, and the reason otherwise.
 *         Everything read after ABO_SPI_OK belongs to that frame until the
 *         next call.
 */
ABO_SPI_Result_t ABO_SPI_RunCycle(ABO_SPI_Handle_t* handle);

/**
 * @brief Microseconds until ABO_SPI_RunCycle would run a cycle.
 *
 * The same calculation RunCycle decides by, so sleeping this long finds it due.
 *
 * @return Microseconds to wait, or zero when a cycle is due - also before the
 *         first cycle and for a handle with no port.
 */
uint32_t ABO_SPI_UsUntilDue(const ABO_SPI_Handle_t* handle);

/**
 * @brief Whether the to-host image holds real values: a frame has been accepted and the device is not lost.
 *
 * Goes false once ABO_SPI_CYCLE_TIME_US x ABO_SPI_HOST_LOSS_CYCLES microseconds
 * have passed without an accepted frame, also when RunCycle is no longer called.
 * ABO_SPI_COUNTER_DEVICE_LOST and the close events follow at the next RunCycle.
 * After a silence longer than the clock wraps (about 71 minutes at 32 bits) it
 * can read true again for one host-loss timeout.
 */
bool ABO_SPI_IsOnline(const ABO_SPI_Handle_t* handle);

/** @brief Read one counter. Counters saturate rather than wrap. */
uint32_t ABO_SPI_Counter(const ABO_SPI_Handle_t* handle, ABO_SPI_Counter_t which);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ABO_SPI_H */
