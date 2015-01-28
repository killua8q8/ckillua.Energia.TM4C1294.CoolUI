#include <SPI.h>
#include <AIR430BoostFCC.h>
#include <Screen_K35.h>
#include <LCD_graphics.h>
#include <LCD_GUI.h>
#include "util.h"

#define ADDRESS_LOCAL 0x01
struct sPacket rxPacket;

// UI related elements
Screen_K35 myScreen;
button homeButton;
imageButton optionButton, nextButton, returnButton, updateButton;
item home_i, option_i;
roomStruct room_l[MAXROOMSIZE];
childStruct child_l[MAXCHILDSIZE];
uint8_t roomSize = 0;

// System time vars
int8_t hour = 0, minute = 0, second = 0;
long recordMillis = 0, leftover = 0;

// other vars
String deviceName;
boolean firstInitialized = false;

void setup()
{
  // start serial communication at 115200 baud
  Serial.begin(115200);
  initAIR();
  initLCD();
  opening();
  firstInit();
  home();
}

void loop()
{
  updateTime(true);
  if (optionButton.check(true)) {
    option();
    home();
  }
  for (int i = 0; i < roomSize; i++) {
    if (room_l[i].button.check(true)) {
      roomConfig(room_l[i]);
      home();
    } 
  }
}

void firstInit() {
  timeSet();
  pairRoom();
  firstInitialized = true;
}

void timeSet() {
  button timeSetOK;
  imageButton up_1, up_2, up_3, down_1, down_2, down_3;
  item timeSetOK_i, up_i, down_i;
  
  myScreen.clear(whiteColour);
  myScreen.setFontSize(3);
  
  /* init elements */
  timeSetOK_i = setItem(0, "OK");
  up_i = setItem(0, "UP");
  down_i = setItem(0, "DOWN");
  
  timeSetOK.dDefine(&myScreen, 228, 195, 60, 40, timeSetOK_i, blueColour, yellowColour);
  up_1.dDefine(&myScreen, g_upImage, 72, 87, up_i);
  up_2.dDefine(&myScreen, g_upImage, 136, 87, up_i);
  up_3.dDefine(&myScreen, g_upImage, 200, 87, up_i);
  down_1.dDefine(&myScreen, g_downImage, 72, 153, down_i);
  down_2.dDefine(&myScreen, g_downImage, 136, 153, down_i);
  down_3.dDefine(&myScreen, g_downImage, 200, 153, down_i);
  
  timeSetOK.enable();
  up_1.enable();
  up_2.enable();
  up_3.enable();
  down_1.enable();
  down_2.enable();
  down_3.enable();
  /*****************/
  
  /* draw element */
  myScreen.gText(1, 51, "What time is it now?", true, grayColour);
  myScreen.gText(0, 50, "What time is it now?", true, blueColour);
  up_1.draw();
  up_2.draw();
  up_3.draw();
  myScreen.gText(72, 124, hour/10 + String(" ") + hour%10 + String(":") + minute/10 + String(" ") + minute%10 + String(":") + second/10 + String(" ") + second%10, true, blackColour);
  down_1.draw();
  down_2.draw();
  down_3.draw();
  timeSetOK.draw(true);
  /*****************/
  
  while (1) {
    if (timeSetOK.isPressed()) {
      Serial.println("Time set " + String(hour) + ":" + String(minute) + ":" + String(second));
      recordMillis = millis();
      return;
    }
    myScreen.setPenSolid(true);
    if (up_1.check(true)) {
      hour++;
      if (hour > 23) hour = 0;
      myScreen.rectangle(72, 124, 120, 148, whiteColour);
      myScreen.gText(72, 124, hour/10 + String(" ") + hour%10, true, blackColour);
    }
    if (up_2.check(true)) {
      minute++;
      if (minute > 59) minute = 0;
      myScreen.rectangle(136, 124, 184, 148, whiteColour);
      myScreen.gText(136, 124, minute/10 + String(" ") + minute%10, true, blackColour);
    }
    if (up_3.check(true)) {
      second++;
      if (second > 59) second = 0;
      myScreen.rectangle(200, 124, 248, 148, whiteColour);
      myScreen.gText(200, 124, second/10 + String(" ") + second%10, true, blackColour);
    }
    if (down_1.check(true)) {
      hour--;
      if (hour < 0) hour = 23;
      myScreen.rectangle(72, 124, 120, 148, whiteColour);
      myScreen.gText(72, 124, hour/10 + String(" ") + hour%10, true, blackColour);
    }
    if (down_2.check(true)) {
      minute--;
      if (minute < 0) minute = 59;
      myScreen.rectangle(136, 124, 184, 148, whiteColour);
      myScreen.gText(136, 124, minute/10 + String(" ") + minute%10, true, blackColour);
    }
    if (down_3.check(true)) {
      second--;
      if (second < 0) second = 59;
      myScreen.rectangle(200, 124, 248, 148, whiteColour);
      myScreen.gText(200, 124, second/10 + String(" ") + second%10, true, blackColour);
    }    
  }
}

