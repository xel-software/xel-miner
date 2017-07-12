/*
* Copyright 2017 sprocket
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*/

#define VALIDATION_ENGINE_VERSION "0.1"

#ifdef WIN32
#define  _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include "miner.h"
#include <openssl/md5.h>

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
#define MAX_BUF_SIZE 100000		// TODO: Finalize Size

#ifdef WIN32
	SOCKET s, core;
#else
	long s, core;
#endif

struct request {
	uint32_t req_id;
	uint32_t req_type;
	char *package;
	uint32_t work_pkg_id;
	uint64_t work_id;
	uint32_t input[VM_M_ARRAY_SIZE];
	uint32_t target[8];
	uint32_t iteration_id;
	uint32_t storage_id;
	uint32_t storage_sz;
	uint32_t *storage;
	bool success;
	char *err_msg;
};

pthread_mutex_t response_lock = PTHREAD_MUTEX_INITIALIZER;

uint32_t g_ve_ints = 0;
uint32_t g_ve_uints = 0;
uint32_t g_ve_longs = 0;
uint32_t g_ve_ulongs = 0;
uint32_t g_ve_floats = 0;
uint32_t g_ve_doubles = 0;
uint32_t **g_ve_storage = NULL;
uint32_t **g_ve_storage_ptr = NULL;

bool g_upd_ve_ints = false;
bool g_upd_ve_uints = false;
bool g_upd_ve_longs = false;
bool g_upd_ve_ulongs = false;
bool g_upd_ve_floats = false;
bool g_upd_ve_doubles = false;

bool g_new_ve_library = false;
bool g_ve_library_loaded = false;

extern void *validation_engine_thread(void *userdata) {
	struct thr_info *mythr = (struct thr_info*)userdata;
	char ip[] = "127.0.0.1";	// For Now All Connections Are On LocalHost
	unsigned short port = 4016;
	char buf[MAX_BUF_SIZE], msg[256], *str = NULL;
	int i, n, len;
	struct sockaddr_in server = { 0 };
	struct sockaddr_in client = { 0 };
	json_t *val = NULL, *input = NULL, *state = NULL, *target = NULL;
	json_error_t err;
	uint32_t idx, req_id, req_type;

	applog(LOG_NOTICE, "Starting Validation Engine...");

	// Initialize The WebSocket
#ifdef WIN32
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		applog(LOG_ERR, "ERROR: Unable to initialize Validation socket (%s)", strerror(errno));
		goto out;
	}
#endif

	// Create Socket For Core Server To Connect To
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) {
		applog(LOG_ERR, "ERROR: Unable to create Validation socket (%s)", strerror(errno));
		goto out;
	}

	// Prepare Socket Address For Validation
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr(ip);
	server.sin_port = htons(port);
	if (server.sin_addr.s_addr == (in_addr_t)INADDR_NONE) {
		applog(LOG_ERR, "ERROR: Unable to configure Validation IP address (%s)", strerror(errno));
		goto out;
	}

#ifndef WIN32
	// On linux with SO_REUSEADDR, bind will get the port if the previous
	// socket is closed (even if it is still in TIME_WAIT) but fail if
	// another program has it open - which is what we want
	int optval = 1;
	// If it doesn't work, we don't really care - just show a debug message
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)(&optval), sizeof(optval)) <0)
		applog(LOG_DEBUG, "API setsockopt SO_REUSEADDR failed (ignored): %s", strerror(errno));
