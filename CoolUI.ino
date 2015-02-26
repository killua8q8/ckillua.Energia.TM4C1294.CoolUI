#include <SPI.h>
#include <Scheduler.h>
#include <RTChardware.h>
#include <AIR430BoostFCC.h>
#include <Screen_K35.h>
#include <LCD_graphics.h>
#include <LCD_GUI.h>
#include "util.h"
#include "Keyboard.h"
#include "Job.h"

#define DEBUG true
#define ADDRESS_LOCAL 0x00  /* Every slave controller increments by 0x10, max total 4 controllers, so node up to 0x30 */
#define TYPE MASTER
struct sPacket rxPacket;
struct sPacket txPacket;

// UI related elements
Screen_K35 myScreen;
button homeButton, updateTimeModeToggle;
imageButton optionButton, nextButton, returnButton, updateButton, removeButton, onButton, offButton, jobButton;
imageButton addJobButton, checkButton, uncheckButton, previousButton, returnsButton, roomInfoButton, removeJobButton;
imageButton timeCheckButton, minTempCheckButton, maxTempCheckButton, onCheckButton, offCheckButton;
imageButton hourPlusButton, minPlusButton, minTempPlusButton, maxTempPlusButton;
imageButton hourMinusButton, minMinusButton, minTempMinusButton, maxTempMinusButton;
item home_i, option_i;

// System main vars
String deviceName;
boolean firstInitialized = false, celsius = true;
boolean timeInterrupt = false, _idle = false, _idleInterrupt = false;
roomStruct room_l[MAXROOMSIZE];
uint8_t roomSize = 0;
uint8_t updateTimeMode;
long _timer = 0;
Job job;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  initAIR();
  initLCD();
  firstInitialized = firstInit();
  Scheduler.startLoop(idle);
  Scheduler.startLoop(jobs);
  home();
}

