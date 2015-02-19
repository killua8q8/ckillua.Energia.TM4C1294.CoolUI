#include <SPI.h>
#include <Scheduler.h>
#include <RTChardware.h>
#include <AIR430BoostFCC.h>
#include <Screen_K35.h>
#include <LCD_graphics.h>
#include <LCD_GUI.h>
#include "util.h"
#include "Keyboard.h"

#define DEBUG true
#define ADDRESS_LOCAL 0x00  /* Every slave controller increments by 0x10, max total 4 controllers, so node up to 0x30 */
#define TYPE MASTER
struct sPacket rxPacket;
struct sPacket txPacket;

// UI related elements
Screen_K35 myScreen;
button homeButton;
imageButton optionButton, nextButton, returnButton, updateButton, removeButton, onButton, offButton;
item home_i, option_i;

// System main vars
String deviceName;
boolean firstInitialized = false;
boolean timeInterrupt = false;
roomStruct room_l[MAXROOMSIZE];
childStruct child_l[MAXCHILDSIZE];
uint8_t roomSize = 0;
long _timer = 0;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  initAIR();
  initLCD();
  firstInitialized = firstInit();
  Scheduler.startLoop(idle);
  home();
}

void loop()
{
  // put your main code here, to run repeatedly:
  if (optionButton.check(true)) {
    option();
    home();
  }
  for (int i = 0; i < roomSize; i++) {
    if (room_l[i].button.check(true)) {
      roomConfig(&room_l[i]);
      home();
    } 
  }
}

void home() {
  initUI();
}

/********************************************************************************/
/*                                Child Controls                                */
/********************************************************************************/
boolean childConfig(roomStruct* room) {
  resetChildConfigUI(room);
  while(1) {
    if (homeButton.isPressed()) {
      return HOME;
    }
    if (returnButton.check(true)) {
       return RETURN; 
    }
    for (int i = 0; i < room->childSize; i++) {
      if (room->childList[i].button.check(true)) {
        if (childControl(room, room->childList[i], i)) return HOME;
        resetChildConfigUI(room);
      }
    }
  }
}

boolean childControl(roomStruct* room, childStruct child, uint8_t index) {
  uint8_t count = 0;
  uiBackground();
  onButton.dDefine(&myScreen, g_onImage, xy[count][0], xy[count++][1], setItem(100, "ON"));
  onButton.enable();
  onButton.draw();
  offButton.dDefine(&myScreen, g_offImage, xy[count][0], xy[count++][1], setItem(100, "OFF"));
  offButton.enable();
  offButton.draw();
  removeButton.dDefine(&myScreen, g_removeImage, xy[count][0], xy[count++][1], setItem(100, "REMOVE"));
  removeButton.enable();
  removeButton.draw();
  returnButton.dDefine(&myScreen, g_returnImage, xy[count][0], xy[count++][1], setItem(100, "RETURN"));
  returnButton.enable();
  returnButton.draw();
  
  while(1) {
    if (homeButton.isPressed()) {
      return HOME;
    }
    if (returnButton.check(true)) {
      return RETURN; 
    }
    if (onButton.check(true)) {
      childCommand(child, "ON");
    }
    if (offButton.check(true)) {
      childCommand(child, "OFF");
    }
    if (removeButton.check(true)) {
      childCommand(child, "DEL");
      removeChild(room, index);
      return RETURN;
    }
  }
}

void childCommand(childStruct child, char* cmd) {
  txPacket.node = child.node;
  txPacket.parent = ADDRESS_LOCAL;
  strcpy((char*)txPacket.msg, cmd);
  Radio.transmit(child.node, (unsigned char*)&txPacket, sizeof(txPacket));
  while(Radio.busy()){}
  if (Radio.receiverOn((unsigned char*)&rxPacket, sizeof(rxPacket), 10000) <= 0) {
    // Failed connection
    debug("No ACK from node " + String(rxPacket.node));
    return;
  }
  // Below happens when successful connected
  if (!strcmp((char*)rxPacket.msg, "ACK")) {
    debug("ACK from node " + String(rxPacket.node));
  }
}

