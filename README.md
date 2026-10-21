# Anybus ONE — SPI host driver

Driver version 0.1.0 · header schema 1 · frame protocol edition 1

## What this is for

Your product has its own microcontroller and needs to exchange data with a PLC
over an industrial network such as PROFINET or EtherNet/IP, without you
implementing those protocols yourself.

The Anybus ONE does that part. It sits on your board, talks to the PLC over the
network, and talks to your microcontroller over SPI. This driver is the code on
your microcontroller that does the SPI side, so that your application only sees
named values:

```
   PLC  <--- PROFINET or EtherNet/IP --->  Anybus ONE  <--- SPI --->  Your microcontroller
                                                                       |  your application
                                                                       |  this driver
                                                                       |  abo_spi_config.h (generated)
```

```c
setpoint = ABO_SPI_PUMP_CONTROL_Get_SPEED_SETPOINT(&spi);   /* what the PLC sent  */
ABO_SPI_PUMP_STATUS_Set_ACTUAL_SPEED(&spi, MeasureSpeed());  /* what the PLC reads */
```

You never handle a PROFINET or EtherNet/IP frame, and you never look up a byte
offset. The names come from your configuration in Anybus Studio: here, a data set
named *Pump control* holding a value named *Speed setpoint*.

## How your data reaches the PLC

You decide what your product exchanges with the PLC in **Anybus Studio**. Studio
produces two files from that one configuration, and both are needed:

- a **configuration file** that you load into the Anybus ONE, and
- **`abo_spi_config.h`**, a C header that you compile into your firmware.

They describe the same data, so the module and your code agree about where each
value is. If you change the configuration, generate both again.

What you set up in Studio, and what it becomes on each side:

| In Studio | On PROFINET | On EtherNet/IP | In your C code |
|---|---|---|---|
| **Data set**, direction *Input* | The input data of a submodule | An input assembly instance | Functions you call to **set** its values |
| **Data set**, direction *Output* | The output data of a submodule | An output assembly instance | Functions you call to **get** its values |
| **Parameter** placed on the SPI interface | Not supported | Not supported | A function to get or set it |

**Input and Output are named from the PLC's side**, as PROFINET and EtherNet/IP
name them: *Input* is data the PLC receives from your product, so your code
writes it; *Output* is data the PLC sends to your product, so your code reads it.

A data set travels cyclically, once per cycle, while a PLC has a connection to
it. In the header each data set is an **IO channel**, and its functions are
named after the data set, such as `ABO_SPI_PUMP_CONTROL_...`.

A parameter is a single value exchanged between your microcontroller and the
Anybus ONE only, such as the network LED status, which your product shows on its
own LEDs, or a configured serial number. It is
exchanged on every SPI frame, whether a PLC is connected or not. Studio does not
map parameters to the network.

The names in this README - `PUMP_CONTROL`, `PUMP_STATUS`, `SPEED_SETPOINT` - come
from an imaginary pump. Yours come from the names you type in Studio, converted
to upper case. `examples/abo_spi_config.h` is the header for that pump, to look
at.

## When the PLC connects, stops or goes away

The PLC is not always there, and not always running. Each IO channel tells you
what state it is in, and you tell the Anybus ONE how your side is doing.

Every channel has these two:

| Call | Answers | On PROFINET | On EtherNet/IP |
|---|---|---|---|
| `_IsOpen` | Does a PLC have a connection to this data set? | An established AR containing the submodule | An open I/O connection to the assembly |
| `_TakeCloseEvent` | Has a connection ended since you last asked? Asking clears it. | The AR was released or aborted | The connection was closed, aborted or timed out |

An *Output* channel, from the PLC to you, also has:

| Call | Answers | On PROFINET | On EtherNet/IP |
|---|---|---|---|
| `_IsDataValid` | Are these real values from a running PLC? | The AR open, and output IOPS good | The connection open, and the PLC in Run rather than Idle |
| `_SetConsumerGood` | Your side: did you take the PLC's values? Starts as good. | Your IOCS | Not carried as a status |

