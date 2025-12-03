/*
 * Asterisk -- An open source telephony toolkit.
 *
 * chan_moq - Media over QUIC (MoQ) channel driver with WebSocket signaling
 *
 * Copyright (C) 2025
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2.
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

/* Define module self symbol for external compilation */
#define AST_MODULE_SELF_SYM __internal_chan_moq_self
#define AST_MODULE "chan_moq"

/* Include third-party libraries that use pthread types BEFORE Asterisk headers */
#include <json-c/json.h>
#include <libwebsockets.h>

/* System headers */
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <endian.h>

/* Asterisk headers after system and third-party libraries */
#include <asterisk.h>
#include <asterisk/module.h>
#include <asterisk/channel.h>
#include <asterisk/config.h>
#include <asterisk/logger.h>
#include <asterisk/pbx.h>
#include <asterisk/acl.h>
#include <asterisk/callerid.h>
#include <asterisk/frame.h>
#include <asterisk/utils.h>
#include <asterisk/lock.h>
#include <asterisk/astobj2.h>
#include <asterisk/format_cache.h>
#include <asterisk/format_cap.h>
#include <asterisk/format.h>
#include <asterisk/rtp_engine.h>
#include <asterisk/sched.h>
#include <asterisk/io.h>
#include <asterisk/causes.h>

#define MOQ_CONFIG "moq.conf"
#define DEFAULT_WS_PORT 8088
#define DEFAULT_CONTEXT "default"
#define MOQ_QUIC_PORT 4433
#define MOQ_MAX_PACKET_SIZE 1500
#define MOQ_BUFFER_SIZE 8192

/* Channel states */
enum moq_state {
	MOQ_STATE_DOWN,
	MOQ_STATE_CALLING,
	MOQ_STATE_RINGING,
	MOQ_STATE_UP,
	MOQ_STATE_HANGUP
};

/* MoQ object types */
enum moq_object_type {
	MOQ_OBJECT_STREAM = 0,
	MOQ_OBJECT_DATAGRAM = 1,
	MOQ_OBJECT_TRACK = 2
};

/* MoQ message types */
enum moq_message_type {
	MOQ_MSG_SUBSCRIBE = 0x01,
	MOQ_MSG_SUBSCRIBE_OK = 0x02,
	MOQ_MSG_SUBSCRIBE_ERROR = 0x03,
	MOQ_MSG_ANNOUNCE = 0x04,
	MOQ_MSG_ANNOUNCE_OK = 0x05,
	MOQ_MSG_UNSUBSCRIBE = 0x06,
	MOQ_MSG_OBJECT = 0x07,
	MOQ_MSG_GOAWAY = 0x08
};

/* MoQ media frame header */
struct moq_media_header {
	uint8_t type;
	uint32_t track_id;
	uint64_t sequence;
	uint64_t timestamp;
	uint16_t payload_size;
} __attribute__((packed));

/* QUIC connection structure (simplified) */
struct moq_quic_conn {
	int socket_fd;
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	uint8_t *send_buffer;
	uint8_t *recv_buffer;
	size_t send_buffer_len;
	size_t recv_buffer_len;
	uint32_t connection_id;
	int connected;
};

/* MoQ session structure */
struct moq_session {
	struct ast_channel *owner;
	char session_id[64];
	char remote_id[64];
	enum moq_state state;
	struct ast_sockaddr media_addr;
	int media_socket;
	struct lws *ws;
	pthread_t media_thread;
	int running;
	ast_mutex_t lock;
	uint32_t ssrc;
	unsigned int lastts;
	
	/* MoQ/QUIC specific */
	struct moq_quic_conn *quic_conn;
	uint32_t track_id;
	uint64_t send_sequence;
	uint64_t recv_sequence;
	uint64_t last_timestamp;
};

