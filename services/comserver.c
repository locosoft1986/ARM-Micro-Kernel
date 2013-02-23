#include <services.h>
#include <klib.h>
#include <unistd.h>

#define COM_BUFFER_SIZE			1000
#define COM_NOTIFIER_PRIORITY	1

#define IO_QUERY_TYPE_GETC		0
#define IO_QUERY_TYPE_PUTC		1
#define IO_QUERY_TYPE_PUTS		2

#define COM1_REG_NAME 			"C1"
#define COM2_REG_NAME 			"C2"
#define COM_NAME_ARRAY_LEN		3

#define GETTER_BUFFER_SIZE		10

typedef struct putc_query {
	int type;
	char ch;
} PutcQuery;

typedef struct getc_query {
	int type;
} GetcQuery;

typedef struct puts_query {
	int type;
	char *msg;
	int msglen;
} PutsQuery;

typedef union {
	int 		type;
	char		ch;
	PutcQuery	putcQuery;
	PutsQuery	putsQuery;
	GetcQuery	getcQuery;
} IOMsg;

typedef struct server_info {
	int server_tid;
	int channel_id;
} ServerInfo;

ServerInfo receiveChannelInfo() {
	int server_tid = -1;
	int channel_id = -1;
	int result = Receive(&server_tid, (char *)(&channel_id), sizeof(int));
	assert(result > 0, "Notifier receive channel info failed");
	assert(channel_id == COM1 || channel_id == COM2, "Notifier received invalid channel id");
	Reply(server_tid, NULL, 0);

	ServerInfo serverInfo;
	serverInfo.server_tid = server_tid;
	serverInfo.channel_id = channel_id;

	return serverInfo;
}

void comSendNotifier() {
	ServerInfo serverInfo = receiveChannelInfo();
	unsigned int eventId = (serverInfo.channel_id == COM1 ? EVENT_COM1_TX :
															EVENT_COM2_TX);
	char ch = '\0';
	char empty = '\0';
	int result = -1;

	while (1) {
		result = Send(serverInfo.server_tid, &empty, sizeof(char), &ch, sizeof(char));
		assert(result == sizeof(char), "Send Notifier got invalid reply size");

		result = AwaitEvent(eventId , &ch, sizeof(char));
		assert(result == 0, "Send Notifier wait event failed");
	}
}

void comReceiveNotifier() {
	ServerInfo serverInfo = receiveChannelInfo();
	unsigned int eventId = (serverInfo.channel_id == COM1 ? EVENT_COM1_RX :
															EVENT_COM2_RX);

	char ch = '\0';
	int result = -1;

	while (1) {
		result = AwaitEvent(eventId , &ch, sizeof(char));
		assert(result == 0, "Receive Notifier wait event failed");

		result = Send(serverInfo.server_tid, &ch, sizeof(char), NULL, 0);
		assert(result == 0, "Receive Notifier sent to server failed");
	}
}

int serverTidForChannel(int channel_id) {
	char server_name[COM_NAME_ARRAY_LEN] = "";
	if(channel_id == COM1) strncpy(server_name, COM1_REG_NAME, COM_NAME_ARRAY_LEN);
	else if(channel_id == COM2) strncpy(server_name, COM2_REG_NAME, COM_NAME_ARRAY_LEN);
	assert(strlen(server_name) != 0, "Invalid COM server name");

	int server_tid = WhoIs(server_name);
	assert(server_tid >= 0, "Get COM server tid failed");

	return server_tid;
}

int Getc(int channel) {
	int server_tid = serverTidForChannel(channel);

	int reply;
	GetcQuery getc_query;
	getc_query.type = IO_QUERY_TYPE_GETC;

	int rtn = Send(server_tid, (char *)(&getc_query), sizeof(GetcQuery), (char *)(&reply), sizeof(int));

	return rtn < 0 ? rtn : reply;
}

int Putc(int channel, char ch) {
	int server_tid = serverTidForChannel(channel);

	PutcQuery putc_query;
	putc_query.type = IO_QUERY_TYPE_PUTC;
	putc_query.ch = ch;

	int rtn = Send(server_tid, (char *)(&putc_query), sizeof(PutcQuery), NULL, 0);

	return rtn;
}

int Puts(int channel, char *msg, int msglen) {
	int server_tid = serverTidForChannel(channel);

	PutsQuery puts_query;
	puts_query.type = IO_QUERY_TYPE_PUTS;
	puts_query.msg = msg;
	if (msglen == 0) {
		puts_query.msglen = strlen(msg);
	} else {
		puts_query.msglen = msglen;
	}
	
	int rtn = Send(server_tid, (char *)(&puts_query), sizeof(PutsQuery), NULL, 0);
	return rtn;
}

