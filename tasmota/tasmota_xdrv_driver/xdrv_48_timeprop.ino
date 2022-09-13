/*
  xdrv_48_timeprop.ino - Timeprop support for Sonoff-Tasmota

  Copyright (C) 2021  Colin Law and Thomas Herrmann

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_TIMEPROP
#ifndef FIRMWARE_MINIMAL

#define XDRV_48 48

/*********************************************************************************************\
 * Code to drive one or more relays in a time proportioned manner give a
 * required power value.
 *
 * The following commands are accepted. <x> being the number of the timeprop
 * to be controlled. <x> can be 1 to 4.
 *
 * Since Relay in Tasmota is already super flexible a direct connection has
 * been made. Timeprop1 is controlling Relay 1 and so on.
 *
 * TimepropSet<x>: set the actual value from 0 to 100.
 *
 * TimepropTimeBase<x>: set the time base for the given timprop in minutes. Accepted
 *                      values are between 0 and 31. A value of 0 disables the given
 *                      timeprop.
 *
 * TimepropFallbackValue<x>: In case a timeprop has not received a Set command
 *                           within a given time this value will be used.
 *                           Set between 0 and 100. Will be rounded to one of
 *                           8 values to keep configuration memory low.
 *
 * TimepropStartWithFallback: if set all timeprops will start with their fallback
 *                            values. Global setting. Set 0 or 1.
 *
 * TimepropFallbackAfter: If a timeprp has not received an update within set
 *                        time a fallback value will be set. This is setting the
 *                        time. Set hours between 0 and 127.
 *
 * Remarks:
 * The number of Timeprops is defined as MAX_TIMEPROPS. It can be increased, but
 * each timeprop is using 8 byte of flash. before increasing make sure that
 * free flash is available behind the timeprops. (see tasmota/include/tasmota_types.h).
 *
 * Setting TimepropTimeBase<x> to zero is disabling the timeprop.
 *
 * To keep the amount if necessary flash low timebase is limited to 31 minutes.
 * and Fallback value is limited to 8 values between 0 and 100. (3 bit)
 *
 * Relay configuration is already super flexible. It has been voted against adding
 * another layer of flexibility. The relation between timeprop and relay is
 * fixed.
 * Timeprop1 -> Relay1
 * Timeprop2 -> Relay2
 * Timeprop3 -> Relay3
 * Timeprop4 -> Relay4
 * ...
 * if TimepropTimeBase<x> is set to zero no POER commands will be sent to the
 * given relay.
 * But make sure no Pulse is configured for one of the relays.
 *
 * The code has been written with floor heating in mind. normal cycle times
 * for this use case are between 15 and 30 minutes. In case longer times are
 * needed a global multiplier could be added but that is not implemented today.
 *
 **/

uint32_t fallback_after_seconds = 0; // To keep settings low we set fallback time in hours. We need seconds
bool start_with_fallback = false;    // shall we start with fallback or not

struct TIMEPROPSSTATE
{
  bool is_running = false;      // set to true with first set value
  bool is_open = false;         // set to true with first set value
  uint8_t set_value = 0;        // value that has been set via Set command
  uint8_t fallback_value = 0;   // Fallback Value as set via configuration
  uint32_t seconds_running = 0; // increased every second. Reset when time base reached
  uint32_t last_received = 0;   // Minutes increasing. Set to zero if a command is recieved
  uint32_t timebase = 0;        // Timebase in Seconds. In Settings is minute based. We run on seconds.

} Timepropsstate[MAX_TIMEPROPS];

const char kTimepropCommands[] PROGMEM = D_PRFX_TIMEPROP "|" D_CMND_TIMEPROP_SET "|" D_CMND_TIMEPROP_TIMEBASE "|" D_CMND_TIMEPROP_FALLBACKVALUE "|" D_CMND_TIMEPROP_STARTWITHFALLBACK "|" D_CMND_TIMEPROP_FALLBACKAFTER "|";

