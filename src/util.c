/*
 * The process in the flask.
 *
 * Copyright (c) 2013, Alessandro Ghedini
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

#include <stdlib.h>
#include <string.h>

#include "printf.h"
#include "util.h"

size_t split_str(char *orig, char ***dest, char *needle) {
	size_t size  = 0;
	char  *token = NULL;

	if (orig == NULL || dest == NULL)
		return 0;

	token = strtok(orig, needle);

	do {
		char **tmp = realloc(*dest, sizeof(char *) * (size + 1));

		if (tmp == NULL) {
			if (*dest != NULL)
				free(*dest);

			return 0;
		}

		*dest = tmp;
		(*dest)[size++] = token;
	} while ((token = strtok(NULL, needle)) != NULL);

	return size;
}

size_t validate_optlist(const char *name, const char *opts) {
	size_t i, c;
	_free_ char **vars = NULL;

	_free_ char *tmp = strdup(opts);
	if (tmp == NULL) fail_printf("OOM");

	c = split_str(tmp, &vars, ",");
	if (c == 0) fail_printf("Invalid value '%s' for %s", opts, name);

	for (i = 0; i < c; i++) {
		if (vars[i] == '\0')
			fail_printf("Invalid value '%s' for %s", opts, name);
	}

	return c;
}