An *Input* channel, from you to the PLC, also has:

| Call | Answers | On PROFINET | On EtherNet/IP |
|---|---|---|---|
| `_IsConsumerGood` | Did the PLC take the values you sent? | Input IOCS | Not reported: good while the connection is open |
| `_SetProducerGood` | Your side: are your values real? Starts as not good, and the Anybus ONE only uses them while they are good. | Your IOPS | Not carried as a status |

Producer and consumer are the words the PROFINET and Anybus documentation use:
the producer says whether its values are valid, the consumer whether it took
them.

**What an Output channel holds when the PLC is not driving it.** When the PLC
stops, disconnects or reports its outputs as bad, the Anybus ONE replaces the
channel's values with the **offline action** you chose in Studio: *Freeze*
keeps the last values the PLC sent, *Default* writes the default values you
configured. `_IsDataValid` turns false.

So read the values whatever `_IsDataValid` says: they are always the values
your configuration wants your product to act on. If you keep
your own copy of the old values while `_IsDataValid` is false, you have turned a
configured *Default* into a freeze. Read them on *every* `ABO_SPI_OK`, not every
few: the driver confirms to the Anybus ONE that you have seen a disconnection
with the very next frame.

**What you send before your values are ready.** Until you call
`_SetProducerGood(&spi, true)`, the Anybus ONE does not use your Input values:
the PLC receives the channel's offline action, *Default* values or frozen ones,
on every network, and on PROFINET your data is also marked bad. Call it once
your values are real, and with `false` whenever they stop being so.

## What you get, and what you write

| Path | What it is |
|---|---|
| `inc/abo_spi.h` | The only header your application includes. Add `inc/` to your include path. |
| `src/` | The driver. Compile the four `.c` files; the headers there are internal. |
| `abo_spi_config.h` | **Generated by Anybus Studio for your configuration.** The pump's is in `examples/`. |
| `examples/io_channels.c` | The example: a main loop using data sets, to read and copy. |
| `examples/ports/template.c` | **The file you write**: two functions, described below. |
| `.clang-format` | The formatting. |
| `LICENSE` | The licence; see the end of this file. |

The driver is plain C99 with no operating system, heap, locks or vendor library.
On a toolchain that defaults to C90 (Keil ARMCC5, some IAR configurations), turn
C99 on.

You write two functions, called a *port*:

- **`transfer`** - send and receive one frame over SPI, full duplex.
- **`nowUs`** - a free-running microsecond clock.

`template.c` has both, empty, and its comments say what each must do. The driver
does not touch your SPI peripheral itself, so you keep your own DMA, interrupt
priorities and chip-select handling.

`transfer` returns when the whole frame has been exchanged. With an RTOS, start
the DMA and wait on a semaphore that the DMA-complete interrupt gives, so other
tasks run meanwhile. Without one, the call waits for the frame: at 30 MHz, about
27 µs for 100 bytes and 400 µs for the largest frame, 1500 bytes.

Three things catch people out:

- **The SPI clock has a minimum as well as a maximum:** between
  `ABO_SPI_MIN_FREQUENCY_HZ` and `ABO_SPI_FREQUENCY_HZ`. Prescalers usually step
  in powers of two, so "one step slower to be safe" can fall below the minimum.
  The symptom is transfers timing out.
- **`timeoutUs` is advisory.** A millisecond API may round it up; Linux `spidev`
  cannot express it.
- **DMA on a processor with a data cache** (STM32F7/H7, i.MX RT): the buffers the
  driver hands you are cache-line aligned under GCC, Clang, ARM Compiler 5 and 6,
  MSVC and any C11 compiler, as long as the handle is a static or global
  variable; memory you allocate for it must be aligned to
  `ABO_SPI_WIRE_ALIGN_BYTES` (32) yourself. On another compiler the build stops
  until you define `ABO_SPI_WIRE_ALIGN`: as that compiler's alignment attribute,
  or empty if your port does not use DMA. Getting this wrong corrupts the
  driver's state with no error.

