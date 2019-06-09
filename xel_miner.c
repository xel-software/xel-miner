/*
* Copyright 2010 Jeff Garzik
* Copyright 2012-2014 pooler
* Copyright 2014 Lucas Jones
* Copyright 2016 sprocket
* Copyright 2016 Evil-Knievel
*
* This program is free software; you can redistribuSte it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*/

#define _GNU_SOURCE

#include <curl/curl.h>
#include <ctype.h>
#include <getopt.h>
#include <jansson.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include "miner.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef WIN32
#include <malloc.h>
#include "compat/winansi.h"
#else
#include <mm_malloc.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <ctype.h> // avoid implicit declaration of tolower
#endif


enum prefs {
	PREF_PROFIT,	// Estimate most profitable based on WCET vs reward
	PREF_WCET,		// Fewest cycles required by work item
	PREF_WORKID,	// Specify work ID
	PREF_COUNT
};

static const char *pref_type[] = {
	"profit",
	"wcet",
	"workid",
	"\0"
};

bool opt_debug = false;
bool opt_debug_epl = false;
bool opt_debug_vm = false;
bool opt_continuous_test_bty = false;
bool opt_continuous_test_pow = false;

bool opt_quiet = false;
bool opt_norenice = false;
bool opt_protocol = false;
bool use_colors = true;
static int opt_retries = -1;
static int opt_fail_pause = 10;
static int opt_scantime = 60;  // Get New Work From Server At Least Every 60s
bool opt_test_miner = false;
bool opt_test_vm = false;
bool opt_verify_only = false;
bool opt_test_wcet_main = false;
bool opt_test_wcet_verify = false;
bool opt_test_stdin = false;
bool went_through = false;

int opt_limit_storage = -1;
uint64_t opt_wcet_main = 0;
uint64_t opt_wcet_verify = 0;
int opt_deadswitch = 0;
bool opt_opencl = false;
int opt_opencl_gthreads = 0;
int opt_opencl_vwidth = 0;
int opt_timeout = 30;
int opt_n_threads = 0;
static enum prefs opt_pref = PREF_PROFIT;
char pref_workid[32];
bool opt_validate_work = false;
volatile bool* g_pow_ignore = NULL;
volatile bool* g_bounty_ignore = NULL;

int delay_sleep = 0;
int ignore_mask = 0;
int num_cpus;
char g_work_nm[50];
char g_work_id[22];
volatile uint64_t g_cur_work_id;
volatile uint64_t g_cur_block_id;
volatile uint64_t g_cur_pow_cnt;
unsigned char g_pow_target_str[33];
uint32_t g_pow_target[4];

__thread _ALIGN(64) uint32_t *vm_m = NULL;
__thread _ALIGN(64) int32_t *vm_i = NULL;
__thread _ALIGN(64) uint32_t *vm_u = NULL;
__thread _ALIGN(64) int64_t *vm_l = NULL;
__thread _ALIGN(64) uint64_t *vm_ul = NULL;
__thread _ALIGN(64) float *vm_f = NULL;
__thread _ALIGN(64) double *vm_d = NULL;
__thread _ALIGN(64) uint32_t *vm_s = NULL;

bool use_elasticpl_math;

pthread_mutex_t applog_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t work_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t submit_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t longpoll_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t went_through_lock = PTHREAD_MUTEX_INITIALIZER;


uint8_t *rpc_url = NULL;
uint8_t *rpc_user = NULL;
uint8_t *rpc_pass = NULL;
uint8_t *rpc_userpass = NULL;
uint8_t *passphrase = NULL;
uint8_t publickey[32];
uint8_t *test_filename = NULL;

struct timeval g_miner_start_time;
struct work g_work = { 0 };
volatile time_t g_work_time = 0;
struct work_package *g_work_package;
volatile int g_work_package_cnt = 0;
volatile int g_work_package_idx = 0;
const uint8_t basepoint[32] = { 9 };

struct submit_req *g_submit_req;
volatile int g_submit_req_cnt = 0;
volatile int g_new_block = false;

uint32_t g_bounty_accepted_cnt = 0;
uint32_t g_bounty_rejected_cnt = 0;
uint32_t g_bounty_timeout_cnt = 0;
uint32_t g_bounty_deprecated_cnt = 0;
uint32_t g_bounty_error_cnt = 0;
uint32_t g_pow_accepted_cnt = 0;
uint32_t g_pow_rejected_cnt = 0;
uint32_t g_pow_discarded_cnt = 0;
bool g_opt_avoidcache = false;
int work_thr_id;
struct thr_info *thr_info;
struct thr_info *thr_deadswitch = NULL;

struct work_restart *work_restart = NULL;

#ifdef USE_OPENCL
struct opencl_device *gpu;
#endif

