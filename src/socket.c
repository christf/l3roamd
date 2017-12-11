/*
   Copyright (c) 2017, Christof Schulze <christof.schulze@gmx.net>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

#include <stdio.h>
#include <unistd.h>

#include "socket.h"
#include "error.h"
#include "l3roamd.h"

void socket_init(socket_ctx *ctx, char *path) {
	if (!path) {
		ctx->fd = -1;
		return;
	}

	printf("Initializing unix socket: %s\n", path);

	unlink(path);

	size_t status_socket_len = strlen(path);
	size_t len = offsetof(struct sockaddr_un, sun_path) + status_socket_len + 1;
	uint8_t buf[len] __attribute__((aligned(__alignof__(struct sockaddr_un))));
	memset(buf, 0, offsetof(struct sockaddr_un, sun_path));

	struct sockaddr_un *sa = (struct sockaddr_un *)buf;
	sa->sun_family = AF_UNIX;
	memcpy(sa->sun_path, path, status_socket_len+1);

	ctx->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

	if (bind(ctx->fd, (struct sockaddr *)sa, len)) {
		switch (errno) {
			case EADDRINUSE:
				exit_error("unable to create status socket: the path `%s' already exists", path);
			default:
				exit_errno("unable to create status socket");
		}
	}

	if (listen(ctx->fd, 5)) {
		perror("unable to listen on unix-socket");
		exit(EXIT_FAILURE);
	}
}

bool parse_command(char *cmd, enum socket_command *scmd) {
	if (!strncmp(cmd, "get_clients", 11)) {
		*scmd = GET_CLIENTS;
		return true;
	}
	if (!strncmp(cmd, "del_prefix", 10)) {
		*scmd = DEL_PREFIX;
		return true;
	}
	if (!strncmp(cmd, "add_prefix", 10)) {
		*scmd = ADD_PREFIX;
		return true;
	}
	return false;
}

void socket_handle_in(socket_ctx *ctx) {
	int fd = accept(ctx->fd, NULL, NULL);
	char line[LINEBUFFER_SIZE];

	int len=0;
	int fill=0;
	// TODO: it would be nice to be able to set a timeout here after which the fd is closed
	while (fill<LINEBUFFER_SIZE) {
		len = read(fd, &(line[fill]), 1);
		if (line[fill] == '\n' || line[fill] == '\r') {
			line[fill]='\0';
			break;
		}
		fill+=len;
	}

	enum socket_command cmd;
	if (!parse_command(line, &cmd) ) {
		fprintf(stderr, "Could not parse command on socket (%s)\n",line);
		goto end;
	}

	struct prefix _prefix = {};

	switch (cmd) {
		case GET_CLIENTS:
			dprintf(fd, "{\"clients\":%zu}", VECTOR_LEN(CTX(clientmgr)->clients));
			break;
		case ADD_PREFIX:
			if (parse_prefix(&_prefix, &line[11])) {
				add_prefix(&CTX(clientmgr)->prefixes, _prefix);
				routemgr_insert_route(CTX(routemgr), 254, if_nametoindex(CTX(ipmgr)->ifname), (struct in6_addr*)(_prefix.prefix.s6_addr), _prefix.plen );
				dprintf(fd, "Added prefix: %s", &line[11]);
			}
			break;
		case DEL_PREFIX:
			if (parse_prefix(&_prefix, &line[11])) {
				del_prefix(&CTX(clientmgr)->prefixes, _prefix);
				routemgr_remove_route(CTX(routemgr), 254, (struct in6_addr*)(_prefix.prefix.s6_addr), _prefix.plen );
				dprintf(fd, "Deleted prefix: %s", &line[11]);
			}
			break;
	}

end:
	close(fd);
}