void loop()
{
  // put your main code here, to run repeatedly:
  if (_idle) {
    while(_idle){delay(1);}
    home();
  }
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
/*                                  Job Controls                                */
/********************************************************************************/
void jobs() {  
  if (!job.onUpdate && job.scheduleSize > 0) {
    job.onLoop = true;
    RTCTime current;
    for (int i = 0 ; i < job.scheduleSize; i++) {
      if (job.schedules[i].enable) {
        RTC.GetAll(&current);
        if (job.schedules[i].cond.cond_type == 0) {  //Time base          
          if (!job.schedules[i].done && current.hour == job.schedules[i].cond.time.hour && current.minute == job.schedules[i].cond.time.minute/* && abs(current.second - job.schedules[i].cond.time.second) <= 1000*/) {
            childCommand(room_l[0].childList[job.schedules[i].childIndex], job.cmdTypeToString(job.schedules[i].command));
            job.setJobDone(i, true);
          } else if (abs(current.minute - job.schedules[i].cond.time.minute) > 0 && job.schedules[i].done) {
            job.setJobDone(i, false);
          }
        } else if (job.schedules[i].cond.cond_type == 1) {  //Temp base
          if (!job.schedules[i].done && job.schedules[i].cond.minTemp == room_l[0].roomTempF) {
            childCommand(room_l[0].childList[job.schedules[i].childIndex], job.cmdTypeToString(job.schedules[i].command));
            job.setJobDoneTime(i, current);
            job.setJobDone(i, true);
          } else {
            if (abs(current.minute - job.schedules[i].cond.time.minute) >= 2) {
              job.setJobDone(i, false);
            }
          }
        } else if (job.schedules[i].cond.cond_type == 2) {  //range
          if (!job.schedules[i].done) {
            debug(String(room_l[0].roomTempF) + " " + job.schedules[i].cond.minTemp + " " + job.schedules[i].cond.maxTemp);
            if (room_l[0].roomTempF <= job.schedules[i].cond.minTemp) {
              childCommand(room_l[0].childList[job.schedules[i].childIndex], job.cmdTypeToString(OFF));
            } else if (room_l[0].roomTempF >= job.schedules[i].cond.maxTemp) {
              childCommand(room_l[0].childList[job.schedules[i].childIndex], job.cmdTypeToString(ON));
            }
            job.setJobDoneTime(i, current);
            job.setJobDone(i, true);
          } else {
            if (abs(current.minute - job.schedules[i].cond.time.minute) >= 1) {
              job.setJobDone(i, false);
            }
          }
        }
      }
    }
  } else {
    delay(5); 
  }
  job.onLoop = false;
  delay(5);
}

boolean jobConfig(roomStruct* room) {
  uint8_t page = 1;
  if (room->type == MASTER) {
    resetJobConfigUI(page, job.scheduleSize > 6);
  } else {
    // TODO: Slave portion 
  }
  
  while (1) {
    if (_idle) {
      while(_idle){delay(1);}
      resetJobConfigUI(page, job.scheduleSize > 6);
    }
    if (homeButton.isPressed()) {
      return HOME; 
    }
    if (returnsButton.check(true)) {
      return RETURN; 
    }
    for (int i = 0; i < job.scheduleSize; i++) {
      if (job.schedules[i].list.check(true)) {
        if (addJob(room, false, i)) return HOME;
        resetJobConfigUI(page, job.scheduleSize > 6);
      }
      if (job.schedules[i].checkBox.check(true)) {
        job.setJobEnable(i, !job.isEnable(i));
        resetJobConfigUI(page, job.scheduleSize > 6);
      }      
    }
    if (addJobButton.check(true)) {
      if (room->childSize > 0 && job.scheduleSize < MAXSCHEDULE && addJob(room, true, -1)) return HOME;
      resetJobConfigUI(page, job.scheduleSize > 6);
    }
    if (nextButton.check(true)) {
      resetJobConfigUI(++page, job.scheduleSize > 6);
    }
    if (previousButton.check(true)) {
      resetJobConfigUI(--page, job.scheduleSize > 6);
    }
  }
}

boolean addJob(roomStruct* room, boolean add, uint8_t index) {
  setIdleInterrupt(true);
  if (add) debug("ADD JOB");
  else debug("EDIT JOB");
  boolean timeCheck = false, minTempCheck = false, maxTempCheck = false, onCheck = false, offCheck = false;
  int16_t data[] = {0, 0, 72, 76};  // 0 - hour, 1 - mins, 2 - minT, 3 - maxT
  RTCTime t;
  cmd_type cmd;
  uint8_t condition;
  childButton temp[room->childSize];
  String selection = "Nothing";
  if (add) {
    aj_init_once(room, temp);
    resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 0);
  } else {
    data[0] = job.schedules[index].cond.time.hour;
    data[1] = job.schedules[index].cond.time.minute;
    data[2] = job.schedules[index].cond.minTemp;
    data[3] = job.schedules[index].cond.maxTemp;
    cmd = job.schedules[index].command;
    selection = job.schedules[index].childName;
    condition = job.schedules[index].cond.cond_type;
    if (condition == 0) {  // 0 - timebase, 1 - tempbase, 2 - range
      timeCheck = true;
    } else if (condition == 1) {
      minTempCheck = true;
    } else if (condition == 2) {
      minTempCheck = true;
      maxTempCheck = true;
    }
    if (condition == 0 || condition == 1) {
      if (cmd == ON)  onCheck = true;
      else offCheck = true; 
    }
    dj_init_once(room, temp);
    resetDeleteJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 0);
  }
  
  
  while (1) {
    if (homeButton.isPressed()) { setIdleInterrupt(false); return HOME; }
    if (returnsButton.check(true)) { setIdleInterrupt(false); return RETURN; }
    if (nextButton.check(true)) {
      if (selection != "Nothing" && (maxTempCheck || (timeCheck || minTempCheck) && (onCheck || offCheck))) {
        if (timeCheck) { condition = 0; }
        else if (minTempCheck && !maxTempCheck) { condition = 1; }
        else if (minTempCheck && maxTempCheck) { condition = 2; }
        t.hour = data[0];
        t.minute = data[1];
        if (onCheck) { cmd = ON; }
        else { cmd = OFF; }
        uint8_t ci = getChildNameByIndex(room, selection);
        if (add) job.addSchedule(selection, ci, cmd, condition, t, data[2], data[3]);
        else job.editSchedule(index, selection, ci, cmd, condition, t, data[2], data[3]);
        setIdleInterrupt(false);
        return RETURN;
      }
    }
    if (!add && removeJobButton.check(true)) {
      debug("TODO: Finish remove job"); 
      job.removeSchedule(index);
      setIdleInterrupt(false);
      return RETURN;
    }
    for (int i = 0; i < room->childSize; i++) {
      if (temp[i].check(true)) {
        selection = room->childList[i].name;
        resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 1);
      }
    }
    if (timeCheckButton.check(true)) {
      if (timeCheck) {
        timeCheck = false;
      } else {
        timeCheck = true;
        minTempCheck = false;
        maxTempCheck = false;
      }
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 2);
    }
    if (minTempCheckButton.check(true)) {
      if (minTempCheck) {
        minTempCheck = false;
      } else {
        timeCheck = false;
        minTempCheck = true;
      }
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 3);
    }
    if (maxTempCheckButton.check(true)) {
      if (maxTempCheck) {
        maxTempCheck = false;
      } else {
        timeCheck = false;
        minTempCheck = true;
        maxTempCheck = true;
        onCheck = false;
        offCheck = false;
      }      
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 3);
    }
    if (onCheckButton.check(true)) {
      if (onCheck) {
        onCheck = false;
      } else {
        onCheck = true;
        offCheck = false;
        maxTempCheck = false;
      }      
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 4);
    }
    if (offCheckButton.check(true)) {
      if (offCheck) {
        offCheck = false;
      } else {
        onCheck = false;
        offCheck = true;
        maxTempCheck = false;
      }      
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 4);
    }
    if (hourPlusButton.check(true)) {
      if (++data[0] > 23) data[0] = 0;
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 2);
    }
    if (minPlusButton.check(true)) {
      if (++data[1] > 59) data[0] = 0;
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 2);
    }
    if (minTempPlusButton.check(true)) {
      if (++data[2] >= data[3]) ++data[3];
      if (data[3] > MAXTEMP) { --data[3]; --data[2]; }
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 3);
    }
    if (maxTempPlusButton.check(true)) {
      if (++data[3] > MAXTEMP) --data[3];
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 3);
    }
    if (hourMinusButton.check(true)) {
      if (--data[0] < 0) data[0] = 23;
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 2);
    }
    if (minMinusButton.check(true)) {
      if (--data[1] < 0) data[0] = 59;
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 2);
    }
    if (minTempMinusButton.check(true)) {
      if (--data[2] < MINTEMP) ++data[2];
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 3);
    }
    if (maxTempMinusButton.check(true)) {
      if (--data[3] <= data[2]) --data[2];
      if (data[2] < MINTEMP) { ++data[2]; ++data[3]; }
      resetAddJobUI(room, temp, timeCheck, minTempCheck, maxTempCheck, onCheck, offCheck, selection, data, 3);
    }    
  }  
}