void removeChild(roomStruct* room, uint8_t index) {
  if (index == room->childSize-1) {
    ;
  } else {
    for (int i = 0; i < room->childSize-1; i++) {
      if (i == index++) {
        room->childList[i].name = room->childList[index].name;
        room->childList[i].type = room->childList[index].type;
        room->childList[i].button.define(&myScreen, room->childList[index].button.getIcon(), xy[i][0], xy[i][1], room->childList[i].name);
        room->childList[i].button.enable();
        room->childList[i].node = room->childList[index].node;
      }
    }
//    memmove(&room->childList[index], &room->childList[index+1], sizeof(childStruct) * (room->childSize-1));
  }
  room->childSize--;
}

void resetChildConfigUI(roomStruct* room) {
  uint8_t count = 0;  
  uiBackground(); 
  for (int i = 0; i < room->childSize; i++) {
    room->childList[i].button.draw();
    count++;
  }
  returnButton.dDefine(&myScreen, g_returnImage, xy[count][0], xy[count][1], setItem(100, "RETURN"));
  returnButton.enable();
  returnButton.draw();
}
/********************************************************************************/
/*                                 Room Controls                                */
/********************************************************************************/
void roomConfig(roomStruct *room) {
  resetRoomConfigUI(room);
  long current = millis();
  while(1) {
    if (millis() - current > 60000) {
      updateRoomInfo(room);
      current = millis();
    }
    if (returnButton.check(true) || homeButton.isPressed()) {
      return;
    }
    if (updateButton.check(true)) {
      updateRoomInfo(room);
      current = millis();
    }
    if (optionButton.check(true)) {
      if (childConfig(room)) {
        return;
      } else {
        resetRoomConfigUI(room);
      }
    }
  }
}

void resetRoomConfigUI(roomStruct* room) {
  uint8_t count = 3;  
  uiBackground();
  updateRoomInfo(room);
  updateButton.dDefine(&myScreen, g_updateImage, xy[count][0], xy[count][1], setItem(100, "UPDATE"));
  updateButton.enable();
  updateButton.draw();
  optionButton.dDefine(&myScreen, g_optionImage, xy[count+1][0], xy[count+1][1], setItem(100, "Option"));
  optionButton.enable();
  optionButton.draw();  
  returnButton.dDefine(&myScreen, g_returnImage, xy[count+2][0], xy[count+2][1], setItem(100, "RETURN"));
  returnButton.enable();
  returnButton.draw();  
}

void updateRoomInfo(roomStruct* room) {
  uint8_t x = 28, y = 60, count = 0;
  float tempF = 0.0;
  myScreen.drawImage(g_infoImage, x, y);
  myScreen.setFontSize(3);
  myScreen.gText(x + 45, y + 20, room->name, true);
  if (room->type == MASTER) {
    if (room->childSize > 0) {
      tempF += getChildrenTemp(room);
      count = 1;
    }
    tempF = (tempF + getLocalTemp()) / (float)++count;
    tempF = tempF * 9 / 5 + 32;
    room->roomTemp = (int16_t)ceil(tempF);
  } else {
  /***************************
  TODO: get temperature from slave
  ***************************/    
  }
  myScreen.gText(x + 177,y + 20, String((int16_t)ceil(tempF)) + (char)0xB0 + "F", true);
}

float getChildrenTemp(roomStruct* room) {
  uint16_t temp = 0, count = 0;
  strcpy((char*)txPacket.msg, "TEMP");
  for (int i = 0; i < room->childSize; i++) {
    uint8_t node = room->childList[i].node;
    txPacket.parent = ADDRESS_LOCAL;
    txPacket.node = node;
    Radio.transmit(node, (unsigned char*)&txPacket, sizeof(txPacket));
    while(Radio.busy()){}
    if (Radio.receiverOn((unsigned char*)&rxPacket, sizeof(rxPacket), 5000) <= 0) {
      // Failed connection
      continue;
    }
    // Below happens when successful connected
    if (!strcmp((char*)rxPacket.msg, "ACK")) {          
      temp += (rxPacket.upper << 8) | rxPacket.lower;
      count++;
      debug("ACK from node " + String(rxPacket.node));
    }
  }
  return 3.6 * ((float)temp / count) * 100.0 / 1024.0;
}