/* Global configuration */
static struct {
	char context[AST_MAX_CONTEXT];
	int ws_port;
	struct lws_context *ws_context;
	pthread_t ws_thread;
	int running;
} moq_config;

AST_MUTEX_DEFINE_STATIC(moq_lock);

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

/* Create QUIC connection (simplified implementation) */
static struct moq_quic_conn *moq_quic_create(const char *host, int port)
{
	struct moq_quic_conn *conn = ast_calloc(1, sizeof(*conn));
	if (!conn) {
		return NULL;
	}
	
	/* Create UDP socket for QUIC */
	conn->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (conn->socket_fd < 0) {
		ast_log(LOG_ERROR, "Failed to create QUIC socket: %s\n", strerror(errno));
		ast_free(conn);
		return NULL;
	}
	
	/* Set non-blocking */
	int flags = fcntl(conn->socket_fd, F_GETFL, 0);
	fcntl(conn->socket_fd, F_SETFL, flags | O_NONBLOCK);
	
	/* Allocate buffers */
	conn->send_buffer = ast_malloc(MOQ_BUFFER_SIZE);
	conn->recv_buffer = ast_malloc(MOQ_BUFFER_SIZE);
	if (!conn->send_buffer || !conn->recv_buffer) {
		ast_log(LOG_ERROR, "Failed to allocate QUIC buffers\n");
		if (conn->send_buffer) ast_free(conn->send_buffer);
		if (conn->recv_buffer) ast_free(conn->recv_buffer);
		close(conn->socket_fd);
		ast_free(conn);
		return NULL;
	}
	
	/* Set up peer address */
	struct sockaddr_in *addr = (struct sockaddr_in *)&conn->peer_addr;
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	if (inet_pton(AF_INET, host, &addr->sin_addr) <= 0) {
		addr->sin_addr.s_addr = INADDR_ANY;
	}
	conn->peer_addr_len = sizeof(struct sockaddr_in);
	
	/* Generate connection ID */
	conn->connection_id = (uint32_t)ast_random();
	conn->connected = 0;
	
	ast_log(LOG_NOTICE, "Created MoQ QUIC connection (conn_id: 0x%08x)\n", 
		conn->connection_id);
	
	return conn;
}

/* Destroy QUIC connection */
static void moq_quic_destroy(struct moq_quic_conn *conn)
{
	if (!conn) {
		return;
	}
	
	if (conn->socket_fd >= 0) {
		close(conn->socket_fd);
	}
	
	if (conn->send_buffer) {
		ast_free(conn->send_buffer);
	}
	
	if (conn->recv_buffer) {
		ast_free(conn->recv_buffer);
	}
	
	ast_free(conn);
}

/* Send MoQ message over QUIC */
static int moq_quic_send_message(struct moq_quic_conn *conn, uint8_t msg_type, 
	const uint8_t *payload, size_t payload_len)
{
	if (!conn || conn->socket_fd < 0) {
		return -1;
	}
	
	/* Simple message format: [type(1)][length(2)][payload] */
	if (payload_len + 3 > MOQ_BUFFER_SIZE) {
		ast_log(LOG_ERROR, "MoQ message too large: %zu bytes\n", payload_len);
		return -1;
	}
	
	uint8_t *buf = conn->send_buffer;
	buf[0] = msg_type;
	buf[1] = (payload_len >> 8) & 0xFF;
	buf[2] = payload_len & 0xFF;
	
	if (payload && payload_len > 0) {
		memcpy(buf + 3, payload, payload_len);
	}
	
	ssize_t sent = sendto(conn->socket_fd, buf, payload_len + 3, 0,
		(struct sockaddr *)&conn->peer_addr, conn->peer_addr_len);
	
	if (sent < 0) {
		ast_log(LOG_ERROR, "Failed to send MoQ message: %s\n", strerror(errno));
		return -1;
	}
	
	return 0;
}