## Wiring

Your microcontroller is the SPI master:

| Signal | Direction | Notes |
|---|---|---|
| SCLK | to the Anybus ONE | Up to 30 MHz; see the clock range above. |
| MOSI | to the Anybus ONE | |
| MISO | from the Anybus ONE | |
| CS | to the Anybus ONE | Asserted once per frame, held for the whole frame, and released between frames. |

Set your SPI peripheral from the generated header: `ABO_SPI_MODE` is the SPI
mode, 0 to 3 (clock polarity and phase), and `ABO_SPI_MSB_FIRST` the bit order.

There is no interrupt or ready line. Your side decides when each frame happens,
and the driver keeps the required pause between frames (`ABO_SPI_MIN_GAP_US`,
50 µs) by itself.

While the Anybus ONE starts up after power-on it does not answer yet, so
`ABO_SPI_RunCycle` returns `ABO_SPI_ERR_IDLE` and the link may be reported lost.
That is normal; the first frame it answers ends it.

## Quick start

1. In Anybus Studio, set up your data sets and parameters and generate. Load
   the configuration file into the Anybus ONE, and put `abo_spi_config.h` in
   your project. Both must come from the **same** generation.
2. Copy this folder into your project, compile the four `.c` files in `src/`,
   and add `inc/` and the folder holding `abo_spi_config.h` to your include
   path.
3. Write your port, starting from `examples/ports/template.c`.
4. Read `examples/io_channels.c`, and copy what you need.
5. Call `ABO_SPI_RunCycle` in a loop, and report your Input values as good with
   `_SetProducerGood` once they are (see above).

```c
ABO_SPI_Init(&spi, &port);
ABO_SPI_PUMP_STATUS_Set_ACTUAL_SPEED(&spi, 0u);   /* not used until reported good */

for (;;)
{
   if (ABO_SPI_RunCycle(&spi) != ABO_SPI_OK)
   {
      continue;   /* usually: not time for the next frame yet */
   }

   /* A new frame has arrived. Read what the PLC sent... */
   setpoint = ABO_SPI_PUMP_CONTROL_Get_SPEED_SETPOINT(&spi);
   running  = ABO_SPI_PUMP_CONTROL_IsDataValid(&spi);

   /* ...and write what it will receive with the next frame. */
   ABO_SPI_PUMP_STATUS_Set_ACTUAL_SPEED(&spi, MeasureSpeed());
   ABO_SPI_PUMP_STATUS_SetProducerGood(&spi, true);
}
```

`ABO_SPI_RunCycle` returns `ABO_SPI_OK` once per cycle, when a frame has been
exchanged and checked. Every value you read after that belongs to that one
frame, so inside that branch nothing needs checking.

Call it as often as you like, and at least once per cycle time. The cycle time
is the one you set in Studio, `ABO_SPI_CYCLE_TIME_US` in the header.

### Outside the loop

Another task or a display that wants to know whether the values are current
asks `ABO_SPI_IsOnline`.

If the Anybus ONE stops answering - powered off, reset, unplugged -
`ABO_SPI_IsOnline` turns false and every channel reads as closed. Act on it
once, when it changes from true to false, as the example does.

The offline action does not cover this case. It is applied by the Anybus ONE, so
when the module itself is gone, the *Output* values simply keep the last values
the PLC sent, even if you configured *Default*. Decide what your product does
then - stop, or go to a safe state - as the `io_channels` example does by
zeroing its setpoint.

## Using the values

### Numbers

Every number in a data set gets one function, named after the data set and the
value as you typed them in Studio. A data set *Pump control* (*Output*) holding
*Speed setpoint*, and a data set *Pump status* (*Input*) holding *Actual speed*,
give you:

