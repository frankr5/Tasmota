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
 *                           8 values to keep configuration memory low. This is
 *                           giving us 8 possible fallback values:
 *                           0, 14, 29, 43, 57, 71, 86, 100
 *                           every other value is rounded up or down to those.
 *
 * TimepropStartWithFallback: if set all timeprops will start with their fallback
 *                            values. Global setting. Set 0 or 1.
 *                            If set to zero, Timeprop will do nothing unless the
 *                            first Set<x> command is received. (Per timeprop of course.)
 *
 * TimepropFallbackAfter: If a timeprp has not received an update within set
 *                        time a fallback value will be set. This is setting the
 *                        time. Set hours between 0 and 127. if set to zero
 *                        fallback is disabled.
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
 * if TimepropTimeBase<x> is set to zero no POWER commands will be sent to the
 * given relay and the relay can be used otherwise even with timeprop enabled.
 * Make sure no Pulse is configured for one of the relays if TimepropTimeBase<x>
 * is set above zero and the timeprop is used.
 *
 * The code has been written with floor heating in mind. normal cycle times
 * for this use case are between 15 and 30 minutes. In case longer times are
 * needed a global multiplier could be added but that is not implemented today.
 *
 * Fallback time and start with fallback are global options. they are applied
 * for all timeprops.
 *
 * A started circle will end with the value Set at circle start. A new Set value
 * will be picked up with the next circle. Basevalue changes are picked up
 * immediately because the base time will normally be set once and not during
 * daily operations.
 *
 **/

uint32_t fallback_after_seconds = 0; // To keep settings low we set fallback time in hours. We need seconds
bool start_with_fallback = false;    // shall we start with fallback or not

struct TIMEPROPSSTATE
{
  bool is_running = false;            // set to true with first set value
  bool is_open = false;               // set to true with first set value
  uint8_t incoming_percent_value = 0; // value that will be copied to set_value at next start
  uint8_t open_seconds_active = 0;    // value that will be copied to set_value at next start
  uint8_t fallback_value = 0;         // Fallback Value as set via configuration
  uint32_t seconds_running = 0;       // increased every second. Reset when time base reached
  uint32_t last_received = 0;         // Minutes increasing. Set to zero if a command is recieved
  uint32_t timebase = 0;              // Timebase in Seconds. In Settings is minute based. We run on seconds.

} Timepropsstate[MAX_TIMEPROPS];

const char kTimepropCommands[] PROGMEM = D_PRFX_TIMEPROP "|" D_CMND_TIMEPROP_SET "|" D_CMND_TIMEPROP_TIMEBASE "|" D_CMND_TIMEPROP_FALLBACKVALUE "|" D_CMND_TIMEPROP_STARTWITHFALLBACK "|" D_CMND_TIMEPROP_FALLBACKAFTER "|";

void (*const TimepropCommand[])(void) PROGMEM = {
    &CmndTimepropSet, &CmndTimepropTimeBase, &CmndTimepropFallbackvalue,
    &CmndTimepropStartWithFallback, &CmndTimepropFallbackAfter};