void (*const TimepropCommand[])(void) PROGMEM = {
    &CmndTimepropSet, &CmndTimepropTimeBase, &CmndTimepropFallbackvalue, &CmndTimepropStartWithFallback, &CmndTimepropFallbackAfter};

/*********************************************************************************************\
 * Init
\*********************************************************************************************/
void TimepropInit(void)
{
  SyncSettings();

  if (start_with_fallback)
  {
    for (uint32_t i = 0; i < MAX_TIMEPROPS; i++)
    {
      if (Timepropsstate[i].timebase == 0)
      {
        continue;
      }

      Timepropsstate[i].is_running = true;
      Timepropsstate[i].set_value = round((float)Timepropsstate[i].fallback_value * (float)100 / (float)7);
    }
  }
}

/*********************************************************************************************\
 * Periodic
\*********************************************************************************************/
void TimepropEverySecond(void)
{

  for (uint32_t i = 0; i < MAX_TIMEPROPS; i++)
  {
    if (Timepropsstate[i].timebase == 0)
    {
      continue;
    }
    if (Timepropsstate[i].is_running == false)
    {
      continue;
    }

    if (Timepropsstate[i].set_value == 0)
    {
      Timepropsstate[i].is_open = false;
    }
    else
    {
      if (Timepropsstate[i].seconds_running == 0)
      {
        Timepropsstate[i].is_open = true;
        ExecuteCommandPower(i + 1, POWER_ON, SRC_IGNORE);
      }
      if (Timepropsstate[i].is_open == true && Timepropsstate[i].seconds_running >= OpenSeconds(i))
      {
        Timepropsstate[i].is_open = false;
        ExecuteCommandPower(i + 1, POWER_OFF, SRC_IGNORE);
      }
    }

    Timepropsstate[i].seconds_running++;

    Timepropsstate[i].last_received++;

    if (Timepropsstate[i].last_received >= fallback_after_seconds)
    {
      Timepropsstate[i].set_value = Timepropsstate[i].fallback_value;
      Timepropsstate[i].last_received = 0;
    }

    if (Timepropsstate[i].seconds_running >= Timepropsstate[i].timebase)
    {
      Timepropsstate[i].seconds_running = 0;
    }
  }
}

/*********************************************************************************************\
 * Commands
\*********************************************************************************************/
void CmndTimepropSet(void)
{

  if (XdrvMailbox.index < 1 || XdrvMailbox.index > MAX_TIMEPROPS)
  {
    return;
  }

  uint32_t incoming_value;
  if (XdrvMailbox.data_len > 0)
  {
    char sub_string[XdrvMailbox.data_len];

    incoming_value = atoi(subStr(sub_string, XdrvMailbox.data, ",", 1));

    if (incoming_value < 0 || incoming_value > 100)
    {
      return;
    }

    Timepropsstate[XdrvMailbox.index - 1].set_value = incoming_value;
    Timepropsstate[XdrvMailbox.index - 1].is_running = true;
    Timepropsstate[XdrvMailbox.index - 1].last_received = 0;
  }

  ResponseCmndNumber(Timepropsstate[XdrvMailbox.index - 1].set_value);
}

void CmndTimepropTimeBase(void)
{
  if (XdrvMailbox.index < 1 || XdrvMailbox.index > MAX_TIMEPROPS)
  {
    return;
  }

  uint8_t incoming_value;
  if (XdrvMailbox.data_len > 0)
  {
    char sub_string[XdrvMailbox.data_len];

    incoming_value = atoi(subStr(sub_string, XdrvMailbox.data, ",", 1));

    if (incoming_value < 0 || incoming_value > 31)
    {
      return;
    }

    Settings->timeprop[XdrvMailbox.index - 1].timebase = incoming_value;
    SettingsSave(0);

    SyncSettings();
  }

  ResponseCmndNumber(Settings->timeprop[XdrvMailbox.index - 1].timebase);
}

