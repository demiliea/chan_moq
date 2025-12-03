/*
 * Asterisk -- An open source telephony toolkit.
 *
 * chan_moq - Media over QUIC (MoQ) channel driver
 *
 * Copyright (C) 2025
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2.
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/config.h"
#include "asterisk/logger.h"
#include "asterisk/pbx.h"
#include "asterisk/acl.h"
#include "asterisk/callerid.h"
#include "asterisk/frame.h"
#include "asterisk/utils.h"
#include "asterisk/lock.h"
#include "asterisk/astobj2.h"
#include "asterisk/format_cache.h"
#include "asterisk/format_cap.h"
#include "asterisk/format.h"
#include "asterisk/sched.h"
#include "asterisk/io.h"

#include <pthread.h>

#define MOQ_CONFIG "moq.conf"
#define DEFAULT_CONTEXT "default"
#define FRAME_SIZE 160

/* Channel states */
enum moq_state {
	MOQ_STATE_DOWN,
	MOQ_STATE_CALLING,
	MOQ_STATE_RINGING,
	MOQ_STATE_UP,
	MOQ_STATE_HANGUP
};

/* MoQ session structure */
struct moq_session {
	struct ast_channel *owner;
	char session_id[64];
	char remote_id[64];
	enum moq_state state;
	struct ast_sockaddr media_addr;
	int media_socket;
	pthread_t media_thread;
	int running;
	ast_mutex_t lock;
	uint32_t ssrc;
	unsigned int lastts;
};

/* Global configuration */
static struct {
	char context[AST_MAX_CONTEXT];
	int running;
} moq_config;

static ast_mutex_t moq_lock = AST_MUTEX_INITIALIZER;

/* Forward declarations */
static struct ast_channel *moq_request(const char *type, struct ast_format_cap *cap,
	const struct ast_assigned_ids *assignedids, const struct ast_channel *requestor,
	const char *addr, int *cause);
static int moq_call(struct ast_channel *ast, const char *dest, int timeout);
static int moq_hangup(struct ast_channel *ast);
static struct ast_frame *moq_read(struct ast_channel *ast);
static int moq_write(struct ast_channel *ast, struct ast_frame *frame);
static int moq_answer(struct ast_channel *ast);
static int moq_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen);
static int moq_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);

/* Channel technology definition */
static struct ast_channel_tech moq_tech = {
	.type = "MOQ",
	.description = "Media over QUIC Channel Driver",
	.requester = moq_request,
	.call = moq_call,
	.hangup = moq_hangup,
	.answer = moq_answer,
	.read = moq_read,
	.write = moq_write,
	.indicate = moq_indicate,
	.fixup = moq_fixup,
};

/* Utility function to generate session ID */
static void generate_session_id(char *buf, size_t len)
{
	snprintf(buf, len, "moq-%08x-%04x", (unsigned int)time(NULL), (unsigned int)ast_random());
}

/* Media thread - handles MoQ media transport */
static void *moq_media_thread(void *data)
{
	struct moq_session *session = data;
	unsigned char buffer[FRAME_SIZE];
	struct ast_frame frame;
	fd_set fds;
	struct timeval tv;
	
	ast_log(LOG_NOTICE, "MoQ media thread started for session %s\n", session->session_id);
	
	while (session->running) {
		tv.tv_sec = 0;
		tv.tv_usec = 20000; /* 20ms */
		
		FD_ZERO(&fds);
		FD_SET(session->media_socket, &fds);
		
		int ret = select(session->media_socket + 1, &fds, NULL, NULL, &tv);
		if (ret > 0 && FD_ISSET(session->media_socket, &fds)) {
			struct sockaddr_storage from;
			socklen_t fromlen = sizeof(from);
			
			ssize_t len = recvfrom(session->media_socket, buffer, sizeof(buffer), 0,
				(struct sockaddr *)&from, &fromlen);
			
			if (len > 0 && session->owner) {
				/* Queue frame to Asterisk */
				memset(&frame, 0, sizeof(frame));
				frame.frametype = AST_FRAME_VOICE;
				frame.subclass.format = ast_format_ulaw;
				frame.data.ptr = buffer;
				frame.datalen = len;
				frame.samples = len;
				frame.src = "MOQ";
				
				ast_mutex_lock(&session->lock);
				if (session->owner) {
					ast_queue_frame(session->owner, &frame);
				}
				ast_mutex_unlock(&session->lock);
			}
		}
	}
	
	ast_log(LOG_NOTICE, "MoQ media thread stopped for session %s\n", session->session_id);
	return NULL;
}