float getLocalTemp() {
  int val = 0;
  for (int i = 0; i < 10; i++) {
    val = analogRead(SENSOR);
  }
  val += analogRead(SENSOR);
  val += analogRead(SENSOR);
  val += analogRead(SENSOR);
  return 3.3 * ((float)val / 4.0) * 100.0 / 4096.0;
}
/********************************************************************************/
/*                                    Option                                    */
/********************************************************************************/
void option() {
  imageButton setTimeButton, pairSlaveButton, pairChildButton;
  pairSlaveButton.dDefine(&myScreen, g_pairSlaveImage, xy[0][0], xy[0][1], setItem(96, "PAIRSLAVE"));
  pairSlaveButton.enable();
  pairChildButton.dDefine(&myScreen, g_pairChildImage, xy[1][0], xy[1][1], setItem(97, "PAIRCHILD"));
  pairChildButton.enable();
  setTimeButton.dDefine(&myScreen, g_setTimeImage, xy[2][0], xy[2][1], setItem(98, "SETTIME"));
  setTimeButton.enable();
  returnButton.dDefine(&myScreen, g_returnImage, xy[3][0], xy[3][1], setItem(100, "RETURN"));
  returnButton.enable();
  restOption(setTimeButton, pairSlaveButton, pairChildButton);
  
  while (1) {
    if (homeButton.isPressed() || returnButton.check(true)) {
      return;
    }
    if (setTimeButton.check(true)) {
      debug("Set Time is pressed");
      dateSet();
      timeSet();
      restOption(setTimeButton, pairSlaveButton, pairChildButton);
    }
    if (pairSlaveButton.check(true)) {
      debug("Pair Slave is pressed");
      if (pairRoom()) {
        return;
      } else {
        restOption(setTimeButton, pairSlaveButton, pairChildButton);
      }
    }
    if (pairChildButton.check(true)) {
      debug("Pair Child is pressed");
      if (pairChild(&room_l[0])) {
        return;
      } else {
        restOption(setTimeButton, pairSlaveButton, pairChildButton);
      }
    }
  }
}

void restOption(imageButton setTimeButton, imageButton pairSlaveButton, imageButton pairChildButton) {
  uiBackground();
  setTimeButton.draw();
  pairSlaveButton.draw();
  pairChildButton.draw();
  returnButton.draw();
}

/********************************************************************************/
/*                                   New Child                                  */
/********************************************************************************/
boolean pairChild(roomStruct *room) {
  imageButton pairFan, pairVent, pairBlind;
  pairFan.dDefine(&myScreen, g_pairFanImage, xy[0][0], xy[0][1], setItem(89, "PAIRFAN"));
  pairFan.enable();  
  pairVent.dDefine(&myScreen, g_pairVentImage, xy[1][0], xy[1][1], setItem(88, "PAIRVENT"));
  pairVent.enable();  
  pairBlind.dDefine(&myScreen, g_pairBlindImage, xy[2][0], xy[2][1], setItem(87, "PAIRBLIND"));
  pairBlind.enable();  
  returnButton.dDefine(&myScreen, g_returnImage, xy[3][0], xy[3][1], setItem(100, "RETURN"));
  returnButton.enable();
  resetPairChild(pairFan, pairVent, pairBlind);
  
  while(1) {
    if (homeButton.isPressed()) {
      debug("Home is pressed");
      return HOME;
    }
    if (returnButton.check(true)) {
      debug("Return is pressed");
      return RETURN;
    }
    if (pairFan.check(true)) {
      if (newChild(room, NEWFAN)) return HOME;
    }
    if (pairVent.check(true)) {
      if (newChild(room, NEWVENT)) return HOME;
    }
    if (pairBlind.check(true)) {
      if (newChild(room, NEWBLIND)) return HOME;
    }
  }
}

void resetPairChild(imageButton pairFan, imageButton pairVent, imageButton pairBlind) {
  uiBackground();
  pairFan.draw();
  pairVent.draw();
  pairBlind.draw();
  returnButton.draw();
}

