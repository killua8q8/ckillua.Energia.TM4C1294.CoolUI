#include <Energia.h>

#define HOME true
#define RETURN false
#define NEWFAN 0x01
#define NEWVENT 0x02
#define NEWBLIND 0x03
#define MAXROOMSIZE 4
#define MAXCHILDSIZE 5

#define INITKB initKeyboard(&left, &right, &row_1, &row_2, &row_3, &row_4, keyRow_1, keyRow_2, keyRow_3, keyRow_4, keyRow_1_i, keyRow_2_i, keyRow_3_i, keyRow_4_i)
#define GETKEY getKey(left, right, row_1, row_2, row_3, row_4, keyRow_1, keyRow_2, keyRow_3, keyRow_4, keyRow_1_i, keyRow_2_i, keyRow_3_i, keyRow_4_i)

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
extern const uint8_t g_keyboardImage[];
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
  uint8_t parent;
  uint8_t node;
  uint8_t msg[58];
};

typedef struct childStruct {
  String name;
  child_t type;
  childButton button;
  uint8_t node;
} childStruct;

typedef struct roomStruct {
  String name;
  roomButton button;
  uint8_t childSize;
  uint8_t v = 0,f = 0,b = 0;
  childStruct childList[MAXCHILDSIZE];
  uint8_t node;
  control_t type;
} roomStruct;

int xy[6][2] = {
  {28, 60}, {128, 60}, {228, 60}, {28, 156}, {128, 156}, {228, 156}
};

char keyOrder[4][11] = {
  {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0x08},
  {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '\''},
  {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '(', ')'},
  {'Z', 'X', 'C', 'V', 'B', 'N', 'M', '.', '_', '-', ' '}
};


