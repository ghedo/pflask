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

#include "capabilities.h"

#include "printf.h"
#include "util.h"

void setup_capabilities(bool clear_caps, size_t total_caps, struct cap_action *caps) {
	int rc;

	// initialize capng state
	capng_get_caps_process();

	if (clear_caps) {
		capng_clear(CAPNG_SELECT_BOTH);
	}

	for (size_t i = 0; i < total_caps; i++) {
		rc = capng_update(caps[i].action, CAPNG_EFFECTIVE|CAPNG_PERMITTED|CAPNG_INHERITABLE|CAPNG_BOUNDING_SET, caps[i].capability);
		fail_if(rc != 0, "Error updating capabilities");
	}

	rc = capng_apply(CAPNG_SELECT_BOTH);
	fail_if(rc != 0, "Could not apply capabilities");
}