/* Receive MoQ message from QUIC */
static int moq_quic_recv_message(struct moq_quic_conn *conn, uint8_t *msg_type,
	uint8_t *payload, size_t *payload_len, size_t max_payload_len)
{
	if (!conn || conn->socket_fd < 0) {
		return -1;
	}
	
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	
	ssize_t received = recvfrom(conn->socket_fd, conn->recv_buffer, 
		MOQ_BUFFER_SIZE, 0, (struct sockaddr *)&from, &fromlen);
	
	if (received < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0; /* No data available */
		}
		ast_log(LOG_ERROR, "Failed to receive MoQ message: %s\n", strerror(errno));
		return -1;
	}
	
	if (received < 3) {
		ast_log(LOG_WARNING, "Received truncated MoQ message\n");
		return -1;
	}
	
	/* Parse message */
	*msg_type = conn->recv_buffer[0];
	uint16_t len = (conn->recv_buffer[1] << 8) | conn->recv_buffer[2];
	
	if (len > received - 3) {
		ast_log(LOG_WARNING, "Invalid MoQ message length\n");
		return -1;
	}
	
	if (len > max_payload_len) {
		ast_log(LOG_WARNING, "MoQ message payload too large\n");
		return -1;
	}
	
	*payload_len = len;
	if (len > 0 && payload) {
		memcpy(payload, conn->recv_buffer + 3, len);
	}
	
	return 1; /* Message received */
}

/* Send MoQ media object */
static int moq_send_media_object(struct moq_session *session, const uint8_t *data, 
	size_t len, uint64_t timestamp)
{
	if (!session || !session->quic_conn) {
		return -1;
	}
	
	/* Construct MoQ media header */
	struct moq_media_header header;
	header.type = MOQ_MSG_OBJECT;
	header.track_id = htonl(session->track_id);
	header.sequence = htobe64(session->send_sequence++);
	header.timestamp = htobe64(timestamp);
	header.payload_size = htons(len);
	
	/* Allocate buffer for header + payload */
	size_t total_len = sizeof(header) + len;
	uint8_t *packet = ast_malloc(total_len);
	if (!packet) {
		return -1;
	}
	
	memcpy(packet, &header, sizeof(header));
	memcpy(packet + sizeof(header), data, len);
	
	/* Send via QUIC */
	int ret = moq_quic_send_message(session->quic_conn, MOQ_MSG_OBJECT, 
		packet, total_len);
	
	ast_free(packet);
	return ret;
}

/* Receive MoQ media object */
static int moq_recv_media_object(struct moq_session *session, uint8_t *data,
	size_t *len, size_t max_len, uint64_t *timestamp)
{
	if (!session || !session->quic_conn) {
		return -1;
	}
	
	uint8_t msg_type;
	uint8_t buffer[MOQ_BUFFER_SIZE];
	size_t msg_len;
	
	int ret = moq_quic_recv_message(session->quic_conn, &msg_type, 
		buffer, &msg_len, sizeof(buffer));
	
	if (ret <= 0) {
		return ret;
	}
	
	if (msg_type != MOQ_MSG_OBJECT) {
		ast_log(LOG_DEBUG, "Received non-media MoQ message type: %d\n", msg_type);
		return 0;
	}
	
	if (msg_len < sizeof(struct moq_media_header)) {
		ast_log(LOG_WARNING, "Received incomplete MoQ media object\n");
		return -1;
	}
	
	/* Parse header */
	struct moq_media_header header;
	memcpy(&header, buffer, sizeof(header));
	
	uint32_t track_id = ntohl(header.track_id);
	uint64_t sequence = be64toh(header.sequence);
	*timestamp = be64toh(header.timestamp);
	uint16_t payload_size = ntohs(header.payload_size);
	
	if (track_id != session->track_id) {
		ast_log(LOG_DEBUG, "Received media for different track: %u\n", track_id);
		return 0;
	}
	
	/* Check for lost packets */
	if (sequence > session->recv_sequence + 1) {
		ast_log(LOG_WARNING, "Lost %llu MoQ packets\n", 
			(unsigned long long)(sequence - session->recv_sequence - 1));
	}
	session->recv_sequence = sequence;
	
	/* Extract payload */
	size_t payload_offset = sizeof(header);
	size_t available_payload = msg_len - payload_offset;
	
	if (payload_size != available_payload) {
		ast_log(LOG_WARNING, "MoQ payload size mismatch: expected %u, got %zu\n",
			payload_size, available_payload);
		payload_size = available_payload;
	}
	
	if (payload_size > max_len) {
		ast_log(LOG_WARNING, "MoQ payload too large: %u > %zu\n", payload_size, max_len);
		payload_size = max_len;
	}
	
	*len = payload_size;
	memcpy(data, buffer + payload_offset, payload_size);
	
	return 1;
}