// section 0: ALL; 1: Str; 2: time; 3: temps; 4: command;
void resetAddJobUI(roomStruct* room, childButton* children, boolean tc, boolean mintc, boolean maxtc, boolean oc, boolean ofc, String sc, int16_t* d, uint8_t section) {
  uint16_t textColor = myScreen.rgb(77, 132, 171);
  uint8_t xoff = 1;
  if (section == 0) {
    uiBackground();
    nextButton.draw();
    returnsButton.draw();
    for (int i = 0; i < room->childSize; i++) {
      children[i].draw();
    }
  } 
  if (section == 1 || section == 0) {
    myScreen.drawImage(g_0120BKImage, 0, 120);
    gText(0, 120, sc + " is selected.", textColor, 2);
  }
 if (section == 2 || section == 3 || section == 0) {
    myScreen.drawImage(g_0144BKImage, 0, 144);
    gText(42, 144, "Time:", textColor, 2);
    gText(186, 144, ":", textColor, 2);
    
    if (tc) {
      timeCheckButton.dDefine(&myScreen, g_check16Image, 108, 144, setItem(201, "TC"));
    } else {
      timeCheckButton.dDefine(&myScreen, g_uncheck16Image, 108, 144, setItem(201, "TC"));
    }
    timeCheckButton.draw();
    gText(146 - xoff, 144, String(d[0]/10) + String(d[0]%10), textColor, 2);  
    gText(214 - xoff, 144, String(d[1]/10) + String(d[1]%10), textColor, 2);
  }
 if (section == 2 || section == 3 || section == 4 || section == 0) {
    myScreen.drawImage(g_0168BKImage, 0, 168);    
    gText(66, 168, String((char)0xB0) + "F:", textColor, 2);
    gText(130, 168, ">", textColor, 2);
    gText(306, 168, "<", textColor, 2);
    if (mintc) {
      minTempCheckButton.dDefine(&myScreen, g_check16Image, 108, 168, setItem(202, "MTC"));
    } else {
      minTempCheckButton.dDefine(&myScreen, g_uncheck16Image, 108, 168, setItem(202, "MTC"));
    }
    if (maxtc) {
      maxTempCheckButton.dDefine(&myScreen, g_check16Image, 216, 168, setItem(203, "MTC"));
    } else {
      maxTempCheckButton.dDefine(&myScreen, g_uncheck16Image, 216, 168, setItem(203, "MTC"));
    }
    minTempCheckButton.draw();
    maxTempCheckButton.draw();
    gText(158 - xoff, 168, String(d[2]), textColor, 2);
    gText(254 - xoff, 168, String(d[3]), textColor, 2);
  }
 if (section == 4 || section == 3 || section == 0) {
    myScreen.drawImage(g_0192BKImage, 0, 192);
    gText(6, 192, "Command:", textColor, 2);
    gText(130, 192, "ON", textColor, 2);
    gText(182, 192, "OFF", textColor, 2);
    if (oc) {
      onCheckButton.dDefine(&myScreen, g_check16Image, 108, 192, setItem(204, "OC"));
    } else {
      onCheckButton.dDefine(&myScreen, g_uncheck16Image, 108, 192, setItem(204, "OC"));
    }
    if (ofc) {
      offCheckButton.dDefine(&myScreen, g_check16Image, 160, 192, setItem(205, "OFC"));
    } else {
      offCheckButton.dDefine(&myScreen, g_uncheck16Image, 160, 192, setItem(205, "OFC"));
    }
    onCheckButton.draw();
    offCheckButton.draw();
  }
  aj_checkerEnable();
  aj_buttonDraw();
}

