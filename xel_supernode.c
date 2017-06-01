/*
* Copyright 2017 sprocket
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*/

#define SUPERNODE_VERSION "0.1"

#ifdef WIN32
#define  _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include "miner.h"

#ifndef WIN32
# include <errno.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
#else
#include <ws2tcpip.h>
# include <winsock2.h>
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif

#ifndef in_addr_t
#define in_addr_t uint32_t
#endif

#define MAX_QUEUED_CONNECTIONS 1
#define SOCKET_BUF_SIZE 4096
#define MAX_BUF_SIZE 100000 // Need To Finalize This

#ifdef WIN32
	SOCKET s, core;
#else
	long s, core;
#endif

struct request {
	uint32_t req_id;
	uint32_t req_type;
	char *source;
	uint64_t work_id;
	uint32_t input[12];
	uint32_t state[32];
	bool success;
	char *err_msg;
};

pthread_mutex_t response_lock = PTHREAD_MUTEX_INITIALIZER;


extern void *supernode_thread(void *userdata) {
	struct thr_info *mythr = (struct thr_info*)userdata;
	char ip[] = "127.0.0.1";	// For Now All Connections Are On LocalHost
	unsigned short port = 4016;
	char buf[MAX_BUF_SIZE], msg[256], *str = NULL;
	int i, n, len;
	struct sockaddr_in server = { 0 };
	struct sockaddr_in client = { 0 };
	json_t *val;
	json_error_t err;
	uint32_t idx, req_id, req_type;

	applog(LOG_NOTICE, "Starting SuperNode...");

	// Initialize The WebSocket
#ifdef WIN32
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		applog(LOG_ERR, "ERROR: Unable to initialize SuperNode socket (%s)", strerror(errno));
		goto out;
	}
#else
	// Need Linux init logic
#endif

	// Create Socket For Core Server To Connect To
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) {
		applog(LOG_ERR, "ERROR: Unable to create SuperNode socket (%s)", strerror(errno));
		goto out;
	}

	// Prepare Socket Address For SuperNode
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr(ip);
	server.sin_port = htons(port);
	if (server.sin_addr.s_addr == (in_addr_t)INADDR_NONE) {
		applog(LOG_ERR, "ERROR: Unable to configure SuperNode IP address (%s)", strerror(errno));
		goto out;
	}

	// Bind Socket To SuperNode Port (6 Tries Max)
	applog(LOG_DEBUG, "DEBUG: Binding Socket to port %d", port);
	for (i = 0; i < 10; i++) {
		n = bind(s, (struct sockaddr *)(&server), sizeof(server));
		if (n >= 0)
			break;
		sleep(10);
	}

	if (n < 0) {
		applog(LOG_ERR, "ERROR: Unable to bind Socket to port $d (%s)", port, strerror(errno));
		goto out;
	}

	// Listen For Incoming Connections
	if (listen(s, MAX_QUEUED_CONNECTIONS) < 0) {
		applog(LOG_ERR, "ERROR: Unable to set Queue for SuperNode socket (%s)", strerror(errno));
		goto out;
	}


	// Main Loop To Reconnect If Connection Is Lost
	while (1) {

		// Accept Connection From Core Server
		len = sizeof(struct sockaddr_in);
		core = accept(s, (struct sockaddr *)&client, &len);
		if (core < 0) {
			applog(LOG_ERR, "ERROR: Unable to accept connection to SuperNode (%s)", strerror(errno));
			goto out;
		}

		if (client.sin_addr.S_un.S_addr == server.sin_addr.S_un.S_addr)
			applog(LOG_NOTICE, "SuperNode connected to Elastic Core Server");
		else {
			applog(LOG_ERR, "ERROR: SuperNode blocked connection from IP: %s", inet_ntoa(client.sin_addr));
			continue;
		}

		// Secondary Loop To Read Requests From Core Server
		while (1) {

			// Read Request Into Buffer
			idx = 0;
			while (idx < MAX_BUF_SIZE) {
				n = recv(core, &buf[idx], SOCKET_BUF_SIZE, 0);
				if (n < SOCKET_BUF_SIZE)
					break;
				idx += SOCKET_BUF_SIZE;
			}

			// Reset If There Is A Connection Error
			if (n < 0)
				break;

			pthread_mutex_lock(&response_lock);

			// Make Sure Request Is Zero Terminated
			buf[idx + n] = '\0';

			// Get Request ID / Type From JSON
			val = JSON_LOADS(buf, &err);
			if (!val) {
				applog(LOG_ERR, "ERROR: JSON decode failed (code=%d): %s", err.line, err.text);

				// Send Acknowledgment To Core Server (Error)
				n = send(core, "ERROR", 6, 0);
				pthread_mutex_unlock(&response_lock);
				if (n <= 0)	break;
			}
			else {
				// Send Acknowledgment To Core Server (OK)
				n = send(core, "OK", 3, 0);
				pthread_mutex_unlock(&response_lock);
				if (n <= 0)	break;

				// Dump JSON For Debugging
				if (opt_protocol) {
					str = json_dumps(val, JSON_INDENT(3));
					applog(LOG_DEBUG, "DEBUG: JSON SuperNode Request -\n%s", str);
					free(str);
					str = NULL;
				}

				// Get Request ID / Type
				req_id = (uint32_t)json_integer_value(json_object_get(val, "req_id"));
				req_type = (uint32_t)json_integer_value(json_object_get(val, "req_type"));

				// Check For Valid Request Type
				if (!req_type || (req_type > 4)) {
					applog(LOG_DEBUG, "DEBUG: Invalid request type - Req_Id: %d, Req_Type: %d", req_id, req_type);
					sleep(1);
					sprintf(msg, "{\"req_id\": %lu,\"req_type\": %lu,\"success\": %d,\"error\": \"%s\"}", req_id, req_type, 0, "Invalid Request Type");
					pthread_mutex_lock(&response_lock);
					send(core, msg, strlen(msg), 0);
					pthread_mutex_unlock(&response_lock);
					continue;
				}
			}

			// Create A New Request
			struct request req;
			req.req_id = req_id;
			req.req_type = req_type;

			// Validate ElasticPL Syntax 
			if (req_type == 2) { 

				// Get Encoded Source
				str = (char *)json_string_value(json_object_get(val, "source"));
				if (str)
					req.source = strdup(str);
				else
					req.source = NULL;

				// Push Request To Validate ElasticPL Queue
				tq_push(thr_info[1].q, &req);
			}

			// Validate POW / Bounty Solutions 
			else if ((req_type == 3) || (req_type == 4)) {

				// Get Work ID
				str = (char *)json_string_value(json_object_get(val, "work_id"));
				if (str)
					req.work_id = strtoull(str, NULL, 10);
				else
					req.work_id = 0;

// TODO - Logic To Load Inputs / State

				// Push Request To Validate Results Queue
				tq_push(thr_info[2].q, &req);
			}

			if (val)
				json_decref(val);
		}

#ifdef WIN32
		if (core != INVALID_SOCKET)
			closesocket(core);
#else
		if (core)
			close(core);
#endif

	}