#endif

	// Bind Socket To Validation Port (6 Tries Max)
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
		applog(LOG_ERR, "ERROR: Unable to set Queue for Validation socket (%s)", strerror(errno));
		goto out;
	}
	
	// Main Loop To Reconnect If Connection Is Lost
	while (1) {

		// Accept Connection From Core Server
		len = sizeof(struct sockaddr_in);
		core = accept(s, (struct sockaddr *)&client, &len);
		if (core < 0) {
			applog(LOG_ERR, "ERROR: Unable to accept connection to Validation engine (%s)", strerror(errno));
			goto out;
		}

#ifdef WIN32
		if (client.sin_addr.S_un.S_addr == server.sin_addr.S_un.S_addr)
#else
		if (client.sin_addr.s_addr == server.sin_addr.s_addr)
#endif
			applog(LOG_NOTICE, "Validation Engine connected to Elastic Core Server");
		else {
			applog(LOG_ERR, "ERROR: Validation Engine blocked connection from IP: %s", inet_ntoa(client.sin_addr));
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
			if (n <= 0) break;

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
					applog(LOG_DEBUG, "DEBUG: JSON Validation Request -\n%s", str);
					free(str);
					str = NULL;
				}

				// Get Request ID / Type
				req_id = (uint32_t)json_integer_value(json_object_get(val, "req_id"));
				req_type = (uint32_t)json_integer_value(json_object_get(val, "req_type"));

				// Check For Valid Request Type
				if (!req_type || (req_type > 5)) {
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
			req.work_pkg_id = -1;

			// Validate ElasticPL Syntax 
			if (req_type == 2) {

				applog(LOG_DEBUG, "DEBUG: Req_Id: %d, Req_Type: %d", req_id, req_type);

				// Copy Package To Be Validated
				req.package = strdup(buf);

				// Push Request To Validate ElasticPL Queue
				tq_push(thr_info[1].q, &req);
			}
			else {

				// Get Work ID
				str = (char *)json_string_value(json_object_get(val, "work_id"));
				if (str)
					req.work_id = strtoull(str, NULL, 10);
				else
					req.work_id = 0;

				// Check If Work Package Exists
				for (i = 0; i < g_work_package_cnt; i++) {
					if (req.work_id == g_work_package[i].work_id) {
						req.work_pkg_id = i;
						break;
					}
				}

				if (req.work_pkg_id < 0) {
					applog(LOG_DEBUG, "DEBUG: No Package exists for work_id: %llu - Req_Id: %d, Req_Type: %d", req.work_id, req_id, req_type);
					sleep(1);
					sprintf(msg, "{\"req_id\": %lu,\"req_type\": %lu,\"success\": %d,\"error\": \"%s\"}", req_id, req_type, 0, "Job not found");
					pthread_mutex_lock(&response_lock);
					send(core, msg, strlen(msg), 0);
					pthread_mutex_unlock(&response_lock);
					if (val) json_decref(val);
					continue;
				}

				// Get Iteration / Storage ID
				req.iteration_id = (uint32_t)json_integer_value(json_object_get(val, "iteration_id"));
				req.storage_id = (uint32_t)json_integer_value(json_object_get(val, "storage_id"));

				// Get Storage
				req.storage = NULL;
				req.storage_sz = 0;
				state = json_object_get(val, "state");
				if (state) {
					n = json_array_size(state);
					if (n) {
						req.storage = malloc(n * sizeof(uint32_t));
						req.storage_sz = n;
					}
				}

				// Push Request To Update Storage Queue
				if (req_type == 5) {
					tq_push(thr_info[3].q, &req);
					if (val) json_decref(val);
					continue;
				}

				// Get Inputs
				memset(&req.input[0], 0, VM_M_ARRAY_SIZE * sizeof(uint32_t));
				input = json_object_get(val, "input");
				n = json_array_size(input);

				if (n != VM_M_ARRAY_SIZE) {
					applog(LOG_DEBUG, "DEBUG: Invalid Inputs - Req_Id: %d, Req_Type: %d", req_id, req_type);
					sleep(1);
					sprintf(msg, "{\"req_id\": %lu,\"req_type\": %lu,\"success\": %d,\"error\": \"%s\"}", req_id, req_type, 0, "Invalid Inputs");
					pthread_mutex_lock(&response_lock);
					send(core, msg, strlen(msg), 0);
					pthread_mutex_unlock(&response_lock);
					if (val) json_decref(val);
					continue;
				}

				for (i = 0; i < n; i++) {
					req.input[i] = (uint32_t)json_integer_value(json_array_get(input, i));
				}

				// Get Target
				if (req_type == 3) {
					target = json_object_get(val, "target");
					n = json_array_size(target);

					if (n != 8) {
						applog(LOG_DEBUG, "DEBUG: Invalid Target - Req_Id: %d, Req_Type: %d", req_id, req_type);
						sleep(1);
						sprintf(msg, "{\"req_id\": %lu,\"req_type\": %lu,\"success\": %d,\"error\": \"%s\"}", req_id, req_type, 0, "Invalid Inputs");
						pthread_mutex_lock(&response_lock);
						send(core, msg, strlen(msg), 0);
						pthread_mutex_unlock(&response_lock);
						if (val) json_decref(val);
						continue;
					}

					for (i = 0; i < n; i++) {
						req.target[i] = (uint32_t)json_integer_value(json_array_get(target, i));
					}
				}

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

extern void *ve_validate_package_thread(void *userdata) {
	struct thr_info *mythr = (struct thr_info*)userdata;
	struct request *req;
	char msg[512], err_msg[256], *elastic_src;
	uint32_t success;
	json_t *val = NULL, *wrk = NULL, *pkg = NULL;
	json_error_t err;

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

		if (req->package)
			val = JSON_LOADS(req->package, &err);

		if (!req->package || !val) {
			success = 0;
			sprintf(err_msg, "Unable to read JSON Object");
			goto send;
		}

		// Get Package Data From JSON Object
		wrk = json_object_get(val, "work_packages");
		if (wrk)
			pkg = json_array_get(wrk, 0);

		if (!wrk || !pkg) {
			success = 0;
			sprintf(err_msg, "Unable to parse Package data");
			goto send;
		}

		// Validate Contents Of Package
		if (!ve_validate_package(pkg, elastic_src, err_msg)) {
			success = 0;
			goto send;
		}

send:
		if (!success)
			applog(LOG_DEBUG, "DEBUG: %s", err_msg);

		// Create Response
		sprintf(msg, "{\"req_id\": %lu,\"req_type\": %lu,\"success\": %d,\"error\": \"%s\"}", req->req_id, req->req_type, success, err_msg);

		// Send Response
		pthread_mutex_lock(&response_lock);
		send(core, msg, strlen(msg), 0);
		pthread_mutex_unlock(&response_lock);

		if (val)
			json_decref(val);

		//if (req->package)
		//	free(req->package);
	}

	if (elastic_src)
		free(elastic_src);

	tq_freeze(mythr->q);
	return NULL;
}

extern void *ve_validate_result_thread(void *userdata) {
	struct thr_info *mythr = (struct thr_info*)userdata;
	struct request *req;
	char msg[512], err_msg[256], pow_msg[64], hash[32];
	int i, rc, success;
	struct instance *inst = NULL;
	uint32_t *msg32 = (uint32_t *)pow_msg;
	uint32_t *hash32 = (uint32_t *)hash;

	if (!vm_m)
		vm_m = calloc(VM_M_ARRAY_SIZE, sizeof(uint32_t));

	if (!vm_s)
		vm_s = calloc(1, sizeof(uint32_t **));

	if (!vm_state)
		vm_state = calloc(4, sizeof(uint32_t));

	while (1) {

		success = 1;
		err_msg[0] = '\0';

		// Wait For New Requests On Queue
		req = (struct request *) tq_pop(mythr->q, NULL);
		applog(LOG_DEBUG, "Validating result for work_id %llu (req_id: %d)", req->work_id, req->req_id);

		
		if (!g_ve_library_loaded) {
			success = 0;
			sprintf(err_msg, "Validate Library Not Loaded");
			goto send;
		}

		if (!req->work_id) {
			success = 0;
			sprintf(err_msg, "Invalid work_id");
			goto send;
		}
		
		if (g_new_ve_library) {

			// Allocate Memory For Global Variables
			if (g_upd_ve_ints)
				vm_i = realloc(vm_i, g_ve_ints * sizeof(int32_t));
			if (g_upd_ve_uints)
				vm_u = realloc(vm_u, g_ve_uints * sizeof(uint32_t));
			if (g_upd_ve_longs)
				vm_l = realloc(vm_l, g_ve_longs * sizeof(int64_t));
			if (g_upd_ve_ulongs)
				vm_ul = realloc(vm_ul, g_ve_ulongs * sizeof(uint64_t));
			if (g_upd_ve_floats)
				vm_f = realloc(vm_f, g_ve_floats * sizeof(float));
			if (g_upd_ve_doubles)
				vm_d = realloc(vm_d, g_ve_doubles * sizeof(double));

			if ((g_ve_ints && !vm_i) ||
				(g_ve_uints && !vm_u) ||
				(g_ve_longs && !vm_l) ||
				(g_ve_ulongs && !vm_ul) ||
				(g_ve_floats && !vm_f) ||
				(g_ve_doubles && !vm_d)) {

				applog(LOG_ERR, "ERROR: Unable to allocate VM memory");

				tq_freeze(mythr->q);
				return NULL;
			}

			g_upd_ve_ints = false;
			g_upd_ve_uints = false;
			g_upd_ve_longs = false;
			g_upd_ve_ulongs = false;
			g_upd_ve_floats = false;
			g_upd_ve_doubles = false;

			// Free Old Instance Of Validation Libaray
			if (inst) free_library(inst);

			// Copy The Updated Validation Libary
#ifdef WIN32
			remove("./work/job_validate.dll");
			rename("./work/_validate.dll", "./work/job_validate.dll");
#else
			remove("./work/job_validate.so");
			rename("./work/_validate.so", "./work/job_validate.so");
#endif

			// Link Updated Validation Library
			inst = calloc(1, sizeof(struct instance));
			create_instance(inst, NULL);
			inst->initialize(vm_m, vm_i, vm_u, vm_l, vm_ul, vm_f, vm_d, vm_s);

			g_new_ve_library = false;
		}

		// Reset VM Memory
		if (g_ve_ints) memset(vm_i, 0, g_ve_ints * sizeof(int32_t));
		if (g_ve_uints) memset(vm_u, 0, g_ve_uints * sizeof(uint32_t));
		if (g_ve_longs) memset(vm_l, 0, g_ve_longs * sizeof(int64_t));
		if (g_ve_ulongs) memset(vm_ul, 0, g_ve_ulongs * sizeof(uint64_t));
		if (g_ve_floats) memset(vm_f, 0, g_ve_floats * sizeof(float));
		if (g_ve_doubles) memset(vm_d, 0, g_ve_doubles * sizeof(double));

		// Load Input Data
		for (i = 0; i < VM_M_ARRAY_SIZE; i++) {
			vm_m[i] = req->input[i];
		}

		// Point To Storage For This Work ID
		if (g_work_package[req->work_pkg_id].storage_sz && req->iteration_id) {
			if (g_work_package[req->work_pkg_id].storage[req->iteration_id][req->storage_id]) {
				*vm_s = &g_work_package[req->work_pkg_id].storage[req->iteration_id][req->storage_id];
			}
			else {
				success = 0;
				sprintf(err_msg, "Storage not found");
			}
		}

		// Validate Bounty
		if (req->req_type == 4) {
			rc = inst->execute(req->work_id);
			if (rc == 1) {
				success = 1;
				sprintf(err_msg, "");

				// Check If Storage Needs To be Updated
				if (req->storage) {
					if (!ve_update_storage(req->work_id, req->iteration_id, req->storage_id, req->storage, req->storage_sz, err_msg)) {
						success = 0;
						free(req->storage);
					}
				}
			}
			else if (rc == 0) {
				success = 0;
				sprintf(err_msg, "Invalid solution");
			}
			else {
				success = 0;
				sprintf(err_msg, "Job Not Found");
			}
		}

		// Validate POW
		else {
			rc = inst->execute(req->work_id);

// TODO - Need New Logic For vm_state

			// Check For POW Result
			memcpy(&pow_msg[0], &vm_state[0], 16);
			msg32[0] = swap32(msg32[0]);
			msg32[1] = swap32(msg32[1]);
			msg32[2] = swap32(msg32[2]);
			msg32[3] = swap32(msg32[3]);

			for (i = 0; i < VM_M_ARRAY_SIZE; i++)
				msg32[i + 4] = swap32(req->input[i]);

			MD5(pow_msg, 64, hash);

			// Check For POW Solution
			for (i = 0; i < 4; i++) {

				hash32[i] = swap32(hash32[i]);

				if (hash32[i] > req->target[i]) {
					success = 0;
					sprintf(err_msg, "Hash above target");
					goto send;
				}
				else if (hash32[i] < req->target[i]) {
					break;
				}
			}
		}

	send:
		// Create Response
		sprintf(msg, "{\"req_id\": %lu,\"req_type\": %lu,\"success\": %d,\"error\": \"%s\"}", req->req_id, req->req_type, success, err_msg);

		// Send Response
		pthread_mutex_lock(&response_lock);
		send(core, msg, strlen(msg), 0);
		pthread_mutex_unlock(&response_lock);
	}

	if (inst) free(inst);
	if (vm_m) free(vm_m);
	if (vm_i) free(vm_i);
	if (vm_u) free(vm_u);
	if (vm_l) free(vm_l);
	if (vm_ul) free(vm_ul);
	if (vm_f) free(vm_f);
	if (vm_d) free(vm_d);
	if (vm_s) free(vm_s);

// TODO: Add Code To Free Work Packages

	tq_freeze(mythr->q);

	return NULL;
}

static bool ve_validate_package(const json_t *pkg, char *elastic_src, char *err_msg) {
	int i, rc, work_pkg_id;
	uint64_t work_id;
	char *str = NULL;
	struct work_package work_package;

	memset(&work_package, 0, sizeof(struct work_package));

	// Get Work ID
	str = (char *)json_string_value(json_object_get(pkg, "work_id"));
	if (!str) {
		sprintf(err_msg, "Invalid 'work_id'");
		return false;
	}
	work_id = strtoull(str, NULL, 10);
	applog(LOG_DEBUG, "DEBUG: Validating Package for work_id: %s", str);

	// Check If Work Package Exists
	work_pkg_id = -1;
	for (i = 0; i < g_work_package_cnt; i++) {
		if (work_id == g_work_package[i].work_id) {
			work_pkg_id = i;
			break;
		}
	}

	if (work_pkg_id >= 0) {
		sprintf(err_msg, "Package already exists for work_id: %llu", work_id);
		return false;
	}

	work_package.work_id = work_id;
	strncpy(work_package.work_str, str, 21);

	// Extract The ElasticPL Source Code
	str = (char *)json_string_value(json_object_get(pkg, "source"));
	if (!str || strlen(str) > MAX_SOURCE_SIZE || strlen(str) == 0) {
		sprintf(err_msg, "Invalid 'source' for work_id: %s", work_package.work_str);
		return false;
	}

	// Decode Source Into ElasticPL
	rc = ascii85dec(elastic_src, MAX_SOURCE_SIZE, str);
	if (!rc) {
		sprintf(err_msg, "Unable to decode 'source' for work_id: %s", work_package.work_str);
		return false;
	}

	// Convert ElasticPL Into AST
	if (!create_epl_ast(elastic_src)) {
		sprintf(err_msg, "Unable to convert 'source' to AST for work_id: %s", work_package.work_str);
		return false;
	}

	// Copy Global Array Sizes Into Work Package
	work_package.vm_ints = ast_vm_ints;
	work_package.vm_uints = ast_vm_uints;
	work_package.vm_longs = ast_vm_longs;
	work_package.vm_ulongs = ast_vm_ulongs;
	work_package.vm_floats = ast_vm_floats;
	work_package.vm_doubles = ast_vm_doubles;

	// Copy Storage Variables Into Work Package
	work_package.iteration_id = 0;
	work_package.storage_id = 0;
	work_package.storage_sz = ast_storage_sz;
	work_package.storage_idx = ast_storage_idx;
	work_package.storage = NULL;

// TODO: Add Number Of Storage Solutions To Work Package Message
//	work_package.storage_cnt = ????
	work_package.storage_cnt = 10;
// TODO: Add Number Of Storage Solutions To Work Package Message

	if (work_package.storage_cnt)
		work_package.storage = calloc(work_package.storage_cnt, sizeof(uint32_t));
	else
		work_package.storage = NULL;

	// Calculate WCET
	if (!calc_wcet()) {
		sprintf(err_msg, "Unable to calculate WCET for work_id: %s", work_package.work_str);
		return false;
	}

	applog(LOG_DEBUG, "Convert The ElasticPL Source Into A C Program");

	// Convert The ElasticPL Source Into A C Program
	if (!convert_ast_to_c(work_package.work_str)) {
		sprintf(err_msg, "Unable to convert 'source' to C for work_id: %s", work_package.work_str);
		return false;
	}

	// Add Work Package To Global List
	work_package.active = true;
	add_work_package(&work_package);
	work_pkg_id = g_work_package_cnt - 1;

	// Create A C Program Library With All Active Packages
	if (!compile_library(g_work_package[work_pkg_id].work_str)) {
		sprintf(err_msg, "Unable to create Validation C Library");
		return false;
	}

	// Check VM Memory Requirements
	if (work_package.vm_ints > g_ve_ints) { g_ve_ints = work_package.vm_ints; g_upd_ve_ints = true; }
	if (work_package.vm_uints > g_ve_uints) { g_ve_uints = work_package.vm_uints; g_upd_ve_uints = true; }
	if (work_package.vm_longs > g_ve_longs) { g_ve_longs = work_package.vm_longs; g_upd_ve_longs = true; }
	if (work_package.vm_ulongs > g_ve_ulongs) { g_ve_ulongs = work_package.vm_ulongs; g_upd_ve_ulongs = true; }
	if (work_package.vm_ints > g_ve_floats) { g_ve_floats = work_package.vm_floats; g_upd_ve_floats = true; }
	if (work_package.vm_ints > g_ve_doubles) { g_ve_doubles = work_package.vm_doubles; g_upd_ve_doubles = true; }

	g_new_ve_library = true;
	g_ve_library_loaded = true;

	return true;
}

extern void *ve_update_storage_thread(void *userdata) {
	struct thr_info *mythr = (struct thr_info*)userdata;
	struct request *req;
	char msg[512], err_msg[256];
	uint32_t success;

	while (1) {

		success = 1;
		err_msg[0] = '\0';

		// Wait For New Requests On Queue
		req = (struct request *) tq_pop(mythr->q, NULL);
		applog(LOG_DEBUG, "Updating Storage (req_id: %d)", req->req_id);

		if (!ve_update_storage(req->work_id, req->iteration_id, req->storage_id, req->storage, req->storage_sz, err_msg)) {
			success = 0;
			applog(LOG_DEBUG, "DEBUG: Unable to update storage for work_id: %lu, iteration_Id: %d, storage_id: %d", req->work_id, req->iteration_id, req->storage_id);
		}

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

static bool ve_update_storage(uint64_t work_id, uint32_t iteration_id, uint32_t storage_id, uint32_t *storage, uint32_t storage_sz, char *err_msg) {
	int i, work_pkg_id;
	char *str = NULL;

	applog(LOG_DEBUG, "DEBUG: Updating storage for work_id: %lu", work_id);

	// Check If Work Package Exists
	work_pkg_id = -1;
	for (i = 0; i < g_work_package_cnt; i++) {
		if (work_id == g_work_package[i].work_id) {
			work_pkg_id = i;
			break;
		}
	}

	if (work_pkg_id < 0) {
		sprintf(err_msg, "No Package exists for work_id: %llu", work_id);
		return false;
	}

	if (!g_work_package[work_pkg_id].storage) {
		sprintf(err_msg, "Package for work_id: %llu does not support storage", work_id);
		return false;
	}

	if (storage_sz != g_work_package[work_pkg_id].storage_sz) {
		sprintf(err_msg, "Submitted storage size (%d) does not match 'storage_sz' in ElasticPL", storage_sz);
		return false;
	}

	if (iteration_id != g_work_package[work_pkg_id].iteration_id) {
		sprintf(err_msg, "Value for iteration_id: %d does not match current iteration for package", iteration_id);
		return false;
	}

	if (storage_id > g_work_package[work_pkg_id].storage_cnt) {
		sprintf(err_msg, "Value for storage_id: %d exceeds max allowed", storage_id);
		return false;
	}

	// Check If Storage Values Already Exist For The Storage ID
	if (g_work_package[work_pkg_id].storage[storage_id]) {
		free(g_work_package[work_pkg_id].storage[storage_id]);
	}

	// Update Values For The Storage ID
	g_work_package[work_pkg_id].storage[storage_id] = malloc(32 * sizeof(uint32_t));

	if (!g_work_package[work_pkg_id].storage[storage_id]) {
		sprintf(err_msg, "Unable to allocate storage");
		return false;
	}

	g_work_package[work_pkg_id].storage[storage_id] = storage;

	return true;
}