void comserver() {
	// Receive serving channel id
	int tid;
	int channel_id = -1;
	Receive(&tid, (char *)(&channel_id), sizeof(int));
	Reply(tid, NULL, 0);

	// Define server_name and register
	char server_name[COM_NAME_ARRAY_LEN] = "";
	if(channel_id == COM1) strncpy(server_name, COM1_REG_NAME, COM_NAME_ARRAY_LEN);
	else if(channel_id == COM2) strncpy(server_name, COM2_REG_NAME, COM_NAME_ARRAY_LEN);
	assert(strlen(server_name) != 0, "Invalid COM server name");
	assert(RegisterAs(server_name) == 0, "COM server register failed");

	// Create send and receive buffer
	CircularBuffer send_buffer;
	CircularBuffer receive_buffer;
	char send_array[COM_BUFFER_SIZE];
	char receive_array[COM_BUFFER_SIZE];

	bufferInitial(&send_buffer, CHARS, send_array, COM_BUFFER_SIZE);
	bufferInitial(&receive_buffer, CHARS, receive_array, COM_BUFFER_SIZE);

	// Create send and receive notifier
	int send_notifier_tid;
	int receive_notifier_tid;
	send_notifier_tid = Create(COM_NOTIFIER_PRIORITY, comSendNotifier);
	receive_notifier_tid = Create(COM_NOTIFIER_PRIORITY, comReceiveNotifier);

	// Send server info to notifiers
	Send(send_notifier_tid, (char *)(&channel_id), sizeof(int), NULL, 0);
	Send(receive_notifier_tid, (char *)(&channel_id), sizeof(int), NULL, 0);

	// Prepare message and reply data structure
	IOMsg message;
	int reply;
	char ch;
	
	// Create getter buffer
	CircularBuffer getter_buffer;
	int getter_array[GETTER_BUFFER_SIZE];
	
	bufferInitial(&getter_buffer, INTS, getter_array, GETTER_BUFFER_SIZE);
	
	// Prepare flag variables
	int send_notifier_is_waiting = 0;
	int char_getter_tid = 0;

	while(1) {
		/* Receive and process a message */
		Receive(&tid, (char *)(&message), sizeof(IOMsg));

		// If is from send notifier, set send_notifier_is_waiting
		if (tid == send_notifier_tid) {
			send_notifier_is_waiting = 1;
		}
		// Or is from receive notifier,
		else if (tid == receive_notifier_tid) {
			// Reply FIRST, then push the char to receive_buffer
			Reply(tid, NULL, 0);
			bufferPushChar(&receive_buffer, message.ch);
			// If is from COM2, echo it
			if(channel_id == COM2) {
				bufferPushChar(&send_buffer, message.ch);
				if (message.ch == '\b') {
					bufferPushChar(&send_buffer, '\e');
					bufferPushChar(&send_buffer, '[');
					bufferPushChar(&send_buffer, 'K');
				}
			}
		}
		// Or is a getc msg, set char_getter_is_waiting and save its tid
		else if (message.type == IO_QUERY_TYPE_GETC) {
			bufferPush(&getter_buffer, tid);
		}
		// Or is a putc msg,
		else if (message.type == IO_QUERY_TYPE_PUTC) {
			// Push the char to send_buffer, then reply
			bufferPushChar(&send_buffer, message.putcQuery.ch);
			Reply(tid, NULL, 0);
		}
		// Or is a puts msg,
		else if (message.type == IO_QUERY_TYPE_PUTS) {
			// copy the whole string into send_buffer, then reply
			int i = 0;
			for (i = 0; i < message.putsQuery.msglen; i++) {
				bufferPushChar(&send_buffer, message.putsQuery.msg[i]);
			}
			Reply(tid, NULL, 0);
		}
		/* Serve waiting getter/send notifier */
		if (send_notifier_is_waiting && send_buffer.current_size > 0) {
			ch = (char) bufferPop(&send_buffer);
			send_notifier_is_waiting = 0;
			Reply(send_notifier_tid, &ch, sizeof(char));
		}
		if (getter_buffer.current_size > 0 && receive_buffer.current_size > 0) {
			reply = bufferPop(&receive_buffer);
			Reply(bufferPop(&getter_buffer), (char *)(&reply), sizeof(int));
		}
	}
}