```c
uint16_t ABO_SPI_PUMP_CONTROL_Get_SPEED_SETPOINT(const struct ABO_SPI_Handle* handle);
void     ABO_SPI_PUMP_STATUS_Set_ACTUAL_SPEED(struct ABO_SPI_Handle* handle, uint16_t value);
```

The C type follows the type you chose in Studio, and the byte order is taken
care of. You read *Output* values and write *Input* values; there is no setter
for a value the PLC sends, and no getter for one it receives.

Parameters work the same way, without a data set in the name:
`ABO_SPI_Get_<VALUE>` and `ABO_SPI_Set_<VALUE>`.

### Arrays and strings

An array or a string gets no function, because there is no single right way to
copy one. Instead, the header tells you where it is in the SPI frame and how
many bytes it takes, and you copy it yourself:

```c
/* "Temperatures", 8 x UINT16, in the data set "Pump status" */
uint8_t* at = ABO_SPI_FromHostImage(&spi) + ABO_SPI_PUMP_STATUS_TEMPERATURES_OFFSET;

for (i = 0u; i < (ABO_SPI_PUMP_STATUS_TEMPERATURES_SIZE / 2u); i++)
{
   ABO_SPI_WrU16Be(at + (2u * i), temperature[i]);   /* Be: ..._TEMPERATURES_BE is 1 */
}
```

`ABO_SPI_FromHostImage` is the frame you send next. To read a value instead,
start from `ABO_SPI_ToHostImage(&spi)`, the last frame received, and use the
matching `Rd` helper, such as `ABO_SPI_RdU16Be`. Pick `Be` or `Le` from the
value's `..._BE` macro.

A string works the same way, without the byte order: copy its bytes, at most
`..._SIZE` of them.

### Data sets that share bytes

You can skip this unless you set it up in Studio.

Studio can let several data sets use the same bytes of the SPI frame, to keep
the frame short when the data sets are not all in use at once. The data sets
then have the same `..._GROUP` number in the header.

- **Output** data sets that share bytes are never connected at the same time.
  The bytes hold the values of whichever one is connected, so ask each one's
  `_IsOpen` to know whose values you are reading.
- **Input** data sets that share bytes can be connected together. You write the
  values once, and every PLC connection receives them.

A typical use is a choice of control profiles. Say the pump can be run either
by speed or by torque, and you offer that as two *Output* data sets, *Speed
control* and *Torque control*, each a module of its own on PROFINET. The PLC
engineer plugs the one the application needs. Since only
one is ever connected, they can share bytes, which keeps the frame shorter; on
separate bytes they would work just as well. When they share, the bytes alone
cannot tell you which profile they belong to, so your code asks which one the
PLC chose:

```c
if (ABO_SPI_SPEED_CONTROL_IsOpen(&spi))
{
   RunAtSpeed(ABO_SPI_SPEED_CONTROL_Get_SETPOINT(&spi));
}
else if (ABO_SPI_TORQUE_CONTROL_IsOpen(&spi))
{
   RunAtTorque(ABO_SPI_TORQUE_CONTROL_Get_SETPOINT(&spi));
}
else
{
   Stop();   /* no PLC is controlling the pump */
}
```

## Where to call it from

**A main loop:** every pass, as above.

**An RTOS task or a thread:** sleep until the next cycle is due instead of
spinning.

```c
for (;;)
{
   uint32_t wait = ABO_SPI_UsUntilDue(&spi);

   if (wait > 0u)
   {
      SleepMicroseconds(wait);   /* vTaskDelay, usleep, whatever you have */
      continue;
   }

   if (ABO_SPI_RunCycle(&spi) == ABO_SPI_OK)
   {
      UseTheFrame();
   }
}
```

A periodic timer works too; a tick that fires slightly early still runs the
cycle.

**Not from an interrupt.** `transfer` waits for a whole frame.