/* Send WebSocket message */
static int moq_ws_send_message(struct lws *wsi, const char *message)
{
	size_t len = strlen(message);
	unsigned char *buf = ast_malloc(LWS_PRE + len);
	if (!buf) {
		return -1;
	}

	memcpy(&buf[LWS_PRE], message, len);
	int written = lws_write(wsi, &buf[LWS_PRE], len, LWS_WRITE_TEXT);
	ast_free(buf);

	return (written < 0) ? -1 : 0;
}

/* Send call message via WebSocket */
static int moq_send_call(struct moq_session *session, const char *dest)
{
	struct json_object *jobj = json_object_new_object();
	json_object_object_add(jobj, "type", json_object_new_string("call"));
	json_object_object_add(jobj, "session_id", json_object_new_string(session->session_id));
	json_object_object_add(jobj, "dest", json_object_new_string(dest));
	
	const char *msg = json_object_to_json_string(jobj);
	int ret = moq_ws_send_message(session->ws, msg);
	json_object_put(jobj);
	
	return ret;
}

/* Send answer message via WebSocket */
static int moq_send_answer(struct moq_session *session)
{
	struct json_object *jobj = json_object_new_object();
	json_object_object_add(jobj, "type", json_object_new_string("answer"));
	json_object_object_add(jobj, "session_id", json_object_new_string(session->session_id));
	
	const char *msg = json_object_to_json_string(jobj);
	int ret = moq_ws_send_message(session->ws, msg);
	json_object_put(jobj);
	
	return ret;
}

/* Send hangup message via WebSocket */
static int moq_send_hangup(struct moq_session *session)
{
	struct json_object *jobj = json_object_new_object();
	json_object_object_add(jobj, "type", json_object_new_string("hangup"));
	json_object_object_add(jobj, "session_id", json_object_new_string(session->session_id));
	
	const char *msg = json_object_to_json_string(jobj);
	int ret = moq_ws_send_message(session->ws, msg);
	json_object_put(jobj);
	
	return ret;
}