boolean newChild(roomStruct *room, uint8_t type) {
  String name = "";
  uint8_t key;
  boolean flag;
  
  uiBackground();
  updateNameField(name);
  nextButton.draw();
  if (type == NEWFAN) {
    newPairMessage("FAN");
  } else if (type == NEWVENT) {
    newPairMessage("VENT");
  } else if (type == NEWBLIND) {
    newPairMessage("BLIND");
  } else {
    ;
  }
  
  KB.draw();
  while (1) {
    if (nextButton.check(true)) {
      if (!isEmpty(name)) {
        if (room->childSize < MAXCHILDSIZE) {
          if (room->childSize > 0 && repeatChildName(room, name)) {
            addErrorMessage(childNameRepeat);
          } else {
            if (type == NEWFAN) {
              if (room->f >= 11) {
                childExceeded(FAN);
                return HOME;
              }              
              if (flag = addChild(room, name, FAN, g_fanImage)) debug("FAN added");
              else debug("Failed connect FAN");
            } else if (type == NEWVENT) {
              if (room->v >= 11) {
                childExceeded(VENT);
                return HOME;
              }
              if (flag = addChild(room, name, VENT, g_ventImage)) debug("VENT added");
              else debug("Failed connect VENT");
            } else if (type == NEWBLIND) {
              if (room->b >= 11) {
                childExceeded(BLIND);
                return HOME;
              }
              if (flag = addChild(room, name, BLIND, g_blindImage)) debug("BLIND added");
              else debug("Failed connect BLIND");
            } else {
              KB.setEnable(false);
              return HOME;
            }
            if (flag) {
              KB.setEnable(false);
              return HOME;
            }
          }          
        } else {
          maxNumberError("Children");
          KB.setEnable(false);
          delay(3000);
          return HOME;
        }        
      } else {
        emptyStringMessage();
      }
    }
    
    key = KB.getKey();
    if (key == 0xFF) {
      delay (1); 
    } else if (key == 0x0D) {
      KB.setEnable(false);
      return HOME;
    } else if (key == 0x08) {
      if (name.length() > 0) {
        name = name.substring(0, name.length()-1);
        updateNameField(name);
      } 
    } else {
      if (name.length() < MAXNAMELENGTH) {
        name += char(key);
        updateNameField(name);
      } 
    }
  }
}

boolean addChild(roomStruct *room, String name, child_t type, const uint8_t *icon) {
  uint8_t childSize = room->childSize;
  uint8_t position = childSize % 6;
  /******* Connecting ******/
  uint8_t newNode = getChildNode(room, type) + 1;
  txPacket.node = newNode;
  strcpy((char*)txPacket.msg, "PAIR");
  Radio.transmit((uint8_t)type, (unsigned char*)&txPacket, sizeof(txPacket));
  while(Radio.busy()){}
  if (Radio.receiverOn((unsigned char*)&rxPacket, sizeof(rxPacket), 30000) <= 0) {
    // Failed connection
    addErrorMessage(timeout);
    return false;
  }
  // Below happens when successful connected
  if (!strcmp((char*)rxPacket.msg, "ACK")) {
    debug(String(rxPacket.node) + " " + String((char*)rxPacket.msg));
    room->childList[childSize].name = name;
    room->childList[childSize].type = type;
    room->childList[childSize].button.define(&myScreen, icon, xy[position][0], xy[position][1], name);
    room->childList[childSize].button.enable();
    room->childList[childSize].node = newNode;  
    room->childSize++;
    increment(room, type);
    return true;
  }
  addErrorMessage(retry);
  return false;
}

uint8_t getChildNode(roomStruct *room, child_t type) {
  switch(type) {
  case VENT:
    return VENT + ADDRESS_LOCAL + room->v;
    break;
  case FAN:
    return FAN + ADDRESS_LOCAL + room->f;
    break;
  case BLIND:
    return BLIND + ADDRESS_LOCAL + room->b;
    break;
  }
}

void childExceeded (child_t type) {
  addErrorMessage(getChildTypeString(type) + outOfLimit);
  KB.setEnable(false);
  delay(2000);
}

void increment(roomStruct *room, child_t type) {
  switch(type) {
  case VENT:
    room->v++;
    break;
  case FAN:
    room->f++;
    break;
  case BLIND:
    room->b++;
    break;
  }
}