/*********************************************************************************************\
 * Init
\*********************************************************************************************/
void TimepropInit(void)
{
  // AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop Init"));

  SyncSettings();

  for (uint32_t i = 0; i < MAX_TIMEPROPS; i++)
  {
    if (Timepropsstate[i].timebase > 0)
    {
      ExecuteCommandPower(i + 1, POWER_OFF, SRC_IGNORE);
    }
  }

  if (start_with_fallback)
  {
    for (uint32_t i = 0; i < MAX_TIMEPROPS; i++)
    {
      if (Timepropsstate[i].timebase == 0)
      {
        continue;
      }
      if (Timepropsstate[i].fallback_value == 0)
      {
        continue;
      }

      Timepropsstate[i].is_running = true;
      Timepropsstate[i].incoming_percent_value = Timepropsstate[i].fallback_value;
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

    if (Timepropsstate[i].seconds_running == 0)
    {
      // open_seconds_active is filled at the beginning of cycle and is used
      // throughout the complete cycle regardless of new incling value
      Timepropsstate[i].open_seconds_active = OpenSecondsForPercentValue(i, Timepropsstate[i].incoming_percent_value);
      // prevent quick on/off event
      if (Timepropsstate[i].open_seconds_active > 0)
      {
        AddLog(LOG_LEVEL_INFO, PSTR("TPR: TimepropEverySecond Begining of cycle open"));
        ExecuteCommandPower(i + 1, POWER_ON, SRC_IGNORE);
        Timepropsstate[i].is_open = true;
      }
    }
    if (Timepropsstate[i].seconds_running >= Timepropsstate[i].open_seconds_active && Timepropsstate[i].is_open == true)
    {
      AddLog(LOG_LEVEL_INFO, PSTR("TPR: TimepropEverySecond mid of cycle closing"));
      ExecuteCommandPower(i + 1, POWER_OFF, SRC_IGNORE);
      Timepropsstate[i].is_open = false;
    }

    Timepropsstate[i].seconds_running++;

    Timepropsstate[i].last_received++;

    // FallbackAfter time can be zero. Which de facto should disable fallback at all
    if (fallback_after_seconds > 0 && Timepropsstate[i].last_received >= fallback_after_seconds)
    {
      AddLog(LOG_LEVEL_INFO, PSTR("TPR: Fallback on Number %d to %d percent"), i+1, Timepropsstate[i].fallback_value);

      Timepropsstate[i].incoming_percent_value = Timepropsstate[i].fallback_value;
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

    // AddLog(LOG_LEVEL_INFO, PSTR("TPR: Set incoming Value: %d"), incoming_value);

    Timepropsstate[XdrvMailbox.index - 1].incoming_percent_value = incoming_value;
    Timepropsstate[XdrvMailbox.index - 1].is_running = true;
    Timepropsstate[XdrvMailbox.index - 1].last_received = 0;
  }

  ResponseCmndNumber(Timepropsstate[XdrvMailbox.index - 1].incoming_percent_value);
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
    Timepropsstate[i].fallback_value = FallbackValueSettingCalculated(i);
    Timepropsstate[i].timebase = Settings->timeprop[i].timebase * 60;
  }
  fallback_after_seconds = Settings->timeprop_cfg.fallback_time * 60 * 60;
  start_with_fallback = Settings->timeprop_cfg.start_with_fallback;
}

uint8_t OpenSecondsForPercentValue(uint8_t index, uint8_t percent_value)
{
  float f_set_value = (float)percent_value / 100.0f * (float)Timepropsstate[index].timebase;
  uint8_t set_value = round(f_set_value);

  return set_value;
}

uint8_t FallbackValueSettingCalculated(uint8_t index)
{
  float f_fallback = (float)Settings->timeprop[index].fallback_value * 100.0f / 7.0f;
  uint8_t fallback = round(f_fallback);

  return fallback;
}

#define WEB_HANDLE_TIMEPROP "s48"

/*********************************************************************************************\
 * WebUI
\*********************************************************************************************/
const char HTTP_BTN_MENU_TIMEPROP[] PROGMEM =
    "<p><form action='" WEB_HANDLE_TIMEPROP "' method='get'><button>" D_CONFIGURE_TIMEPROP "</button></form></p>";

const char HTTP_FORM_TIMEPROPSTRT[] PROGMEM =
    "<fieldset><legend><b>&nbsp;" D_TIMEPROP_PARAMETERS "&nbsp;</b></legend>"
    "<form method='get' action='" WEB_HANDLE_TIMEPROP "'>";

const char HTTP_FORM_TIMEPROP_RELAY[] PROGMEM =
    "<p></p><fieldset><legend><b>&nbsp;" D_TIMEPROP_RELAY " %d &nbsp;</b></legend><p>"
    "<p><b>" D_TIMEPROP_TIMEBASE "</b> (" D_TIMEPROP_MINUTES ")<br><input id='tptb%d' placeholder='" STR(0) "' value='%d'></p>"
                                                                                                            "<p><b>" D_TIMEPROP_FALLBACK "</b> (" D_TIMEPROP_PERCENT ")<br><input id='tpfa%d' placeholder='" STR(0) "' value='%d'></p>"
                                                                                                                                                                                                                    "</p></fieldset>";

const char HTTP_FORM_TIMEPROPGBL[] PROGMEM =
    "<p><label><input id='tpsu' type='checkbox'%s><b>" D_TIMEPROP_STARTUPFALLBACK "</b></label><br>"
    "<p><b>" D_TIMEPROP_FALLBACKAFTER "</b> (" STR(0) ")<br><input id='tpfba' placeholder='" STR(0) "' value='%d'></p>";

void HandleTimepropConfiguration(void)
{

  if (!HttpCheckPriviledgedAccess())
  {
    return;
  }

  AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_HTTP D_CONFIGURE_TIMEPROP));

  if (Webserver->hasArg(F("save")))
  {
    TimePropSaveSettings();
    WebRestart(1);
    return;
  }

  WSContentStart_P(PSTR(D_CONFIGURE_TIMEPROP));
  WSContentSendStyle();

  WSContentSend_P(HTTP_FORM_TIMEPROPSTRT, Settings->timeprop_cfg.fallback_time, Settings->timeprop_cfg.fallback_time);

  for (uint32_t i = 0; i < MAX_TIMEPROPS; i++)
  {
    WSContentSend_P(HTTP_FORM_TIMEPROP_RELAY, i + 1, i, Settings->timeprop[i].timebase, i, FallbackValueSettingCalculated(i));
  }
  WSContentSend_P(HTTP_FORM_TIMEPROPGBL, Settings->timeprop_cfg.start_with_fallback ? PSTR(" checked") : "", Settings->timeprop_cfg.fallback_time);

  WSContentSend_P(HTTP_FORM_END);
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();
}

