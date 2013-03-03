#include <unistd.h>
#include <klib.h>
#include <services.h>
#include <train.h>
#include <ts7200.h>

#define DELAY_REVERSE 200

inline void setTrainSpeed(int train_id, int speed, int com1_tid) {
	char cmd[2];
	cmd[0] = speed;
	cmd[1] = train_id;
	Puts(com1_tid, cmd, 2);
}

inline int getDistance(track_node *track_nodes, int start, int end, int depth) {

}

void trainReverser(int train_id, int new_speed, int com1_tid) {
	char cmd[2];
	cmd[0] = 0;
	cmd[1] = train_id;
	Puts(com1_tid, cmd, 2);

	// Delay 2s then turn off the solenoid
	Delay(DELAY_REVERSE);

	// Reverse
	cmd[0] = TRAIN_REVERSE;
	Puts(com1_tid, cmd, 2);

	// Re-accelerate
	cmd[0] = new_speed;
	Puts(com1_tid, cmd, 2);
}

void trainDelayReaccelerator(int train_id, int new_speed, int com1_tid) {
	Delay(300);
	setTrainSpeed(train_id, new_speed, com1_tid);
}

void trainDriver(TrainGlobal *train_global, TrainProperties *train_properties) {
	track_node *track_nodes = train_global->track_nodes;
	int tid, result;
	int train_id = train_properties->id;
	int speed = 0;

	int com1_tid = train_global->com1_tid;
	int com2_tid = train_global->com2_tid;

	TrainMsg msg;
	char str_buf[1024];

	// Initialize trian speed
	setTrainSpeed(train_id, speed, com1_tid);

	int time1 = 0;
	int time2 = 0;
	int time3 = 0;
	int s1 = 0;
	int s2 = 0;
	int v = 0;

	while(1) {
		result = Receive(&tid, (char *)(&msg), sizeof(TrainMsg));
		assert(result >= 0, "TrainDriver receive failed");

		switch (msg.type) {
			case CMD_SPEED:
				Reply(tid, NULL, 0);
				speed = msg.cmd_msg.value;
				setTrainSpeed(train_id, speed, com1_tid);
				break;
			case CMD_REVERSE:
				Reply(tid, NULL, 0);
				CreateWithArgs(2, trainReverser, train_id, speed, com1_tid, 0);
				break;
			case LOCATION_CHANGE:
				Reply(tid, NULL, 0);
				if(msg.location_msg.value) {
					// sprintf(str_buf, "T#%d -> %s\n", train_id, track_nodes[msg.location_msg.id].name);
					// Puts(com2_tid, str_buf, 0);
					// if(msg.location_msg.id == 71) {
					// 	speed = 0;
					// 	setTrainSpeed(train_id, speed, com1_tid);
					// 	// CreateWithArgs(2, trainDelayReaccelerator, train_id, speed++, com1_tid, 0);
					// }
					// else if(msg.location_msg.id == 1) {
					// 	// speed = 0;
					// 	setTrainSpeed(train_id, TRAIN_REVERSE, com1_tid);
					// }


					if(msg.location_msg.id == 0) {
						time1 = getTimerValue(TIMER3_BASE);
						// CreateWithArgs(2, trainDelayReaccelerator, train_id, speed++, com1_tid, 0);
					}
					else if(msg.location_msg.id == 1) {
						setTrainSpeed(train_id, TRAIN_REVERSE, com1_tid);
						// CreateWithArgs(2, trainDelayReaccelerator, train_id, speed++, com1_tid, 0);
					}
					else if(msg.location_msg.id == 44) {
						s1 = speed;
						speed = 26;
						setTrainSpeed(train_id, speed, com1_tid);
						time2 = getTimerValue(TIMER3_BASE);
						unsigned int d = 469 << 14;
						v = d / (time1 - time2);
						// CreateWithArgs(2, trainDelayReaccelerator, train_id, speed++, com1_tid, 0);
					}
					else if(msg.location_msg.id == 70) {
						time3 = getTimerValue(TIMER3_BASE);
						s2 = speed;
						int v1 = train_properties->velocities[s1];
						int v2 = train_properties->velocities[s2];
						unsigned int diff = time2 - time3;
						unsigned int d = 780 << 14;
						unsigned int t1 = (diff * v2 - d) / (v2 - v1);
						unsigned int perdict_dist = (t1 * v1) + (diff - t1) * v2;
						// int a = (d - (time2 - time3) * v) / ((time2 - time3) * (time2 - time3)) * 2;
						// bwprintf(COM2, "d: %u v: %d a: %u t1: %u t2: %u dt: %d\n", d, v, a, time2, time3, time2 - time3);
						sprintf(str_buf, "d: %u v: %d t1: %u pd: %u\n", d, v, t1, perdict_dist);
						setTrainSpeed(train_id, TRAIN_REVERSE, com1_tid);
						Puts(com2_tid, str_buf, 0);
						Delay(150);
						setTrainSpeed(train_id, speed, com1_tid);
					}


				}
				break;
			default:
				sprintf(str_buf, "Driver got unknown msg, type: %d\n", msg.type);
				Puts(com2_tid, str_buf, 0);
				break;
		}

		str_buf[0] = '\0';
	}
}