String getChildTypeString(child_t type) {
  switch (type) {
    case VENT:
      return "vent";
      break;
    case FAN:
      return "fan";
      break;
    case BLIND:
      return "blind";
      break;
    default:
      return "";
      break;
  } 
}
/********************************************************************************/
/*                                   New Rooms                                  */
/********************************************************************************/
boolean pairRoom() {
  
  String name = "";
  uint8_t key;
  nextButton.dDefine(&myScreen, g_nextImage, 223, 96, setItem(80, "NEXT"));
  nextButton.enable();
  
  uiBackground();
  updateNameField(name);
  nextButton.draw();  
  if (firstInitialized) {
    newPairMessage("ROOM");
  } else {
    myScreen.setFontSize(3);
    myScreen.gText(10, 44, "Name this room...", true);
    myScreen.gText(10, 70, "Name: ", true);
    myScreen.setFontSize(1);
    myScreen.gText(210, 82, "Max " + String(MAXNAMELENGTH) + " letters", true);
  }
  
  KB.draw();  
  while (1) {
    if (firstInitialized && homeButton.isPressed()) {
      return HOME; 
    }    
    if (nextButton.check(true)) {
      if (!isEmpty(name)) {
        if (roomSize > 0 && repeatRoomName(name)) {
          addErrorMessage(roomNameRepeat);
        } else {
          if (roomSize < MAXROOMSIZE) {
            newRoom(name);
          } else {
            maxNumberError("Rooms");
          }
          KB.setEnable(false);
          return HOME;
        }
      } else {
        emptyStringMessage();
      }
    }
    
    key = KB.getKey();
    if (key == 0xFF) {
      delay(1);
    } else if (key == 0x0D) {
      if (firstInitialized) {
        KB.setEnable(false);
        return HOME;
      }
    } else if (key == 0x08) {
      if (name.length() > 0) {
        name = name.substring(0, name.length()-1);
        updateNameField(name);
      }
    } else {
      if (name.length() < MAXNAMELENGTH) {
        name += char(key);
        updateNameField(name);
      }
    }
  }
}

void newRoom(String name) {
  uint8_t position = roomSize % 6;

  room_l[roomSize].name = name;
  room_l[roomSize].button.define(&myScreen, g_room, xy[position][0], xy[position][1], name);
  room_l[roomSize].button.enable();
  if (firstInitialized) {
    /*************************
    pair device
    room_l[roomSize].node = 0;
    *************************/
    room_l[roomSize].type = SLAVE;
  } else {
    room_l[roomSize].childSize = 0;
    room_l[roomSize].node = ADDRESS_LOCAL;
    room_l[roomSize].type = TYPE; 
  }
  roomSize++;
}

/********************************************************************************/
/*                                  UI Related                                  */
/********************************************************************************/
void uiBackground() {
  myScreen.clear(blackColour);
  myScreen.drawImage(g_bgImage, 0, 0);
  homeButton.dDefine(&myScreen, 10, 4, 32, 32, setItem(0, "HOME"), whiteColour, redColour);
  homeButton.enable();
}

void initUI() {
  debug("Init UI");
  uiBackground();  
  uint8_t count = 0;  
  if (roomSize > 0) {
      for (int i = 0; i < roomSize; i++) {
        room_l[i].button.draw();
        count++;
      }
  }  
  optionButton.dDefine(&myScreen, g_optionImage, xy[count][0], xy[count][1], setItem(99, "OPTION"));
  optionButton.enable();
  optionButton.draw();
}

/*****************************************************************************************/
/*                                  Time & Date Related                                  */
/*****************************************************************************************/
void updateTime() {
  if (!timeInterrupt) {
    RTCTime time;
    for (int i = 0; i < 5; i++) {
      if (timeInterrupt) continue;
      _updateTime(&time);
      String wday = String(wd[(int)time.wday][0]) + String(wd[(int)time.wday][1]) + String(wd[(int)time.wday][2]);
      String _t = String(time.hour) + ":" + time.minute + ":" + time.second;
      uint16_t color = whiteColour;
      if (time.wday == 0 || time.wday == 6) color = redColour;
      myScreen.gText(142, 8, wday, true, color);
      myScreen.gText(320 - _t.length() * 16, 8, _t, true, whiteColour);
      delay(1000);
    }
    for (int i = 0; i < 3; i++) {
      if (timeInterrupt) continue;
      _updateTime(&time);
      String date = String(time.month) + "/" + time.day + "/" + time.year;
      myScreen.gText(320 - date.length() * 16, 8, date, true, whiteColour);
      delay(1000);
    }
  } else {
    delay(1); 
  }
}

void _updateTime(RTCTime *time) {
  RTC.GetAll(time);
  myScreen.drawImage(g_timebarImage, 80, 0);
  myScreen.setFontSize(3);
}

void setTimeInterupt(boolean flag) {
  timeInterrupt = flag;
}