void resetDeleteJobUI(roomStruct* room, childButton* children, boolean tc, boolean mintc, boolean maxtc, boolean oc, boolean ofc, String sc, int16_t* d, uint8_t section) {
  resetAddJobUI(room, children, tc, mintc, maxtc, oc, ofc, sc, d, section);
  removeJobButton.draw();
}

void dj_init_once(roomStruct* room, childButton* children) {
  aj_init_once(room, children);
}

void aj_init_once(roomStruct* room, childButton* children) {
  nextButton.dDefine(&myScreen, g_nextImage, 228, 216, setItem(100, "NEXT"));
  nextButton.enable();
  removeJobButton.dDefine(&myScreen, g_deleteImage, 114, 216, setItem(100, "DELETE"));
  removeJobButton.enable();
  returnsButton.dDefine(&myScreen, g_returnSImage, 0, 216, setItem(100, "RETURNS"));
  returnsButton.enable();
  
  uint8_t xoff = (320 - room->childSize*64)/(room->childSize + 1);
  for (int i = 0; i < room->childSize; i++) {
    children[i].define(&myScreen, room->childList[i].button.getIcon(), i+xoff, 48, room->childList[i].name);
    children[i].enable();
  }
  timeCheckButton.dDefine(&myScreen, g_uncheck16Image, 108, 144, setItem(201, "TC"));
  minTempCheckButton.dDefine(&myScreen, g_uncheck16Image, 108, 168, setItem(202, "MTC"));
  maxTempCheckButton.dDefine(&myScreen, g_uncheck16Image, 216, 168, setItem(203, "MTC"));
  onCheckButton.dDefine(&myScreen, g_uncheck16Image, 108, 192, setItem(204, "OC"));
  offCheckButton.dDefine(&myScreen, g_uncheck16Image, 160, 192, setItem(205, "OFC"));
  hourPlusButton.dDefine(&myScreen, g_plusImage, 130, 144, setItem(206, "HPB"));
  minPlusButton.dDefine(&myScreen, g_plusImage, 198, 144, setItem(207, "MPB"));
  minTempPlusButton.dDefine(&myScreen, g_plusImage, 142, 168, setItem(208, "MTPB"));
  maxTempPlusButton.dDefine(&myScreen, g_plusImage, 238, 168, setItem(209, "MTPB"));
  hourMinusButton.dDefine(&myScreen, g_minusImage, 170, 144, setItem(210, "HMB"));
  minMinusButton.dDefine(&myScreen, g_minusImage, 238, 144, setItem(211, "MMB"));
  minTempMinusButton.dDefine(&myScreen, g_minusImage, 194, 168, setItem(212, "MTMB"));
  maxTempMinusButton.dDefine(&myScreen, g_minusImage, 290, 168, setItem(213, "MTMB"));
  aj_checkerEnable();
  hourPlusButton.enable();
  minPlusButton.enable();
  minTempPlusButton.enable();
  maxTempPlusButton.enable();
  hourMinusButton.enable();
  minMinusButton.enable();
  minTempMinusButton.enable();
  maxTempMinusButton.enable();
}