void CmndTimepropFallbackvalue(void)
{
  if (XdrvMailbox.index < 1 || XdrvMailbox.index > MAX_TIMEPROPS)
  {
    return;
  }

  uint32_t incoming_value;
  if (XdrvMailbox.data_len > 0)
  {
    char sub_string[XdrvMailbox.data_len];

    incoming_value = atoi(subStr(sub_string, XdrvMailbox.data, ",", 1));
    if (incoming_value < 0 || incoming_value > 100)
    {
      return;
    }

    uint8_t store_value = round((float)7 * (float)incoming_value / (float)100);
    Settings->timeprop[XdrvMailbox.index - 1].fallback_value = store_value;
    SettingsSave(0);

    SyncSettings();
  }

  ResponseCmndNumber(round((float)Settings->timeprop[XdrvMailbox.index - 1].fallback_value * (float)100 / (float)7));
}

void CmndTimepropStartWithFallback(void)
{
  if (XdrvMailbox.data_len == 1)
  {
    char sub_string[XdrvMailbox.data_len];

    uint8_t incoming_value = atoi(subStr(sub_string, XdrvMailbox.data, ",", 1));
    if (incoming_value < 0 || incoming_value > 1)
    {
      return;
    }

    Settings->timeprop_cfg.start_with_fallback = incoming_value;
    SettingsSave(0);

    SyncSettings();
  }

  ResponseCmndNumber(Settings->timeprop_cfg.start_with_fallback);
}

void CmndTimepropFallbackAfter(void)
{
  AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop CmndTimepropFallbackAfter called."));

  if (XdrvMailbox.data_len > 0)
  {
    char sub_string[XdrvMailbox.data_len];

    uint8_t incoming_value = atoi(subStr(sub_string, XdrvMailbox.data, ",", 1));
    if (incoming_value < 0 || incoming_value > 127)
    {
      return;
    }

    Settings->timeprop_cfg.fallback_time = incoming_value;
    SettingsSave(0);

    SyncSettings();
  }
  ResponseCmndNumber(Settings->timeprop_cfg.fallback_time);
}

/*********************************************************************************************\
 * Helper
\*********************************************************************************************/
void SyncSettings(void)
{
  for (uint32_t i = 0; i < MAX_TIMEPROPS; i++)
  {
    Timepropsstate[i].fallback_value = round((float)Settings->timeprop[i].fallback_value * (float)100 / (float)7);
    Timepropsstate[i].timebase = Settings->timeprop[i].timebase * 60;

    AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop SyncSettings timeprop %d fallback_vlaue %d."), i, Timepropsstate[i].fallback_value);
    AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop SyncSettings timeprop %d timebase %d."), i, Timepropsstate[i].timebase);
  }
  fallback_after_seconds = Settings->timeprop_cfg.fallback_time * 60 * 60;
  start_with_fallback = Settings->timeprop_cfg.start_with_fallback;

  AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop SyncSettings fallback_after_seconds %d."), fallback_after_seconds);
  AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop SyncSettings start_with_fallback %d."), start_with_fallback);
}

uint8_t OpenSeconds(uint8_t index)
{
  float f_set_value = (float)Timepropsstate[index].set_value / 100.0f * (float)Timepropsstate[index].timebase;
  uint8_t set_value = round(f_set_value);

  return set_value;
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/
bool Xdrv48(byte function)
{
  bool result = false;

  switch (function)
  {
  case FUNC_INIT:
    TimepropInit();
    break;
  case FUNC_EVERY_SECOND:
    TimepropEverySecond();
    break;
  case FUNC_COMMAND:
    result = DecodeCommand(kTimepropCommands, TimepropCommand);
    break;
  }
  return result;
}

#endif // FIRMWARE_MINIMAL
#endif // USE_TIMEPROP