void dateSet() {
  setTimeInterupt(true);
  button timeSetOK;
  imageButton monthUp, dayUp, yearUp, monthDown, dayDown, yearDown;
  
  myScreen.clear(whiteColour);
  myScreen.setFontSize(3);
  
  /* init elements */
  timeSetOK.dDefine(&myScreen, 228, 195, 60, 40, setItem(0, "OK"), blueColour, yellowColour);
  monthUp.dDefine(&myScreen, g_upImage, 64, 87, setItem(0, "UP"));
  dayUp.dDefine(&myScreen, g_upImage, 128, 87, setItem(0, "UP"));
  yearUp.dDefine(&myScreen, g_upImage, 200, 87, setItem(0, "UP"));
  monthDown.dDefine(&myScreen, g_downImage, 64, 153, setItem(0, "DOWN"));
  dayDown.dDefine(&myScreen, g_downImage, 128, 153, setItem(0, "DOWN"));
  yearDown.dDefine(&myScreen, g_downImage, 200, 153, setItem(0, "DOWN"));
  
  timeSetOK.enable();
  monthUp.enable();
  dayUp.enable();
  yearUp.enable();
  monthDown.enable();
  dayDown.enable();
  yearDown.enable();
  /*****************/
  
  uint8_t mon = 1, day = 1;
  uint16_t year = 1970;
  char str[13] = "0 1/0 1/1970";
  if (firstInitialized) {
    RTCTime time;
    RTC.GetAll(&time);
    mon = time.month;
    day = time.day;
    year = time.year;
    str[0] = mon / 10 + '0';    str[2] = mon % 10 + '0';
    str[4] = day / 10 + '0';    str[6] = day % 10 + '0';
    str[8] = year / 1000 + '0';    str[9] = year % 1000 / 100 + '0';    str[10] = year % 100 / 10  + '0';    str[11] = year % 10  + '0';
  }
  
  /* draw element */  
  myScreen.gText(56, 51, "Today's date?", true, grayColour);
  myScreen.gText(55, 50, "Today's date?", true, blueColour);
  monthUp.draw();  dayUp.draw();  yearUp.draw();
  myScreen.gText(64, 124, str, true, blackColour);
  monthDown.draw();  dayDown.draw();  yearDown.draw();
  timeSetOK.draw(true);
  /*****************/
  
  while(1) {
    if (timeSetOK.isPressed()) {
      debug("Date set " + String(str));
      RTC.SetDate(day, mon, year%100, (uint8_t)(year / 100));
      setTimeInterupt(false);
      return;
    }
    myScreen.setPenSolid(true);
    if (monthUp.check(true)) {
      if (++mon > 12) mon = 1;
      str[0] = mon/10 + '0';
      str[2] = mon%10 + '0';
      drawTimeDateSetting(64, str);
    }
    if (dayUp.check(true)) {
      if (mon == 1 || mon == 3 || mon == 5 || mon == 7 || mon == 8 || mon == 10 || mon == 12) {
        if (++day > 31) day = 1; 
      } else if (mon == 2) {
        if (year%4 == 0) {
          if (++day > 29) day = 1;    
        } else {
          if (++day > 28) day = 1;   
        }
      } else {
        if (++day > 30) day = 1; 
      }
      str[4] = day/10 + '0';
      str[6] = day%10 + '0';
      drawTimeDateSetting(64, str);
    }
    if (yearUp.check(true)) {
      if (++year > 2100) year = 2100;
      str[8] = year / 1000 + '0';
      str[9] = year % 1000 / 100 + '0';
      str[10] = year % 100 / 10  + '0';
      str[11] = year % 10  + '0';
      drawTimeDateSetting(64, str);
    }
    if (monthDown.check(true)) {
      if (--mon < 1) mon = 12;
      str[0] = mon/10 + '0';
      str[2] = mon%10 + '0';
      drawTimeDateSetting(64, str);
    }
    if (dayDown.check(true)) {
      if (mon == 1 || mon == 3 || mon == 5 || mon == 7 || mon == 8 || mon == 10 || mon == 12) {
        if (--day < 1) day = 31; 
      } else if (mon == 2) {
        if (year%4 == 0) {
          if (--day < 1) day = 29;    
        } else {
          if (--day < 1) day = 28;   
        }
      } else {
        if (--day < 1) day = 30; 
      }
      str[4] = day/10 + '0';
      str[6] = day%10 + '0';
      drawTimeDateSetting(64, str);
    }
    if (yearDown.check(true)) {
      if (--year < 1970) year = 1970;
      str[8] = year / 1000 + '0';
      str[9] = year % 1000 / 100 + '0';
      str[10] = year % 100 / 10  + '0';
      str[11] = year % 10  + '0';
      drawTimeDateSetting(64, str);
    }
  }
}

