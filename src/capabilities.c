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

#include "printf.h"
#include "util.h"

void setup_capabilities(char *caps) {
	int rc;
	capng_act_t cap_action;
	char *cap, *name;
	char *remainder = ",";

	// initialize capng state
	capng_get_caps_process();

	// get first cap
	cap = strtok(caps, remainder);

	if (!strcasecmp(caps, "+all") || !strcasecmp(caps, "all")) {
		// nop

		cap = strtok(NULL, remainder);
	} else if (!strcasecmp(caps, "-all")) {
		capng_clear(CAPNG_SELECT_BOTH);

		cap = strtok(NULL, remainder);
	}

	while (cap != NULL) {
		sys_fail_if(strlen(cap) == 0, "Empty capability name specified");

		if (cap[0] == '+') {
			cap_action = CAPNG_ADD;

			name = &cap[1];
		} else if (cap[0] == '-') {
			cap_action = CAPNG_DROP;

			name = &cap[1];
		} else {
			// implicit '+'
			cap_action = CAPNG_ADD;

			name = cap;
		}

		if (!strcasecmp(name, "all")) {
			fail_printf("Alias '%s' is valid only as first capability", cap);
			return;
		}

		rc = capng_name_to_capability(name);
		if (rc == -1) {
			fail_printf("Invalid capability name: '%s'", name);
			return;
		}

		rc = capng_update(cap_action, CAPNG_EFFECTIVE|CAPNG_PERMITTED|CAPNG_INHERITABLE|CAPNG_BOUNDING_SET, rc);
		if (rc != 0) {
			fail_printf("Error updating capabilities");
			return;
		}

		cap = strtok(NULL, remainder);
	}

	rc = capng_apply(CAPNG_SELECT_BOTH);
	if (rc != 0) {
		fail_printf("Could not apply capabilities");
		return;
	}
}