/* Media thread - handles MoQ media transport */
static void *moq_media_thread(void *data)
{
	struct moq_session *session = data;
	unsigned char buffer[MOQ_MAX_PACKET_SIZE];
	struct ast_frame frame;
	uint64_t timestamp;
	
	ast_log(LOG_NOTICE, "MoQ media thread started for session %s\n", session->session_id);
	
	while (session->running) {
		/* Receive media via MoQ/QUIC */
		size_t len = sizeof(buffer);
		
		fd_set fds;
		struct timeval tv = {0, 20000}; /* 20ms timeout for low latency */
		
		if (session->quic_conn && session->quic_conn->socket_fd >= 0) {
			FD_ZERO(&fds);
			FD_SET(session->quic_conn->socket_fd, &fds);
			
			int ret = select(session->quic_conn->socket_fd + 1, &fds, NULL, NULL, &tv);
			if (ret > 0 && FD_ISSET(session->quic_conn->socket_fd, &fds)) {
				/* Receive MoQ media object */
				ret = moq_recv_media_object(session, buffer, &len, sizeof(buffer), &timestamp);
				
				if (ret > 0 && len > 0 && session->owner) {
					/* Queue frame to Asterisk */
					memset(&frame, 0, sizeof(frame));
					frame.frametype = AST_FRAME_VOICE;
					frame.subclass.format = ast_format_ulaw;
					frame.data.ptr = buffer;
					frame.datalen = len;
					frame.samples = len;
					frame.delivery.tv_sec = timestamp / 1000000;
					frame.delivery.tv_usec = timestamp % 1000000;
					
					ast_mutex_lock(&session->lock);
					if (session->owner) {
						ast_queue_frame(session->owner, &frame);
					}
					ast_mutex_unlock(&session->lock);
				}
			}
		} else {
			/* Fallback to UDP if QUIC not available */
			FD_ZERO(&fds);
			FD_SET(session->media_socket, &fds);
			
			int ret = select(session->media_socket + 1, &fds, NULL, NULL, &tv);
			if (ret > 0 && FD_ISSET(session->media_socket, &fds)) {
				struct sockaddr_in from;
				socklen_t fromlen = sizeof(from);
				
				ssize_t received = recvfrom(session->media_socket, buffer, 
					sizeof(buffer), 0, (struct sockaddr *)&from, &fromlen);
				
				if (received > 0 && session->owner) {
					memset(&frame, 0, sizeof(frame));
					frame.frametype = AST_FRAME_VOICE;
					frame.subclass.format = ast_format_ulaw;
					frame.data.ptr = buffer;
					frame.datalen = received;
					frame.samples = received;
					
					ast_mutex_lock(&session->lock);
					if (session->owner) {
						ast_queue_frame(session->owner, &frame);
					}
					ast_mutex_unlock(&session->lock);
				}
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
	
	/* Initialize MoQ/QUIC parameters */
	session->track_id = (uint32_t)ast_random();
	session->send_sequence = 0;
	session->recv_sequence = 0;
	session->last_timestamp = 0;
	
	/* Create QUIC connection for MoQ transport */
	session->quic_conn = moq_quic_create("127.0.0.1", MOQ_QUIC_PORT);
	if (!session->quic_conn) {
		ast_log(LOG_WARNING, "Failed to create QUIC connection, using UDP fallback\n");
	}
	
	/* Create fallback UDP socket */
	session->media_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (session->media_socket < 0) {
		ast_log(LOG_ERROR, "Failed to create media socket\n");
		if (session->quic_conn) {
			moq_quic_destroy(session->quic_conn);
		}
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
		if (session->quic_conn) {
			moq_quic_destroy(session->quic_conn);
		}
		ast_free(session);
		return NULL;
	}
	
	session->running = 1;
	
	ast_log(LOG_NOTICE, "Created MoQ session %s for destination %s (track_id: %u)\n", 
		session->session_id, dest, session->track_id);
	
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
	
	if (session->quic_conn) {
		moq_quic_destroy(session->quic_conn);
	}
	
	if (session->media_socket >= 0) {
		close(session->media_socket);
	}
	
	ast_mutex_destroy(&session->lock);
	ast_free(session);
}

/* WebSocket callback */
static int moq_ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
	void *user, void *in, size_t len)
{
	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			ast_log(LOG_NOTICE, "WebSocket connection established\n");
			break;
			
		case LWS_CALLBACK_RECEIVE:
			ast_log(LOG_DEBUG, "WebSocket received: %.*s\n", (int)len, (char *)in);
			
			/* Parse JSON message */
			struct json_object *jobj = json_tokener_parse(in);
			if (jobj) {
				struct json_object *type_obj = json_object_object_get(jobj, "type");
				if (type_obj) {
					const char *type = json_object_get_string(type_obj);
					ast_log(LOG_NOTICE, "WebSocket message type: %s\n", type);
					
					/* Handle incoming call, answer, hangup, etc. */
					if (strcmp(type, "incoming_call") == 0) {
						struct json_object *session_id_obj = json_object_object_get(jobj, "session_id");
						struct json_object *from_obj = json_object_object_get(jobj, "from");
						
						if (session_id_obj && from_obj) {
							const char *session_id = json_object_get_string(session_id_obj);
							const char *from = json_object_get_string(from_obj);
							
							/* Create incoming channel */
							struct ast_format_cap *cap = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT);
							if (cap) {
								ast_format_cap_append(cap, ast_format_ulaw, 0);
								
							struct ast_channel *chan = ast_channel_alloc(1, AST_STATE_RING,
								from, NULL, NULL, NULL, NULL, NULL, NULL, 0, "MOQ/%s", session_id);
								
								if (chan) {
									ast_channel_tech_set(chan, &moq_tech);
									ast_channel_nativeformats_set(chan, cap);
									ast_channel_set_writeformat(chan, ast_format_ulaw);
									ast_channel_set_readformat(chan, ast_format_ulaw);
									ast_channel_set_rawwriteformat(chan, ast_format_ulaw);
									ast_channel_set_rawreadformat(chan, ast_format_ulaw);
									
									struct moq_session *session = moq_session_new(from);
									if (session) {
										ast_copy_string(session->session_id, session_id, sizeof(session->session_id));
										session->ws = wsi;
										session->owner = chan;
										ast_channel_tech_pvt_set(chan, session);
										
										ast_channel_unlock(chan);
										
										if (ast_pbx_start(chan)) {
											ast_log(LOG_ERROR, "Failed to start PBX\n");
											ast_hangup(chan);
										}
									} else {
										ast_hangup(chan);
									}
								}
								
								ao2_ref(cap, -1);
							}
						}
					}
				}
				json_object_put(jobj);
			}
			break;
			
		case LWS_CALLBACK_CLOSED:
			ast_log(LOG_NOTICE, "WebSocket connection closed\n");
			break;
			
		default:
			break;
	}
	
	return 0;
}