void timeSet() {
  setTimeInterupt(true);
  button timeSetOK;
  imageButton dayUp, hourUp, minUp, secUp, dayDown, hourDown, minDown, secDown;
  item timeSetOK_i, up_i, down_i;
  
  myScreen.clear(whiteColour);
  myScreen.setFontSize(3);
  
  /* init elements */
  timeSetOK_i = setItem(0, "OK");
  up_i = setItem(0, "UP");
  down_i = setItem(0, "DOWN");
  
  timeSetOK.dDefine(&myScreen, 228, 195, 60, 40, timeSetOK_i, blueColour, yellowColour);
  dayUp.dDefine(&myScreen, g_upImage, 40, 87, up_i);
  hourUp.dDefine(&myScreen, g_upImage, 104, 87, up_i);
  minUp.dDefine(&myScreen, g_upImage, 168, 87, up_i);
  secUp.dDefine(&myScreen, g_upImage, 232, 87, up_i);
  dayDown.dDefine(&myScreen, g_downImage, 40, 153, down_i);
  hourDown.dDefine(&myScreen, g_downImage, 104, 153, down_i);
  minDown.dDefine(&myScreen, g_downImage, 168, 153, down_i);
  secDown.dDefine(&myScreen, g_downImage, 232, 153, down_i);
  
  timeSetOK.enable();
  dayUp.enable();
  hourUp.enable();
  minUp.enable();
  secUp.enable();
  dayDown.enable();
  hourDown.enable();
  minDown.enable();
  secDown.enable();
  /*****************/
  
  int8_t hour = 0, mins = 0, secs = 0, day = 1;
  char str[16] = "MON 0 0:0 0:0 0";
  if (firstInitialized) {
    RTCTime time;
    RTC.GetAll(&time);
    hour = time.hour;
    mins = time.minute;
    secs = time.second;
    day = (uint8_t)time.wday;
    str[0] = wd[day][0];    str[1] = wd[day][1];    str[2] = wd[day][2];
    str[4] = hour / 10 + '0';    str[6] = hour % 10 + '0';
    str[8] = mins / 10 + '0';    str[10] = mins % 10 + '0';
    str[12] = secs / 10 + '0';    str[14] = secs % 10 + '0';
  }
  debug(String(str));
  
  /* draw element */  
  myScreen.gText(8, 51, "How about the time?", true, grayColour);
  myScreen.gText(7, 50, "How about the time?", true, blueColour);
  dayUp.draw();  hourUp.draw();  minUp.draw();  secUp.draw();
  dayDown.draw();  hourDown.draw();  minDown.draw();  secDown.draw();
  myScreen.gText(40, 124, str, true, blackColour);
  timeSetOK.draw(true);
  /*****************/
  
  while(1) {
    if (timeSetOK.isPressed()) {
      debug("Time set " + String(str));
      RTC.SetTime(hour, mins, secs, day);
      setTimeInterupt(false);
      return;
    }
    myScreen.setPenSolid(true);
    if (dayUp.check(true)) {
      if (++day > 6) day = 0;
      str[0] = wd[day][0];
      str[1] = wd[day][1];
      str[2] = wd[day][2];
      drawTimeDateSetting(40, str);
    }
    if (hourUp.check(true)) {
      if (++hour > 23) hour = 0;
      str[4] = hour / 10 + '0';
      str[6] = hour % 10 + '0';
      drawTimeDateSetting(40, str);
    }
    if (minUp.check(true)) {
      if (++mins > 59) mins = 0;
      str[8] = mins / 10 + '0';
      str[10] = mins % 10 + '0';
      drawTimeDateSetting(40, str);
    }
    if (secUp.check(true)) {
      if (++secs > 59) secs = 0;
      str[12] = secs / 10 + '0';
      str[14] = secs % 10 + '0';
      drawTimeDateSetting(40, str);
    }
    if (dayDown.check(true)) {
      if (--day < 0) day = 6;
      str[0] = wd[day][0];
      str[1] = wd[day][1];
      str[2] = wd[day][2];
      drawTimeDateSetting(40, str);
    }
    if (hourDown.check(true)) {
      if (--hour < 0) hour = 23;
      str[4] = hour / 10 + '0';
      str[6] = hour % 10 + '0';
      drawTimeDateSetting(40, str);
    }
    if (minDown.check(true)) {
      if (--mins < 0) mins = 59;
      str[8] = mins / 10 + '0';
      str[10] = mins % 10 + '0';
      drawTimeDateSetting(40, str);
    }
    if (secDown.check(true)) {
      if (--secs < 0) secs = 59;
      str[12] = secs / 10 + '0';
      str[14] = secs % 10 + '0';
      drawTimeDateSetting(40, str);
    }
  }  
}

