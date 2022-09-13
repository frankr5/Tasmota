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

uint8_t seconds_modulo = 0;          // increased every second. If dividable by 60 we have a minute
uint32_t fallback_after_minutes = 0; // To keep settings low we set fallback time in hours. We need minutes

struct TIMEPROPSSTATE
{
  bool is_running = false;     // set to true with first set value
  uint8_t minutes_running = 0; // increased every minute. Reset when time base reached
  uint8_t set_value = 0;       // value that has been set via Set command
  uint32_t last_received = 0;  // Minutes increasing. Set to zero if a command is recieved
} Timepropsstate[MAX_TIMEPROPS];

const char kTimepropCommands[] PROGMEM = D_PRFX_TIMEPROP "|" D_CMND_TIMEPROP_SET "|" D_CMND_TIMEPROP_TIMEBASE "|" D_CMND_TIMEPROP_FALLBACKVALUE "|" D_CMND_TIMEPROP_STARTWITHFALLBACK "|" D_CMND_TIMEPROP_FALLBACKAFTER "|";

void (*const TimepropCommand[])(void) PROGMEM = {
    &CmndTimepropSet, &CmndTimepropTimeBase, &CmndTimepropFallbackvalue, &CmndTimepropStartWithFallback, &CmndTimepropFallbackAfter};

// #define D_PRFX_TIMEPROP "Timeprop"
// #define D_CMND_TIMEPROP_TIMEBASE "TimepropTimeBase1 15"
// #define D_CMND_TIMEPROP_FALLBACKVALUE "TimepropFallbackValue1 0.4"
// #define D_CMND_TIMEPROP_SET "TimepropSet2 0.4"

void TimepropInit(void)
{
  fallback_after_minutes = Settings->timeprop_cfg.fallback_time * 60;

  if (Settings->timeprop_cfg.start_with_fallback)
  {
    AddLog(LOG_LEVEL_INFO, PSTR("TPR: Init timeprop_start_with_fallback is set"));

    for (uint32_t i = 0; i < MAX_TIMEPROPS; i++)
    {
      if (Settings->timeprop[i].timebase == 0)
      {
        continue;
      }

      Timepropsstate[i].is_running = true;
      Timepropsstate[i].set_value = round((float)Settings->timeprop[i].fallback_value * (float)100 / (float)7);
    }
  }
  else
  {
    AddLog(LOG_LEVEL_INFO, PSTR("TPR: Init timeprop_start_with_fallback is not set"));
  }

  AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop Init"));
}

void TimepropEveryMinute(void)
{
  for (uint32_t i = 0; i < MAX_TIMEPROPS; i++)
  {
    if (Settings->timeprop[i].timebase == 0)
    {
      continue;
    }
    if (Timepropsstate[i].is_running == false)
    {
      continue;
    }

    if (Timepropsstate[i].set_value == 0)
    {
      // AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop TimepropEveryMinute set_value is zero for Number %d"), i);
    }
    else
    {
      if (Timepropsstate[i].minutes_running == 0)
      {
        AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop TimepropEveryMinute start cycle %d with openening for %d minutes"), i, OpenMinutes(i));
      }
      if (Timepropsstate[i].minutes_running >= OpenMinutes(i))
      {
        AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop TimepropEveryMinute mid cycle %d with closing"), i);
      }
    }

    Timepropsstate[i].minutes_running++;

    Timepropsstate[i].last_received++;

    AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop TimepropEveryMinute last_received %d %d %d"), i, Timepropsstate[i].last_received, fallback_after_minutes);

    if (Timepropsstate[i].minutes_running >= Settings->timeprop[i].timebase)
    {
      Timepropsstate[i].minutes_running = 0;
    }
  }
}

void TimepropEverySecond(void)
{
  seconds_modulo++;

  if ((seconds_modulo % 60) == 0)
  {
    TimepropEveryMinute();
    seconds_modulo = 0;
  }
}