out:

#ifdef WIN32
	if (s != INVALID_SOCKET)
		closesocket(s);
#else
	if (s)
		close(s);
#endif

	tq_freeze(mythr->q);
	return NULL;
}

extern void *sn_validate_elasticpl_thread(void *userdata) {
	struct thr_info *mythr = (struct thr_info*)userdata;
	struct request *req;
	char msg[512], err_msg[256];
	char *elastic_src = NULL;
	int rc, success;

	elastic_src = malloc(MAX_SOURCE_SIZE);
	if (!elastic_src) {
		applog(LOG_ERR, "ERROR: Unable to allocate memory for ElasticPL Source");
		return NULL;
	}

	while (1) {

		success = 1;
		err_msg[0] = '\0';

		// Wait For New Requests On Queue
		req = (struct request *) tq_pop(mythr->q, NULL);
		applog(LOG_DEBUG, "Validating ElasticPL (req_id: %d)", req->req_id);

		// Check For Missing Source
		if (!req->source) {
			success = 0;
			sprintf(err_msg, "Source data is NULL");
			goto send;
		}

		// Decode Source To ElasticPL
		rc = ascii85dec(elastic_src, MAX_SOURCE_SIZE, req->source);
		if (!rc) {
			success = 0;
			sprintf(err_msg, "Unable to decode source");
			goto send;
		}

		// Validate ElasticPL For Syntax Errors
		if (!create_epl_vm(elastic_src, NULL)) {
			success = 0;
			sprintf(err_msg, "Syntax Error");  // TODO - Get Error Message From Parser
			goto send;
		}

		// Calculate WCET
		if (!calc_wcet()) {
			success = 0;
			sprintf(err_msg, "WCET Error");  // TODO - Get Error Message From Parser
			goto send;
		}

send:
		// Create Response
		sprintf(msg, "{\"req_id\": %lu,\"req_type\": %lu,\"success\": %d,\"error\": \"%s\"}", req->req_id, req->req_type, success, err_msg);

		// Send Response
		pthread_mutex_lock(&response_lock);
		send(core, msg, strlen(msg), 0);
		pthread_mutex_unlock(&response_lock);

		if (req->source)
			free(req->source);
	}

	if (elastic_src)
		free(elastic_src);
	tq_freeze(mythr->q);
	return NULL;
}

extern void *sn_validate_result_thread(void *userdata) {
	struct thr_info *mythr = (struct thr_info*)userdata;
	struct request *req;
	char msg[512], err_msg[256];
	int success;

	while (1) {

		success = 1;
		err_msg[0] = '\0';

		// Wait For New Requests On Queue
		req = (struct request *) tq_pop(mythr->q, NULL);
		applog(LOG_DEBUG, "Validating result for work_id %llu (req_id: %d)", req->work_id, req->req_id);

		if (!req->work_id) {
			success = 0;
			sprintf(err_msg, "Invalid work_id");
			goto send;
		}
		
		// TODO - Add Logic To Call DLL Main / Verify Functions
//		if () {
			success = 0;
			sprintf(err_msg, "Logic Not Implemented Yet");
			goto send;
//		}

	send:
		// Create Response
		sprintf(msg, "{\"req_id\": %lu,\"req_type\": %lu,\"success\": %d,\"error\": \"%s\"}", req->req_id, req->req_type, success, err_msg);

		// Send Response
		pthread_mutex_lock(&response_lock);
		send(core, msg, strlen(msg), 0);
		pthread_mutex_unlock(&response_lock);
	}

	tq_freeze(mythr->q);
	return NULL;
}