void drawTimeDateSetting(uint8_t x, char* str) {
  myScreen.rectangle(0, 124, 320, 148, whiteColour);
  myScreen.gText(x, 124, str, true, blackColour);
}

/************************************************************************************/
/*                                  Error Messages                                  */
/************************************************************************************/
void newPairMessage(String s) {
  myScreen.setFontSize(3);
  myScreen.gText(10, 44, "Pairing new " + s + "...", true);
  myScreen.gText(10, 70, "Name: ", true);
  myScreen.setFontSize(1);
  myScreen.gText(210, 82, "Max " + String(MAXNAMELENGTH) + " letters", true);
}


void emptyStringMessage() {
  myScreen.setFontSize(1);
  myScreen.gText(10, 102, nameEmpty, true, redColour);
}

void addErrorMessage(String s) {
  myScreen.setFontSize(1);
  myScreen.gText(10, 102, s, true, redColour);
}

void maxNumberError(String s) {
  uiBackground();
  myScreen.setFontSize(3);
  myScreen.gText(8, 60, "Cannot add " + s + "...", true, redColour);
  myScreen.setFontSize(2);
  myScreen.gText((320 - ((13+s.length()) * myScreen.fontSizeX())) >> 1, 90, "Max " + s + " achieved", true, redColour);
  delay(3000);
}

/**********************************************************************************/
/*                                  Initializing                                  */
/**********************************************************************************/
boolean firstInit() {
  RTC.begin();
  KB.begin(&myScreen);
  dateSet();
  timeSet();
  Scheduler.startLoop(updateTime);  
  return pairRoom();
}

void initAIR() {
  // init AIR module
  Radio.begin(ADDRESS_LOCAL, CHANNEL_1, POWER_MAX);
  memset(rxPacket.msg, 0, sizeof(rxPacket.msg));
  memset(txPacket.msg, 0, sizeof(txPacket.msg));
  txPacket.parent = ADDRESS_LOCAL;
}

void initLCD() {
  myScreen.begin();
  myScreen.setOrientation(3);
  myScreen.setFontSize(myScreen.fontMax());
  myScreen.clear(blackColour);
  opening();
}

void opening() {
  // print Logo for 5 second
  myScreen.drawImage(g_logoImage, 0, 0);
  delay(5000);
  myScreen.clearScreen();
}

/***************************************************************************/
/*                                  Utils                                  */
/***************************************************************************/
void updateNameField(String s) {
  myScreen.drawImage(g_9624WhiteImage, 106, 70);
  myScreen.setFontSize(3);
  myScreen.gText(106, 70, s, true, whiteColour);
}

boolean isEmpty(String s) {
  if (s.length() == 0) return true;
  if (s.charAt(s.length() - 1) == ' ') {
    return true && isEmpty(s.substring(0, s.length() - 1));
  } else {
    return false && isEmpty(s.substring(0, s.length() - 1));
  }
}

boolean repeatRoomName(String name) {
  for (int i = 0; i < roomSize; i++) {
    if (name == room_l[i].name) return true;
  }
  return false;
}

boolean repeatChildName(roomStruct *room, String name) {
  for (int i = 0; i < room->childSize; i++) {
    if (name == room->childList[i].name) return true;
  }
  return false;
}

void debug(String s) {
  if (DEBUG)
    Serial.println(s);
}

void idle() {
  if (++_timer == 5000) {
    myScreen.setBacklight(false);
  }
  if (myScreen.isTouch()) {
    _timer = 0;
    myScreen.setBacklight(true);
  }  
}