/* Create new MoQ session */
static struct moq_session *moq_session_new(const char *dest)
{
	struct moq_session *session = ast_calloc(1, sizeof(*session));
	if (!session) {
		return NULL;
	}
	
	generate_session_id(session->session_id, sizeof(session->session_id));
	ast_copy_string(session->remote_id, dest, sizeof(session->remote_id));
	session->state = MOQ_STATE_DOWN;
	ast_mutex_init(&session->lock);
	
	/* Create media socket (UDP placeholder for MoQ/QUIC) */
	session->media_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (session->media_socket < 0) {
		ast_log(LOG_ERROR, "Failed to create media socket\n");
		ast_free(session);
		return NULL;
	}
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0; /* Let kernel assign port */
	
	if (bind(session->media_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		ast_log(LOG_ERROR, "Failed to bind media socket\n");
		close(session->media_socket);
		ast_free(session);
		return NULL;
	}
	
	session->running = 1;
	
	ast_log(LOG_NOTICE, "Created MoQ session %s for destination %s\n", 
		session->session_id, dest);
	
	return session;
}

/* Destroy MoQ session */
static void moq_session_destroy(struct moq_session *session)
{
	if (!session) {
		return;
	}
	
	ast_log(LOG_NOTICE, "Destroying MoQ session %s\n", session->session_id);
	
	session->running = 0;
	
	if (session->media_thread) {
		pthread_join(session->media_thread, NULL);
	}
	
	if (session->media_socket >= 0) {
		close(session->media_socket);
	}
	
	ast_mutex_destroy(&session->lock);
	ast_free(session);
}

/* Channel technology implementations */
static struct ast_channel *moq_request(const char *type, struct ast_format_cap *cap,
	const struct ast_assigned_ids *assignedids, const struct ast_channel *requestor,
	const char *addr, int *cause)
{
	struct ast_channel *chan;
	struct moq_session *session;
	struct ast_format *fmt;
	
	ast_log(LOG_NOTICE, "MoQ channel request: %s\n", addr);
	
	session = moq_session_new(addr);
	if (!session) {
		ast_log(LOG_ERROR, "Failed to create MoQ session\n");
		*cause = AST_CAUSE_CONGESTION;
		return NULL;
	}
	
	chan = ast_channel_alloc(1, AST_STATE_DOWN, NULL, NULL, NULL, NULL, NULL,
		assignedids, requestor, 0, "MOQ/%s", addr);
	
	if (!chan) {
		ast_log(LOG_ERROR, "Failed to allocate channel\n");
		moq_session_destroy(session);
		*cause = AST_CAUSE_CONGESTION;
		return NULL;
	}
	
	ast_channel_tech_set(chan, &moq_tech);
	ast_channel_nativeformats_set(chan, cap);
	
	fmt = ast_format_cap_get_format(cap, 0);
	ast_channel_set_writeformat(chan, fmt);
	ast_channel_set_readformat(chan, fmt);
	ast_channel_set_rawwriteformat(chan, fmt);
	ast_channel_set_rawreadformat(chan, fmt);
	ao2_ref(fmt, -1);
	
	session->owner = chan;
	ast_channel_tech_pvt_set(chan, session);
	
	ast_channel_unlock(chan);
	
	return chan;
}

static int moq_call(struct ast_channel *ast, const char *dest, int timeout)
{
	struct moq_session *session = ast_channel_tech_pvt(ast);
	
	if (!session) {
		ast_log(LOG_ERROR, "No session found for channel\n");
		return -1;
	}
	
	ast_log(LOG_NOTICE, "MoQ calling: %s (timeout: %d)\n", dest, timeout);
	
	session->state = MOQ_STATE_CALLING;
	ast_setstate(ast, AST_STATE_RINGING);
	
	/* Start media thread */
	if (pthread_create(&session->media_thread, NULL, moq_media_thread, session)) {
		ast_log(LOG_ERROR, "Failed to create media thread\n");
		return -1;
	}
	
	ast_queue_control(ast, AST_CONTROL_RINGING);
	
	/* Simulate call progress */
	usleep(500000); /* 500ms */
	ast_queue_control(ast, AST_CONTROL_ANSWER);
	session->state = MOQ_STATE_UP;
	ast_setstate(ast, AST_STATE_UP);
	
	return 0;
}

static int moq_hangup(struct ast_channel *ast)
{
	struct moq_session *session = ast_channel_tech_pvt(ast);
	
	if (!session) {
		return 0;
	}
	
	ast_log(LOG_NOTICE, "MoQ hangup: %s\n", session->session_id);
	
	session->state = MOQ_STATE_HANGUP;
	
	ast_mutex_lock(&session->lock);
	session->owner = NULL;
	ast_mutex_unlock(&session->lock);
	
	ast_channel_tech_pvt_set(ast, NULL);
	moq_session_destroy(session);
	
	return 0;
}

static struct ast_frame *moq_read(struct ast_channel *ast)
{
	/* Frames are queued by media thread */
	return &ast_null_frame;
}

static int moq_write(struct ast_channel *ast, struct ast_frame *frame)
{
	struct moq_session *session = ast_channel_tech_pvt(ast);
	
	if (!session || session->media_socket < 0) {
		return -1;
	}
	
	if (frame->frametype != AST_FRAME_VOICE) {
		return 0;
	}
	
	/* Send media via socket (would use MoQ/QUIC in production) */
	if (session->media_addr.ss.ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)&session->media_addr.ss;
		sendto(session->media_socket, frame->data.ptr, frame->datalen, 0,
			(struct sockaddr *)sin, sizeof(*sin));
	}
	