// Test related stuff
long var_test_block = 123456789;
long var_test_work = 987654321;
int var_test_target[] = {0x0FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
unsigned char var_test_multiplicator[32];
unsigned char var_test_publickey[32];

extern uint32_t swap32(uint32_t a) {
	return ((a << 24) | ((a << 8) & 0x00FF0000) | ((a >> 8) & 0x0000FF00) | ((a >> 24) & 0x000000FF));
}

bool udpSend(const char *msg){
    struct sockaddr_in servaddr;
    int fd = socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0){
        perror("cannot open socket");
        return false;
    }

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(41234);
    if (sendto(fd, msg, strlen(msg), 0, // +1 to include terminator
               (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
        perror("cannot send message");
        close(fd);
        return false;
    }
    close(fd);
    return true;
}

void *deadswitch(void *arg)
{
	applog(LOG_DEBUG, "Activated hard deadswitch at %d seconds", opt_deadswitch);
    time_t start_time = time(NULL);
	for (;;)
	{

		usleep(100000);
		pthread_mutex_lock(&went_through_lock);
		if(went_through)
			break;
		pthread_mutex_unlock(&went_through_lock);

		time_t now = time(NULL);
		time_t diff = now - start_time;

		if (diff >= opt_deadswitch && !went_through)
		{
			applog(LOG_ERR, "ERROR: Deadswitch will kill the instance now. Program did not terminate in time.", opt_deadswitch);
			exit(EXIT_FAILURE);
		}

	}
	applog(LOG_DEBUG, "Deadswitch suspended due to correct termination of work package", opt_deadswitch);
}

static char const usage[] = "\
Usage: " PACKAGE_NAME " [OPTIONS]\n\
Options:\n\
  -c, --config <file>         Use JSON-formated configuration file\n\
      --deadswitch <seconds>  Hardkill the instance after x seconds\n\
  -D, --debug                 Display debug output\n\
      --debug-epl             Display EPL source code\n\
  -d, --delaysleep	     	  Sleep x seconds after submitting POW: useful for burstless debugging\n \
  -i, --ignoremask			  Debug only: ignore 0=nothing, 1=PoW, 2=Bty, 3=Both\n \
  -h, --help                  Display this help text and exit\n\
  -m, --mining PREF[:ID]      Mining preference for choosing work\n\
                                profit       (Default) Estimate most profitable based on POW Reward / WCET\n\
                                wcet         Fewest cycles required by work item \n\
                                workid		 Specify work ID\n\
      --no-color              Don't display colored output\n\
      --opencl	              Run VM using compiled OpenCL code\n\
      --opencl-gthreads <n>   Max Num of Global Threads (256 - 10240, default: 1024)\n\
      --opencl-vwidth <n>	  Vector width of local work size (1 - 256, default: calculated)\n\
  -o, --url=URL               URL of mining server\n\
  -p, --pass <password>       Password for mining server\n\
  -P, --phrase <passphrase>   Secret Passphrase for Elastic account\n\
      --protocol              Display dump of protocol-level activities\n\
  -q, --quiet                 Display minimal output\n\
  -r, --retries <n>           Number of times to retry if a network call fails\n\
                              (Default: Retry indefinitely)\n\
  -R, --retry-pause <n>       Time to pause between retries (Default: 10 sec)\n\
  -s, --scan-time <n>         Max time to scan work before requesting new work (Default: 60 sec)\n\
  	  --test-miner <file>     Run the Miner using JSON formatted work in <file>\n\
      --test-vm <file>        Run the Parser / Compiler using the ElasticPL source code in <file>\n\
	  --test-avoidcache   	  Do not save metadata\n\
      --test-block <block>	  Block-id for test run\n\
	  --test-cont-bounty      Search for bounties within test-vm environment\n\
	  --test-cont-pow         Search for proof-of-work within test-vm environment\n\
	  --test-work <work>	  Work-id for test run\n\
	  --test-limit-storage <ints>			Only allow storage sizes up to <int>\n\
	  --test-multiplicator <32-byte-hex>	Multiplicator for testrun: must be exactly 32 hex chars\n\
	  --test-publickey <32-byte-hex>		Publickey for testrun: must be exactly 32 hex chars\n\
	  --test-stdin		     Read storage values from stdin\n\
	  --test-target <16-byte-hex>		Target for test run: must be exactly 16 hex chars\n\
	  --test-wcet-main <WCET in 20000s>		Do not ignore WCET limits of main function in Test-Vm run\n\
	  --test-wcet-verify <WCET in 20000s>	Do not ignore WCET limits of verify function in Test-Vm run\n\
  -t, --threads <n>           Number of miner threads (Default: Number of CPUs)\n\
  -u, --user <username>       Username for mining server\n\
  -T, --timeout <n>           Timeout for rpc calls (Default: 30 sec)\n\
      --validate              Validate logic in 'main' & 'verify' functions\n\
	  --verify-only           Use verify instead of main\n\
  -v, --version               Display version information and exit\n\
  -X  --no-renice             Do not lower the priority of miner threads\n\
Options while mining ----------------------------------------------------------\n\n\
   s + <enter>                Display mining summary\n\
   d + <enter>                Toggle Debug mode\n\
   q + <enter>                Toggle Quite mode\n\
";

static char const short_options[] = "c:Dd:i:k:hm:o:p:P:qr:R:s:St:T:u:vVX";

static struct option const options[] = {
	{ "config",			1, NULL, 'c' },
	{ "deadswitch",		1, NULL, 1019 },
	{ "debug",			0, NULL, 'D' },
	{ "delaysleep",		1, NULL, 'd' },
	{ "debug-epl",		0, NULL, 1007 },
	{ "help",			0, NULL, 'h' },
	{ "ignoremask",		1, NULL, 'i' },
	{ "mining",			1, NULL, 'm' },
	{ "no-color",		0, NULL, 1001 },
	{ "no-renice",		0, NULL, 'X' },
	{ "opencl",			0, NULL, 1006 },
	{ "opencl-gthreads", 1, NULL, 1008 },
	{ "opencl-vwidth",	1, NULL, 1009 },
	{ "pass",			1, NULL, 'p' },
	{ "phrase",			1, NULL, 'P' },
	{ "protocol",	    0, NULL, 1003 },
	{ "public",			1, NULL, 'k' },
	{ "quiet",			0, NULL, 'q' },
	{ "retries",		1, NULL, 'r' },
	{ "retry-pause",	1, NULL, 'R' },
	{ "scan-time",		1, NULL, 's' },
	{ "test-miner",		1, NULL, 1004 },
	{ "test-vm",		1, NULL, 1005 },
	{ "test-avoidcache",	0, NULL, 1022 },
	{ "test-block",	1, NULL, 1011 },
	{ "test-cont-pow",	0, NULL, 2000 },
	{ "test-cont-bounty",	0, NULL, 2001 },
	{ "test-work",	1, NULL, 1012 },
	{ "test-limit-storage",	1, NULL, 1021 },
	{ "test-multiplicator",	1, NULL, 1013 },
	{ "test-publickey",	1, NULL, 1014 },
	{ "test-stdin",	0, NULL, 1020 },
	{ "test-target",	1, NULL, 1015 },
	{ "test-wcet-main",	1, NULL, 1017 },
	{ "test-wcet-verify",	1, NULL, 1018 },
	{ "threads",		1, NULL, 't' },
	{ "timeout",		1, NULL, 'T' },
	{ "url",			1, NULL, 'o' },
	{ "user",			1, NULL, 'u' },
	{ "validate",		0, NULL, 1010 },
	{ "verify-only",	0, NULL, 1016 },
	{ "version",		0, NULL, 'v' },
	{ 0, 0, 0, 0 }
};

static void parse_cmdline(int argc, char *argv[])
{
	int key;

	while (1) {
		key = getopt_long(argc, argv, short_options, options, NULL);
		if (key < 0)
			break;
		parse_arg(key, optarg);

	}
	if (optind < argc) {
		fprintf(stderr, "%s: unsupported non-option argument -- '%s'\n",
			argv[0], argv[optind]);
		free_up();
		show_usage_and_exit(1);
	}
}

static void strhide(char *s)
{
	if (*s) *s++ = 'x';
	while (*s) *s++ = '\0';
}

void parse_arg(int key, char *arg)
{
	char *p = NULL, *ap = NULL, *nm = NULL;
	int v, i;
	uint64_t xx;

	switch (key) {
	case 'D':
		opt_debug = true;
		break;
	case 'd':
		delay_sleep = atoi(arg);
		applog(LOG_ERR, "Delaysleep has been set to '%s'", arg);
		break;
	case 'i':
		ignore_mask = atoi(arg);
		applog(LOG_ERR, "Ignoremask has been set to '%s'", arg);
		break;
	case 'h':
	  free_up();
		show_usage_and_exit(0);
	case 'm':
		for (i = 0; i < PREF_COUNT; i++) {
			v = (int)strlen(pref_type[i]);
			if (!strncmp(arg, pref_type[i], v)) {
				opt_pref = (enum prefs) i;
				break;
			}
		}
		if (i == PREF_COUNT) {
			applog(LOG_ERR, "Unknown mining preference '%s'", arg);
			free_up();
			show_usage_and_exit(1);
		}
		if (opt_pref == PREF_WORKID) {
			p = strchr(arg, ':');
			if (!p) {
				fprintf(stderr, "Invalid MiningPreference:ID pair -- '%s'\n", arg);
				free_up();
				show_usage_and_exit(1);
			}
			if (p)
				strcpy(pref_workid, ++p);
		}
		break;
	case 'o':
		ap = strstr(arg, "://");
		ap = ap ? ap + 3 : arg;
		nm = strstr(arg, "/nxt");
		if (ap != arg) {
			if (strncasecmp(arg, "http://", 7) && strncasecmp(arg, "https://", 8)) {
				fprintf(stderr, "ERROR: Invalid protocol -- '%s'\n", arg);
				free_up();
				show_usage_and_exit(1);
			}
			if(rpc_url)
				free(rpc_url);
			rpc_url = (char*)malloc(strlen(arg) + 20);
			sprintf(rpc_url, "%s", arg);
		}
		else {
			if (*ap == '\0' || *ap == '/') {
				fprintf(stderr, "ERROR: Invalid URL -- '%s'\n", arg);
				free_up();
				show_usage_and_exit(1);
			}
			if(rpc_url)
				free(rpc_url);
			rpc_url = (char*)malloc(strlen(ap) + 20);
			sprintf(rpc_url, "http://%s", ap);
		}

		if (!nm) {
			char tmp[strlen(ap) + 20];
			sprintf(tmp, "%s/nxt", rpc_url);
			memcpy(rpc_url, tmp, strlen(ap) + 20);
		}

		break;
	case 'p':
		free(rpc_pass);
		rpc_pass = strdupcs(arg);
		strhide(arg);
		break;
	case 'P':
		passphrase = strdupcs(arg);
		strhide(arg);


		// Generate publickey From Secret Phrase
		char* hash_sha256 = (char*)malloc(32 * sizeof(char));
		sha256(passphrase, strlen(passphrase), hash_sha256);

		// Clamp
		hash_sha256[0] &= 248;
		hash_sha256[31] &= 127;
		hash_sha256[31] |= 64;

		// Do "donna"
		curve25519_donna(publickey, hash_sha256, basepoint);

		free(hash_sha256);

		break;
	case 'q':
		opt_quiet = true;
		break;
	case 1022:
			g_opt_avoidcache = true;
			break;
	case 'r':
		v = atoi(arg);
		if (v < -1 || v > 9999){
			free_up();
			show_usage_and_exit(1);
		}
		opt_retries = v;
		break;
	case 1021:
		v = atoi(arg);
		if (v < -1 || v > 9999){
			free_up();
			show_usage_and_exit(1);
		}
		opt_limit_storage = v;
		break;
	case 'R':
		v = atoi(arg);
		if (v < 1 || v > 9999){
			free_up();
			show_usage_and_exit(1);
		}
		opt_fail_pause = v;
		break;
	case 's':
		v = atoi(arg);
		if (v < 1 || v > 9999){
			free_up();
			show_usage_and_exit(1);
		}
		opt_scantime = v;
		break;
	case 't':
		v = atoi(arg);
		if (v < 1 || v > 9999){
			free_up();
			show_usage_and_exit(1);
		}
		opt_n_threads = v;
		break;
	case 'T':
		v = atoi(arg);
		if (v < 1 || v > 9999){
			free_up();
			show_usage_and_exit(1);
		}
		opt_timeout = v;
		break;
	case 'u':
		free(rpc_user);
		rpc_user = strdupcs(arg);
		break;
	case 'v':
		show_version_and_exit();
	case 'X':
		opt_norenice = true;
		break;
	case 1001:
		use_colors = false;
		break;
	case 1016:
		opt_verify_only = true;
		break;
	case 1003:
		opt_protocol = true;
		break;
	case 1004:
		if (!arg){
			free_up();
			show_usage_and_exit(1);
		}
		test_filename = malloc(strlen(arg) + 1);
		strcpy(test_filename, arg);
		opt_test_miner = true;
		break;
	case 1005:
		if (!arg){
			free_up();
			show_usage_and_exit(1);
		}
		test_filename = malloc(strlen(arg) + 1);
		strcpy(test_filename, arg);
		opt_test_vm = true;
		opt_debug = true;
		opt_debug_epl = true;
		opt_debug_vm = true;
		break;
	case 1006:
		opt_opencl = true;
		break;
	case 2000:
		opt_continuous_test_pow = true;
		break;
	case 2001:
		opt_continuous_test_bty = true;
		break;
	case 1007:
		opt_debug = true;
		opt_debug_epl = true;
		break;
	case 1008:
		v = atoi(arg);
		if (v < 256 || v > 10240){
			free_up();
			show_usage_and_exit(1);
		}
		opt_opencl_gthreads = v;
		break;
	case 1019:
		v = atoi(arg);
		if (v < 0){
			free_up();
			show_usage_and_exit(1);
		}
		opt_deadswitch = v;
		break;
	case 1009:
		v = atoi(arg);
		if (v < 1 || v > 256){
			free_up();
			show_usage_and_exit(1);
		}
		else
			opt_opencl_vwidth = v;
		break;
	case 1010:
		opt_validate_work = true;
		break;
	case 1020:
		opt_test_stdin = true;
		break;
	case 1017:
		xx = atol(arg);
		if (v < 0){
			free_up();
			show_usage_and_exit(1);
		}
		opt_test_wcet_main = true;
		opt_wcet_main = xx;
		break;
	case 1018:
		xx = atol(arg);
		if (v < 0){
			free_up();
			show_usage_and_exit(1);
		}
		opt_test_wcet_verify = true;
		opt_wcet_verify = xx;
		break;
	case 1011:
			if (!arg){
				free_up();
				show_usage_and_exit(1);
			}
			xx = atol(arg);
			var_test_block = xx;
			break;
	case 1012:
			if (!arg){
				free_up();
				show_usage_and_exit(1);
			}
			xx = atol(arg);
			var_test_work = xx;
			break;
	case 1013:
			if (!arg || strlen(arg)!=64){
					free_up();
					show_usage_and_exit(1);
				}

			test_filename = malloc(strlen(arg) + 1);
			const char *pos = test_filename;

			strcpy(test_filename, arg);
			for (size_t count = 0; count < 32; count++) {
				sscanf(pos, "%2hhx", &var_test_multiplicator[count]);
				pos += 2;
			}
			break;
	case 1014:
					if (!arg || strlen(arg)!=64){
						free_up();
						show_usage_and_exit(1);
					}

					test_filename = malloc(strlen(arg) + 1);
					const char *pos2 = test_filename;

					strcpy(test_filename, arg);
					for (size_t count = 0; count < 32; count++) {
						sscanf(pos2, "%2hhx", &var_test_publickey[count]);
						publickey[count] = var_test_publickey[count];
						pos2 += 2;
					}
					break;
	case 1015:
					if (!arg || strlen(arg)!=32){
						free_up();
						show_usage_and_exit(1);
					}

					test_filename = malloc(strlen(arg) + 1);
					const char *pos3 = test_filename;
					unsigned char temp[16];
					strcpy(test_filename, arg);
					for (size_t count = 0; count < 16; count++) {
						sscanf(pos3, "%2hhx", &temp[count]);
						pos3 += 2;
					}
					var_test_target[0] = temp[0]<<24 | temp[1]<<16 | temp[2]<<8 | temp[3];
					var_test_target[1] = temp[4]<<24 | temp[5]<<16 | temp[6]<<8 | temp[7];
					var_test_target[2] = temp[8]<<24 | temp[9]<<16 | temp[10]<<8 | temp[11];
					var_test_target[3] = temp[12]<<24 | temp[13]<<16 | temp[14]<<8 | temp[15];
					break;
	default:
		free_up();
		show_usage_and_exit(1);
	}
}

static void show_usage_and_exit(int status)
{
	if (status)
		fprintf(stderr, "Try `" PACKAGE_NAME " -h' for more information.\n");
	else
		printf(usage);
	exit(status);
}

static void show_version_and_exit(void)
{
	printf("\nBuilt on " __DATE__
#ifdef _MSC_VER
		" with VC++ 2015\n\n");
#elif defined(__GNUC__)
		" with GCC");
	printf(" %d.%d.%d\n\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif

	// Dependencies Versions
	printf("curl:     %s\n", curl_version());
#ifdef JANSSON_VERSION
	printf("jansson:  v%s\n", JANSSON_VERSION);
#endif
#ifdef PTW32_VERSION
	printf("pthreads: v%d.%d.%d.%d\n", PTW32_VERSION);
#endif
#ifdef OPENSSL_VERSION_TEXT
	printf("openssl:  %s\n", OPENSSL_VERSION_TEXT);
#endif
	exit(0);
}

static void thread_low_priority() {
#if defined (__unix__)  && defined (__MACH__)
	uint64_t tid = syscall(__NR_gettid);
	int rc = setpriority(PRIO_PROCESS, tid, 7);
	if (rc) {
		applog(LOG_ERR, "Error(%d): failed setting thread priority (you are safe to ignore this message)", rc);
	}
	else {
		if (opt_debug)
			applog(LOG_DEBUG, "setting low thread priority for thread id %ld", tid);
	}
#else
	// TODO: implement for non posix platforms
	// Possibly: Use SetThreadPriority() on windows
#endif// Create Test Work
}

static bool load_test_file(char *file_name, char *buf) {
	int i, fsize, len, bytes;
	char *ptr;
	FILE *fp;

	fp = fopen(file_name, "r");

	if (!fp) {
		applog(LOG_ERR, "ERROR: Unable to open test file: '%s'\n", file_name);
		return false;
	}

	if (0 != fseek(fp, 0, SEEK_END)) {
		applog(LOG_ERR, "ERROR: Unable to determine size of test file: '%s'\n", file_name);
		fclose(fp);
		return false;
	}

	fsize = ftell(fp);

	if (fsize > MAX_SOURCE_SIZE - 4) {
		applog(LOG_ERR, "ERROR: Test file exceeds max size (%d bytes): %zu bytes\n", MAX_SOURCE_SIZE, fsize);
		fclose(fp);
		return false;
	}

	rewind(fp);
	ptr = buf;
	len = fsize;
	while (len > 0) {
		bytes = fread(ptr, 1, ((len > 1024) ? 1024 : len), fp);
		if (bytes == 0) {
			if (feof(fp))
				break;
			else
				applog(LOG_ERR, "ERROR: Unable to read test file: '%s'\n", file_name);
			return false;
		}
		len -= bytes;
		ptr += bytes;
		ptr[0] = 0;
	}
	fclose(fp);

	for (i = 0; i < fsize; i++)
		buf[i] = tolower(buf[i]);

	return true;
}

void tohex(unsigned char * in, size_t insz, char * out, size_t outsz)
{
    unsigned char * pin = in;
    const char * hex = "0123456789ABCDEF";
    char * pout = out;
    for(; pin < in+insz; pout +=2, pin++){
        pout[0] = hex[(*pin>>4) & 0xF];
        pout[1] = hex[ *pin     & 0xF];
    }
    pout[outsz-1] = 0;
}
void tohex_int(unsigned int * in, size_t insz, char * out, size_t outsz)
{
    unsigned int * pin = in;
    const char * hex = "0123456789ABCDEF";
    char * pout = out;
    for(; pin < in+insz; pout +=8, pin++){
        pout[0] = hex[ (*pin>>28) & 0xF];
        pout[1] = hex[ (*pin>>24)     & 0xF];
		pout[2] = hex[ (*pin>>20)     & 0xF];
		pout[3] = hex[ (*pin>>16)     & 0xF];
		pout[4] = hex[ (*pin>>12)     & 0xF];
		pout[5] = hex[ (*pin>>8)     & 0xF];
		pout[6] = hex[ (*pin>>4)     & 0xF];
		pout[7] = hex[ *pin     & 0xF];
    }
    pout[outsz-1] = 0;
}

static void *test_vm_thread(void *userdata) {
	FILE* fin = NULL, *fout = NULL;

	struct thr_info *mythr = (struct thr_info *) userdata;
	int thr_id = mythr->id;
	char *test_code = calloc(MAX_SOURCE_SIZE + 1, sizeof(char));
	struct work work = { 0 };
	struct work_package work_package = { 0 };
	struct instance *inst = NULL;
	int i, rc;
	uint32_t bounty_found, pow_found;
	uint32_t *vm_input = NULL;
	uint32_t *mult32 = (uint32_t *)work.multiplicator;
	unsigned char *ocl_source;
	bool skip_recompile = false;

	// Create Test Work
	work.package_id = 0;
	work.iteration_id = 0;
	work.block_id = var_test_block;
	work.work_id = var_test_work;

  // Determine if we can reuse an already compiled library
	char path_lib[255];
	char path_lib_workstruct[255];
	sprintf(path_lib, "work/job_%lu.so", work.work_id);
	sprintf(path_lib_workstruct, "work/job_%lu.so.metadata", work.work_id);

	if(!g_opt_avoidcache)
		skip_recompile = (access( path_lib, F_OK ) != -1) && (access( path_lib_workstruct, F_OK ) != -1);

	if(skip_recompile){
		// load metadata
		applog(LOG_DEBUG, "DEBUG: Skipping recompilation to be blazing fast");
		fin = fopen(path_lib_workstruct, "rb");
		if(NULL == fin)
    {
        skip_recompile = false;
				applog(LOG_DEBUG, "DEBUG: Failed to open work-structure, skipping cache");
    }else{
			size_t st = fread(&work_package, sizeof(struct work_package), 1, fin);
	    if ( 1 != st){
				skip_recompile = false;
				applog(LOG_DEBUG, "DEBUG: Failed to reconstruct work-structure (read), skipping cache");
			}
			fclose(fin);
		}
	}

	applog(LOG_DEBUG, "DEBUG: TestVM: block id '%lu'", work.block_id);
	applog(LOG_DEBUG, "DEBUG: TestVM: work id '%lu'", work.work_id);

	applog(LOG_DEBUG, "DEBUG: WCET main tester status: %s", opt_test_wcet_main?"ON":"OFF");
	applog(LOG_DEBUG, "DEBUG: WCET verify tester status: %s", opt_test_wcet_verify?"ON":"OFF");

	if(opt_test_wcet_main){
		applog(LOG_DEBUG, "DEBUG: main WCET abortion above: %d*20000", opt_wcet_main);
	}
	if(opt_test_wcet_verify){
		applog(LOG_DEBUG, "DEBUG: verify WCET abortion above: %d*20000", opt_wcet_verify);
	}

	// load multiplicator
	for(int i=0; i<32; ++i)
		work.multiplicator[i] = var_test_multiplicator[i];

	char temp_mult[65];
	tohex(work.multiplicator, 32, temp_mult, 65);
	temp_mult[64] = 0;
	applog(LOG_DEBUG, "DEBUG: TestVM: multiplicator '%s'", temp_mult);

	char temp_pub[65];
	tohex(publickey, 32, temp_pub, 65);
	temp_pub[64] = 0;
	applog(LOG_DEBUG, "DEBUG: TestVM: pubkey '%s'", temp_pub);


	// TODO: generate the VM_M input here based on PUBKEY in var_test_publickey

	// Set The Target For The Work
	g_pow_target[0] = var_test_target[0];
	g_pow_target[1] = var_test_target[1];
	g_pow_target[2] = var_test_target[2];
	g_pow_target[3] = var_test_target[3];

	char temp_tgt[33];
	tohex_int(g_pow_target, 4, temp_tgt, 33);
	temp_tgt[32] = 0;
	applog(LOG_DEBUG, "DEBUG: TestVM: target '%s'", temp_tgt);

	// Create A Test Work Package
	work_package.work_id = work.work_id;
	sprintf(work_package.work_str, "%lu", work_package.work_id);

	// Initialize ints
	get_vm_input(&work);

  if(!skip_recompile){
		applog(LOG_DEBUG, "DEBUG: Loading Test File '%s'", test_filename);
		if (!load_test_file(test_filename, test_code)){
			free_up();
			if(test_code)
				free(test_code);
			exit(EXIT_FAILURE);
		}

		// Convert The Source Code Into ElasticPL AST
		if (!create_epl_ast(test_code)) {
			applog(LOG_ERR, "ERROR: Exiting 'test_vm'");
			// let us clean the ast now
			clean_up_ast();
			free_up();
			if(test_code)
				free(test_code);
			exit(EXIT_FAILURE);
		}


	// Copy Global Array Sizes Into Work Package
	work_package.vm_ints = ast_vm_ints;
	work_package.vm_uints = ast_vm_uints;
	work_package.vm_longs = ast_vm_longs;
	work_package.vm_ulongs = ast_vm_ulongs;
	work_package.vm_floats = ast_vm_floats;
	work_package.vm_doubles = ast_vm_doubles;
	work_package.submit_sz = ast_submit_sz;
	work_package.submit_idx = ast_submit_idx;
	work_package.storage_sz = ast_submit_sz;	// Currently Storage Uses Same Size As Submit
	work_package.storage_idx = ast_submit_idx;	// Currently Storage Uses Same Index As Submit


		fout = fopen(path_lib_workstruct, "wb");
		if(fout == NULL)
		{
				applog(LOG_DEBUG, "DEBUG: Failed to open work-structure for saving");
		}else{
			if ( 1 != fwrite(&work_package, sizeof(struct work_package), 1, fout)){
				applog(LOG_DEBUG, "DEBUG: Failed to write work-structuree");
			}else
				applog(LOG_DEBUG, "DEBUG: Successfully saved metadata (len = %d)", sizeof(struct work_package));
			fclose(fout);
		}

		// Calculate WCET
		if (!calc_wcet()) {
			applog(LOG_ERR, "ERROR: Unable to calculate WCET.  Exiting 'test_vm'\n");
			// let us clean the ast now
			clean_up_ast();
			if(test_code)
				free(test_code);
			exit(EXIT_FAILURE);
		}
		uint64_t wcet = 0;
		if(opt_test_wcet_main){
			wcet = get_main_wcet();
				if(wcet > opt_wcet_main*20000){
					applog(LOG_ERR, "ERROR: The main WCET of %lu is above the threshold of %lu*20000. Your program is too complex! Exiting 'test_vm'", wcet, opt_wcet_main);
					// let us clean the ast now
					clean_up_ast();
					free_up();
					if(test_code)
						free(test_code);
					exit(EXIT_FAILURE);
				}else{
					applog(LOG_DEBUG, "GOOD: The main WCET of %lu is below the threshold of %lu*20000.", wcet, opt_wcet_main);
				}
		}
		if(opt_test_wcet_verify){
			wcet = get_verify_wcet();
				if(wcet > opt_wcet_verify*20000){
					applog(LOG_ERR, "ERROR: The verify WCET of %lu is above the threshold of %lu*20000. Your program is too complex! Exiting 'test_vm'", wcet, opt_wcet_verify);
					// let us clean the ast now
					clean_up_ast();
					free_up();
					if(test_code)
						free(test_code);
					exit(EXIT_FAILURE);
				}else{
					applog(LOG_DEBUG, "GOOD: The verify WCET of %lu is below the threshold of %lu*20000.", wcet, opt_wcet_verify);
				}
		}
	}

	applog(LOG_DEBUG, "DEBUG: storage size: %d", work_package.submit_sz);

	// Convert The ElasticPL Source Into a C or OpenCL Program
	use_elasticpl_math = false;
	if (opt_opencl && !skip_recompile) {
		if (!create_opencl_source(work_package.work_str)) {
			applog(LOG_ERR, "ERROR: Unable to convert 'source' to OpenC.  Exiting 'test_vm'\n");
			// let us clean the ast now
			clean_up_ast();
			free_up();
			if(test_code)
				free(test_code);
			exit(EXIT_FAILURE);
		}
	}
	else if(!skip_recompile) {
		if (!convert_ast_to_c(work_package.work_str)) {
			applog(LOG_ERR, "ERROR: Unable to convert 'source' to C.  Exiting 'test_vm'\n");
			// let us clean the ast now
			clean_up_ast();
			free_up();
			if(test_code)
				free(test_code);
			exit(EXIT_FAILURE);
		}
	}

	// let us clean the ast now
	if(!skip_recompile) {
		clean_up_ast();
	}

	// Add Work Package To Global List
	work_package.active = true;
	add_work_package(&work_package);

	// Inclean_up_ast();itialize Global Variables
	vm_m = calloc(VM_M_ARRAY_SIZE, sizeof(uint32_t));
	memcpy(vm_m, work.vm_input, VM_M_ARRAY_SIZE * sizeof(uint32_t));

	if(opt_limit_storage!=-1){
		if(g_work_package[0].storage_sz>opt_limit_storage){
			applog(LOG_ERR, "ERROR: Your work uses too much storage. You requested %d, but allowed is only up to %d.", g_work_package[0].storage_sz, opt_limit_storage);
			free_up();
			if(test_code)
				free(test_code);
			exit(EXIT_FAILURE);
		}
	}

	if (g_work_package[0].vm_ints) vm_i = calloc(g_work_package[0].vm_ints, sizeof(int32_t));
	if (g_work_package[0].vm_uints) vm_u = calloc(g_work_package[0].vm_uints, sizeof(uint32_t));
	if (g_work_package[0].vm_longs) vm_l = calloc(g_work_package[0].vm_longs, sizeof(int64_t));
	if (g_work_package[0].vm_ulongs) vm_ul = calloc(g_work_package[0].vm_ulongs, sizeof(uint64_t));
	if (g_work_package[0].vm_floats) vm_f = calloc(g_work_package[0].vm_floats, sizeof(float));
	if (g_work_package[0].vm_doubles) vm_d = calloc(g_work_package[0].vm_doubles, sizeof(double));
	if (g_work_package[0].storage_sz) vm_s = calloc(g_work_package[0].storage_sz, sizeof(uint32_t));

	if(opt_test_stdin && g_work_package[0].storage_sz>0){
		applog(LOG_DEBUG, "DEBUG: We will now fill the storage from stdin");
		int counter = 0;
		char *line = NULL;
		size_t size;
		char *ptr;
		for(i=0;i<g_work_package[0].storage_sz;++i){
			errno = 0;
			if (getline(&line, &size, stdin) == -1) {
				applog(LOG_ERR, "ERROR: Your provided only %d storage lines in stdin. This is not enough, should be %d.", counter, g_work_package[0].storage_sz);
				free_up();
				if(test_code)
					free(test_code);
				exit(EXIT_FAILURE);
			} else {
				uint32_t val = (uint32_t)strtoul(line, &ptr, 10);

				if (line == ptr)
					{ applog(LOG_ERR, "ERROR: number : %lu  invalid  (no digits found, 0 returned)\n", val); exit(EXIT_FAILURE); }
				else if (errno == ERANGE && val == LONG_MIN)
					{ applog(LOG_ERR, "ERROR: number : %lu  invalid  (underflow occurred)\n", val); exit(EXIT_FAILURE); }
				else if (errno == ERANGE && val == LONG_MAX)
					{ applog(LOG_ERR, "ERROR: number : %lu  invalid  (overflow occurred)\n", val); exit(EXIT_FAILURE); }
				else if (errno == EINVAL)  /* not in all c99 implementations - gcc OK */
					{ applog(LOG_ERR, "ERROR: number : %lu  invalid  (base contains unsupported value)\n", val); exit(EXIT_FAILURE); }
				else if (errno != 0 && val == 0)
					{ applog(LOG_ERR, "ERROR: number : %lu  invalid  (unspecified error occurred)\n", val); exit(EXIT_FAILURE); }
				/*else if (errno == 0 && line && !*ptr)
					{ applog(LOG_ERR, "ERROR: number : %lu    valid  (and represents all characters read)\n", val); exit(EXIT_FAILURE); }
				else if (errno == 0 && line && *ptr != 0)
					{ applog(LOG_ERR, "ERROR: number : %lu    valid  (but additional characters remain)\n", val); exit(EXIT_FAILURE); }*/


				vm_s[counter] = val;
				counter++;

			}

		}
		applog(LOG_DEBUG, "DEBUG: We have successfully read %d items from stdin and placed them into vm_s.", g_work_package[0].storage_sz);
	}

	if ((g_work_package[0].vm_ints && !vm_i) ||
		(g_work_package[0].vm_uints && !vm_u) ||
		(g_work_package[0].vm_longs && !vm_l) ||
		(g_work_package[0].vm_ulongs && !vm_ul) ||
		(g_work_package[0].vm_floats && !vm_f) ||
		(g_work_package[0].vm_doubles && !vm_d) ||
		(g_work_package[0].storage_sz && !vm_s)) {
			free_up();
			if(test_code)
				free(test_code);
		applog(LOG_ERR, "%s: Unable to allocate VM memory", "'test-vm'");
		exit(EXIT_FAILURE);
	}

	if (opt_opencl) {

#ifdef USE_OPENCL

		// Create OpenCL Buffers
		if (!opencl_create_buffers(&gpu[0]))
			exit(EXIT_FAILURE);

		// Load & Compile OpenCL Code
		ocl_source = opencl_load_source(g_work_package[0].work_str);
//		ocl_source = opencl_load_source("tmp");
		if (!ocl_source)
			exit(EXIT_FAILURE);

		// Calculate Worksize
		if (!opencl_calc_worksize(&gpu[0]))
			exit(EXIT_FAILURE);

		// Create OpenCL Kernel / Set Arguments
		if (!opencl_create_kernel(&gpu[thr_id], ocl_source, g_work_package[0].storage_sz))
			exit(EXIT_FAILURE);

		free(ocl_source);

		// Allocate Memory For OpenCL Inputs / Outputs
		vm_input = (uint32_t *)calloc(24, sizeof(uint32_t));
		uint32_t *rnd_num = (uint32_t *)calloc(1, sizeof(uint32_t));
		uint32_t *result = (uint32_t *)calloc(1, sizeof(uint32_t));
		uint32_t *storage = vm_s;
		uint32_t *output = (uint32_t *)calloc(5, sizeof(uint32_t));
		uint32_t *submit = NULL;
		if (g_work_package[0].storage_sz)
			submit = (uint32_t *)calloc(g_work_package[0].storage_sz, sizeof(uint32_t));

		// Get Random Inputs For OpenCL Inputs
		get_opencl_base_data(&work, vm_input);

		uint32_t data[] = { 0x01,0x00,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19 };
		memcpy(vm_input, data, 80);

		if (!opencl_init_buffer_data(&gpu[0], vm_input, rnd_num, result, storage, 0))
			exit(EXIT_FAILURE);

		// Run A Continuous Test
		int k;

//		0x1DAC2B7C

//		for (i = 0; i < 0xFFFFFFFF; i++) {
		for (i = 0; i < 2; i++) {
//		for (i = 1250; i < 1300; i++) {

			// Update Round Number
			rnd_num[0] = i;

			// Execute The Code On The GPU
			if (!opencl_run_kernel(&gpu[0], rnd_num, result, output, submit, g_work_package[0].storage_sz))
				exit(EXIT_FAILURE);

			if (!result[0])
				continue;

			if ((result[0] == 1) || (result[0] == 3)) {
				applog(LOG_DEBUG, "DEBUG: Found - %08X%08X%08X%08X, Hashes - %u", output[1], output[2], output[3], output[4], ((i + 1) * gpu[0].threads));
			}
			else if ((result[0] == 2) || (result[0] == 3)) {
				applog(LOG_DEBUG, "DEBUG: Bounty Found");
				applog(LOG_DEBUG, "");
				for (k = 0; k < 5; k++) {
					printf("output[%d] = %08X (%u)\n", k, output[k], output[k]);
				}
			}
		}

		printf("\nhashes = %u\n", (i + 1) * gpu[0].threads);

//		if (!opencl_run_kernel(&gpu[0], rnd_num, result, output, submit, g_work_package[0].storage_sz))
//			exit(EXIT_FAILURE);

		printf("\n\tresult = %u\n", result[0]);
		for (i = 0; i < 5; i++)
			printf("\tout[%d] = %08X\n", i, output[i]);

		if ((result[0] == 2) || (result[0] == 3))
			bounty_found = true;

		if ((result[0] == 1) || (result[0] == 3))
			pow_found = true;

		if (g_work_package[0].storage_sz) {
			for (i = 0; i < 10; i++)
				printf("\tsubmit[%d] = %08X\n", i, submit[i]);
		}

		memcpy(work.pow_hash, &output[1], 16);

#endif

	}
	else {
		// Compile The C Program Library
		if (!skip_recompile && !compile_library(g_work_package[0].work_str)) {
			applog(LOG_ERR, "ERROR: Exiting 'test_vm'");
			free_up();
			if(test_code)
				free(test_code);
			exit(EXIT_FAILURE);
		}

		// Link To The C Program Library
		if (inst)
			free_library(inst);
		inst = calloc(1, sizeof(struct instance));
		create_instance(inst, g_work_package[0].work_str);
		inst->initialize(vm_m, vm_i, vm_u, vm_l, vm_ul, vm_f, vm_d, vm_s);

		// Temporary Logic For Miner To Validate 'main' & 'verify'
		// This Should Be Done Prior To Author Submitting The Job By The Node
		if (opt_validate_work && !skip_recompile) {
			if (!validate_work_source(0, inst)) {
				applog(LOG_ERR, "ERROR: Exiting 'test_vm'");
				free_up();
				if(test_code)
					free(test_code);
				exit(EXIT_FAILURE);
			}
		}

		// Execute The VM Logic
//		rc = inst->verify(g_work_package[0].work_id, &bounty_found, 1, &pow_found, g_pow_target, work.pow_hash);

		if(!opt_continuous_test_bty){
			if(!opt_verify_only)
				rc = inst->execute(g_work_package[0].work_id, &bounty_found, 1, &pow_found, g_pow_target, work.pow_hash);
			else
				rc = inst->verify(g_work_package[0].work_id, &bounty_found, 1, &pow_found, g_pow_target, work.pow_hash);
		}else{
			uint32_t* intmult = (uint32_t*)work.multiplicator;
			intmult[2] = 1;
			intmult[3] = genrand_int32();
			intmult[7] = genrand_int32();

			// run continuous test
			applog(LOG_INFO, ">> STARTING CONTINUOUS TEST, THIS MAY TAKE VERY LONG DEPENDING ON YOUR PROBLEM <<");
			for (i = 0; i < 0xFFFFFFFF; i++) {
				intmult[7]++;
				intmult[6]+=3;
				get_vm_input(&work);
				// Reset VM Memory
				memcpy(vm_m, work.vm_input, VM_M_ARRAY_SIZE * sizeof(uint32_t));

				rc = inst->execute(g_work_package[0].work_id, &bounty_found, 1, &pow_found, g_pow_target, work.pow_hash);
				if(i>0 && i%5000000==0){
						applog(LOG_DEBUG, "ran %d iterations, still working ...", i);
				}
				if (bounty_found != 0) {
					applog(LOG_INFO, "********************************");
					applog(LOG_INFO, "FOUND A SOLUTION TO YOUR PROBLEM");
					applog(LOG_INFO, " we will dump all 12 input ints");
					applog(LOG_INFO, "********************************");

					for (int ix = 0; ix < 12; ix++) {
						applog(LOG_INFO, "m[%d]\t=\t%u",ix,work.vm_input[ix]);
					}
					break;
				}
				/*if (pow_found != 0) {
					applog(LOG_INFO, "********************************");
					applog(LOG_INFO, "FOUND SOLUTION TO POW CHALLENGE");
					applog(LOG_INFO, " we will dump all 12 input ints");
					applog(LOG_INFO, "********************************");

					for (int ix = 0; ix < 12; ix++) {
						applog(LOG_INFO, "m[%d]\t=\t%u",ix,work.vm_input[ix]);
					}
					break;
				}*/

			}

		}
		// Run A Continuous Test
		//uint32_t rnd;
		//long hashes_done;
		//for (i = 0; i < 0xFFFFFFFF; i++) {
		//	rc = execute_vm(thr_id, &rnd, 0, &work, inst, &hashes_done1);
		//	if (rc == 1) {
		//		bounty_found = true;
		//		break;
		//	}
		//	else if (rc == 2) {
		//		pow_found = true;
		//		break;
		//	}

		// dump_vm(0);
	}

	applog(LOG_DEBUG, "DEBUG: Bounty Found: %s", (bounty_found == 1) ? "true" : "false");
	applog(LOG_DEBUG, "DEBUG: POW Found: %s", (pow_found == 1) ? "true" : "false");
	applog(LOG_DEBUG, "DEBUG: POW Hash: %08X%08X%08X%08X", work.pow_hash[0], work.pow_hash[1], work.pow_hash[2], work.pow_hash[3]);

	if (inst)
		free_library(inst);

	applog(LOG_NOTICE, "DEBUG: Compiler Test Complete");
	applog(LOG_WARNING, "Exiting " PACKAGE_NAME);

	if (inst) free(inst);
	if (vm_m) free(vm_m);
	if (vm_i) free(vm_i);
	if (vm_u) free(vm_u);
	if (vm_l) free(vm_l);
	if (vm_ul) free(vm_ul);
	if (vm_f) free(vm_f);
	if (vm_d) free(vm_d);
	if (vm_s) free(vm_s);
	if (test_code)
		free(test_code);

	//tq_freeze(mythr->q);

	return NULL;
}

unsigned char tmp_msg[80];

static bool get_vm_input(struct work *work) {
	int i;
	char msg[80];
	char hash[16];
	uint32_t *msg32 = (uint32_t *)msg;
	uint32_t *hash32 = (uint32_t *)hash;
	uint32_t *mult32 = (uint32_t *)&work->multiplicator;
	uint32_t *workid32 = (uint32_t *)&work->work_id;
	uint32_t *blockid32 = (uint32_t *)&work->block_id;

	memcpy(&msg[0], work->multiplicator, 32);
	memcpy(&msg[32], publickey, 32);
	memcpy(&msg[64], &workid32[1], 4);	// Swap First 4 Bytes Of Long
	memcpy(&msg[68], &workid32[0], 4);	// With Second 4 Bytes Of Long
	memcpy(&msg[72], &blockid32[1], 4);
	memcpy(&msg[76], &blockid32[0], 4);
	msg32[16] = swap32(msg32[16]);
	msg32[17] = swap32(msg32[17]);
	msg32[18] = swap32(msg32[18]);
	msg32[19] = swap32(msg32[19]);


	// fix
	memcpy(tmp_msg, msg, 80);
	// fix

	char tempin[161];
	tohex(tmp_msg,80,tempin,161);
	tempin[160] = 0;
	//applog(LOG_DEBUG, "DEBUG: Input to int generator: %s", tempin);


	// Hash The Inputs
	MD5(msg, 80, hash);

	char tempmsg[33];
	tohex(msg,16,tempmsg,33);
	tempmsg[32]=0;
	//applog(LOG_DEBUG, "DEBUG: Digest: %s", tempmsg);

	// Randomize Inputs m[0]-m[9]
	for (i = 0; i < 10; i++) {
		work->vm_input[i] = swap32(hash32[i % 4]);
		if (i > 4)
			work->vm_input[i] = work->vm_input[i] ^ work->vm_input[i - 3];
	}

	// Set m[10] To Round Number
	work->vm_input[10] = mult32[1];

	// Set Inputs m[11] To Iteration Number
	work->vm_input[11] = mult32[2]; /*
printf("INPUT\n");
		for(int i=0;i<12;++i){
			printf("%d\n",work->vm_input[i]);
		}*/
	return true;
}

static bool get_opencl_base_data(struct work *work, uint32_t *vm_input) {
	char msg[80];
	uint32_t *msg32 = (uint32_t *)msg;
	uint32_t *workid32 = (uint32_t *)&work->work_id;
	uint32_t *blockid32 = (uint32_t *)&work->block_id;

	memcpy(&vm_input[0], work->multiplicator, 32);
	memcpy(&vm_input[8], publickey, 32);
	memcpy(&vm_input[16], &workid32[1], 4);	// Swap First 4 Bytes Of Long
	memcpy(&vm_input[17], &workid32[0], 4);	// With Second 4 Bytes Of Long
	memcpy(&vm_input[18], &blockid32[1], 4);
	memcpy(&vm_input[19], &blockid32[0], 4);
	vm_input[16] = swap32(vm_input[16]);
	vm_input[17] = swap32(vm_input[17]);
	vm_input[18] = swap32(vm_input[18]);
	vm_input[19] = swap32(vm_input[19]);

	// Target
	vm_input[20] = g_pow_target[0];
	vm_input[21] = g_pow_target[1];
	vm_input[22] = g_pow_target[2];
	vm_input[23] = g_pow_target[3];
	//vm_input[20] = work->pow_target[0];
	//vm_input[21] = work->pow_target[1];
	//vm_input[22] = work->pow_target[2];
	//vm_input[23] = work->pow_target[3];

	return true;
}

static int execute_vm(int thr_id, uint32_t *rnd, uint32_t iteration, struct work *work, struct instance *inst, long *hashes_done) {
	int rc;
	time_t t_start = time(NULL);
	char msg[64];
	uint32_t bounty_found, pow_found;

	uint32_t *msg32 = (uint32_t *)msg;
	uint32_t *mult32 = (uint32_t *)work->multiplicator;

	mult32[0] = thr_id;												// Ensures Each Thread Is Unique
//	mult32[1] = *rnd;												// Round - Value Will Be Incremented On Each Pass
	mult32[2] = iteration;											// Iteration - Not Implemented Yet
	mult32[3] = 0;													// N/A - GPU OpenCL Thread ID
	mult32[7] = genrand_int32();									// Random Number

	while (1) {
		// Check If New Work Is Available
		if (work_restart[thr_id].restart)
			return 0;

		// Get Values For VM Inputs
		mult32[1] = (*rnd)++;												// Round - Value Will Be Incremented On Each Pass
		get_vm_input(work);

		// Reset VM Memory
		memcpy(vm_m, work->vm_input, VM_M_ARRAY_SIZE * sizeof(uint32_t));

		// Execute The VM Logic
//		rc = inst->execute(work->work_id, &bounty_found, 1, &pow_found, work->pow_target, work->pow_hash);
		rc = inst->execute(work->work_id, &bounty_found, 1, &pow_found, g_pow_target, work->pow_hash);

		if (opt_test_miner) {
			dump_vm(work->package_id);
			exit(EXIT_SUCCESS);
		}

		// Bounty or POW Found, Exit Immediately
		if (bounty_found)
			return 1;
		else if (pow_found)
			return 2;




		(*hashes_done)++;

		// Only Run For 1s Before Returning To Miner Thread
		if ((time(NULL) - t_start) >= 1)
//		if ((time(NULL) - t_start) >= 1000)
				break;

	}
	return 0;
}

static void dump_vm(int idx) {
	uint32_t i;

	// Dump VM Values
	printf("\n\t   VM Initialized Unsigned Integers:\n");
	for (i = 0; i < VM_M_ARRAY_SIZE; i++) {
		printf("\t\t  vm_m[%*d] = %*u\t%08X\n", 4, i, 10, vm_m[i], vm_m[i]);
	}

	if (g_work_package[idx].vm_ints) {
		printf("\n\t   Integers:\n");
		for (i = 0; i < g_work_package[idx].vm_ints; i++) {
			if (vm_i[i])
				printf("\t\t  vm_i[%*d] = %*d\t%08X\n", 4, i, 10, vm_i[i], vm_i[i]);
		}
	}

	if (g_work_package[idx].vm_uints) {
		printf("\n\t   Unsigned Integers:\n");
		for (i = 0; i < g_work_package[idx].vm_uints; i++) {
			if (vm_u[i])
				printf("\t\t  vm_u[%*d] = %*u\t%08X\n", 4, i, 10, vm_u[i], vm_u[i]);
		}
	}

	if (g_work_package[idx].vm_longs) {
		printf("\n\t   Longs:\n");
		for (i = 0; i < g_work_package[idx].vm_longs; i++) {
			if (vm_l[i])
				printf("\t\t  vm_l[%*d] = %*ld\t%08lX\n", 4, i, 10, vm_l[i], vm_l[i]);
		}
	}

	if (g_work_package[idx].vm_ulongs) {
		printf("\n\t   Unsigned Longs:\n");
		for (i = 0; i < g_work_package[idx].vm_ulongs; i++) {
			if (vm_ul[i])
				printf("\t\t  vm_ul[%*d]= %*lu\t%08lX\n", 4, i, 10, vm_ul[i], vm_ul[i]);
		}
	}

	if (g_work_package[idx].vm_floats) {
		printf("\n\t   Floats:\n");
		for (i = 0; i < g_work_package[idx].vm_floats; i++) {
			if (vm_f[i])
				printf("\t\t  vm_f[%*d] = %f\n", 4, i, vm_f[i]);
		}
	}

	if (g_work_package[idx].vm_doubles) {
		printf("\n\t   Doubles:\n");
		for (i = 0; i < g_work_package[idx].vm_doubles; i++) {
			if (vm_d[i])
				printf("\t\t  vm_d[%*d] = %f\n", 4, i, vm_d[i]);
		}
	}
	printf("\n");
}

static void update_pending_cnt(uint64_t work_id, bool add) {
	int i;

	for (i = 0; i < g_work_package_cnt; i++) {
		if (work_id == g_work_package[i].work_id) {

			if (add)
				g_work_package[i].pending_bty_cnt++;
			else
				g_work_package[i].pending_bty_cnt--;

			break;
		}
	}
}

extern void clear_all_workpackages(){
	int i=0;
	for(i=0; i<g_work_package_cnt; ++i){
			if(g_work_package[i].storage)
				free(g_work_package[i].storage);
	}
	free(g_work_package);
}
extern bool add_work_package(struct work_package *work_package) {
	g_work_package = realloc(g_work_package, sizeof(struct work_package) * (g_work_package_cnt + 1));
	if (!g_work_package) {
		applog(LOG_ERR, "ERROR: Unable to allocate memory for work_package");
		return false;
	}

	memcpy(&g_work_package[g_work_package_cnt], work_package, sizeof(struct work_package));
	g_work_package_cnt++;

	return true;
}

static bool get_work(CURL *curl) {
	int err, rc;
	json_t *val;
	struct work work;
	struct timeval tv_start, tv_end, diff;

	memset(g_work_id, 0, sizeof(g_work_id));
	memset(g_work_nm, 0, sizeof(g_work_nm));
	memset(g_pow_target_str, 0, sizeof(g_pow_target_str));

	gettimeofday(&tv_start, NULL);
	if (!opt_test_miner) {
		val = json_rpc_call(curl, rpc_url, rpc_userpass, "requestType=getMineableWork&n=1", &err);
	}
	else {
		json_error_t err_val;
		val = JSON_LOAD_FILE(test_filename, &err_val);
		if (!json_is_object(val)) {
			if (err_val.line < 0)
				applog(LOG_ERR, "%s\n", err_val.text);
			else
				applog(LOG_ERR, "%s:%d: %s\n", test_filename, err_val.line, err_val.text);
			return false;
		}

		char *str = json_dumps(val, JSON_INDENT(3));
		applog(LOG_DEBUG, "DEBUG: JSON Response -\n%s", str);
		free(str);
	}

	if (!val) {
		applog(LOG_ERR, "ERROR: 'json_rpc_call' failed...retrying in %d seconds", opt_fail_pause);
		sleep(opt_fail_pause);
		return false;
	}

	gettimeofday(&tv_end, NULL);
	if (opt_protocol) {
		timeval_subtract(&diff, &tv_end, &tv_start);
		applog(LOG_DEBUG, "DEBUG: Time to get work: %.2f ms", (1000.0 * diff.tv_sec) + (0.001 * diff.tv_usec));
	}

	rc = decode_work(curl, val, &work);
	json_decref(val);

	gettimeofday(&tv_start, NULL);
	if (opt_protocol) {
		timeval_subtract(&diff, &tv_start, &tv_end);
		applog(LOG_DEBUG, "DEBUG: Time to decode work: %.2f ms", (1000.0 * diff.tv_sec) + (0.001 * diff.tv_usec));
	}

	// Update Global Variables With Latest Work
	pthread_mutex_lock(&work_lock);
	g_work_time = time(NULL);

	if (rc > 0) {
		strncpy(g_work_id, work.work_str, 21);
		strncpy(g_work_nm, work.work_nm, 49);
		ints2hex(work.pow_target, 4, g_pow_target_str, 33);
		memcpy(g_pow_target, work.pow_target, 4 * sizeof(uint32_t));
		memcpy(&g_work, &work, sizeof(struct work));

		// Reset POW Counter When Block Changes
		//if (work.block_id != g_cur_block_id) {
		//	g_cur_pow_cnt = 0;
		//	applog(LOG_DEBUG, "New Block - now mining block: %llu", work.block_id);
		//}

		// Restart Miner Threads If Work Package Changes
		if (work.work_id != g_cur_work_id) {
			applog(LOG_NOTICE, "Switching to work_id: %s (target: %s)", work.work_str, g_pow_target_str);
			restart_threads();
		}

		g_cur_work_id = work.work_id;
		g_cur_block_id = work.block_id;
	}
	else {
		g_cur_work_id = 0;
		g_cur_block_id = 0;
		g_cur_pow_cnt = 0;
		memset(&g_work, 0, sizeof(struct work));
		g_work.package_id = -1;
		g_work.iteration_id = -1;
		restart_threads();
	}

	pthread_mutex_unlock(&work_lock);

	if (!rc)
		return false;

	return true;
}

static double calc_diff(uint32_t *target) {
	double diff_1 = 0x0000FFFFFFFFFFFF;
	double diff;

	uint64_t tgt = 0x0ull;
	uint32_t *tgt32 = (uint32_t *)&tgt;

	tgt32[0] = target[1];
	tgt32[1] = target[0];

	diff = (double)tgt;

	if (!diff)
		return diff_1;

	return (double)(diff_1 / diff);
}

static int decode_work(CURL *curl, const json_t *val, struct work *work) {
	int i, j, rc, num_pkg, best_pkg, bty_rcvd, work_pkg_id, iteration_id;
	uint64_t work_id;
	uint32_t storage_id, pow_tgt[4];
	uint64_t best_wcet = 0xFFFFFFFF;
	double difficulty, best_profit = 0, profit = 0;
	char *tgt = NULL, *src = NULL, *str = NULL, *best_src = NULL, *best_tgt = NULL, *elastic_src = NULL;
	json_t *wrk = NULL, *pkg = NULL;

	memset(work, 0, sizeof(struct work));

	if (opt_protocol) {
		str = json_dumps(val, JSON_INDENT(3));
		applog(LOG_DEBUG, "DEBUG: JSON Response -\n%s", str);
		free(str);
	}

	wrk = json_object_get(val, "work_packages");

	if (!wrk) {
		applog(LOG_ERR, "Invalid JSON response to getMineableWork request");
		return 0;
	}

	// Set All Packages In Global List To Inactive
	for (i = 0; i < g_work_package_cnt; i++)
		g_work_package[i].active = false;

	// Check If Any Active Work Packages Are Available
	num_pkg = json_array_size(wrk);
	if (num_pkg == 0) {
		applog(LOG_INFO, "No work available...retrying in %ds", opt_scantime);
		return -1;
	}

	best_pkg = -1;

	for (i = 0; i<num_pkg; i++) {
		pkg = json_array_get(wrk, i);
		tgt = (char *)json_string_value(json_object_get(pkg, "target"));
//		str = (char *)json_string_value(json_object_get(pkg, "work_id"));
		str = (char *)json_string_value(json_object_get(pkg, "id"));

// TODO: Need To Add Current Iteration Number To Message

		int iterations = (int)json_integer_value(json_object_get(pkg, "iterations"));
		int iterations_left = (int)json_integer_value(json_object_get(pkg, "iterations_left"));
		iteration_id = iterations - iterations_left;


		// Temp Fix
//		tgt = (char *)calloc(33, sizeof(char));
//		sprintf(tgt, "00000FFFFFFFFFFFFFFFFFFFFFFFFFFF");
		// Temp Fix


		if (!tgt || !str || (iteration_id < 0)) {
			applog(LOG_ERR, "Unable to parse work package");
			return 0;
		}
		work_id = strtoull(str, NULL, 10);
		applog(LOG_DEBUG, "DEBUG: Checking work_id: %s (iteration_id: %d)", str, iteration_id);

		// Check If Work Package Exists
		work_pkg_id = -1;
		for (j = 0; j < g_work_package_cnt; j++) {
			if (work_id == g_work_package[j].work_id) {
				work_pkg_id = j;

				// Update Iteration ID
				g_work_package[j].iteration_id = iteration_id;

				// Set Status To Active
				g_work_package[j].active = true;

				break;
			}
		}

		// Add New Work Packages
		if (work_pkg_id < 0) {
			struct work_package work_package;
			memset(&work_package, 0, sizeof(struct work_package));

			work_package.work_id = work_id;
			strncpy(work_package.work_str, str, 21);
			str = (char *)json_string_value(json_object_get(pkg, "block_id"));
			work_package.block_id = strtoull(str, NULL, 10);
//			str = (char *)json_string_value(json_object_get(pkg, "title"));
//			strncpy(work_package.work_nm, str, 49);
			work_package.bounty_limit = (uint32_t)json_integer_value(json_object_get(pkg, "bounty_limit_per_iteration"));
			work_package.bty_reward = (uint64_t)json_number_value(json_object_get(pkg, "xel_per_bounty"));
			work_package.pow_reward = (uint64_t)json_number_value(json_object_get(pkg, "xel_per_pow"));
			work_package.pending_bty_cnt = 0;
			work_package.blacklisted = false;

			// Get Source From Node
			if (!get_work_source(curl, work_package.work_str, elastic_src)) {
				work_package.blacklisted = true;
				applog(LOG_ERR, "ERROR: Unable to get 'source' for work_id: %s", work_package.work_str);
				continue;
			}

			// Copy Global Array Sizes Into Work Package
			work_package.vm_ints = ast_vm_ints;
			work_package.vm_uints = ast_vm_uints;
			work_package.vm_longs = ast_vm_longs;
			work_package.vm_ulongs = ast_vm_ulongs;
			work_package.vm_floats = ast_vm_floats;
			work_package.vm_doubles = ast_vm_doubles;

			// Copy Submit Variables Into Work Package
			work_package.submit_sz = ast_submit_sz;
			work_package.submit_idx = ast_submit_idx;

			// Copy Storage Variables Into Work Package
			work_package.iteration_id = iteration_id;
			work_package.storage_id = 0;
			work_package.storage_sz = ast_submit_sz;	// Currently, Storage Size = Submit Size
			work_package.storage_idx = ast_submit_idx;	// Currently, Storage Index = Submti Index
			work_package.storage = malloc(ast_submit_sz * sizeof(uint32_t));
			work_package.iterations = iterations;
			// Calculate WCET
			work_package.WCET = calc_wcet();
			if (!work_package.WCET) {
				work_package.blacklisted = true;
				applog(LOG_ERR, "ERROR: Unable to calculate WCET for work_id: %s", work_package.work_str);
				return 0;
			}

			// Convert The ElasticPL Source Into A C Program
			if (!convert_ast_to_c(work_package.work_str)) {
				work_package.blacklisted = true;
				applog(LOG_ERR, "ERROR: Unable to convert 'source' to C for work_id: %s", work_package.work_str);
				return 0;
			}

			// Convert The ElasticPL Source Into A C Program Library
			if (opt_opencl) {
				if (!create_opencl_source(NULL)) {
					work_package.blacklisted = true;
					applog(LOG_ERR, "ERROR: Unable to convert 'source' to OpenCL for work_id: %s", work_package.work_str);
					return 0;
				}
			}
			else {
				if (!compile_library(work_package.work_str)) {
					work_package.blacklisted = true;
					applog(LOG_ERR, "ERROR: Unable to create C Library for work_id: %s\n\n%s\n", work_package.work_str, str);
					return 0;
				}
			}

			applog(LOG_DEBUG, "DEBUG: Adding work package to list, work_id: %s", work_package.work_str);

			// Add Work Package To Global List
			work_package.active = true;
			add_work_package(&work_package);
			work_pkg_id = g_work_package_cnt - 1;
		}

		// Check If Work Has Been Blacklisted
		if (g_work_package[work_pkg_id].blacklisted) {
			applog(LOG_DEBUG, "DEBUG: Skipping blacklisted work_id: %s", g_work_package[work_pkg_id].work_str);
			continue;
		}

		// Check If Work Has Available Bounties
		bty_rcvd = (int)json_integer_value(json_object_get(pkg, "received_bounties"));
		if (g_work_package[work_pkg_id].bounty_limit*g_work_package[work_pkg_id].iterations <= (bty_rcvd + g_work_package[work_pkg_id].pending_bty_cnt)) {
			applog(LOG_DEBUG, "DEBUG: Skipping work_id: %s - No Bounties Left", g_work_package[work_pkg_id].work_str);
			continue;
		}

		// Get Updated Target For The Job
		rc = hex2ints(pow_tgt, 4, tgt, strlen(tgt));
		if (!rc) {
			applog(LOG_ERR, "Invalid Target in JSON response for work_id: %s", g_work_package[work_pkg_id].work_str);
			return 0;
		}

		difficulty = calc_diff(pow_tgt);
		profit = ((double)g_work_package[work_pkg_id].pow_reward / ((double)g_work_package[work_pkg_id].WCET * difficulty));

		// Select Best Work Package
		if (opt_pref == PREF_WCET && (g_work_package[work_pkg_id].WCET < best_wcet)) {
			best_pkg = work_pkg_id;
			best_src = (char *)json_string_value(json_object_get(pkg, "source"));
			best_tgt = tgt;
			best_wcet = g_work_package[work_pkg_id].WCET;
		}

		//
		// TODO:  Add Bounty Reward Profitability Check
		//

		else if ((opt_pref == PREF_PROFIT) && (profit > best_profit)) {
			best_pkg = work_pkg_id;
			best_src = (char *)json_string_value(json_object_get(pkg, "source"));
			best_tgt = tgt;
			best_profit = profit;
		}
		else if (opt_pref == PREF_WORKID && (!strcmp(g_work_package[work_pkg_id].work_str, pref_workid))) {
			best_pkg = work_pkg_id;
			best_src = (char *)json_string_value(json_object_get(pkg, "source"));
			best_tgt = tgt;
			break;
		}
	}

	// If No Work Matched Current Preference Switch To Profit Mode
	if (best_pkg < 0) {
		opt_pref = PREF_PROFIT;
		applog(LOG_INFO, "No work available that matches preference...retrying in %ds", opt_scantime);
		return -1;
	}

	// Get Updated Storage Data
	if (g_work_package[best_pkg].storage_sz && ((g_work.package_id != best_pkg) || (g_work.iteration_id != g_work_package[best_pkg].iteration_id))) {

		// Allocate Memory For Storage
		if (!g_work_package[best_pkg].storage) {
			g_work_package[best_pkg].storage = malloc(g_work_package[best_pkg].storage_sz * sizeof(uint32_t));
			if (!g_work_package[best_pkg].storage) {
				applog(LOG_ERR, "Unable to allocate storage for work_id: %s", g_work_package[best_pkg].work_str);
				return 0;
			}
		}

		// Get Storage Values From Node
		storage_id = get_work_storage(curl, g_work_package[best_pkg].work_str, g_work_package[best_pkg].storage);
		if (storage_id < 0) {
			applog(LOG_ERR, "ERROR: Unable to get 'storage' for work_id: %s", g_work_package[best_pkg].work_str);
			return 0;
		}else{
			applog(LOG_DEBUG, "First storage int for work_id %s is %u", g_work_package[best_pkg].work_str, g_work_package[best_pkg].storage[0]);
		}

		g_work_package[best_pkg].storage_id = storage_id;
	}

	// Copy Work Package Details To Work
	work->package_id = best_pkg;
	work->block_id = g_work_package[best_pkg].block_id;
	work->work_id = g_work_package[best_pkg].work_id;
	strncpy(work->work_str, g_work_package[best_pkg].work_str, 21);
	strncpy(work->work_nm, g_work_package[best_pkg].work_nm, 49);

	// Convert Hex Target To Int Array
	rc = hex2ints(work->pow_target, 4, tgt, strlen(tgt));
	if (!rc) {
		applog(LOG_ERR, "Invalid Target in JSON response for work_id: %s", work->work_str);
		return 0;
	}

	return 1;
}

static bool get_work_source(CURL *curl, char *work_str, char *elastic_src) {
	int err, rc;
	char req[100], *str = NULL;
	size_t num_pkg;
	json_t *val, *wrk, *pkg;
	struct timeval tv_start, tv_end, diff;

	sprintf(req, "requestType=getWork&work_id=%s&with_source=1&with_finished=0", work_str);

	gettimeofday(&tv_start, NULL);
	val = json_rpc_call(curl, rpc_url, rpc_userpass, req, &err);
	if (!val) {
		applog(LOG_ERR, "ERROR: 'json_rpc_call' for 'source' failed...retrying in %d seconds", opt_fail_pause);
		sleep(opt_fail_pause);
		return false;
	}

	gettimeofday(&tv_end, NULL);
	if (opt_protocol) {
		timeval_subtract(&diff, &tv_end, &tv_start);
		applog(LOG_DEBUG, "DEBUG: Time to get source: %.2f ms", (1000.0 * diff.tv_sec) + (0.001 * diff.tv_usec));
	}

	if (opt_protocol) {
		str = json_dumps(val, JSON_INDENT(3));
		applog(LOG_DEBUG, "DEBUG: JSON Response -\n%s", str);
		free(str);
	}

	wrk = json_object_get(val, "work_packages");

	if (!wrk) {
		applog(LOG_ERR, "Invalid JSON response to getWork request");
		return 0;
	}

	// Check If Any Active Work Packages Are Available
	num_pkg = json_array_size(wrk);
	if (num_pkg != 1) {
		applog(LOG_INFO, "Unable to retrieve source for work_id %s", work_str);
		return false;
	}

	pkg = json_array_get(wrk, 0);

	// Get Encrypted Source From JSON Message
	str = (char *)json_string_value(json_object_get(pkg, "source_code"));

	// Extract The ElasticPL Source Code
	if (!str || strlen(str) > MAX_SOURCE_SIZE || strlen(str) == 0) {
		applog(LOG_ERR, "ERROR: Invalid 'source' for work_id: %s", work_str);
		return false;
	}

	elastic_src = malloc(MAX_SOURCE_SIZE);
	if (!elastic_src) {
		applog(LOG_ERR, "ERROR: Unable to allocate memory for ElasticPL Source");
		return false;
	}

	rc = ascii85dec(elastic_src, MAX_SOURCE_SIZE, str);
	if (!rc) {
		applog(LOG_ERR, "ERROR: Unable to decode 'source' for work_id: %s\n\n%s\n", work_str, str);
		free(elastic_src);
		return false;
	}

	applog(LOG_DEBUG, "DEBUG: Running ElasticPL Parser");

	if (opt_debug_epl)
		applog(LOG_DEBUG, "DEBUG: ElasticPL Source Code -\n%s", elastic_src);

	// Convert ElasticPL Into AST
	if (!create_epl_ast(elastic_src)) {
		applog(LOG_ERR, "ERROR: Unable to convert 'source' to AST for work_id: %s\n\n%s\n", work_str, str);
		free(elastic_src);
		return false;
	}

	gettimeofday(&tv_start, NULL);
	if (opt_protocol) {
		timeval_subtract(&diff, &tv_start, &tv_end);
		applog(LOG_DEBUG, "DEBUG: Time to parse source: %.2f ms", (1000.0 * diff.tv_sec) + (0.001 * diff.tv_usec));
	}

	free(elastic_src);
	json_decref(val);

	return true;
}

static int get_work_storage(CURL *curl, char *work_str, uint32_t *storage) {
	int err;
	uint32_t storage_id, iteration_id;
	size_t num_pkg, max_str_len;
	char req[250], *str = NULL;
	json_t *val, *wrk, *pkg;
	struct timeval tv_start, tv_end, diff;

	sprintf(req, "requestType=getWork&work_id=%s&with_source=false&with_finished=false&with_storage=true", work_str);

	gettimeofday(&tv_start, NULL);
	val = json_rpc_call(curl, rpc_url, rpc_userpass, req, &err);
	if (!val) {
		applog(LOG_ERR, "ERROR: 'json_rpc_call' for 'storage' failed...retrying in %d seconds", opt_fail_pause);
		sleep(opt_fail_pause);
		return -1;
	}

	gettimeofday(&tv_end, NULL);
	if (opt_protocol) {
		timeval_subtract(&diff, &tv_end, &tv_start);
		applog(LOG_DEBUG, "DEBUG: Time to get storage: %.2f ms", (1000.0 * diff.tv_sec) + (0.001 * diff.tv_usec));
	}

	if (opt_protocol) {
		str = json_dumps(val, JSON_INDENT(3));
		applog(LOG_DEBUG, "DEBUG: JSON Response -\n%s", str);
		free(str);
	}

	wrk = json_object_get(val, "work_packages");

	if (!wrk) {
		applog(LOG_ERR, "Invalid JSON response to getWork request");
		return 0;
	}

	// Check If Any Active Work Packages Are Available
	num_pkg = json_array_size(wrk);
	if (num_pkg != 1) {
		applog(LOG_INFO, "Unable to retrieve storage for work_id %s", work_str);
		return false;
	}

	pkg = json_array_get(wrk, 0);

	// Get Storage Values From JSON Message
	str = (char *)json_string_value(json_object_get(pkg, "storage"));
	storage_id = (uint32_t)json_integer_value(json_object_get(pkg, "storage_id"));

// TODO: Need To Add Current Iteration Number To Message

	int iterations = (int)json_integer_value(json_object_get(pkg, "iterations"));
	int iterations_left = (int)json_integer_value(json_object_get(pkg, "iterations_left"));
	iteration_id = iterations - iterations_left;

	// Storage Is Optional For Iteration 0
	if (!str && (iteration_id == 0)) {
		json_decref(val);
		return 0xFFFF;
	}

	if (!str) {
		applog(LOG_ERR, "ERROR: Invalid 'storage' for work_id: %s", work_str);
		json_decref(val);
		return -1;
	}

	if (!hex2ints(storage, sizeof(*storage), str, max_str_len)) {
		applog(LOG_ERR, "ERROR: Unable to convert 'storage' for work_id: %s", work_str);
		json_decref(val);
		return -1;
	}

	gettimeofday(&tv_start, NULL);
	if (opt_protocol) {
		timeval_subtract(&diff, &tv_start, &tv_end);
		applog(LOG_DEBUG, "DEBUG: Time to convert storage: %.2f ms", (1000.0 * diff.tv_sec) + (0.001 * diff.tv_usec));
	}

	json_decref(val);

	return storage_id;
}

static bool submit_work(CURL *curl, struct submit_req *req) {
	int err, submit_data_sz;
	json_t *val = NULL;
	struct timeval tv_start, tv_end, diff;
	char *url = NULL, *data = NULL, *submit_data_hex = NULL, *err_desc = NULL;

	url = calloc(1, strlen(rpc_url) + 50);
	if (!url) {
		applog(LOG_ERR, "ERROR: Unable to allocate memory for submit work url");
		return false;
	}

	if ((req->req_type == SUBMIT_BOUNTY) || (req->req_type == SUBMIT_POW)) {

		// Allocate Memory For Data Buffers
		submit_data_sz = (req->submit_data_sz * 8) + 1;
		data = calloc(1, 512 + submit_data_sz);
		if (!data) {
			applog(LOG_ERR, "ERROR: Unable to allocate memory to submit work data");
			return false;
		}
		if (req->submit_data_sz) {
			submit_data_hex = calloc(1, submit_data_sz);
			if (!submit_data_hex) {
				applog(LOG_ERR, "ERROR: Unable to allocate memory for submit data");
				return false;
			}
		}

		if (req->submit_data_sz) {
			if(!ints2hex(req->submit_data, req->submit_data_sz, submit_data_hex, submit_data_sz))
				return false;
		}
		else {
			submit_data_hex = calloc(1, sizeof(char));
		}

		if (req->req_type == SUBMIT_BOUNTY) {
			sprintf(url, "%s?requestType=submitSolution", rpc_url);
			sprintf(data, "deadline=3&work_id=%s&data=%s&multiplicator=%s&storage_id=%d&is_pow=false&hash=%s&secretPhrase=%s", req->work_str, submit_data_hex, req->mult, req->storage_id, req->hash, passphrase);
		}
		else {
			sprintf(url, "%s?requestType=submitSolution", rpc_url);
//			sprintf(data, "deadline=3&work_id=%s&data=%s&multiplicator=%s&storage_id=%d&is_pow=true&hash=&secretPhrase=%s", req->work_str, submit_data_hex, req->mult, req->storage_id, passphrase);
			sprintf(data, "deadline=1&work_id=%s&data=%s&multiplicator=%s&storage_id=%d&is_pow=true&hash=%s&secretPhrase=%s", req->work_str, submit_data_hex, req->mult, req->storage_id, req->hash, passphrase);
		}
	}
	else {
		applog(LOG_ERR, "ERROR: Unknown request type");
		req->req_type = SUBMIT_COMPLETE;
		free(url);
		return true;
	}

	gettimeofday(&tv_start, NULL);
	val = json_rpc_call(curl, url, rpc_userpass, data, &err);

	if (!val)
		return false;

	gettimeofday(&tv_end, NULL);
	if (opt_protocol) {
		timeval_subtract(&diff, &tv_end, &tv_start);
		applog(LOG_DEBUG, "DEBUG: Time to submit solution: %.2f ms", (1000.0 * diff.tv_sec) + (0.001 * diff.tv_usec));
	}

	// Mask Passphrase
	data[strlen(data) - strlen(passphrase)] = 0;

	applog(LOG_DEBUG, "DEBUG: Submit request - %s %s", url, data);

	err_desc = (char *)json_string_value(json_object_get(val, "errorDescription"));

	if (err_desc)
		applog(LOG_DEBUG, "DEBUG: Submit response - %s", err_desc);

	if (req->req_type == SUBMIT_BOUNTY) {
		if (err_desc) {
			if (strstr(err_desc, "successfully")) {
				applog(LOG_NOTICE, "%s: %s***** Bounty Claimed! *****", thr_info[req->thr_id].name, CL_GRN);
				g_bounty_accepted_cnt++;
			}
			else if ((strstr(err_desc, "limit")) || (strstr(err_desc, "closed"))) {
				applog(LOG_NOTICE, "%s: %s***** Bounty Discarded *****", thr_info[req->thr_id].name, CL_YLW);
				applog(LOG_INFO, "Work ID: %s - All Bounties for this job have been claimed.", req->work_str);
				applog(LOG_DEBUG, "Work ID: %s, Reason: %s", req->work_str, err_desc);
				pthread_mutex_lock(&longpoll_lock);
				g_new_block = true;
				pthread_mutex_unlock(&longpoll_lock);
				pthread_mutex_lock(&submit_lock);
				*g_bounty_ignore=true;
				pthread_mutex_unlock(&submit_lock);
				g_bounty_deprecated_cnt++;
			}
			else {
				applog(LOG_NOTICE, "%s: %s***** Bounty Rejected! *****", thr_info[req->thr_id].name, CL_RED);
				g_bounty_rejected_cnt++;
			}
		}
		else {
			applog(LOG_NOTICE, "%s: %s***** Bounty Claimed! *****", thr_info[req->thr_id].name, CL_GRN);
			g_bounty_accepted_cnt++;
		}
		req->req_type = SUBMIT_COMPLETE;
	}
	else if (req->req_type == SUBMIT_POW) {
		if (err_desc) {
			if (strstr(err_desc, "successfully")) {
				applog(LOG_NOTICE, "%s: %s***** POW Accepted *****", thr_info[req->thr_id].name, CL_CYN);
				g_pow_accepted_cnt++;
			}
			else if (strstr(err_desc, "limit")) {
				applog(LOG_NOTICE, "%s: %s***** POW Discarded *****", thr_info[req->thr_id].name, CL_YLW);
				applog(LOG_INFO, "Work ID: %s - All POW rewards for this block have been claimed.", req->work_str);
				applog(LOG_DEBUG, "Work ID: %s, Reason: %s", req->work_str, err_desc);
				pthread_mutex_lock(&submit_lock);
				*g_pow_ignore=true;
				pthread_mutex_unlock(&submit_lock);
				g_pow_discarded_cnt++;
			}
			else {
				applog(LOG_NOTICE, "%s: %s***** POW Rejected! *****", thr_info[req->thr_id].name, CL_RED);
				applog(LOG_INFO, "Work ID: %s, Reason: %s", req->work_str, err_desc);
				g_pow_rejected_cnt++;
			}
		}
		else {
			applog(LOG_NOTICE, "%s: %s***** POW Accepted! *****", thr_info[req->thr_id].name, CL_CYN);
			g_pow_accepted_cnt++;
		}
		req->req_type = SUBMIT_COMPLETE;
	}

	json_decref(val);
	free(url);
	if (data) free(data);
	if (submit_data_hex) free(submit_data_hex);

	return true;
}

bool validate_work_source(int package_id, struct instance *inst) {
	int rc;
	uint32_t i;
	uint32_t pow_hash_main1[4], pow_hash_main2[4], pow_hash_verify1[4], pow_hash_verify2[4];
	uint32_t bounty_found, pow_found;

	int32_t *tmp_i = NULL;
	uint32_t *tmp_m = NULL, *tmp_u = NULL, *tmp_s = NULL;
	int64_t *tmp_l = NULL;
	uint64_t *tmp_ul = NULL;
	float *tmp_f = NULL;
	double *tmp_d = NULL;

	applog(LOG_DEBUG, "Validate Work - Checking 'main' and 'verify' logic...");

	// Declare Arrays To Store Randomized Test Data
	tmp_m = calloc(VM_M_ARRAY_SIZE, sizeof(uint32_t));
	if (g_work_package[package_id].vm_ints) tmp_i = calloc(g_work_package[package_id].vm_ints, sizeof(int32_t));
	if (g_work_package[package_id].vm_uints) tmp_u = calloc(g_work_package[package_id].vm_uints, sizeof(uint32_t));
	if (g_work_package[package_id].vm_longs) tmp_l = calloc(g_work_package[package_id].vm_longs, sizeof(int64_t));
	if (g_work_package[package_id].vm_ulongs) tmp_ul = calloc(g_work_package[package_id].vm_ulongs, sizeof(uint64_t));
	if (g_work_package[package_id].vm_floats) tmp_f = calloc(g_work_package[package_id].vm_floats, sizeof(float));
	if (g_work_package[package_id].vm_doubles) tmp_d = calloc(g_work_package[package_id].vm_doubles, sizeof(double));
	if (g_work_package[package_id].storage_sz) tmp_s = calloc(g_work_package[package_id].storage_sz, sizeof(uint32_t));

	// Create Random Test Data
	for (i = 0; i < VM_M_ARRAY_SIZE; i++)
		tmp_m[i] = (uint32_t)genrand_int32();

	for (i = 0; i < g_work_package[package_id].vm_ints; i++)
		tmp_i[i] = (int32_t)genrand_int32();

	for (i = 0; i < g_work_package[package_id].vm_uints; i++)
		tmp_u[i] = (uint32_t)genrand_int32();

	for (i = 0; i < g_work_package[package_id].vm_longs; i++)
		tmp_l[i] = (int64_t)genrand_int32();

	for (i = 0; i < g_work_package[package_id].vm_ulongs; i++)
		tmp_ul[i] = (uint64_t)genrand_int32();

	for (i = 0; i < g_work_package[package_id].vm_floats; i++)
		tmp_f[i] = (float)genrand_int32();

	for (i = 0; i < g_work_package[package_id].vm_doubles; i++)
		tmp_d[i] = (double)genrand_int32();

	for (i = 0; i < g_work_package[package_id].storage_sz; i++)
		tmp_s[i] = (uint32_t)genrand_int32();

	// Initialize m[] & s[] Arrays With Their Test Data
	memcpy(vm_m, tmp_m, VM_M_ARRAY_SIZE * sizeof(uint32_t));

	if (g_work_package[0].storage_sz)
		memcpy(vm_s, tmp_s, g_work_package[0].storage_sz * sizeof(uint32_t));

	// Intialize All Other Variable Arrays w/ Zeros
	if (g_work_package[package_id].vm_ints) memset(vm_i, 0, g_work_package[package_id].vm_ints * sizeof(int32_t));
	if (g_work_package[package_id].vm_uints) memset(vm_u, 0, g_work_package[package_id].vm_uints * sizeof(uint32_t));
	if (g_work_package[package_id].vm_longs) memset(vm_l, 0, g_work_package[package_id].vm_longs * sizeof(int64_t));
	if (g_work_package[package_id].vm_ulongs) memset(vm_ul, 0, g_work_package[package_id].vm_ulongs * sizeof(uint64_t));
	if (g_work_package[package_id].vm_floats) memset(vm_f, 0, g_work_package[package_id].vm_floats * sizeof(float));
	if (g_work_package[package_id].vm_doubles) memset(vm_d, 0, g_work_package[package_id].vm_doubles * sizeof(double));



	// Run "main" Function
	rc = inst->execute(g_work_package[package_id].work_id, &bounty_found, 1, &pow_found, g_pow_target, pow_hash_main1);
	applog(LOG_DEBUG, "\tValidate Work - 'main'   Hash1: %08X %08X %08X %08X", pow_hash_main1[0], pow_hash_main1[1], pow_hash_main1[2], pow_hash_main1[3]);

	// Now copy the random test data to the variable arrays, if a different
	// hash is returned, we know there are uninitialized variables being used
	memcpy(vm_m, tmp_m, VM_M_ARRAY_SIZE * sizeof(uint32_t));
	if (g_work_package[0].vm_ints) memcpy(vm_i, tmp_i, g_work_package[0].vm_ints * sizeof(int32_t));
	if (g_work_package[0].vm_uints) memcpy(vm_u, tmp_u, g_work_package[0].vm_uints * sizeof(uint32_t));
	if (g_work_package[0].vm_longs) memcpy(vm_l, tmp_l, g_work_package[0].vm_longs * sizeof(int64_t));
	if (g_work_package[0].vm_ulongs) memcpy(vm_ul, tmp_ul, g_work_package[0].vm_ulongs * sizeof(uint64_t));
	if (g_work_package[0].vm_floats) memcpy(vm_f, tmp_f, g_work_package[0].vm_floats * sizeof(float));
	if (g_work_package[0].vm_doubles) memcpy(vm_d, tmp_d, g_work_package[0].vm_doubles * sizeof(double));
	if (g_work_package[0].storage_sz) memcpy(vm_s, tmp_s, g_work_package[0].storage_sz * sizeof(uint32_t));




	// Run "main" Function Again
	rc = inst->execute(g_work_package[package_id].work_id, &bounty_found, 1, &pow_found, g_pow_target, pow_hash_main2);
	applog(LOG_DEBUG, "\tValidate Work - 'main'   Hash2: %08X %08X %08X %08X", pow_hash_main2[0], pow_hash_main2[1], pow_hash_main2[2], pow_hash_main2[3]);

	// Compare Hashes
	for (i = 0; i < 4; i++) {
		if (pow_hash_main1[i] != pow_hash_main2[i]) {
			applog(LOG_ERR, "Validate Work - ElasticPL 'main' function is using uninitialized values");
			return false;
		}
	}

	// Initialize m[] & s[] Arrays With Their Test Data
	memcpy(vm_m, tmp_m, VM_M_ARRAY_SIZE * sizeof(uint32_t));

	if (g_work_package[0].storage_sz)
		memcpy(vm_s, tmp_s, g_work_package[0].storage_sz * sizeof(uint32_t));

	// Intialize All Other Variable Arrays w/ Zeros
	if (g_work_package[package_id].vm_ints) memset(vm_i, 0, g_work_package[package_id].vm_ints * sizeof(int32_t));
	if (g_work_package[package_id].vm_uints) memset(vm_u, 0, g_work_package[package_id].vm_uints * sizeof(uint32_t));
	if (g_work_package[package_id].vm_longs) memset(vm_l, 0, g_work_package[package_id].vm_longs * sizeof(int64_t));
	if (g_work_package[package_id].vm_ulongs) memset(vm_ul, 0, g_work_package[package_id].vm_ulongs * sizeof(uint64_t));
	if (g_work_package[package_id].vm_floats) memset(vm_f, 0, g_work_package[package_id].vm_floats * sizeof(float));
	if (g_work_package[package_id].vm_doubles) memset(vm_d, 0, g_work_package[package_id].vm_doubles * sizeof(double));

	// Run "verify" Function
	rc = inst->verify(g_work_package[0].work_id, &bounty_found, 1, &pow_found, g_pow_target, pow_hash_verify1);
	applog(LOG_DEBUG, "\tValidate Work - 'verify' Hash1: %08X %08X %08X %08X", pow_hash_verify1[0], pow_hash_verify1[1], pow_hash_verify1[2], pow_hash_verify1[3]);

	// Copy Randomized Input To VM Memory
	memcpy(vm_m, tmp_m, VM_M_ARRAY_SIZE * sizeof(uint32_t));
	if (g_work_package[0].vm_ints) memcpy(vm_i, tmp_i, g_work_package[0].vm_ints * sizeof(int32_t));
	if (g_work_package[0].vm_uints) memcpy(vm_u, tmp_u, g_work_package[0].vm_uints * sizeof(uint32_t));
	if (g_work_package[0].vm_longs) memcpy(vm_l, tmp_l, g_work_package[0].vm_longs * sizeof(int64_t));
	if (g_work_package[0].vm_ulongs) memcpy(vm_ul, tmp_ul, g_work_package[0].vm_ulongs * sizeof(uint64_t));
	if (g_work_package[0].vm_floats) memcpy(vm_f, tmp_f, g_work_package[0].vm_floats * sizeof(float));
	if (g_work_package[0].vm_doubles) memcpy(vm_d, tmp_d, g_work_package[0].vm_doubles * sizeof(double));
	if (g_work_package[0].storage_sz) memcpy(vm_s, tmp_s, g_work_package[0].storage_sz * sizeof(uint32_t));

	// Run "verify" Function Again
	rc = inst->verify(g_work_package[0].work_id, &bounty_found, 1, &pow_found, g_pow_target, pow_hash_verify2);
	applog(LOG_DEBUG, "\tValidate Work - 'verify' Hash2: %08X %08X %08X %08X", pow_hash_verify2[0], pow_hash_verify2[1], pow_hash_verify2[2], pow_hash_verify2[3]);

	if(tmp_i) free(tmp_i);
	if(tmp_u) free(tmp_u);
	if(tmp_l) free(tmp_l);
	if(tmp_ul) free(tmp_ul);
	if(tmp_f) free(tmp_f);
	if(tmp_d) free(tmp_d);
	if(tmp_s) free(tmp_s);
	if(tmp_m) free(tmp_m);

	// Compare Hashes
	for (i = 0; i < 4; i++) {
		if (pow_hash_verify1[i] != pow_hash_verify2[i]) {
			applog(LOG_ERR, "Validate Work - ElasticPL 'verify' function is using uninitialized values");
			return false;
		}
	}

	// Compare "main" Hash to "verify"
	for (i = 0; i < 4; i++) {
		if (pow_hash_verify2[i] != pow_hash_main2[i]) {
			applog(LOG_ERR, "Validate Work - ElasticPL 'main' & 'verify' functions return conflicting results");
			return false;
		}
	}

	applog(LOG_DEBUG, "Validate Work - Success");

	return true;
}

static void *cpu_miner_thread(void *userdata) {
	struct thr_info *mythr = (struct thr_info *) userdata;
	int thr_id = mythr->id;
	struct work work;
	struct workio_cmd *wc = NULL;
	char s[16];
	long hashes_done;
	struct timeval tv_start, tv_end, diff;
	int rc = 0;
	double eval_rate;
	struct instance *inst = NULL;
	uint32_t rnd = 0, iteration = 0;

	uint32_t vm_ints = 0;
	uint32_t vm_uints = 0;
	uint32_t vm_longs = 0;
	uint32_t vm_ulongs = 0;
	uint32_t vm_floats = 0;
	uint32_t vm_doubles = 0;
	uint32_t vm_storage = 0;

	// Set lower priority
	if (!opt_norenice)
		thread_low_priority();

	// Initialize Global Variables
	vm_m = calloc(VM_M_ARRAY_SIZE, sizeof(uint32_t));

	if (!vm_m ) {
		applog(LOG_ERR, "CPU%d: Unable to allocate VM memory", thr_id);
		goto out;
	}

	hashes_done = 0;
	memset(&work, 0, sizeof(work));
	gettimeofday((struct timeval *) &tv_start, NULL);

	while (1) {
		pthread_mutex_lock(&longpoll_lock);
		bool res = *g_pow_ignore && *g_bounty_ignore;
		pthread_mutex_unlock(&longpoll_lock);
		if(res){
			sleep(1);
			continue;
			// do not work if there is not chance to submit anyway
		}

		// No Work Available
		if (!g_work.work_id) {
			if (work.work_id)
				memset(&work, 0, sizeof(struct work));
			sleep(1);
			continue;
		}

		// Check If We Are Mining The Most Current Work
		if (work.work_id != g_work.work_id) {

			// Copy Global Work Into Local Thread Work
			memcpy((void *)&work, (void *)&g_work, sizeof(struct work));
			work.thr_id = thr_id;

			// Allocate Memory For Global Variables
			if (g_work_package[work.package_id].vm_ints > vm_ints) {
				vm_ints = g_work_package[work.package_id].vm_ints;
				vm_i = realloc(vm_i, vm_ints * sizeof(int32_t));
				memset(vm_i, 0, vm_ints * sizeof(int32_t));
			}
			if (g_work_package[work.package_id].vm_uints > vm_uints) {
				vm_uints = g_work_package[work.package_id].vm_uints;
				vm_u = realloc(vm_u, vm_uints * sizeof(uint32_t));
				memset(vm_u, 0, vm_uints * sizeof(uint32_t));
			}
			if (g_work_package[work.package_id].vm_longs > vm_longs) {
				vm_longs = g_work_package[work.package_id].vm_longs;
				vm_l = realloc(vm_l, vm_longs * sizeof(int64_t));
				memset(vm_l, 0, vm_longs * sizeof(int64_t));
			}
			if (g_work_package[work.package_id].vm_ulongs > vm_ulongs) {
				vm_ulongs = g_work_package[work.package_id].vm_ulongs;
				vm_ul = realloc(vm_ul, vm_ulongs * sizeof(uint64_t));
				memset(vm_ul, 0, vm_ulongs * sizeof(uint64_t));
			}
			if (g_work_package[work.package_id].vm_floats > vm_floats) {
				vm_floats = g_work_package[work.package_id].vm_floats;
				vm_f = realloc(vm_f, vm_floats * sizeof(float));
				memset(vm_f, 0, vm_floats * sizeof(float));
			}
			if (g_work_package[work.package_id].vm_doubles > vm_doubles) {
				vm_doubles = g_work_package[work.package_id].vm_doubles;
				vm_d = realloc(vm_d, vm_doubles * sizeof(double));
				memset(vm_d, 0, vm_doubles * sizeof(double));
			}
			if (g_work_package[work.package_id].storage_sz > vm_storage) {
				vm_storage = g_work_package[work.package_id].storage_sz;
				vm_s = realloc(vm_s, vm_storage * sizeof(uint32_t));
				memset(vm_s, 0, vm_storage * sizeof(uint32_t));
			}

			if ((vm_ints && !vm_i) ||
				(vm_uints && !vm_u) ||
				(vm_longs && !vm_l) ||
				(vm_ulongs && !vm_ul) ||
				(vm_floats && !vm_f) ||
				(vm_doubles && !vm_d) ||
				(vm_storage && !vm_s)) {

				applog(LOG_ERR, "CPU%d: Unable to allocate VM memory", thr_id);
				goto out;
			}

			// Create A Compiled VM Instance For The Thread
			if (inst)
				free_library(inst);
			inst = calloc(1, sizeof(struct instance));
			create_instance(inst, work.work_str);

			// Set Round / Iteration For The Work
			rnd = 0;
			iteration = g_work_package[work.package_id].iteration_id;

			// Copy New Storage Values To VM
			if (g_work_package[work.package_id].storage_sz) {
				if (g_work_package[work.package_id].storage_id < 0xFFFF)
					memcpy(vm_s, g_work_package[work.package_id].storage, g_work_package[work.package_id].storage_sz * sizeof(uint32_t));
				else
					memset(vm_s, 0, g_work_package[work.package_id].storage_sz * sizeof(uint32_t));
			}

			inst->initialize(vm_m, vm_i, vm_u, vm_l, vm_ul, vm_f, vm_d, vm_s);


		}
		// Otherwise, Just Update POW Target / Iteration / Storage
		else {
			memcpy(&work.pow_target, &g_work.pow_target, 4 * sizeof(uint32_t));

			if (work.iteration_id != g_work.iteration_id) {

				// Set Round / Iteration For The Work
				rnd = 0;
				iteration = g_work_package[work.package_id].iteration_id;

				// Copy New Storage Values To VM
				if (g_work_package[work.package_id].storage_sz) {
					if (g_work_package[work.package_id].storage_id < 0xFFFF)
						memcpy(vm_s, g_work_package[work.package_id].storage, g_work_package[work.package_id].storage_sz * sizeof(uint32_t));
					else
						memset(vm_s, 0, g_work_package[work.package_id].storage_sz * sizeof(uint32_t));
				}
			}
		}

		work_restart[thr_id].restart = 0;

		// Run VM To Check For POW Hash & Bounties
		rc = execute_vm(thr_id, &rnd, iteration, &work, inst, &hashes_done);

		// Record Elapsed Time
		gettimeofday(&tv_end, NULL);
		timeval_subtract(&diff, &tv_end, &tv_start);

		if (diff.tv_sec >= 1 && !opt_quiet) {
			eval_rate = (double)(hashes_done / (diff.tv_sec + diff.tv_usec * 1e-6));
			// Sent status UDP Datagram to localhost
			sprintf(s,"%d/%d/%d",(int)eval_rate,g_pow_accepted_cnt,g_bounty_rejected_cnt);
			udpSend(s);
		}

		if (diff.tv_sec >= 5) {
			eval_rate = (double)(hashes_done / (diff.tv_sec + diff.tv_usec * 1e-6));
			if (!opt_quiet) {
				sprintf(s, eval_rate >= 1000.0 ? "%0.2f kFLOP/s" : "%0.2f FLOP/s", (eval_rate >= 1000.0) ? eval_rate / 1000 : eval_rate);
				applog(LOG_INFO, "CPU%d: %s", thr_id, s);
			}
			gettimeofday((struct timeval *) &tv_start, NULL);
			hashes_done = 0;
		}

		// Submit Work That Meets Bounty Criteria
		pthread_mutex_lock(&longpoll_lock);
		bool ign = *g_bounty_ignore;
		pthread_mutex_unlock(&longpoll_lock);
		if (ign==false && (rc == 1 && (ignore_mask&2)==0)) {

			if(delay_sleep>0)
				sleep(delay_sleep);

			applog(LOG_DEBUG, "CPU%d: Submitting Bounty Solution", thr_id);

			// Create Submit Request
			wc = (struct workio_cmd *) calloc(1, sizeof(*wc));
			if (!wc) {
				applog(LOG_ERR, "ERROR: Unable to allocate workio_cmd.  Shutting down thread for CPU%d", thr_id);
				goto out;
			}

			wc->cmd = SUBMIT_BOUNTY;
			wc->thr = mythr;
			memcpy(&wc->work, &work, sizeof(struct work));

			// Save Values To Be Submitted To Node
			if (g_work_package[work.package_id].submit_sz) {
				wc->submit_data = malloc(g_work_package[work.package_id].submit_sz * sizeof(uint32_t));
				memcpy(wc->submit_data, &vm_u[g_work_package[work.package_id].storage_idx], g_work_package[work.package_id].submit_sz * sizeof(uint32_t));
			}

			// Add Solution To Queue
			if (!tq_push(thr_info[work_thr_id].q, wc)) {
				applog(LOG_ERR, "ERROR: Unable to add solution to queue.  Shutting down thread for CPU%d", thr_id);
				if (wc->submit_data) free(wc->submit_data);
				free(wc);
				goto out;
			}


		}

		// Submit Work That Meets POW Target
		pthread_mutex_lock(&longpoll_lock);
		ign = *g_pow_ignore;
		pthread_mutex_unlock(&longpoll_lock);
		if (ign==false && (rc == 2 && (ignore_mask&1)==0)) {

			if(delay_sleep>0)
				sleep(delay_sleep);

			applog(LOG_DEBUG, "CPU%d: Submitting POW Solution", thr_id);
			applog(LOG_DEBUG, "DEBUG: Hash - %08X%08X%08X...  Tgt - %s", work.pow_hash[0], work.pow_hash[1], work.pow_hash[2], g_pow_target_str);
			applog(LOG_DEBUG, "DEBUG: First 4 Inputs were: %d, %d, %d, %d", work.vm_input[0], work.vm_input[1], work.vm_input[2], work.vm_input[3]);
			pthread_mutex_lock(&longpoll_lock);
			bool ign2 = *g_pow_ignore && *g_bounty_ignore;
			pthread_mutex_unlock(&longpoll_lock);
			if(ign2){
						sleep(1);
						continue;
						// do not work if there is not chance to submit anyway
					}

			wc = (struct workio_cmd *) calloc(1, sizeof(*wc));
			if (!wc) {
				applog(LOG_ERR, "ERROR: Unable to allocate workio_cmd.  Shutting down thread for CPU%d", thr_id);
				goto out;
			}

			wc->cmd = SUBMIT_POW;
			wc->thr = mythr;
			memcpy(&wc->work, &work, sizeof(struct work));

			// Save Values To Be Submitted To Node
			if (g_work_package[work.package_id].submit_sz) {
				wc->submit_data = malloc(g_work_package[work.package_id].submit_sz * sizeof(uint32_t));
				memcpy(wc->submit_data, &vm_u[g_work_package[work.package_id].storage_idx], g_work_package[work.package_id].submit_sz * sizeof(uint32_t));
			}

			// Add Solution To Queue
			if (!tq_push(thr_info[work_thr_id].q, wc)) {
				applog(LOG_ERR, "ERROR: Unable to add solution to queue.  Shutting down thread for CPU%d", thr_id);
				if (wc->submit_data) free(wc->submit_data);
				free(wc);
				goto out;
			}

		}
	}

out:
	if (inst)
		free_library(inst);
	if (inst) free(inst);
	inst = NULL;
	if (vm_m) free(vm_m);
	if (vm_i) free(vm_i);
	if (vm_u) free(vm_u);
	if (vm_l) free(vm_l);
	if (vm_ul) free(vm_ul);
	if (vm_f) free(vm_f);
	if (vm_d) free(vm_d);
	if (vm_s) free(vm_s);

	tq_freeze(mythr->q);

	return NULL;
}

#ifdef USE_OPENCL
static void *gpu_miner_thread(void *userdata) {
	struct thr_info *mythr = (struct thr_info *) userdata;
	int thr_id = mythr->id;
	uint32_t *vm_input = NULL, *vm_output = NULL, *vm_submit = NULL, *vm_round = NULL, *vm_result = NULL;
	struct work work;
	struct workio_cmd *wc = NULL;
	uint64_t hashes_done;
	struct timeval tv_start, tv_end, diff;
	int rc = 0;
	uint32_t vm_storage = 0;
	unsigned char *ocl_source, str[50];
	double eval_rate;
	struct instance *inst = NULL;
	uint32_t *mult32 = (uint32_t *)work.multiplicator;

	// Set lower priority
	if (!opt_norenice)
		thread_low_priority();

	// Initialize Arrays To Hold OpenCL Core Inputs / Outputs
	vm_input = (uint32_t *)calloc(24, sizeof(uint32_t));
	vm_output = (uint32_t *)calloc(5, sizeof(uint32_t));
	vm_round = (uint32_t *)calloc(1, sizeof(uint32_t));
	vm_result = (uint32_t *)calloc(1, sizeof(uint32_t));
	vm_submit = NULL;

	if (!vm_input || !vm_output) {
		applog(LOG_ERR, "ERROR: Unable to allocate VM memory");
		goto out;
	}

	// Create OpenCL Buffers
	if (!opencl_create_buffers(&gpu[thr_id])) {
		applog(LOG_ERR, "ERROR: Unable to create OpenCL Buffers");
		goto out;
	}

	hashes_done = 0;
	memset(&work, 0, sizeof(work));
	gettimeofday((struct timeval *) &tv_start, NULL);

	while (1) {

		// No Work Available
		if (!g_work.work_id) {
			if (work.work_id)
				memset(&work, 0, sizeof(struct work));
			sleep(1);
			continue;
		}

		// Check If We Are Mining The Most Current Work
		if (work.work_id != g_work.work_id) {



			// Copy Global Work Into Local Thread Work
			memcpy(&work, &g_work, sizeof(struct work));
			work.thr_id = thr_id;

			// Allocate Memory For Storage
			if (g_work_package[work.package_id].storage_sz > vm_storage) {
				vm_storage = g_work_package[work.package_id].storage_sz;
				vm_submit = realloc(vm_s, vm_storage * sizeof(uint32_t));
				vm_s = realloc(vm_s, vm_storage * sizeof(uint32_t));
				memset(vm_s, 0, vm_storage * sizeof(uint32_t));

				if (vm_storage && (!vm_s || !vm_submit)) {
					applog(LOG_ERR, "GPU%d: Unable to allocate memory for storage", thr_id);
					goto out;
				}
			}

			// Copy New Storage Values To VM
			if (g_work_package[work.package_id].storage_sz) {
				if (g_work_package[work.package_id].storage_id < 0xFFFF)
					memcpy(vm_s, g_work_package[work.package_id].storage, g_work_package[work.package_id].storage_sz * sizeof(uint32_t));
				else
					memset(vm_s, 0, g_work_package[work.package_id].storage_sz * sizeof(uint32_t));
			}

			// Randomize Inputs
			mult32[0] = thr_id;												// Ensures Each Thread Is Unique
			mult32[1] = 0;													// Round - Value Will Be Incremented For Each GPU Thread
			mult32[2] = g_work_package[work.package_id].iteration_id;		// Iteration
			mult32[3] = 0;													// GPU OpenCL Thread ID
			mult32[7] = genrand_int32();									// Random Number

			// Load & Compile OpenCL Code
			ocl_source = opencl_load_source(work.work_str);
			if (!ocl_source) {
				memset(&work ,0, sizeof(struct work));
				sleep(15);
				continue;
			}

			// Calculate Worksize For New ElasticPL Job
			if (!opencl_calc_worksize(&gpu[thr_id])) {
				memset(&work, 0, sizeof(struct work));
				sleep(15);
				continue;
			}

			// Create OpenCL Kernel / Set Arguments
			if (!opencl_create_kernel(&gpu[thr_id], ocl_source, g_work_package[0].storage_sz)) {
				memset(&work, 0, sizeof(struct work));
				free(ocl_source);
				sleep(15);
				continue;
			}

			free(ocl_source);
		}
		else {
			// Update Target For Work
			memcpy(&work.pow_target, &g_work.pow_target, 4 * sizeof(uint32_t));

			if (work.iteration_id != g_work.iteration_id) {

				// Randomize Inputs
				mult32[0] = thr_id;												// Ensures Each Thread Is Unique
				mult32[1] = 0;													// Round - Value Will Be Incremented After Each Pass
				mult32[2] = g_work_package[work.package_id].iteration_id;		// Iteration
				mult32[3] = 0;													// GPU OpenCL Thread ID
				mult32[7] = genrand_int32();									// Random Number

				// Copy New Storage Values To VM
				if (g_work_package[work.package_id].storage_sz) {
					if (g_work_package[work.package_id].iteration_id)
						memcpy(vm_s, g_work_package[work.package_id].storage, g_work_package[work.package_id].storage_sz * sizeof(uint32_t));
					else
						memset(vm_s, 0, g_work_package[work.package_id].storage_sz * sizeof(uint32_t));
				}
			}

		}

		work_restart[thr_id].restart = 0;

		// Get Values For VM Inputs
		get_opencl_base_data(&work, vm_input);

		// Execute The VM
		if (!opencl_run_kernel(&gpu[thr_id], vm_round, vm_result, vm_output, vm_submit, g_work_package[0].storage_sz))
			goto out;

		if (!vm_result[0])
			continue;

		// Update Multiplicator To Include Round Number & Thread ID
		mult32[1] = vm_output[0];						// Round Number
		mult32[3] = vm_output[0] % gpu[thr_id].threads;	// GPU Thread ID

		// Copy Hash To Work Structure
		memcpy(&work.pow_hash[0], &vm_output[1], 16);

		// Check For Bounty Solutions
		pthread_mutex_lock(&longpoll_lock);
		bool ign = *g_bounty_ignore;
		pthread_mutex_unlock(&longpoll_lock);

		// Check For POW Solutions
		pthread_mutex_lock(&longpoll_lock);
		bool ignpow = *g_pow_ignore;
		pthread_mutex_unlock(&longpoll_lock);
		if (ign==false && (vm_result[0] > 1)) {
			applog(LOG_NOTICE, "%s - %d: Submitting Bounty Solution", mythr->name, mult32[3]);

			// Create Submit Request
			wc = (struct workio_cmd *) calloc(1, sizeof(*wc));
			if (!wc) {
				applog(LOG_ERR, "ERROR: Unable to allocate workio_cmd.  Shutting down thread for %s", mythr->name);
				goto out;
			}

			wc->cmd = SUBMIT_BOUNTY;
			wc->thr = mythr;
			memcpy(&wc->work, &work, sizeof(struct work));

			// Save Values To Be Submitted To Node
			if (g_work_package[work.package_id].submit_sz) {
				wc->submit_data = malloc(g_work_package[work.package_id].submit_sz * sizeof(uint32_t));
				memcpy(wc->submit_data, vm_submit, g_work_package[work.package_id].submit_sz * sizeof(uint32_t));
			}

			// Add Solution To Queue
			if (!tq_push(thr_info[work_thr_id].q, wc)) {
				applog(LOG_ERR, "ERROR: Unable to add solution to queue.  Shutting down thread for GPU%d", thr_id);
				if (wc->submit_data) free(wc->submit_data);
				free(wc);
				goto out;
			}
		}
		else if (ignpow==false && ((vm_result[0] == 1) || (vm_result[0] == 3))) {

			if (opt_debug) {
				applog(LOG_DEBUG, "DEBUG: Hash - %08X%08X%08X%08X  Tgt - %08X%08X%08X%08X", work.pow_hash[0], work.pow_hash[1], work.pow_hash[2], work.pow_hash[3], g_pow_target[0], g_pow_target[1], g_pow_target[2], g_pow_target[3]);
			}

			applog(LOG_NOTICE, "%s - %d: Submitting POW Solution", mythr->name, mult32[3]);

			wc = (struct workio_cmd *) calloc(1, sizeof(*wc));
			if (!wc) {
				applog(LOG_ERR, "ERROR: Unable to allocate workio_cmd.  Shutting down thread for %s", mythr->name);
				goto out;
			}

			wc->cmd = SUBMIT_POW;
			wc->thr = mythr;
			memcpy(&wc->work, &work, sizeof(struct work));

			// Save Values To Be Submitted To Node
			if (g_work_package[work.package_id].submit_sz) {
				wc->submit_data = malloc(g_work_package[work.package_id].submit_sz * sizeof(uint32_t));
				memcpy(wc->submit_data, vm_submit, g_work_package[work.package_id].submit_sz * sizeof(uint32_t));
			}

			// Add Solution To Queue
			if (!tq_push(thr_info[work_thr_id].q, wc)) {
				applog(LOG_ERR, "ERROR: Unable to add solution to queue.  Shutting down thread for GPU%d", thr_id);
				if (wc->submit_data) free(wc->submit_data);
				free(wc);
				goto out;
			}
		}

		hashes_done += gpu[thr_id].threads;

		// Record Elapsed Time
		gettimeofday(&tv_end, NULL);
		timeval_subtract(&diff, &tv_end, &tv_start);
		if (diff.tv_sec >= 5) {

			mult32[7] = genrand_int32();

			if (!opt_quiet) {
				eval_rate = (double)((hashes_done / (diff.tv_sec + (diff.tv_usec / 1000000.0))) / 1000.0);
				sprintf(str, eval_rate >= 1000.0 ? "%0.2f mFLOP/s" : "%0.2f kFLOP/s", (eval_rate >= 1000.0) ? eval_rate / 1000 : eval_rate);
				applog(LOG_INFO, "%s: %s", mythr->name, str);
			}
			gettimeofday((struct timeval *) &tv_start, NULL);
			hashes_done = 0;
		}

		// Increment multiplicator
		mult32[1] += 1;	// Increment Round
		mult32[3] = 0;	// Reset GPU OpenCL Thread ID
	}

out:
	/*
	clReleaseMemObject(base_data);
	clReleaseMemObject(input);
	clReleaseMemObject(output);
	clReleaseKernel(kernel_execute);
	clReleaseKernel(kernel_initialize);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);
	*/
	if (vm_input) free(vm_input);
	if (vm_output) free(vm_output);
	if (vm_round) free(vm_round);
	if (vm_result) free(vm_result);
	if (vm_submit) free(vm_submit);
	if (vm_s) free(vm_s);
	tq_freeze(mythr->q);

	return NULL;
}
#endif

static void restart_threads(void)
{
	int i;
	for (i = 0; i < opt_n_threads; i++)
		work_restart[i].restart = 1;
}

static void *longpoll_thread(void *userdata)
{
	struct thr_info *mythr = (struct thr_info *) userdata;
	CURL *curl;
	json_t *val = NULL, *obj, *inner_obj;
	int i, err, num_events;
	char *str = NULL;
	bool nosleep = false;

	curl = curl_easy_init();
	if (!curl) {
		applog(LOG_ERR, "CURL initialization failed");
		return NULL;
	}

	pthread_mutex_lock(&longpoll_lock);
	g_new_block = false;
	pthread_mutex_unlock(&longpoll_lock);

	sleep(1);

	while (1) {

		nosleep = false;

		if (val) json_decref(val);
		val = NULL;

		val = json_rpc_call(curl, rpc_url, rpc_userpass, "requestType=longpoll&randomId=1", &err);
		if (err > 0 || !val) {
			applog(LOG_ERR, "ERROR: longpoll failed...retrying in %d seconds", opt_fail_pause);
			sleep(opt_fail_pause);
			continue;
		}

		if (opt_protocol) {
			str = json_dumps(val, JSON_INDENT(3));
			applog(LOG_DEBUG, "DEBUG: JSON Response -\n%s", str);
			free(str);
		}

		obj = json_object_get(val, "event");
		if (!obj) {
			applog(LOG_ERR, "ERROR: longpoll decode failed...retrying in %d seconds", opt_fail_pause);
			sleep(opt_fail_pause);
			continue;
		}

		if (json_is_string(obj)) {
			str = (char *)json_string_value(obj);
			if (strcmp(str, "timeout") == 0) {
				continue;
			}
			else {
				if (strstr(str, "block")) {
					applog(LOG_NOTICE, "Longpoll: detected %s", str);
					pthread_mutex_lock(&longpoll_lock);
					g_new_block = true;
					nosleep = true;

					// Reset POW Counter When Block Changes
					g_cur_pow_cnt = 0;
					*g_pow_ignore = false;
					*g_bounty_ignore = false;
					pthread_mutex_unlock(&longpoll_lock);
				}
			}
		}
		else if (json_is_array(obj)) {
			num_events = json_array_size(obj);
			if (num_events == 0) {
				applog(LOG_ERR, "ERROR: longpoll decode failed...retrying in %d seconds", opt_fail_pause);
				sleep(opt_fail_pause);
				continue;
			}

			for (i = 0; i<num_events; i++) {
				inner_obj = json_array_get(obj, i);
				if (!inner_obj) {
					applog(LOG_ERR, "ERROR: longpoll array parsing failed...retrying in %d seconds", opt_fail_pause);
					sleep(opt_fail_pause);
					continue;
				}

				if (json_is_string(inner_obj)) {
					str = (char *)json_string_value(inner_obj);
					if (strstr(str, "block")) {
						applog(LOG_NOTICE, "Longpoll: detected %s", str);
						pthread_mutex_lock(&longpoll_lock);
						g_new_block = true;
						nosleep = true;

						// Reset POW Counter When Block Changes
						g_cur_pow_cnt = 0;
						*g_pow_ignore = false;
						*g_bounty_ignore = false;
						pthread_mutex_unlock(&longpoll_lock);
					}
				}
			}
		}

		if (!nosleep)
			sleep(1);
	}

	tq_freeze(mythr->q);
	curl_easy_cleanup(curl);

	return NULL;
}

static void *workio_thread(void *userdata)
{
	struct thr_info *mythr = (struct thr_info *) userdata;
	CURL *curl;
	struct workio_cmd *wc;
	int i, failures;

	curl = curl_easy_init();
	if (!curl) {
		applog(LOG_ERR, "CURL initialization failed");
		return NULL;
	}

	failures = 0;

	while (1) {

		sleep(1);

		// Get Work (New Block or Every 'Scantime' To Check For Difficulty Change)
		if (g_new_block || (time(NULL) - g_work_time) >= opt_scantime) {
			applog(LOG_DEBUG, "Getting new work (reason: new block or just too long ago)");


			if (!get_work(curl)) {
				if ((opt_retries >= 0) && (++failures > opt_retries)) {
					applog(LOG_ERR, "ERROR: 'json_rpc_call' failed...terminating workio thread");
					break;
				}

				/* pause, then restart work-request loop */
				applog(LOG_ERR, "ERROR: 'json_rpc_call' failed...retrying in %d seconds", opt_fail_pause);
				sleep(opt_fail_pause);
				continue;
			}

			failures = 0;
			pthread_mutex_lock(&longpoll_lock);
			g_new_block = false;
			pthread_mutex_unlock(&longpoll_lock);


		}

		// Check For New Solutions On Queue
		wc = (struct workio_cmd *) tq_pop_nowait(mythr->q);
		while (wc) {
			add_submit_req(&wc->work, wc->submit_data, wc->cmd);
			free(wc);  // Storage Will Be Cleaned Up After Submit
			wc = (struct workio_cmd *) tq_pop_nowait(mythr->q);
		}

		// Submit POW / Bounty
		for (i = 0; i < g_submit_req_cnt; i++) {

// TODO - The Hold / Complete logic was originally used when bounties were announced, then confirmed.
//        The logic will remain in place until it's determined if we need any retry logic around bounty submissions

			// Skip Completed Requests
			if (g_submit_req[i].req_type == SUBMIT_COMPLETE)
				continue;

			// Skip Requests That Are On Hold
			if (g_submit_req[i].delay_tm >= time(NULL))
				continue;

			// Submit Request
			if (!submit_work(curl, &g_submit_req[i]))
				applog(LOG_ERR, "ERROR: Submit bounty request failed");
		}

		// Remove Completed Solutions
		for (i = 0; i < g_submit_req_cnt; i++) {
			if (g_submit_req[i].req_type == SUBMIT_COMPLETE) {
				applog(LOG_DEBUG, "DEBUG: Submit complete...deleting request");
				delete_submit_req(i);
				continue;
			}

			// Remove Stale Requests After 15min
			if (time(NULL) - g_submit_req[i].start_tm >= 900) {
				applog(LOG_DEBUG, "DEBUG: Submit request timed out after 15min");
				delete_submit_req(i);
				g_bounty_timeout_cnt++;
			}
		}

		// Sleep For 15 Sec If No Work
		if (!g_work.work_id) {
			for (i = 0; i < 15; i++) {
				sleep(1);
				if (g_new_block || g_submit_req_cnt)
					break;
			}
		}
	}

	tq_freeze(mythr->q);
	curl_easy_cleanup(curl);

	return NULL;
}

static bool add_submit_req(struct work *work, uint32_t *data, enum submit_commands req_type) {

	pthread_mutex_lock(&submit_lock);

	if (req_type == SUBMIT_POW) {
		// Ignore Stale Submissions
		if (work->block_id != g_cur_block_id) {
			pthread_mutex_unlock(&submit_lock);
			return true;
		}

		// Don't Exceed Max POW Sumbissions Per Block
		if (g_cur_pow_cnt < MAX_POW_PER_BLOCK) {
			pthread_mutex_lock(&longpoll_lock);
			g_cur_pow_cnt++;
			pthread_mutex_unlock(&longpoll_lock);
		}
		else {
			pthread_mutex_lock(&longpoll_lock);
			if(*g_pow_ignore==false){
				*g_pow_ignore = true;
				applog(LOG_NOTICE, "%s***** miner has already submitted %d POW this block, we pause for a while *****", CL_YLW, MAX_POW_PER_BLOCK);
			}
			pthread_mutex_unlock(&longpoll_lock);

			pthread_mutex_unlock(&submit_lock);

			return true;
		}
	}

	g_submit_req = realloc(g_submit_req, (g_submit_req_cnt + 1) * sizeof(struct submit_req));
	if (!g_submit_req) {
		g_submit_req_cnt = 0;
		applog(LOG_ERR, "ERROR: Bounty request allocation failed");
		return false;
	}
	g_submit_req[g_submit_req_cnt].thr_id = work->thr_id;
	g_submit_req[g_submit_req_cnt].bounty = false;
	g_submit_req[g_submit_req_cnt].req_type = req_type;
	g_submit_req[g_submit_req_cnt].start_tm = time(NULL);
	g_submit_req[g_submit_req_cnt].delay_tm = 0;
	g_submit_req[g_submit_req_cnt].retries = 0;
	g_submit_req[g_submit_req_cnt].work_id = work->work_id;
	strncpy(g_submit_req[g_submit_req_cnt].work_str, work->work_str, 21);
	bin2hex((unsigned char *)work->multiplicator, 32, g_submit_req[g_submit_req_cnt].mult, 65);
	bin2hex((unsigned char *)work->pow_hash, 16, g_submit_req[g_submit_req_cnt].hash, 33);
	g_submit_req[g_submit_req_cnt].iteration_id = work->iteration_id;
	g_submit_req[g_submit_req_cnt].storage_id = (g_work_package[work->package_id].storage_id < 0xFFFF) ? g_work_package[work->package_id].storage_id : 0;
	g_submit_req[g_submit_req_cnt].submit_data_sz = g_work_package[work->package_id].submit_sz;
	g_submit_req[g_submit_req_cnt].submit_data = data;
	if (req_type != SUBMIT_POW) {
		g_submit_req[g_submit_req_cnt].bounty = true;
		update_pending_cnt(work->work_id, true);
	}
	g_submit_req_cnt++;

	pthread_mutex_unlock(&submit_lock);
	return true;
}

static bool delete_submit_req(int idx) {
	struct submit_req *req = NULL;
	int i;

	pthread_mutex_lock(&submit_lock);

	// Clear Storage Buffer
	if (g_submit_req[idx].submit_data)
		free(g_submit_req[idx].submit_data);

	if (g_submit_req[idx].bounty)
		update_pending_cnt(g_submit_req[idx].work_id, false);

	if (g_submit_req_cnt > 1) {
		req = malloc((g_submit_req_cnt - 1) * sizeof(struct submit_req));
		if (!req) {
			applog(LOG_ERR, "ERROR: Bounty request allocation failed");
			pthread_mutex_unlock(&submit_lock);
			return false;
		}
	}
	else {
		if (g_submit_req) free(g_submit_req);
		g_submit_req = 0;
		g_submit_req_cnt = 0;
		pthread_mutex_unlock(&submit_lock);
		return true;
	}

	for (i = 0; i < idx; i++)
		memcpy(&req[i], &g_submit_req[i], sizeof(struct submit_req));

	for (i = idx + 1; i < g_submit_req_cnt; i++)
		memcpy(&req[i - 1], &g_submit_req[i], sizeof(struct submit_req));

	free(g_submit_req);
	g_submit_req = req;
	g_submit_req_cnt--;
	pthread_mutex_unlock(&submit_lock);
	return true;
}

static void *key_monitor_thread(void *userdata)
{
	int i, ch, day, hour, min, sec, total_sec, pending_bty;
	struct timeval now;

	while (true)
	{
		sleep(1);
		ch = getchar();
		ch = toupper(ch);
		if (ch == '\n')
			continue;

		switch (ch)
		{
		case 'S':
		{
			gettimeofday(&now, NULL);
			total_sec = now.tv_sec - g_miner_start_time.tv_sec;
			day = total_sec / 3600 / 24;
			hour = total_sec / 3600 - day * 24;
			min = total_sec / 60 - (day * 24 + hour) * 60;
			sec = total_sec % 60;

			pending_bty = 0;
			for (i = 0; i < g_work_package_cnt; i++) {
				pending_bty += g_work_package[i].pending_bty_cnt;
			}

			applog(LOG_WARNING, "************************** Mining Summary **************************");
			applog(LOG_WARNING, "Run Time: %02d Days %02d:%02d:%02d\t\tWork Name: %s", day, hour, min, sec, g_work_nm ? g_work_nm : "");
			applog(LOG_WARNING, "Bounty Pending:\t%3d\t\tWork ID:   %s", pending_bty, g_work_id ? g_work_id : "");
			applog(LOG_WARNING, "Bounty Accept:\t%3d\t\tTarget:    %s", g_bounty_accepted_cnt, g_pow_target_str);
			applog(LOG_WARNING, "Bounty Reject:\t%3d", g_bounty_rejected_cnt);
			applog(LOG_WARNING, "Bounty Deprct:\t%3d\t\tPOW Accept:  %3d", g_bounty_deprecated_cnt, g_pow_accepted_cnt);
			applog(LOG_WARNING, "Bounty Timeout:\t%3d\t\tPOW Reject:  %3d", g_bounty_timeout_cnt, g_pow_rejected_cnt);
			applog(LOG_WARNING, "Bounty Error:\t%3d\t\tPOW Discard: %3d", g_bounty_error_cnt, g_pow_discarded_cnt);
			applog(LOG_WARNING, "********************************************************************");

		}
		break;
		case 'D':
			opt_debug = !opt_debug;
			applog(LOG_WARNING, "Debug Mode: %s", opt_debug ? "On" : "Off");
			break;
		case 'E':
			opt_debug_epl = !opt_debug_epl;
			applog(LOG_WARNING, "Debug ElasticPL Mode: %s", opt_debug_epl ? "On" : "Off");
			break;
		case 'P':
			opt_protocol = !opt_protocol;
			applog(LOG_WARNING, "Protocol Mode: %s", opt_protocol ? "On" : "Off");
			break;
		case 'Q':
			opt_quiet = !opt_quiet;
			applog(LOG_WARNING, "Quiet Mode: %s", opt_quiet ? "On" : "Off");
			break;
		case 'V':
			opt_debug_vm = !opt_debug_vm;
			applog(LOG_WARNING, "Debug VM Mode: %s", opt_debug_vm ? "On" : "Off");
			break;
		}
	}
	return 0;
}

#ifndef WIN32
static void signal_handler(int sig)
{
	switch (sig) {
	case SIGHUP:
		applog(LOG_INFO, "SIGHUP received");
		break;
	case SIGINT:
		applog(LOG_INFO, "SIGINT received, exiting");
		exit(0);
		break;
	case SIGTERM:
		applog(LOG_INFO, "SIGTERM received, exiting");
		exit(0);
		break;
	}
}
#else
BOOL WINAPI ConsoleHandler(DWORD dwType)
{
	switch (dwType) {
	case CTRL_C_EVENT:
		applog(LOG_INFO, "CTRL_C_EVENT received, exiting");
		exit(0);
		break;
	case CTRL_BREAK_EVENT:
		applog(LOG_INFO, "CTRL_BREAK_EVENT received, exiting");
		exit(0);
		break;
	default:
		return false;
	}
	return true;
}
#endif

static void free_up(){

	int i;

		applog(LOG_DEBUG, "Cleaning up");

		if(g_bounty_ignore) free((void*)g_bounty_ignore);
		if(g_pow_ignore) free((void*)g_pow_ignore);
		if (rpc_url) free(rpc_url);
		if (rpc_user) free(rpc_user);
		if (rpc_pass) free(rpc_pass);
		if (rpc_userpass) free(rpc_userpass);

		if (test_filename)
			free(test_filename);
		clear_all_workpackages();

		// now clear Threads
		if(thr_info){
			for(i=0; i<opt_n_threads + 3; ++i){
					// clear theads
					if(thr_info[i].q)
						tq_free(thr_info[i].q);
			}
			free(thr_info);
		}

		// clear work restart
		if(work_restart)
			free(work_restart);


		if(thr_deadswitch){
			tq_free(thr_deadswitch->q);
			free(thr_deadswitch);
		}
}

static int thread_create(struct thr_info *thr, void* func)
{
	int err = 0;
	pthread_attr_init(&thr->attr);
	err = pthread_create(&thr->pth, &thr->attr, func, thr);
	pthread_attr_destroy(&thr->attr);
	return err;
}

int main(int argc, char **argv) {
	struct thr_info *thr;
	int i, err, thr_idx, num_gpus = 0;

	g_pow_ignore = (bool*)malloc(sizeof(bool));
	g_bounty_ignore = (bool*)malloc(sizeof(bool));
	*g_pow_ignore = false;
	*g_bounty_ignore = false;

	fprintf(stdout, "** Elastic Compute Engine **\n");
	fprintf(stdout, "   Miner Version: " MINER_VERSION"\n");
	fprintf(stdout, "   ElasticPL Version: " ELASTICPL_VERSION"\n");

	pthread_mutex_init(&applog_lock, NULL);
	if(opt_deadswitch>0)
		pthread_mutex_init(&went_through_lock, NULL);

#if defined(WIN32)
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	num_cpus = sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_CONF)
	num_cpus = sysconf(_SC_NPROCESSORS_CONF);
#elif defined(CTL_HW) && defined(HW_NCPU)
	int req[] = { CTL_HW, HW_NCPU };
	size_t len = sizeof(num_cpus);
	sysctl(req, 2, &num_cpus, &len, NULL, 0);
#else
	num_cpus = 1;
#endif
	if (num_cpus < 1)
		num_cpus = 1;

	// Process Command Line Before Starting Any Threads
	parse_cmdline(argc, argv);

#ifdef USE_OPENCL
	// Initialize GPU Devices
	if (opt_opencl) {
		num_gpus = opencl_init_devices();
		if (num_gpus == 0) {
			applog(LOG_ERR, "ERROR: No OpenCL Devices that support 64bit Floating Point math found");
			return 1;
		}
	}
#else
	if (opt_opencl) {
		applog(LOG_ERR, "ERROR: OpenCL not found on this system.  Running miner with compiled C code instead");
		opt_opencl = false;
	}
#endif


	if (!opt_n_threads) {
		if (!opt_opencl)
			opt_n_threads = num_cpus;
		else
			opt_n_threads = num_gpus;
	}

	if (!rpc_url)
		rpc_url = strdupcs("http://127.0.0.1:6876/nxt");

	if (rpc_user && rpc_pass) {
		rpc_userpass = (char*)malloc(strlen(rpc_user) + strlen(rpc_pass) + 2);
		if (!rpc_userpass){
			free_up();
			return 1;
		}
		sprintf(rpc_userpass, "%s:%s", rpc_user, rpc_pass);
	}

	if (!opt_test_vm && !passphrase) {
		applog(LOG_ERR, "ERROR: Passphrase (option -P) is required");
		free_up();
		return 1;
	}

	// Seed Random Number Generator
	init_genrand((unsigned long)time(NULL));
//	RAND_poll();

	// Reset Package / Iteration
	g_work.package_id = -1;
	g_work.iteration_id = -1;

#ifndef WIN32
	/* Always catch Ctrl+C */
	signal(SIGINT, signal_handler);
#else
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE);
#endif

	work_restart = (struct work_restart*) calloc(opt_n_threads, sizeof(*work_restart));
	if (!work_restart){
		free_up();
		return 1;
	}
	thr_info = (struct thr_info*) calloc(opt_n_threads + 3, sizeof(struct thr_info));
	if (!thr_info){
		free_up();
		return 1;
	}

	// In Test Compiler Mode, Run Parser / Complier Using Source Code From Test File
	if (opt_test_vm) {
		thr = &thr_info[0];
		thr->id = 0;
		thr->q = tq_new();
		if (!thr->q){
			free_up();
			return 1;
		}

		if(opt_deadswitch > 0){
			thr_deadswitch = (struct thr_info*) calloc(1, sizeof(*thr_deadswitch));
			if (thread_create(thr_deadswitch, deadswitch)) {
				applog(LOG_ERR, "Test VM thread create failed!");
				free_up();
				return 1;
			}

		}

		if (thread_create(thr, test_vm_thread)) {
			applog(LOG_ERR, "Test VM thread create failed!");
			free_up();
			return 1;
		}

		pthread_join(thr_info[0].pth, NULL);
		if(opt_deadswitch>0){
			pthread_mutex_lock(&went_through_lock);
			went_through=true;
			pthread_mutex_unlock(&went_through_lock);
			pthread_join(thr_deadswitch->pth, NULL);
		}
		free_up();
		return 0;
	}

	// Miner Threads
	pthread_mutex_init(&work_lock, NULL);
	pthread_mutex_init(&submit_lock, NULL);
	pthread_mutex_init(&longpoll_lock, NULL);

	// Init workio Thread Info
	work_thr_id = opt_n_threads;
	thr = &thr_info[work_thr_id];
	thr->id = work_thr_id;
	thr->q = tq_new();
	if (!thr->q){
		free_up();
		return 1;
	}

	// Start workio Thread
	if (thread_create(thr, workio_thread)) {
		applog(LOG_ERR, "work thread create failed");
		free_up();
		return 1;
	}

	applog(LOG_INFO, "Attempting to start %d miner threads", opt_n_threads);

	thr_idx = 0;

	// Start Mining Threads
	for (i = 0; i < opt_n_threads; i++) {
		thr = &thr_info[thr_idx];

		thr->id = thr_idx++;
		thr->q = tq_new();
		if (!thr->q){
			free_up();
			return 1;
		}
		if (opt_opencl) {
#ifdef USE_OPENCL
			err = thread_create(thr, gpu_miner_thread);
#endif
			sprintf(thr->name, "GPU%d", i);
		}
		else {
			err = thread_create(thr, cpu_miner_thread);
			sprintf(thr->name, "CPU%d", i);
		}
		if (err) {
			applog(LOG_ERR, "%s mining thread create failed!", thr->name);
			free_up();
			return 1;
		}
	}

	applog(LOG_INFO, "%d mining threads started", opt_n_threads);

	gettimeofday(&g_miner_start_time, NULL);

	// Start Longpoll Thread
	thr = &thr_info[opt_n_threads + 1];
	thr->id = opt_n_threads + 1;
	thr->q = tq_new();
	if (!thr->q){
		free_up();
		return 1;
	}
	if (thread_create(thr, longpoll_thread)) {
		applog(LOG_ERR, "Longpoll thread create failed");
		free_up();
		return 1;
	}

	// Start Key Monitor Thread
	thr = &thr_info[opt_n_threads + 2];
	thr->id = opt_n_threads + 2;
	thr->q = tq_new();
	if (!thr->q){
		free_up();
		return 1;
	}
	if (thread_create(thr, key_monitor_thread)) {
		applog(LOG_ERR, "Key monitor thread create failed");
		free_up();
		return 1;
	}

	// Main Loop - Wait for workio thread to exit
	pthread_join(thr_info[work_thr_id].pth, NULL);

	free_up();

	applog(LOG_WARNING, "Exiting " PACKAGE_NAME);

// TODO: Cleanup any still running threads on exit.

// TODO: Cleanup global workpackages on exit.

	return 0;
}