**From one task only.** Call every driver function, including the get and set
functions, from the same task. If other tasks need the values, let them
exchange them with that task, through your RTOS's queues or a copy guarded by a
mutex.

**Your scheduler must be able to call it in time.** If the Anybus ONE hears
nothing for `ABO_SPI_CYCLE_TIME_US` × `ABO_SPI_HOST_LOSS_CYCLES` microseconds,
it treats your side as gone and reports your data as bad to the PLC. In the
example header that is 1 ms × 8 = 8 ms, so a scheduler with a 10 ms tick, such as
ESP-IDF's default, is too coarse.

## What is on the wire

You do not need this to use the driver. It is for reading a capture from a logic
analyzer.

Every SPI transfer is one frame, and both directions are the same length,
`ABO_SPI_FRAME_LENGTH`. The frame you send and the frame you receive are laid out
independently, but both look like this:

```
+-----------+----------+------------+-------------------------+-----------+
| Layout ID | Sequence | Parameters | Data sets               | CRC       |
| 2 bytes   | 1 byte   | if any     | a status byte each,     | 2 bytes,  |
|           |          |            | then their values       | if used   |
+-----------+----------+------------+-------------------------+-----------+
```

- **Layout ID** identifies the configuration, `ABO_SPI_LAYOUT_ID` in the header,
  sent low byte first. `00 00` or `FF FF` means nobody is driving the line: that
  is what `ABO_SPI_ERR_IDLE` reports.
- **Sequence** changes with every new frame. Your side's counts up by one per
  frame. If the Anybus ONE's stays the same, it re-sent its last frame:
  `ABO_SPI_REPEATED`.
- **Parameters and data sets** hold your values. To find one in a capture, look
  up its `..._OFFSET` macro in the header: the offset counts from the first byte
  of the frame. Bytes nothing is placed in are zero.
- **CRC**, when your configuration uses one, is the last two bytes, sent low
  byte first: CRC-16/CCITT-FALSE (polynomial `0x1021`, initial value `0xFFFF`)
  over every byte before it.

Between frames, chip select is released for at least 50 µs.

## When it does not work

**The build stops:**

| Message mentions | What it means |
|---|---|
| *header schema* or *frame protocol edition* | The generated header and this driver are from different releases. |
| *makes the host the SPI slave* | Your configuration makes the Anybus ONE the SPI master. Not supported. |
| *frame too short*, *more IO channels*, *host-loss timeout too long*, *of zero cycles* or *lies_in_the_frame* | The header was not produced by Studio. |
| *cannot align the wire buffers* | Define `ABO_SPI_WIRE_ALIGN` for your compiler; see the DMA note above. |
| *cannot be clocked within its cycle time* | Generate again with a longer cycle time or a smaller frame. |

**What `ABO_SPI_RunCycle` returns:**

| Result | What it means |
|---|---|
| `ABO_SPI_OK` | A new frame was exchanged and checked. Read and write your values now. |
| `ABO_SPI_NOT_DUE` | It is not yet time for the next frame. Nothing was sent. Not an error. |
| `ABO_SPI_REPEATED` | The Anybus ONE re-sent its last frame. Not an error now and then; see below if it climbs steadily. |
| `ABO_SPI_ERR_ARGUMENT` | A null handle, or a port without its two functions. `ABO_SPI_Init` returns it too. |
| `ABO_SPI_ERR_LAYOUT` | The header and the module's configuration are from different generations. Generate again and load both. |
| `ABO_SPI_ERR_IDLE` | Nothing answers on the bus: the Anybus ONE is unpowered, not started, or miswired. |
| `ABO_SPI_ERR_TRANSFER` | Your `transfer` reported a failure. |
| `ABO_SPI_ERR_SHORT_FRAME` | Your `transfer` moved the wrong number of bytes: check the length or the DMA descriptor. |
| `ABO_SPI_ERR_CRC` | The frame was corrupted on the wire: clock rate, signal integrity, chip-select timing. |

