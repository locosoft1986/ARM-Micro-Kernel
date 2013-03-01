#ifndef __TRAIN_H__
#define __TRAIN_H__

/* Train Server */
#include <intern/track_data.h>

#define SWITCH_STR 33
#define SWITCH_CUR 34
#define SWITCH_OFF 32

#define SENSOR_DECODER_TOTAL 5
#define SENSOR_BYTES_TOTAL 10
#define SENSOR_BYTE_SIZE 8
#define SENSOR_BIT_MASK 0x1

typedef struct train_global {
	track_node	*track_nodes;
} TrainGlobal;

typedef enum train_msg_type {
	CMD_SPEED = 0,
	CMD_REVERSE,
	CMD_SWITCH,
	CMD_QUIT,
	CMD_MAX,
	SENSOR_DATA,
	TRAIN_MSG_MAX,
} TrainMsgType;

typedef struct sensor_msg {
	TrainMsgType	type;
	char			sensor_data[SENSOR_BYTES_TOTAL];
} SensorMsg;

typedef struct cmd_msg {
	TrainMsgType	type;
	int				id;
	int				value;
} CmdMsg;

typedef union train_msg {
	TrainMsgType	type;
	SensorMsg		sensor_msg;
	CmdMsg			cmd_msg;
} TrainMsg;

void trainBootstrap();
void trainCentral();
void trainCmdNotifier();
void trainSensorNotifier();
void trainclockserver();

#endif // __TRAIN_H__
