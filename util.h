#include <Energia.h>

#define HOME true
#define RETURN false
#define SENSOR PE_1
#define NEWFAN 0x01
#define NEWVENT 0x02
#define NEWBLIND 0x03
#define MAXROOMSIZE 4
#define MAXCHILDSIZE 5
#define MAXNAMELENGTH 6

extern const uint8_t g_logoImage[];
extern const uint8_t g_room[];
extern const uint8_t g_downImage[];
extern const uint8_t g_upImage[];
extern const uint8_t g_bgImage[];
extern const uint8_t g_optionImage[];
extern const uint8_t g_timebarImage[];
//extern const uint8_t g_addImage[];
extern const uint8_t g_setTimeImage[];
extern const uint8_t g_pairSlaveImage[];
extern const uint8_t g_pairChildImage[];
extern const uint8_t g_pairFanImage[];
extern const uint8_t g_pairVentImage[];
extern const uint8_t g_pairBlindImage[];
extern const uint8_t g_returnImage[];
extern const uint8_t g_nextImage[];
extern const uint8_t g_9624WhiteImage[];
extern const uint8_t g_fanImage[];
extern const uint8_t g_blindImage[];
extern const uint8_t g_ventImage[];
extern const uint8_t g_infoImage[];
extern const uint8_t g_updateImage[];
extern const uint8_t g_removeImage[];
extern const uint8_t g_onImage[];
extern const uint8_t g_offImage[]; 

const String nameEmpty = "Name cannot be empty";
const String roomNameRepeat = "Room name existed";
const String childNameRepeat = "Child name existed";
const String timeout = "Timeout, please retry";
const String retry = "Error, please retry";
const String outOfLimit = " reaches max limit";

typedef enum {
  VENT = 0x60,
  FAN = 0x70,
  BLIND = 0x80
} child_t;

typedef enum {
  MASTER, SLAVE
} control_t;

// RF packet struct
struct sPacket
{
  uint8_t upper, lower;
  uint8_t parent;
  uint8_t node;
  uint8_t msg[56];
};

typedef struct childStruct {
  String name;
  uint8_t node;
  child_t type;  
  childButton button;
} childStruct;

typedef struct roomStruct {
  String name;
  uint8_t node;
  control_t type;
  roomButton button;
  uint8_t childSize;
  uint8_t v = 0,f = 0,b = 0;
  int16_t roomTemp;
  childStruct childList[MAXCHILDSIZE];
} roomStruct;

int xy[6][2] = {
  {28, 60}, {128, 60}, {228, 60}, {28, 156}, {128, 156}, {228, 156}
};