**Something looks wrong:**

| What you see | What it means |
|---|---|
| One transfer, then never another | `nowUs` is not advancing. |
| `REPEATED` climbing steadily | Your loop runs faster than the Anybus ONE produces frames, or the cycle time is too short for it. |
| `LATE` climbing | Whole cycles are missed: your loop is slower than the cycle time. |
| `IsOnline` false between frames, or `DEVICE_LOST` climbing while frames arrive | Your loop leaves more than the host-loss timeout between calls. |
| `ERR_IDLE` right after power-up | The Anybus ONE is still starting. Normal; it ends with the first frame it answers. |
| `DEVICE_LOST` 1 and `ACCEPTED` 0 well after power-up | The Anybus ONE has never answered: check power, wiring, and that it has a configuration loaded. |
| Everything reads zero | No frame has been received yet. |
| The PLC gets default or frozen values from you, or your Input data shows as bad on PROFINET | You have not called `_SetProducerGood(&spi, true)`; see "What you send before your values are ready". |
| A close event right after the Anybus ONE was lost | Intended: you cannot know what happened while it was gone. |

## Counters

`ABO_SPI_Counter(&spi, which)` reads one of nine counters: `ACCEPTED`,
`TRANSFER`, `SHORT_FRAME`, `LAYOUT`, `CRC`, `REPEATED`, `IDLE`, `LATE`,
`DEVICE_LOST`. They stop at their maximum rather than wrap. **For a support
case, send the driver version and all nine.** The version is the string
`ABO_SPI_DRIVER_VERSION` in `abo_spi.h`.

## Size and limits

| | |
|---|---|
| Flash | About 1.7 KB (Cortex-M33, GCC `-Os`, a configuration with a 15-byte frame and two data sets). |
| RAM | The handle: a little over four times the frame length. 192 bytes for a 15-byte frame, about 6.1 KB for a 1500-byte frame. |
| Frame length | Up to 1500 bytes. |
| Cycle time | A multiple of 1 ms, at least 1 ms. |
| SPI clock | Up to 30 MHz. |

The frame length and cycle time are set in Studio. The driver allocates nothing:
the handle is a variable you declare, usually static.

## The example

`examples/io_channels.c` is the pump in this README as a main loop: it reads the
setpoint and the network LED status, writes the actual speed and reports it as
good, and handles disconnections and losing the module. It is meant to be read
and copied, not run. Its names come from `examples/abo_spi_config.h`, and the
functions starting with `App_` are your application.

## Licence

The driver is licensed under the **HMS INT 4-Clause License, Version 1.0**
(`LicenseRef-HMS-INT-4-Clause-1.0`). The full text is in the [LICENSE](LICENSE)
file.

In short:

- **You may** use, copy, modify and redistribute the driver, in source or binary
  form, including in commercial products.
- **Only with HMS products:** the driver, modified or not, must only be used
  with products from HMS Networks AB or its subsidiaries.
- **Keep the notices:** source redistributions must keep the copyright notice
  and license text. Binary redistributions, including firmware in your own
  products, must include them in the documentation or other materials
  shipped with the product.
- **No warranty, no liability:** the driver is provided "as is". HMS gives no
  warranty and is not liable for any damage resulting from its use.
- **No endorsement:** you may not use the HMS name to promote products derived
  from the driver without written permission.

This summary is for convenience only. If it differs from the [LICENSE](LICENSE)
file, the LICENSE file applies.

The license is based on the BSD-3-Clause license, with one added condition
(clause 4) that limits use to HMS products. Because of that condition it is
not BSD-3-Clause, and the driver is source-available rather than open source as
defined by the OSI. When registering the driver in a license-compliance tool or
SBOM, use the identifier `LicenseRef-HMS-INT-4-Clause-1.0`, not
`BSD-3-Clause`.

The driver contains no third-party software.