void initUI() {
  Serial.println("Init UI");
  uiBackground();
  updateTime(true);
  
  uint8_t count = 0;
  
  if (roomSize > 0) {
      for (int i = 0; i < roomSize; i++) {
        room_l[i].button.draw();
        count++;
      }
  }
  
  option_i = setItem(99, "OPTION");
  optionButton.dDefine(&myScreen, g_optionImage, xy[count][0], xy[count][1], option_i);
  optionButton.enable();
  optionButton.draw();
}

void home() {
  
  /* Redraw all buttons and enable them */
  ////////////////////////////////////////
  /**************************************/
  initUI();
  
}

boolean childConfig(roomStruct room) {
  
  uint8_t count = 0;
  
  uiBackground();
  updateTime(true);
  
  for (int i = 0; i < room.childSize; i++) {
    room.childList[i].button.draw();
    count++;
  }
  returnButton.dDefine(&myScreen, g_returnImage, xy[count][0], xy[count][1], setItem(100, "RETURN"));
  returnButton.enable();
  returnButton.draw();
  
  while(1) {
    updateTime(true);
    if (homeButton.isPressed()) {
      return HOME;
    }
    if (returnButton.check(true)) {
       return RETURN; 
    }
  }
  
  //TODO: control children
}

void roomConfig(roomStruct room) {
  resetRoomConfigUI(room);
  while (1) {
    updateTime(true);
    if (returnButton.check(true) || homeButton.isPressed()) {
      return;
    }
    if (updateButton.check(true)) {
      updateRoomInfo(room);
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

void resetRoomConfigUI(roomStruct room) {
  uint8_t count = 3;
  
  uiBackground();
  updateRoomInfo(room);
  updateTime(true);
  
  // TODO: Need g_updateImage 
  updateButton.dDefine(&myScreen, g_addImage, xy[count][0], xy[count][1], setItem(100, "UPDATE"));
  updateButton.enable();
  updateButton.draw();

  optionButton.dDefine(&myScreen, g_optionImage, xy[count+1][0], xy[count+1][1], setItem(100, "Option"));
  optionButton.enable();
  optionButton.draw();
  
  returnButton.dDefine(&myScreen, g_returnImage, xy[count+2][0], xy[count+2][1], setItem(100, "RETURN"));
  returnButton.enable();
  returnButton.draw();
  
}

void updateRoomInfo(roomStruct room) {
  uint8_t temp = 0, x = 28, y = 60;
  myScreen.drawImage(g_infoImage, x, y);
  myScreen.setFontSize(3);
  myScreen.gText(x + 45, y + 20, room.name, true);
  if (room.type == MASTER) {
    if (room.childSize > 0) {
      /***************************
      TODO: get temperature from children
      ***************************/
    }
    uint8_t localTemp = getLocalTemp();
  } else {
  /***************************
  TODO: get temperature from slave
  ***************************/    
  }
  myScreen.gText(x + 177,y + 20, String(temp) + (char)0xB0 + "F", true);
}

uint8_t getLocalTemp() {
  // TODO: Return calculated local temp 
}

void uiBackground() {
  myScreen.clear(blackColour);
  myScreen.drawImage(g_bgImage, 0, 0);
  
  home_i = setItem(0, "HOME");
  homeButton.dDefine(&myScreen, 10, 4, 32, 32, home_i, whiteColour, redColour);
  homeButton.enable();
}

void updateTime(boolean flag) {
  int tempS;
  long dTime = millis() - recordMillis + leftover;
  if (dTime < 1000) return;

  recordMillis = millis();
  leftover = dTime - floor(dTime/1000)*1000;
  //Serial.println(dTime);
  
  tempS = second + dTime/1000;
  //Serial.println(String("temps : ") + tempS);
  
  if (tempS > 59) {
    minute += tempS/60;
    second = tempS%60;
  } else {
    second = tempS; 
  }
  if (minute > 59) {
    hour += minute/60;
    minute %= 60;
  }
  if (hour > 23) {
    hour %= 24; 
  }
  
  if (flag) {
    myScreen.drawImage(g_timebarImage, 80, 0);
    myScreen.setFontSize(3);
    myScreen.gText(182, 8, hour + String(":") + minute + String(":") + second, true, whiteColour);
  }
}

void restOption(imageButton setTimeButton, imageButton pairSlaveButton, imageButton pairChildButton) {
  uiBackground();
  setTimeButton.draw();
  pairSlaveButton.draw();
  pairChildButton.draw();
  returnButton.draw();
}

void option() {
  
  imageButton setTimeButton, pairSlaveButton, pairChildButton;
  item setTime_i = setItem(98, "SETTIME");
  item pairslave_i = setItem(96, "PAIRSLAVE");
  item pairChild_i = setItem(97, "PAIRCHILD");
  item return_i = setItem(100, "RETURN");
  
  /* Disable all buttons */
  /*  Maybe not needed  */
  /***********************/
  pairSlaveButton.dDefine(&myScreen, g_pairSlaveImage, xy[0][0], xy[0][1], pairslave_i);
  pairSlaveButton.enable();
  pairChildButton.dDefine(&myScreen, g_pairChildImage, xy[1][0], xy[1][1], pairChild_i);
  pairChildButton.enable();
  setTimeButton.dDefine(&myScreen, g_setTimeImage, xy[2][0], xy[2][1], setTime_i);
  setTimeButton.enable();
  returnButton.dDefine(&myScreen, g_returnImage, xy[3][0], xy[3][1], return_i);
  returnButton.enable();
  restOption(setTimeButton, pairSlaveButton, pairChildButton);
  
  while (1) {
    updateTime(true);
    if (homeButton.isPressed()) {
      Serial.println("Home is pressed");
      return;
    }
    if (setTimeButton.check(true)) {
      Serial.println("Set Time is pressed");
      timeSet();
      restOption(setTimeButton, pairSlaveButton, pairChildButton);
    }
    if (pairSlaveButton.check(true)) {
      if (pairRoom()) {
        return;
      } else {
        restOption(setTimeButton, pairSlaveButton, pairChildButton);
      }
    }
    if (pairChildButton.check(true)) {
      Serial.println("Pair Child is pressed");
      if (pairChild(&room_l[0])) {
        return;
      } else {
        restOption(setTimeButton, pairSlaveButton, pairChildButton);
      }
    }
    if (returnButton.check(true)) {
      Serial.println("Return from Option");
      return; 
    }
  }  
}

boolean pairRoom() {
  // Keyboard elements
  area row_1, row_2, row_3,row_4, left, right;
  button keyRow_1[11], keyRow_2[11], keyRow_3[11], keyRow_4[11];
  item keyRow_1_i[11], keyRow_2_i[11], keyRow_3_i[11], keyRow_4_i[11];

  String name = "";
  uint8_t key;
  item next_i = setItem(80, "NEXT");  
  nextButton.dDefine(&myScreen, g_nextImage, 223, 96, next_i);
  nextButton.enable();
  
  uiBackground();
  updateNameField(name);
  nextButton.draw();
  if (firstInitialized) {
    newPairMessage("Room");
  } else {
    myScreen.setFontSize(3);
    myScreen.gText(10, 44, "Name this room...", true);
    myScreen.gText(10, 70, "Name: ", true);
    myScreen.setFontSize(1);
    myScreen.gText(210, 82, "Max 6 letters", true);
  }
  INITKB;
  
  while (1) {
    updateTime(true);
    key = GETKEY;
    if (key == 0x0D) {
      if (firstInitialized) {
        return HOME;  
      }
    } else if (key == 0x06) {
      if (!isEmpty(name)) {
        if (roomSize > 0 && repeatRoomName(name)) {
          repeatNameMessage(roomNameRepeat);
        } else {
          if (roomSize < MAXROOMSIZE) {
            newRoom(name);
          } else {
            maxNumberError("Rooms");
          }
          return HOME;
        }
      } else {
        emptyStringMessage();
      }
    } else if (key == 0x08) {
      if (name.length() > 0) {
        name = name.substring(0, name.length()-1);
        updateNameField(name);
      }
    } else {
      if (name.length() < 6) {
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
  room_l[roomSize].childSize = 0; 
  if (firstInitialized) {
    /*************************
    pair device
    room_l[roomSize].node = 0;
    *************************/
    room_l[roomSize].type = SLAVE;
  } else {
    room_l[roomSize].type = MASTER; 
  }
  roomSize++;
}

void resetPairChild(imageButton pairFan, imageButton pairVent, imageButton pairBlind) {
  uiBackground();
  pairFan.draw();
  pairVent.draw();
  pairBlind.draw();
  returnButton.draw();
}

boolean pairChild(roomStruct *room) {
  imageButton pairFan, pairVent, pairBlind;
  item fan_i = setItem(89, "PAIRFAN");
  item vent_i = setItem(88, "PAIRVENT");
  item blind_i = setItem(87, "PAIRBLIND");
  item return_i = setItem(100, "RETURN");  
  
  pairFan.dDefine(&myScreen, g_pairFanImage, xy[0][0], xy[0][1], fan_i);
  pairFan.enable();  
  pairVent.dDefine(&myScreen, g_pairVentImage, xy[1][0], xy[1][1], vent_i);
  pairVent.enable();  
  pairBlind.dDefine(&myScreen, g_pairBlindImage, xy[2][0], xy[2][1], blind_i);
  pairBlind.enable();  
  returnButton.dDefine(&myScreen, g_returnImage, xy[3][0], xy[3][1], return_i);
  returnButton.enable();
  resetPairChild(pairFan, pairVent, pairBlind);
  
  while(1) {
    updateTime(true);
    if (homeButton.isPressed()) {
      Serial.println("Home is pressed");
      return HOME;
    }
    if (returnButton.check(true)) {
      Serial.println("Return is pressed");
      return RETURN;
    }
    if (pairFan.check(true)) {
      if (newChild(room, NEWFAN) == HOME) return HOME;
    }
    if (pairVent.check(true)) {
      if (newChild(room, NEWVENT) == HOME) return HOME;
    }
    if (pairBlind.check(true)) {
      if (newChild(room, NEWBLIND) == HOME) return HOME;
    }
  }
}

boolean newChild(roomStruct *room, uint8_t type) {
  // Keyboard elements
  area row_1, row_2, row_3,row_4, left, right;
  button keyRow_1[11], keyRow_2[11], keyRow_3[11], keyRow_4[11];
  item keyRow_1_i[11], keyRow_2_i[11], keyRow_3_i[11], keyRow_4_i[11];

  String name = "";
  uint8_t key;
  item next_i = setItem(80, "NEXT");  
  nextButton.dDefine(&myScreen, g_nextImage, 223, 96, next_i);
  nextButton.enable();

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
  INITKB;

  while (1) {    
    updateTime(true);
    key = GETKEY;
    if (key == 0x0D) {
      return HOME;
    } else if (key == 0x06) {
      if (!isEmpty(name)) {
        if (room->childSize < MAXCHILDSIZE) {
          if (room->childSize > 0 && repeatChildName(room, name)) {
            repeatNameMessage(childNameRepeat);
          } else {
            if (type == NEWFAN) {
              addChild(room, name, FAN, g_fanImage);
              Serial.println("FAN added");
            } else if (type == NEWVENT) {
              addChild(room, name, VENT, g_ventImage);
            } else if (type == NEWBLIND) {
              addChild(room, name, BLIND, g_blindImage);
            } else {
              return HOME;
            }
            return HOME;
          }          
        } else {
          maxNumberError("Children");
          delay(3000);
          return HOME;
        }        
      } else {
        emptyStringMessage();
      }
    } else if (key == 0x08) {
      if (name.length() > 0) {
        name = name.substring(0, name.length()-1);
        updateNameField(name);
      }
    } else {
      if (name.length() < 6) {
        name += char(key);
        updateNameField(name);
      }
    }    
  } 
  
}

void addChild(roomStruct *room, String name, child_t type, const uint8_t *icon) {
  uint8_t childSize = room->childSize;
  uint8_t position = childSize % 6;
  
  room->childList[childSize].name = name;
  room->childList[childSize].type = type;
  room->childList[childSize].button.define(&myScreen, icon, xy[position][0], xy[position][1], name);
  room->childList[childSize].button.enable();
  /********************
  pair child
  room->childList[childSize].node = 0;
  ********************/
  room->childSize++;
  
  
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

void updateNameField(String s) {
  myScreen.drawImage(g_9624WhiteImage, 106, 70);
  myScreen.setFontSize(3);
  myScreen.gText(106, 70, s, true, whiteColour);
}

void emptyStringMessage() {
  myScreen.setFontSize(1);
  myScreen.gText(10, 102, nameEmpty, true, redColour);
}

void repeatNameMessage(String s) {
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

void newPairMessage(String s) {
  myScreen.setFontSize(3);
  myScreen.gText(10, 44, "Pairing new " + s + "...", true);
  myScreen.gText(10, 70, "Name: ", true);
  myScreen.setFontSize(1);
  myScreen.gText(210, 82, "Max 6 letters", true);
}

void initKeyboard(area* left, area* right, area* row_1, area* row_2, area* row_3, area* row_4, button keyRow_1[], button keyRow_2[], button keyRow_3[], button keyRow_4[], item keyRow_1_i[], item keyRow_2_i[], item keyRow_3_i[], item keyRow_4_i[]) { 
  left->dDefine(&myScreen, 0, 123, 174, 116, 0);
  right->dDefine(&myScreen, 175, 123, 145, 116, 0);
  row_1->dDefine(&myScreen, 0, 123, 320, 29, 1);
  row_2->dDefine(&myScreen, 0, 152, 320, 29, 2);
  row_3->dDefine(&myScreen, 0, 181, 320, 29, 3);
  row_4->dDefine(&myScreen, 0, 210, 320, 29, 4);
  left->enable();
  right->enable();
  row_1->enable();
  row_2->enable();
  row_3->enable();
  row_4->enable();
  
  for (int i = 0; i < 11; i++) {
    keyRow_1_i[i] = setItem(keyOrder[0][i], String(keyOrder[0][i]));
    keyRow_2_i[i] = setItem(keyOrder[1][i], String(keyOrder[1][i]));
    keyRow_3_i[i] = setItem(keyOrder[2][i], String(keyOrder[2][i]));
    keyRow_4_i[i] = setItem(keyOrder[3][i], String(keyOrder[3][i]));
    keyRow_1[i].dDefine(&myScreen, 29 * i, 123, 29, 29, keyRow_1_i[i], whiteColour, redColour);
    keyRow_2[i].dDefine(&myScreen, 29 * i, 152, 29, 29, keyRow_2_i[i], whiteColour, redColour); 
    keyRow_3[i].dDefine(&myScreen, 29 * i, 181, 29, 29, keyRow_3_i[i], whiteColour, redColour);
    keyRow_4[i].dDefine(&myScreen, 29 * i, 210, 29, 29, keyRow_4_i[i], whiteColour, redColour);
    keyRow_1[i].enable();
    keyRow_2[i].enable();
    keyRow_3[i].enable();
    keyRow_4[i].enable();
  }  
  myScreen.drawImage(g_keyboardImage, 0, 123); 
}

uint8_t getKey(area left, area right, area row_1, area row_2, area row_3, area row_4, button keyRow_1[], button keyRow_2[], button keyRow_3[], button keyRow_4[], item keyRow_1_i[], item keyRow_2_i[], item keyRow_3_i[], item keyRow_4_i[]) {
  uint8_t key;
  while (1) {
    updateTime(true);
    // check next button
    if (nextButton.check(true)) {
      return 0x06;
    } 
    // check home button
    if (homeButton.isPressed()) {
      return 0x0D;
    }    
    // check left
    if (left.check(true)) {
      key = _getKey(0, 6, left, right, row_1, row_2, row_3, row_4, keyRow_1, keyRow_2, keyRow_3, keyRow_4, keyRow_1_i, keyRow_2_i, keyRow_3_i, keyRow_4_i);
      if (key != 0) return key;
    }
    // check right
    if (right.check(true)) {
      key = _getKey(6, 11, left, right, row_1, row_2, row_3, row_4, keyRow_1, keyRow_2, keyRow_3, keyRow_4, keyRow_1_i, keyRow_2_i, keyRow_3_i, keyRow_4_i);
      if (key != 0) return key;
    }
  }
}

uint8_t _getKey(int _start, int _end, area left, area right, area row_1, area row_2, area row_3, area row_4, button keyRow_1[], button keyRow_2[], button keyRow_3[], button keyRow_4[], item keyRow_1_i[], item keyRow_2_i[], item keyRow_3_i[], item keyRow_4_i[]) {
  if (row_1.check(true)) {
    for (int i = _start; i < _end; i++) {
      if (keyRow_1[i].isPressed()) {
        return (uint8_t)keyRow_1_i[i].index;
      }
    }
  }
  if (row_2.check(true)) {
    for (int i = _start; i < _end; i++) {
      if (keyRow_2[i].isPressed()) {
        return (uint8_t)keyRow_2_i[i].index;
      }
    }
  }
  if (row_3.check(true)) {
    for (int i = _start; i < _end; i++) {
      if (keyRow_3[i].isPressed()) {
        return (uint8_t)keyRow_3_i[i].index;
      }
    }
  }
  if (row_4.check(true)) {
    for (int i = _start; i < _end; i++) {
      if (keyRow_4[i].isPressed()) {
        return (uint8_t)keyRow_4_i[i].index;
      }
    }
  }
  return 0;
}

void initAIR() {
  // init AIR module
  Radio.begin(ADDRESS_LOCAL, CHANNEL_1, POWER_MAX);
  rxPacket.node = 0;
  memset(rxPacket.msg, 0, sizeof(rxPacket.msg));
}

void initLCD() {
  myScreen.begin();
  myScreen.setOrientation(3);
  myScreen.setFontSize(myScreen.fontMax());
  myScreen.clear(blackColour);
}

void opening() {
  // print Logo for 5 second
  myScreen.drawImage(g_logoImage, 0, 0);
  delay(5000);
  myScreen.clearScreen();
}
