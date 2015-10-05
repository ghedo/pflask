/*
 * The process in the flask.
 *
 * Copyright (c) 2013, gdm85
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#include <cap-ng.h>

#include "ut/utlist.h"

#include "printf.h"
#include "util.h"

struct capability {
	capng_act_t action;
	int capability;

	struct capability *next, *prev;
};

void capability_add(struct capability **caps, char *cap) {
	int rc;

	struct capability *c = malloc(sizeof(struct capability));
	fail_if(!c, "OOM");

	fail_if(!cap[0], "Invalid empty capability name");

	switch (cap[0]) {
	case '+':
		c->action = CAPNG_ADD;
		cap++;
		break;

	case '-':
		c->action = CAPNG_DROP;
		cap++;
		break;

	default:
		c->action = CAPNG_ADD;
		break;
	}

	if (!strcasecmp(cap, "all")) {
		/* *caps == NULL means that the list is empty */
		fail_if(*caps, "Alias 'all' is valid only as first capability");

		c->capability = -1;
	} else {
		rc = capng_name_to_capability(cap);
		fail_if(rc < 0, "Invalid capability name: '%s'", cap);

		c->capability = rc;
	}

	DL_APPEND(*caps, c);
}

void setup_capabilities(struct capability *caps) {
	int rc;

	struct capability *i = NULL;

	capng_get_caps_process();

	DL_FOREACH(caps, i) {
		if (i->capability < 0) {
			if (i->action == CAPNG_DROP)
				capng_clear(CAPNG_SELECT_BOTH);

			continue;
		}

		rc = capng_update(i->action, CAPNG_EFFECTIVE |
		                             CAPNG_PERMITTED |
		                             CAPNG_INHERITABLE |
		                             CAPNG_BOUNDING_SET,
		                             i->capability);
		fail_if(rc != 0, "Error updating capabilities");
	}

	rc = capng_apply(CAPNG_SELECT_BOTH);
	fail_if(rc != 0, "Error applying capabilities");
}