/* WebSocket protocols */
static struct lws_protocols protocols[] = {
	{
		"moq-signaling",
		moq_ws_callback,
		0,
		4096,
		0, NULL, 0
	},
	{ NULL, NULL, 0, 0, 0, NULL, 0 }
};

/* WebSocket thread */
static void *moq_ws_thread(void *data)
{
	ast_log(LOG_NOTICE, "WebSocket signaling thread started on port %d\n", moq_config.ws_port);
	
	while (moq_config.running) {
		lws_service(moq_config.ws_context, 50);
	}
	
	ast_log(LOG_NOTICE, "WebSocket signaling thread stopped\n");
	return NULL;
}

/* Channel technology implementations */
static struct ast_channel *moq_request(const char *type, struct ast_format_cap *cap,
	const struct ast_assigned_ids *assignedids, const struct ast_channel *requestor,
	const char *addr, int *cause)
{
	struct ast_channel *chan;
	struct moq_session *session;
	
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
	
	struct ast_format *fmt = ast_format_cap_get_format(cap, 0);
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
	
	ast_log(LOG_NOTICE, "MoQ calling: %s\n", dest);
	
	session->state = MOQ_STATE_CALLING;
	ast_setstate(ast, AST_STATE_RINGING);
	
	/* Send call via WebSocket */
	if (moq_config.ws_context) {
		moq_send_call(session, dest);
	}
	
	/* Start media thread */
	if (pthread_create(&session->media_thread, NULL, moq_media_thread, session)) {
		ast_log(LOG_ERROR, "Failed to create media thread\n");
		return -1;
	}
	
	ast_queue_control(ast, AST_CONTROL_RINGING);
	
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
	
	/* Send hangup via WebSocket */
	moq_send_hangup(session);
	
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
	
	if (!session) {
		return -1;
	}
	
	if (frame->frametype != AST_FRAME_VOICE) {
		return 0;
	}
	
	/* Calculate timestamp in microseconds */
	uint64_t timestamp;
	if (frame->delivery.tv_sec || frame->delivery.tv_usec) {
		timestamp = (uint64_t)frame->delivery.tv_sec * 1000000 + 
			frame->delivery.tv_usec;
	} else {
		struct timeval now;
		gettimeofday(&now, NULL);
		timestamp = (uint64_t)now.tv_sec * 1000000 + now.tv_usec;
	}
	
	/* Send media via MoQ/QUIC if available */
	if (session->quic_conn && session->quic_conn->connected) {
		if (moq_send_media_object(session, frame->data.ptr, frame->datalen, 
			timestamp) < 0) {
			ast_log(LOG_WARNING, "Failed to send MoQ media object\n");
		}
	} else if (session->media_socket >= 0 && 
		session->media_addr.ss.ss_family == AF_INET) {
		/* Fallback to UDP */
				sendto(session->media_socket, frame->data.ptr, frame->datalen, 0,
			(const struct sockaddr *)&session->media_addr.ss, sizeof(struct sockaddr_in));
	}
	
	session->last_timestamp = timestamp;
	
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
	
	/* Send answer via WebSocket */
	moq_send_answer(session);
	
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
		moq_config.ws_port = DEFAULT_WS_PORT;
		return 0;
	}
	
	/* Parse general section */
	for (v = ast_variable_browse(cfg, "general"); v; v = v->next) {
		if (!strcasecmp(v->name, "context")) {
			ast_copy_string(moq_config.context, v->value, sizeof(moq_config.context));
		} else if (!strcasecmp(v->name, "ws_port")) {
			moq_config.ws_port = atoi(v->value);
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
	moq_config.ws_port = DEFAULT_WS_PORT;
	
	if (load_config(0)) {
		return AST_MODULE_LOAD_DECLINE;
	}
	
	/* Initialize WebSocket server */
	struct lws_context_creation_info info;
	memset(&info, 0, sizeof(info));
	info.port = moq_config.ws_port;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;
	
	moq_config.ws_context = lws_create_context(&info);
	if (!moq_config.ws_context) {
		ast_log(LOG_ERROR, "Failed to create WebSocket context\n");
		return AST_MODULE_LOAD_DECLINE;
	}
	
	/* Start WebSocket thread */
	moq_config.running = 1;
	if (pthread_create(&moq_config.ws_thread, NULL, moq_ws_thread, NULL)) {
		ast_log(LOG_ERROR, "Failed to create WebSocket thread\n");
		lws_context_destroy(moq_config.ws_context);
		return AST_MODULE_LOAD_DECLINE;
	}
	
	/* Register channel technology */
	if (ast_channel_register(&moq_tech)) {
		ast_log(LOG_ERROR, "Failed to register channel technology\n");
		moq_config.running = 0;
		pthread_join(moq_config.ws_thread, NULL);
		lws_context_destroy(moq_config.ws_context);
		return AST_MODULE_LOAD_DECLINE;
	}
	
	ast_log(LOG_NOTICE, "chan_moq loaded successfully\n");
	
	return AST_MODULE_LOAD_SUCCESS;
}

/* Module unload */
static int unload_module(void)
{
	ast_log(LOG_NOTICE, "Unloading chan_moq module\n");
	
	/* Stop WebSocket thread */
	moq_config.running = 0;
	pthread_join(moq_config.ws_thread, NULL);
	
	/* Destroy WebSocket context */
	if (moq_config.ws_context) {
		lws_context_destroy(moq_config.ws_context);
	}
	
	/* Unregister channel technology */
	ast_channel_unregister(&moq_tech);
	
	ast_log(LOG_NOTICE, "chan_moq unloaded successfully\n");
	
	return 0;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Media over QUIC Channel Driver");