void TimePropSaveSettings(void)
{
  String cmnd = F(D_CMND_BACKLOG "0 ");
  cmnd += AddWebCommand(PSTR(D_PRFX_TIMEPROP D_CMND_TIMEPROP_FALLBACKAFTER), PSTR("tpfba"), PSTR("1"));

  if (Webserver->hasArg(F("tpsu")))
  {
    cmnd += AddWebCommand(PSTR(D_PRFX_TIMEPROP D_CMND_TIMEPROP_STARTWITHFALLBACK), PSTR("1"), PSTR("1"));
  }
  else
  {
    cmnd += AddWebCommand(PSTR(D_PRFX_TIMEPROP D_CMND_TIMEPROP_STARTWITHFALLBACK), PSTR("0"), PSTR("0"));
  }

  for (uint32_t i = 0; i < MAX_TIMEPROPS; i++)
  {
    char timebase_command[18];      // D_PRFX_TIMEPROP D_CMND_TIMEPROP_TIMEBASE index;
    char fallbackvalue_command[23]; // D_PRFX_TIMEPROP D_CMND_TIMEPROP_FALLBACKVALUE index;
    snprintf_P(timebase_command, sizeof(timebase_command), PSTR(D_PRFX_TIMEPROP D_CMND_TIMEPROP_TIMEBASE "%d"), i + 1);
    snprintf_P(fallbackvalue_command, sizeof(fallbackvalue_command), PSTR(D_PRFX_TIMEPROP D_CMND_TIMEPROP_FALLBACKVALUE "%d"), i + 1);

    char timebase_propcounter[6];      // tptb index;
    char fallbackvalue_propcounter[6]; // tpfa D_PRFX_TIMEPROP D_CMND_TIMEPROP_TIMEBASE index;
    snprintf_P(timebase_propcounter, sizeof(timebase_command), PSTR("tptb%d"), i);
    snprintf_P(fallbackvalue_propcounter, sizeof(timebase_command), PSTR("tpfa%d"), i);

    cmnd += AddWebCommand(timebase_command, timebase_propcounter, PSTR("1"));
    cmnd += AddWebCommand(fallbackvalue_command, fallbackvalue_propcounter, PSTR("1"));
  }

  ExecuteWebCommand((char *)cmnd.c_str());
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
  case FUNC_WEB_ADD_BUTTON:
    WSContentSend_P(HTTP_BTN_MENU_TIMEPROP);
    break;
  case FUNC_WEB_ADD_HANDLER:
    WebServer_on(PSTR("/" WEB_HANDLE_TIMEPROP), HandleTimepropConfiguration);
    break;
  }
  return result;
}

#endif // FIRMWARE_MINIMAL
#endif // USE_TIMEPROP