void CmndTimepropSet(void)
{

  AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop CmndTimepropSet XdrvMailbox.index: %d"), XdrvMailbox.index);

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

    fallback_after_minutes = incoming_value * 60;
  }
  ResponseCmndNumber(Settings->timeprop_cfg.fallback_time);
}

/*********************************************************************************************\
 * Helper
\*********************************************************************************************/
uint8_t OpenMinutes(uint8_t index)
{
  float f_set_value = (float)Timepropsstate[index].set_value / 100.0f * (float)Settings->timeprop[index].timebase;
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

/*********************************************************************************************\
 * Code to drive one or more relays in a time proportioned manner give a
 * required power value.
 *
 * Given required power values in the range 0.0 to 1.0 the relays will be
 * driven on/off in such that the average power suppled will represent
 * the required power.
 * The cycle time is configurable.  If, for example, the
 * period is set to 10 minutes and the power input is 0.2 then the output will
 * be on for two minutes in every ten minutes.
 *
 * A value for actuator dead time may be provided. If you have a device that
 * takes a significant time to open/close then set this to the average of the
 * open and close times.  The algorithim will then adjust the output timing
 * accordingly to ensure that the output is not switched more rapidly than
 * the actuator can cope with.
 *
 * A facility to invert the output is provided which can be useful when used in
 * refrigeration processes and similar.
 *
 * In the case where only one relay is being driven the power value is set by
 * writing the value to the mqtt topic cmnd/timeprop_setpower_0.  If more than
 * one relay is being driven (as might be the case for a heat/cool application
 * where one relay drives the heater and the other the cooler) then the power
 * for the second relay is written to topic cmnd/timeprop_setpower_1 and so on.
 *
 * To cope with the problem of temporary wifi failure etc a
 * TIMEPROP_MAX_UPDATE_INTERVALS value is available. This can be set to the max
 * expected time between power updates and if this time is exceeded then the
 * power will fallback to a given safe value until a new value is provided. Set
 * the interval to 0 to disable this feature.
 *
 * Usage:
 * Place this file in the sonoff folder.
 * Clone the library https://github.com/colinl/process-control.git from Github
 * into a subfolder of lib.
 * In user_config.h or user_config_override.h for a single relay, include
 * code as follows:

 #define USE_TIMEPROP    //  include the timeprop feature (+1.2k)
   // for single output
   #define TIMEPROP_NUM_OUTPUTS          1       // how many outputs to control (with separate alogorithm for each)
   #define TIMEPROP_CYCLETIMES           60      // cycle time seconds
   #define TIMEPROP_DEADTIMES            0       // actuator action time seconds
   #define TIMEPROP_OPINVERTS            false   // whether to invert the output
   #define TIMEPROP_FALLBACK_POWERS      0       // falls back to this if too long betwen power updates
   #define TIMEPROP_MAX_UPDATE_INTERVALS 120     // max no secs that are allowed between power updates (0 to disable)
   #define TIMEPROP_RELAYS               1       // which relay to control 1:8

 * or for two relays:
 #define USE_TIMEPROP    //  include the timeprop feature (+1.2k)
   // for single output
   #define TIMEPROP_NUM_OUTPUTS          2               // how many outputs to control (with separate alogorithm for each)
   #define TIMEPROP_CYCLETIMES           60,     10      // cycle time seconds
   #define TIMEPROP_DEADTIMES            0,      0       // actuator action time seconds
   #define TIMEPROP_OPINVERTS            false,  false   // whether to invert the output
   #define TIMEPROP_FALLBACK_POWERS      0,      0       // falls back to this if too long betwen power updates
   #define TIMEPROP_MAX_UPDATE_INTERVALS 120,    120     // max no secs that are allowed between power updates (0 to disable)
   #define TIMEPROP_RELAYS               1,      2       // which relay to control 1:8

 * Publish values between 0 and 1 to the topic(s) described above
 *
**/

// #define D_CMND_TIMEPROP "timeprop_"
// #define D_CMND_TIMEPROP_SETPOWER "setpower_"    // add index no on end (0:8) and data is power 0:1

// #include "Timeprop.h"

// enum TimepropCommands { CMND_TIMEPROP_SETPOWER };
// const char kTimepropCommands[] PROGMEM = D_CMND_TIMEPROP_SETPOWER;

// #ifndef TIMEPROP_NUM_OUTPUTS
// #define TIMEPROP_NUM_OUTPUTS          1       // how many outputs to control (with separate alogorithm for each)
// #endif
// #ifndef TIMEPROP_CYCLETIMES
// #define TIMEPROP_CYCLETIMES           60      // cycle time seconds
// #endif
// #ifndef TIMEPROP_DEADTIMES
// #define TIMEPROP_DEADTIMES            0       // actuator action time seconds
// #endif
// #ifndef TIMEPROP_OPINVERTS
// #define TIMEPROP_OPINVERTS            false   // whether to invert the output
// #endif
// #ifndef TIMEPROP_FALLBACK_POWERS
// #define TIMEPROP_FALLBACK_POWERS      0       // falls back to this if too long betwen power updates
// #endif
// #ifndef TIMEPROP_MAX_UPDATE_INTERVALS
// #define TIMEPROP_MAX_UPDATE_INTERVALS 120     // max no secs that are allowed between power updates (0 to disable)
// #endif
// #ifndef TIMEPROP_RELAYS
// #define TIMEPROP_RELAYS               1       // which relay to control 1:8
// #endif

// static Timeprop timeprops[TIMEPROP_NUM_OUTPUTS];
// static int relayNos[TIMEPROP_NUM_OUTPUTS] = {TIMEPROP_RELAYS};
// static long currentRelayStates = 0;  // current actual relay states. Bit 0 first relay

// struct {
//   Timeprop timeprops[TIMEPROP_NUM_OUTPUTS];
//   int relay_nos[TIMEPROP_NUM_OUTPUTS] = {TIMEPROP_RELAYS};
//   long current_relay_states = 0;  // current actual relay states. Bit 0 first relay
//   long current_time_secs = 0;  // a counter that counts seconds since initialisation
// } Tprop;

// /* call this from elsewhere if required to set the power value for one of the timeprop instances */
// /* index specifies which one, 0 up */
// void TimepropSetPower(int index, float power) {
//   if (index >= 0  &&  index < TIMEPROP_NUM_OUTPUTS) {
//     Tprop.timeprops[index].setPower( power, Tprop.current_time_secs);
//   }
// }

// void TimepropInit(void) {
//   // AddLog(LOG_LEVEL_INFO, PSTR("TPR: Timeprop Init"));
//   int cycleTimes[TIMEPROP_NUM_OUTPUTS] = {TIMEPROP_CYCLETIMES};
//   int deadTimes[TIMEPROP_NUM_OUTPUTS] = {TIMEPROP_DEADTIMES};
//   int opInverts[TIMEPROP_NUM_OUTPUTS] = {TIMEPROP_OPINVERTS};
//   int fallbacks[TIMEPROP_NUM_OUTPUTS] = {TIMEPROP_FALLBACK_POWERS};
//   int maxIntervals[TIMEPROP_NUM_OUTPUTS] = {TIMEPROP_MAX_UPDATE_INTERVALS};

//   for (int i = 0; i < TIMEPROP_NUM_OUTPUTS; i++) {
//     Tprop.timeprops[i].initialise(cycleTimes[i], deadTimes[i], opInverts[i], fallbacks[i],
//       maxIntervals[i], Tprop.current_time_secs);
//   }
// }

// void TimepropEverySecond(void) {
//   Tprop.current_time_secs++;    // increment time
//   for (int i=0; i<TIMEPROP_NUM_OUTPUTS; i++) {
//     int newState = Tprop.timeprops[i].tick(Tprop.current_time_secs);
//     if (newState != bitRead(Tprop.current_relay_states, Tprop.relay_nos[i]-1)){
//       // remove the third parameter below if using tasmota prior to v6.0.0
//       ExecuteCommandPower(Tprop.relay_nos[i], newState,SRC_IGNORE);
//     }
//   }
// }

// // called by the system each time a relay state is changed
// void TimepropXdrvPower(void) {
//   // for a single relay the state is in the lsb of index, I have think that for
//   // multiple outputs then succesive bits will hold the state but have not been
//   // able to test that
//   Tprop.current_relay_states = XdrvMailbox.index;
// }

// /* struct XDRVMAILBOX { */
// /*   uint16_t      valid; */
// /*   uint16_t      index; */
// /*   uint16_t      data_len; */
// /*   int16_t       payload; */
// /*   char         *topic; */
// /*   char         *data; */
// /* } XdrvMailbox; */

// // To get here post with topic cmnd/timeprop_setpower_n where n is index into timeprops 0:7
// bool TimepropCommand()
// {
//   char command [CMDSZ];
//   bool serviced = true;
//   uint8_t ua_prefix_len = strlen(D_CMND_TIMEPROP); // to detect prefix of command
//   /*
//   AddLog(LOG_LEVEL_INFO, PSTR("Command called: "
//     "index: %d data_len: %d payload: %d topic: %s data: %s"),
//     XdrvMailbox.index,
//     XdrvMailbox.data_len,
//     XdrvMailbox.payload,
//     (XdrvMailbox.payload >= 0 ? XdrvMailbox.topic : ""),
//     (XdrvMailbox.data_len >= 0 ? XdrvMailbox.data : ""));
//   */
//   if (0 == strncasecmp_P(XdrvMailbox.topic, PSTR(D_CMND_TIMEPROP), ua_prefix_len)) {
//     // command starts with timeprop_
//     int command_code = GetCommandCode(command, sizeof(command), XdrvMailbox.topic + ua_prefix_len, kTimepropCommands);
//     if (CMND_TIMEPROP_SETPOWER == command_code) {
//       /*
//       AddLog(LOG_LEVEL_INFO, PSTR("Timeprop command timeprop_setpower: "
//         "index: %d data_len: %d payload: %d topic: %s data: %s"),
// 	      XdrvMailbox.index,
// 	      XdrvMailbox.data_len,
// 	      XdrvMailbox.payload,
// 	      (XdrvMailbox.payload >= 0 ? XdrvMailbox.topic : ""),
// 	      (XdrvMailbox.data_len >= 0 ? XdrvMailbox.data : ""));
//       */
//       if (XdrvMailbox.index >=0 && XdrvMailbox.index < TIMEPROP_NUM_OUTPUTS) {
//         timeprops[XdrvMailbox.index].setPower( atof(XdrvMailbox.data), Tprop.current_time_secs );
//       }
//       Response_P(PSTR("{\"" D_CMND_TIMEPROP D_CMND_TIMEPROP_SETPOWER "%d\":\"%s\"}"), XdrvMailbox.index, XdrvMailbox.data);
//     }
//     else {
//       serviced = false;
//     }
//   } else {
//     serviced = false;
//   }
//   return serviced;
// }

// /*********************************************************************************************\
//  * Interface
// \*********************************************************************************************/

// #define XDRV_48       48

// bool Xdrv48(byte function) {
//   bool result = false;

//   switch (function) {
//     case FUNC_INIT:
//       TimepropInit();
//       break;
//     case FUNC_EVERY_SECOND:
//       TimepropEverySecond();
//       break;
//     case FUNC_COMMAND:
//       result = TimepropCommand();
//       break;
//     case FUNC_SET_POWER:
//       TimepropXdrvPower();
//       break;
//   }
//   return result;
// }

#endif // FIRMWARE_MINIMAL
#endif // USE_TIMEPROP
