/* pkcs5_pbkdf2.c - Password-Based Key Derivation Function 2
 * Copyright (c) 2008 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Modifications for Claws Mail are:
 * Copyright (c) 2016 the Claws Mail team
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <glib.h>
#include <sys/types.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define CHECKSUM_BLOCKLEN 64
/*
 * HMAC-SHA-1 (from RFC 2202).
 */
static void
hmac_sha1(const guchar *text, size_t text_len, const guchar *key,
    size_t key_len, guchar *digest)
{
	GChecksum *cksum;
	gssize digestlen = g_checksum_type_get_length(G_CHECKSUM_SHA1);
	gsize outlen;
	guchar k_pad[CHECKSUM_BLOCKLEN];
	guchar tk[digestlen];
	gint i;

	if (key_len > CHECKSUM_BLOCKLEN) {
		cksum = g_checksum_new(G_CHECKSUM_SHA1);
		g_checksum_update(cksum, key, key_len);
		outlen = digestlen;
		g_checksum_get_digest(cksum, tk, &outlen);
		g_checksum_free(cksum);

		key = tk;
		key_len = digestlen;
	}

	bzero(k_pad, sizeof k_pad);
	bcopy(key, k_pad, key_len);
	for (i = 0; i < CHECKSUM_BLOCKLEN; i++)
		k_pad[i] ^= 0x36;

	cksum = g_checksum_new(G_CHECKSUM_SHA1);
	g_checksum_update(cksum, k_pad, CHECKSUM_BLOCKLEN);
	g_checksum_update(cksum, text, text_len);
	outlen = digestlen;
	g_checksum_get_digest(cksum, digest, &outlen);
	g_checksum_free(cksum);

	bzero(k_pad, sizeof k_pad);
	bcopy(key, k_pad, key_len);
	for (i = 0; i < CHECKSUM_BLOCKLEN; i++)
		k_pad[i] ^= 0x5c;

	cksum = g_checksum_new(G_CHECKSUM_SHA1);
	g_checksum_update(cksum, k_pad, CHECKSUM_BLOCKLEN);
	g_checksum_update(cksum, digest, digestlen);
	outlen = digestlen;
	g_checksum_get_digest(cksum, digest, &outlen);
	g_checksum_free(cksum);
}

#undef CHECKSUM_BLOCKLEN

/*
 * Password-Based Key Derivation Function 2 (PKCS #5 v2.0).
 * Code based on IEEE Std 802.11-2007, Annex H.4.2.
 */
gint
pkcs5_pbkdf2(const gchar *pass, size_t pass_len, const guchar *salt,
    size_t salt_len, guchar *key, size_t key_len, guint rounds)
{
	gssize digestlen = g_checksum_type_get_length(G_CHECKSUM_SHA1);
	guchar *asalt, obuf[digestlen];
	guchar d1[digestlen], d2[digestlen];
	guint i, j;
	guint count;
	size_t r;

	if (rounds < 1 || key_len == 0)
		return -1;
	if (salt_len == 0 || salt_len > SIZE_MAX - 4)
		return -1;
	if ((asalt = malloc(salt_len + 4)) == NULL)
		return -1;

	memcpy(asalt, salt, salt_len);

	for (count = 1; key_len > 0; count++) {
		asalt[salt_len + 0] = (count >> 24) & 0xff;
		asalt[salt_len + 1] = (count >> 16) & 0xff;
		asalt[salt_len + 2] = (count >> 8) & 0xff;
		asalt[salt_len + 3] = count & 0xff;
		hmac_sha1(asalt, salt_len + 4, pass, pass_len, d1);
		memcpy(obuf, d1, sizeof(obuf));

		for (i = 1; i < rounds; i++) {
			hmac_sha1(d1, sizeof(d1), pass, pass_len, d2);
			memcpy(d1, d2, sizeof(d1));
			for (j = 0; j < sizeof(obuf); j++)
				obuf[j] ^= d1[j];
		}

		r = MIN(key_len, digestlen);
		memcpy(key, obuf, r);
		key += r;
		key_len -= r;
	};
	bzero(asalt, salt_len + 4);
	free(asalt);
	bzero(d1, sizeof(d1));
	bzero(d2, sizeof(d2));
	bzero(obuf, sizeof(obuf));

	return 0;
}