void aj_checkerEnable() {
  timeCheckButton.enable();
  minTempCheckButton.enable();
  maxTempCheckButton.enable();
  onCheckButton.enable();
  offCheckButton.enable();
}

void aj_buttonDraw() {
  hourPlusButton.draw();
  minPlusButton.draw();
  minTempPlusButton.draw();
  maxTempPlusButton.draw();
  hourMinusButton.draw();
  minMinusButton.draw();
  minTempMinusButton.draw();
  maxTempMinusButton.draw();
}

void resetJobConfigUI(uint8_t page, boolean multipage) {
  int x;
  jc_init_once();
  uiBackground();
  addJobButton.draw();
  if (multipage) {
    if (page == 1) {
      previousButton.enable(false);
      nextButton.enable(true);
      nextButton.draw();
      if (job.scheduleSize > 6) x = 6;
      else x = job.scheduleSize;
    } else {
      nextButton.enable(false);
      previousButton.enable(true);
      previousButton.draw();      
      x = job.scheduleSize;
    }
  } else {
    x = job.scheduleSize;
  }
  for (int i = (page-1)*6; i < x; i++) {
    job.schedules[i].checkBox.draw();
    job.schedules[i].list.draw();
  }
  returnsButton.draw();
}

void jc_init_once() {
  if (job.scheduleSize >= MAXSCHEDULE) {
    addJobButton.dDefine(&myScreen, g_jobAddBlackImage, 0, 48, setItem(100, "ADDJOB"));
    addJobButton.enable(false);
  } else {
    addJobButton.dDefine(&myScreen, g_jobAddImage, 0, 48, setItem(100, "ADDJOB"));
    addJobButton.enable();
  }
  previousButton.dDefine(&myScreen, g_previousImage, 0, 216, setItem(100, "PREVIOUS"));
  returnsButton.dDefine(&myScreen, g_returnSImage, 114, 216, setItem(100, "RETURNS"));
  nextButton.dDefine(&myScreen, g_nextImage, 228, 216, setItem(100, "NEXT"));
  previousButton.enable(false);
  returnsButton.enable();
  nextButton.enable(false);
}
/********************************************************************************/
/*                                Child Controls                                */
/********************************************************************************/
boolean childConfig(roomStruct* room) {
  resetChildConfigUI(room);
  while(1) {
    if (_idle) {
      while(_idle){delay(1);}
      resetChildConfigUI(room);
    }
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
  onButton.dDefine(&myScreen, g_onImage, xy[count][0], xy[count++][1], setItem(100, "ON"));
  onButton.enable();  
  offButton.dDefine(&myScreen, g_offImage, xy[count][0], xy[count++][1], setItem(100, "OFF"));
  offButton.enable();  
  removeButton.dDefine(&myScreen, g_removeImage, xy[count][0], xy[count++][1], setItem(100, "REMOVE"));
  removeButton.enable();  
  returnButton.dDefine(&myScreen, g_returnImage, xy[count][0], xy[count++][1], setItem(100, "RETURN"));
  returnButton.enable();
  resetChildControlUI();
  
  while(1) {
    if (_idle) {
      while(_idle){delay(1);}
      resetChildControlUI();
    }
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

void resetChildControlUI() {
  uiBackground();
  onButton.draw();
  offButton.draw();
  removeButton.draw();
  returnButton.draw();
}

void childCommand(childStruct child, char* cmd) {
  txPacket.node = child.node;
  txPacket.parent = ADDRESS_LOCAL;
  strcpy((char*)txPacket.msg, cmd);
  Radio.transmit(child.node, (unsigned char*)&txPacket, sizeof(txPacket));
  while(Radio.busy()){}
  if (Radio.receiverOn((unsigned char*)&rxPacket, sizeof(rxPacket), 10000) <= 0) {
    // Failed connection
    debug("No ACK from node " + String(txPacket.node));
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
      if (i == index) {
        index++;
        room->childList[i].name = room->childList[index].name;
        room->childList[i].type = room->childList[index].type;
        room->childList[i].button.define(&myScreen, room->childList[index].button.getIcon(), xy[i][0], xy[i][1], room->childList[i].name);
        room->childList[i].button.enable();
        room->childList[i].node = room->childList[index].node;
      }
    }
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

uint8_t getChildNameByIndex(roomStruct* room, String name) {
  for (int i = 0; i <room->childSize; i++) {
    if (name ==  room->childList[i].name) return i;
  }
}
/********************************************************************************/
/*                                 Room Controls                                */
/********************************************************************************/
void roomConfig(roomStruct *room) {
  rc_init_once();  
  resetRoomConfigUI(room);
  long current = millis();
  while(1) {
    if (_idle) {
      while(_idle){delay(1);}
      resetRoomConfigUI(room);
    }
    if (millis() - current > 60000) {
      updateRoomInfo(room);
      current = millis();
    }
    if (homeButton.isPressed()) {
      return;
    }
    if (roomInfoButton.check(true)) {
      celsius = !celsius;
      updateRoomInfo(room);
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
    if (jobButton.check(true)) {
      if (jobConfig(room)) {
        return;
      } else {
        resetRoomConfigUI(room);
      } 
    }
  }
}

void rc_init_once() {
  uint8_t count = 3; 
  updateButton.dDefine(&myScreen, g_updateImage, xy[count][0], xy[count][1], setItem(100, "UPDATE"));
  updateButton.enable();
  optionButton.dDefine(&myScreen, g_optionImage, xy[count+1][0], xy[count+1][1], setItem(100, "OPTION"));
  optionButton.enable();
  jobButton.dDefine(&myScreen, g_jobImage, xy[count+2][0], xy[count+2][1], setItem(100, "JOB"));
  jobButton.enable();
  roomInfoButton.dDefine(&myScreen, g_infoImage, 28, 60, setItem(100, "INFO"));
  roomInfoButton.enable();
}

void resetRoomConfigUI(roomStruct* room) {
  uiBackground();
  updateRoomInfo(room);  
  updateButton.draw();  
  optionButton.draw();
  jobButton.draw();
}

void updateRoomInfo(roomStruct* room) {
  uint8_t x = 28, y = 60, count = 0;
  float tempF = 0.0;
  roomInfoButton.draw();
  gText(x + 45, y + 20, room->name, whiteColour, 3);
  if (room->type == MASTER) {
    if (room->childSize > 0) {
      tempF += getChildrenTemp(room);
      count = 1;
    }
    tempF = (tempF + getLocalTemp()) / (float)++count;
    room->roomTempC = (int16_t)ceil(tempF);
    room->roomTempF = room->roomTempC  * 9 / 5 + 32;
  } else {
  /***************************
  TODO: get temperature from slave
  ***************************/    
  }
  if (celsius) {
    gText(x + 177,y + 20, String(room->roomTempC) + (char)0xB0 + "C", whiteColour, 3);
  } else {
    gText(x + 177,y + 20, String(room->roomTempF) + (char)0xB0 + "F", whiteColour, 3);
  }
  
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
    if (_idle) {
      while(_idle){delay(1);}
      restOption(setTimeButton, pairSlaveButton, pairChildButton);
    }
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
    if (_idle) {
      while(_idle){delay(1);}
      resetPairChild(pairFan, pairVent, pairBlind);
    }   
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
  setIdleInterrupt(true);
  String name = "";
  uint8_t key;
  boolean flag;
  
  uiBackground();
  updateNameField(name);
  nextButton.dDefine(&myScreen, g_nextImage, 223, 96, setItem(80, "NEXT"));
  nextButton.enable();
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
    if (homeButton.isPressed()) {
      setIdleInterrupt(false);
      return HOME; 
    }
    if (nextButton.check(true)) {
      if (!isEmpty(name)) {
        if (room->childSize < MAXCHILDSIZE) {
          if (room->childSize > 0 && repeatChildName(room, name)) {
            addErrorMessage(childNameRepeat);
          } else {
            if (type == NEWFAN) {
              if (room->f >= 11) {
                childExceeded(FAN);
                setIdleInterrupt(false);
                return HOME;
              }              
              if (flag = addChild(room, name, FAN, g_fanImage)) debug("FAN added");
              else debug("Failed connect FAN");
            } else if (type == NEWVENT) {
              if (room->v >= 11) {
                childExceeded(VENT);
                setIdleInterrupt(false);
                return HOME;
              }
              if (flag = addChild(room, name, VENT, g_ventImage)) debug("VENT added");
              else debug("Failed connect VENT");
            } else if (type == NEWBLIND) {
              if (room->b >= 11) {
                childExceeded(BLIND);
                setIdleInterrupt(false);
                return HOME;
              }
              if (flag = addChild(room, name, BLIND, g_blindImage)) debug("BLIND added");
              else debug("Failed connect BLIND");
            } else {
              KB.setEnable(false);
              setIdleInterrupt(false);
              return HOME;
            }
            if (flag) {
              KB.setEnable(false);
              setIdleInterrupt(false);
              return HOME;
            }
          }          
        } else {
          maxNumberError("Children");
          KB.setEnable(false);
          delay(3000);
          setIdleInterrupt(false);
          return HOME;
        }        
      } else {
        addErrorMessage(nameEmpty);
      }
    }
    
    key = KB.getKey();
    if (key == 0xFF) {
      delay (1); 
    } else if (key == 0x0D) {
      KB.setEnable(false);
      setIdleInterrupt(false);
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
  addErrorMessage(connecting);
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
  setIdleInterrupt(true);
  String name = "";
  uint8_t key;
  nextButton.dDefine(&myScreen, g_nextImage, 223, 96, setItem(80, "NEXT"));
  nextButton.enable();
  
  uiBackground();
  updateNameField(name);
  nextButton.enable();
  nextButton.draw();  
  if (firstInitialized) {
    newPairMessage("ROOM");
  } else {
    gText(10, 44, "Name this room...", whiteColour, 3);
    gText(10, 70, "Name: ", whiteColour, 3);
    gText(210, 82, "Max " + String(MAXNAMELENGTH) + " letters", whiteColour, 1);
  }
  
  KB.draw();  
  while (1) {
    if (firstInitialized && homeButton.isPressed()) {
      setIdleInterrupt(false);
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
          setIdleInterrupt(false);
          return HOME;
        }
      } else {
        addErrorMessage(nameEmpty);
      }
    }
    
    key = KB.getKey();
    if (key == 0xFF) {
      delay(1);
    } else if (key == 0x0D) {
      if (firstInitialized) {
        KB.setEnable(false);
        setIdleInterrupt(false);
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
    addErrorMessage(connecting);
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
    if (updateTimeMode == TIMEMODE) {
      if (!timeInterrupt) {
        _updateTime(&time);
        String wday = String(wd[(int)time.wday][0]) + String(wd[(int)time.wday][1]) + String(wd[(int)time.wday][2]);
        String _t = String(time.hour) + ":" + time.minute + ":" + time.second;
        uint16_t color = whiteColour;
        if (time.wday == 0 || time.wday == 6) color = redColour;
        gText(142, 8, wday, color, 3);
        gText(320 - _t.length() * 16, 8, _t, whiteColour, 3);
        delay(1000);
      }
    } else {
      if (!timeInterrupt) {
        _updateTime(&time);
        String date = String(time.month) + "/" + time.day + "/" + time.year;
        gText(320 - date.length() * 16, 8, date, whiteColour, 3);
        delay(1000);
      }
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

void dateSet() {
  setTimeInterrupt(true);
  setIdleInterrupt(true);
  button timeSetOK;
  imageButton monthUp, dayUp, yearUp, monthDown, dayDown, yearDown;
  
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
  ds_ui_init(str, monthUp, dayUp, yearUp, monthDown, dayDown, yearDown, timeSetOK);
  
  while(1) {
    if (timeSetOK.isPressed()) {
      debug("Date set " + String(str));
      RTC.SetDate(day, mon, year%100, (uint8_t)(year / 100));
      setTimeInterrupt(false);
      setIdleInterrupt(false);
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

void ds_ui_init(char* str, imageButton mu, imageButton du, imageButton yu, imageButton md, imageButton dd, imageButton yd, button ts) {
  myScreen.clear(whiteColour);
  dsts_ui_init(56, "Today's date?");
  mu.draw();  du.draw();  yu.draw();
  drawTimeDateSetting(64, str);
  md.draw();  dd.draw();  yd.draw();
  ts.draw(true);
}

void timeSet() {
  setTimeInterrupt(true);
  setIdleInterrupt(true);
  button timeSetOK;
  imageButton dayUp, hourUp, minUp, secUp, dayDown, hourDown, minDown, secDown;
  item timeSetOK_i, up_i, down_i;
  
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
  ts_ui_init(str, dayUp, hourUp, minUp, secUp, dayDown, hourDown, minDown, secDown, timeSetOK);
  
  while(1) {
    if (timeSetOK.isPressed()) {
      debug("Time set " + String(str));
      RTC.SetTime(hour, mins, secs, day);
      setTimeInterrupt(false);
      setIdleInterrupt(false);
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

void ts_ui_init(char* str, imageButton du, imageButton hu, imageButton mu, imageButton su, imageButton dd, imageButton hd, imageButton md, imageButton sd, button ts) {
  myScreen.clear(whiteColour);
  dsts_ui_init(8, "How about the time?");
  du.draw();  hu.draw();  mu.draw();  su.draw();
  dd.draw();  hd.draw();  md.draw();  sd.draw();
  drawTimeDateSetting(40, str);
  ts.draw(true);
}

void dsts_ui_init(uint8_t x, String text) {
  myScreen.clear(whiteColour);
  gText(x, 51, text, grayColour, 3);
  gText(x-1, 50, text, blueColour, 3);
}

void drawTimeDateSetting(uint8_t x, char* str) {
  myScreen.rectangle(0, 124, 320, 148, whiteColour);
  gText(x, 124, str, blackColour, 3);
}

/************************************************************************************/
/*                                  Error Messages                                  */
/************************************************************************************/
void newPairMessage(String s) {
  gText(10, 44, "Pairing new " + s + "...", whiteColour, 3);
  gText(10, 70, "Name: ", whiteColour, 3);
  gText(210, 82, "Max " + String(MAXNAMELENGTH) + " letters", whiteColour, 1);
}

void addErrorMessage(String s) {
  myScreen.drawImage(g_10102BKImage, 10, 102);
  gText(10, 102, s, redColour, 1);
}

void maxNumberError(String s) {
  uiBackground();
  gText(8, 60, "Cannot add " + s + "...", redColour, 3);
  gText((320 - ((13+s.length()) * 12)) >> 1, 90, "Max " + s + " achieved", redColour, 2);
  delay(3000);
}

/**********************************************************************************/
/*                                  Initializing                                  */
/**********************************************************************************/
boolean firstInit() {
  RTC.begin();
  KB.begin(&myScreen);
  dateSet();
  deviceTimeInit();
  job.init(&myScreen);
  Scheduler.startLoop(updateTime);
  Scheduler.startLoop(timeModeToggle);
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

void deviceTimeInit() {
  timeSet();
  updateTimeMode = TIMEMODE;
  updateTimeModeToggle.dDefine(&myScreen, 140, 1, 180, 39, setItem(233, "TOGGLE"), whiteColour, blackColour);
  updateTimeModeToggle.enable();
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
  gText(106, 70, s, whiteColour, 3);
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
  if (!_idle && !_idleInterrupt && ++_timer == 1000) {
    debug("Screen enters idle");
    _idle = true;
    drawIdle();    
    setTimeInterrupt(true); 
//    myScreen.setBacklight(true);
  }
  if (myScreen.isTouch()) {
    _timer = 0;
    if (_idle) {
      debug("Screen leaves idle");
      _idle = false;
      setTimeInterrupt(false);
//      myScreen.setBacklight(false);
    }
  }  
}

void drawIdle() {
  myScreen.drawImage(g_idleImage, 0, 0);
}

void gText(uint16_t x, uint16_t y, String s, uint16_t c, uint8_t size) {
  myScreen.setFontSize(size);
  myScreen.gText(x, y, s, true, c); 
}

void timeModeToggle() {
  delay(5000);
  updateTimeMode = 2;
  delay(5000);
  updateTimeMode = 1;
}

void setTimeInterrupt(boolean flag) {
  timeInterrupt = flag;
}

void setIdleInterrupt(boolean flag) {
  _idleInterrupt = flag;
}