	return 0;
}

static int moq_answer(struct ast_channel *ast)
{
	struct moq_session *session = ast_channel_tech_pvt(ast);
	
	if (!session) {
		return -1;
	}
	
	ast_log(LOG_NOTICE, "MoQ answer: %s\n", session->session_id);
	
	session->state = MOQ_STATE_UP;
	ast_setstate(ast, AST_STATE_UP);
	
	/* Start media thread if not already running */
	if (!session->media_thread) {
		if (pthread_create(&session->media_thread, NULL, moq_media_thread, session)) {
			ast_log(LOG_ERROR, "Failed to create media thread\n");
			return -1;
		}
	}
	
	return 0;
}

static int moq_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen)
{
	(void)data;
	(void)datalen;
	
	switch (condition) {
		case AST_CONTROL_RINGING:
			ast_log(LOG_DEBUG, "MoQ indicate: ringing\n");
			return 0;
		case AST_CONTROL_PROGRESS:
			ast_log(LOG_DEBUG, "MoQ indicate: progress\n");
			return 0;
		case AST_CONTROL_PROCEEDING:
			ast_log(LOG_DEBUG, "MoQ indicate: proceeding\n");
			return 0;
		default:
			return -1;
	}
}

static int moq_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	struct moq_session *session = ast_channel_tech_pvt(newchan);
	
	(void)oldchan;
	
	if (session) {
		ast_mutex_lock(&session->lock);
		session->owner = newchan;
		ast_mutex_unlock(&session->lock);
	}
	
	return 0;
}

/* Load configuration */
static int load_config(int reload)
{
	struct ast_config *cfg;
	struct ast_variable *v;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };
	
	cfg = ast_config_load(MOQ_CONFIG, config_flags);
	
	if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		return 0;
	}
	
	if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file %s is invalid\n", MOQ_CONFIG);
		return -1;
	}
	
	if (!cfg) {
		ast_log(LOG_WARNING, "Config file %s not found, using defaults\n", MOQ_CONFIG);
		ast_copy_string(moq_config.context, DEFAULT_CONTEXT, sizeof(moq_config.context));
		return 0;
	}
	
	/* Parse general section */
	for (v = ast_variable_browse(cfg, "general"); v; v = v->next) {
		if (!strcasecmp(v->name, "context")) {
			ast_copy_string(moq_config.context, v->value, sizeof(moq_config.context));
		}
	}
	
	ast_config_destroy(cfg);
	
	return 0;
}

/* Module load */
static int load_module(void)
{
	ast_log(LOG_NOTICE, "Loading chan_moq module\n");
	
	/* Initialize configuration */
	memset(&moq_config, 0, sizeof(moq_config));
	ast_copy_string(moq_config.context, DEFAULT_CONTEXT, sizeof(moq_config.context));
	
	if (load_config(0)) {
		return AST_MODULE_LOAD_DECLINE;
	}
	
	moq_config.running = 1;
	
	/* Register channel technology */
	if (ast_channel_register(&moq_tech)) {
		ast_log(LOG_ERROR, "Failed to register channel technology\n");
		return AST_MODULE_LOAD_DECLINE;
	}
	
	ast_log(LOG_NOTICE, "chan_moq loaded successfully\n");
	
	return AST_MODULE_LOAD_SUCCESS;
}

/* Module unload */
static int unload_module(void)
{
	ast_log(LOG_NOTICE, "Unloading chan_moq module\n");
	
	moq_config.running = 0;
	
	/* Unregister channel technology */
	ast_channel_unregister(&moq_tech);
	
	ast_log(LOG_NOTICE, "chan_moq unloaded successfully\n");
	
	return 0;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Media over QUIC Channel Driver");
