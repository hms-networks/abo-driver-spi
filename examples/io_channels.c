/*
 * SPDX-License-Identifier: LicenseRef-HMS-INT-4-Clause-1.0
 * Copyright (c) 2026 HMS Industrial Networks AB
 *
 * This file is part of the HMS Anybus ONE SDK and must only be used with
 * HMS products. See the LICENSE file in the root of the SDK for full
 * license terms.
 *
 * File:        io_channels.c
 * Description: Data sets: their values, whether they are live, disconnections,
 *              and losing the module
 */

#include "port.h"

/*
** Your application: a pump, as in the README.
*/
void App_RunAtSpeed(uint16_t setpoint);
void App_ShowPlcInControl(bool inControl);
void App_ShowNetworkLed(uint8_t ledStatus);
void App_ConnectionEnded(void);
uint16_t App_MeasureSpeed(void);
bool App_SpeedIsMeasured(void);

static ABO_SPI_Handle_t spi;

int main(void)
{
   ABO_SPI_Port_t port;
   bool wasOnline = false;

   if (!Port_Open(&port) || (ABO_SPI_Init(&spi, &port) != ABO_SPI_OK))
   {
      Port_Close();
      return 1;
   }

   /*
   ** Before the first cycle: a value. The module does not use it until
   ** SetProducerGood below says it is good.
   */
   ABO_SPI_PUMP_STATUS_Set_ACTUAL_SPEED(&spi, 0u);

   for (;;)
   {
      ABO_SPI_Result_t result = ABO_SPI_RunCycle(&spi);
      bool online = ABO_SPI_IsOnline(&spi);

      /*
      ** The module stopped answering. No offline action covers this case, so
      ** decide here, once, what the product does.
      */
      if (wasOnline && !online)
      {
         App_RunAtSpeed(0u);
      }

      wasOnline = online;

      if (result != ABO_SPI_OK)
      {
         continue;
      }

      /*
      ** Read on every accepted frame. While the PLC is not producing, these
      ** values are the offline action the configuration chose, frozen or
      ** default. IsDataValid says which.
      */
      App_RunAtSpeed(ABO_SPI_PUMP_CONTROL_Get_SPEED_SETPOINT(&spi));
      App_ShowPlcInControl(ABO_SPI_PUMP_CONTROL_IsDataValid(&spi));

      /*
      ** A parameter: between this product and the module only, in every frame
      ** whether or not a PLC is connected.
      */
      App_ShowNetworkLed(ABO_SPI_Get_NETWORK_LED_STATUS(&spi));

      /*
      ** A connection ended since the last frame, even one too short to see in
      ** IsOpen. Asking clears it.
      */
      if (ABO_SPI_PUMP_CONTROL_TakeCloseEvent(&spi))
      {
         App_ConnectionEnded();
      }

      /*
      ** What this product produces, and whether it is good, sent with the
      ** next frame.
      */
      ABO_SPI_PUMP_STATUS_Set_ACTUAL_SPEED(&spi, App_MeasureSpeed());
      ABO_SPI_PUMP_STATUS_SetProducerGood(&spi, App_SpeedIsMeasured());
   }
}
