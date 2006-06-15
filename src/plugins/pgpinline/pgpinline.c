/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto & the Sylpheed-Claws team
 * This file (C) 2004 Colin Leroy <colin@colino.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_GPGME

#include "defs.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <errno.h>
#include <gpgme.h>

#include "utils.h"
#include "privacy.h"
#include "procmime.h"
#include "pgpinline.h"
#include <plugins/pgpcore/sgpgme.h>
#include <plugins/pgpcore/prefs_gpg.h>
#include <plugins/pgpcore/passphrase.h>
#include "quoted-printable.h"
#include "base64.h"
#include "codeconv.h"

extern struct GPGConfig prefs_gpg;

typedef struct _PrivacyDataPGP PrivacyDataPGP;

struct _PrivacyDataPGP
{
	PrivacyData	data;
	
	gboolean	done_sigtest;
	gboolean	is_signed;
	gpgme_verify_result_t	sigstatus;
	gpgme_ctx_t 	ctx;
};

static PrivacySystem pgpinline_system;

static gint pgpinline_check_signature(MimeInfo *mimeinfo);

static PrivacyDataPGP *pgpinline_new_privacydata()
{
	PrivacyDataPGP *data;

	data = g_new0(PrivacyDataPGP, 1);
	data->data.system = &pgpinline_system;
	data->done_sigtest = FALSE;
	data->is_signed = FALSE;
	data->sigstatus = NULL;
	gpgme_new(&data->ctx);
	
	return data;
}

static void pgpinline_free_privacydata(PrivacyData *_data)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) _data;
	gpgme_release(data->ctx);
	g_free(data);
}

static gchar *fp_read_noconv(FILE *fp)
{
	GByteArray *array;
	guchar buf[BUFSIZ];
	gint n_read;
	gchar *result = NULL;

	if (!fp)
		return NULL;
	array = g_byte_array_new();

	while ((n_read = fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		if (n_read < sizeof(buf) && ferror(fp))
			break;
		g_byte_array_append(array, buf, n_read);
	}

	if (ferror(fp)) {
		FILE_OP_ERROR("file stream", "fread");
		g_byte_array_free(array, TRUE);
		return NULL;
	}

	buf[0] = '\0';
	g_byte_array_append(array, buf, 1);
	result = (gchar *)array->data;
	g_byte_array_free(array, FALSE);
	
	return result;
}

static gchar *get_part_as_string(MimeInfo *mimeinfo)
{
	gchar *textdata = NULL;

	g_return_val_if_fail(mimeinfo != NULL, 0);
	procmime_decode_content(mimeinfo);
	if (mimeinfo->content == MIMECONTENT_MEM)
		textdata = g_strdup(mimeinfo->data.mem);
	else {
		/* equals file_read_to_str but without conversion */
		FILE *fp = fopen(mimeinfo->data.filename, "r");
		if (!fp)
			return NULL;
		textdata = fp_read_noconv(fp);
		fclose(fp);
	}

	if (!g_utf8_validate(textdata, -1, NULL)) {
		gchar *tmp = NULL;
		codeconv_set_strict(TRUE);
		if (procmime_mimeinfo_get_parameter(mimeinfo, "charset")) {
			tmp = conv_codeset_strdup(textdata,
				procmime_mimeinfo_get_parameter(mimeinfo, "charset"),
				CS_UTF_8);
		}
		if (!tmp) {
			tmp = conv_codeset_strdup(textdata,
				conv_get_locale_charset_str_no_utf8(), 
				CS_UTF_8);
		}
		codeconv_set_strict(FALSE);
		if (!tmp) {
			tmp = conv_codeset_strdup(textdata,
				conv_get_locale_charset_str_no_utf8(), 
				CS_UTF_8);
		}
		if (tmp) {
			g_free(textdata);
			textdata = tmp;
		}
	}

	return textdata;	
}

static gboolean pgpinline_is_signed(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = NULL;
	const gchar *sig_indicator = "-----BEGIN PGP SIGNED MESSAGE-----";
	gchar *textdata, *sigpos;
	
	g_return_val_if_fail(mimeinfo != NULL, FALSE);
	
	if (procmime_mimeinfo_parent(mimeinfo) == NULL)
		return FALSE; /* not parent */
	
	if (mimeinfo->type != MIMETYPE_TEXT &&
		(mimeinfo->type != MIMETYPE_APPLICATION ||
		 g_ascii_strcasecmp(mimeinfo->subtype, "pgp")))
		return FALSE;

	/* Seal the deal. This has to be text/plain through and through. */
	if (mimeinfo->type == MIMETYPE_APPLICATION)
	{
		mimeinfo->type = MIMETYPE_TEXT;
		g_free(mimeinfo->subtype);
		mimeinfo->subtype = g_strdup("plain");
	}

	if (mimeinfo->privacy != NULL) {
		data = (PrivacyDataPGP *) mimeinfo->privacy;
		if (data->done_sigtest)
			return data->is_signed;
	}
	
	textdata = get_part_as_string(mimeinfo);
	if (!textdata)
		return FALSE;
	
	if ((sigpos = strstr(textdata, sig_indicator)) == NULL) {
		g_free(textdata);
		return FALSE;
	}

	if (!(sigpos == textdata) && !(sigpos[-1] == '\n')) {
		g_free(textdata);
		return FALSE;
	}

	g_free(textdata);

	if (data == NULL) {
		data = pgpinline_new_privacydata();
		mimeinfo->privacy = (PrivacyData *) data;
	}
	data->done_sigtest = TRUE;
	data->is_signed = TRUE;

	return TRUE;
}

static gint pgpinline_check_signature(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = NULL;
	gchar *textdata = NULL, *tmp = NULL;
	gpgme_data_t plain = NULL, cipher = NULL;
	gpgme_ctx_t ctx;
	
	g_return_val_if_fail(mimeinfo != NULL, 0);

	if (procmime_mimeinfo_parent(mimeinfo) == NULL)
		return 0; /* not parent */
	if (mimeinfo->type != MIMETYPE_TEXT)
		return 0;

	g_return_val_if_fail(mimeinfo->privacy != NULL, 0);
	data = (PrivacyDataPGP *) mimeinfo->privacy;

	textdata = get_part_as_string(mimeinfo);
	
	if (!textdata) {
		g_free(textdata);
		privacy_set_error(_("Couldn't get text data."));
		return 0;
	}

	/* gtk2: convert back from utf8 */
	tmp = conv_codeset_strdup(textdata, CS_UTF_8,
			procmime_mimeinfo_get_parameter(mimeinfo, "charset"));
	if (!tmp) {
		tmp = conv_codeset_strdup(textdata, CS_UTF_8,
			conv_get_locale_charset_str_no_utf8());
	}
	if (!tmp) {
		g_warning("Can't convert charset to anything sane\n");
		tmp = conv_codeset_strdup(textdata, CS_UTF_8, CS_US_ASCII);
	}
	g_free(textdata);

	if (!tmp) {
		privacy_set_error(_("Couldn't convert text data to any sane charset."));
		return 0;
	}
	textdata = g_strdup(tmp);
	g_free(tmp);
	
	gpgme_new(&ctx);
	gpgme_set_textmode(ctx, 1);
	gpgme_set_armor(ctx, 1);
	
	gpgme_data_new_from_mem(&plain, textdata, strlen(textdata), 1);
	gpgme_data_new(&cipher);

	data->sigstatus = sgpgme_verify_signature(ctx, plain, NULL, cipher);
	
	gpgme_data_release(plain);
	gpgme_data_release(cipher);
	
	g_free(textdata);
	
	return 0;
}

static SignatureStatus pgpinline_get_sig_status(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) mimeinfo->privacy;
	
	g_return_val_if_fail(data != NULL, SIGNATURE_INVALID);

	if (data->sigstatus == NULL && 
	    prefs_gpg_get_config()->auto_check_signatures)
		pgpinline_check_signature(mimeinfo);

	return sgpgme_sigstat_gpgme_to_privacy(data->ctx, data->sigstatus);
}

static gchar *pgpinline_get_sig_info_short(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) mimeinfo->privacy;
	
	g_return_val_if_fail(data != NULL, g_strdup("Error"));

	if (data->sigstatus == NULL && 
	    prefs_gpg_get_config()->auto_check_signatures)
		pgpinline_check_signature(mimeinfo);
	
	return sgpgme_sigstat_info_short(data->ctx, data->sigstatus);
}

static gchar *pgpinline_get_sig_info_full(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) mimeinfo->privacy;
	
	g_return_val_if_fail(data != NULL, g_strdup("Error"));

	return sgpgme_sigstat_info_full(data->ctx, data->sigstatus);
}



static gboolean pgpinline_is_encrypted(MimeInfo *mimeinfo)
{
	const gchar *enc_indicator = "-----BEGIN PGP MESSAGE-----";
	gchar *textdata;
	
	g_return_val_if_fail(mimeinfo != NULL, FALSE);
	
	if (procmime_mimeinfo_parent(mimeinfo) == NULL)
		return FALSE; /* not parent */
	
	if (mimeinfo->type != MIMETYPE_TEXT &&
		(mimeinfo->type != MIMETYPE_APPLICATION ||
		 g_ascii_strcasecmp(mimeinfo->subtype, "pgp")))
		return FALSE;
	
	/* Seal the deal. This has to be text/plain through and through. */
	if (mimeinfo->type == MIMETYPE_APPLICATION)
	{
		mimeinfo->type = MIMETYPE_TEXT;
		g_free(mimeinfo->subtype);
		mimeinfo->subtype = g_strdup("plain");
	}
	
	textdata = get_part_as_string(mimeinfo);
	if (!textdata)
		return FALSE;
	
	if (!strstr(textdata, enc_indicator)) {
		g_free(textdata);
		return FALSE;
	}

	g_free(textdata);
	return TRUE;
}

static MimeInfo *pgpinline_decrypt(MimeInfo *mimeinfo)
{
	MimeInfo *decinfo, *parseinfo;
	gpgme_data_t cipher, plain;
	FILE *dstfp;
	gchar *fname;
	gchar *textdata = NULL;
	static gint id = 0;
	const gchar *src_codeset = NULL;
	gpgme_verify_result_t sigstat = 0;
	PrivacyDataPGP *data = NULL;
	gpgme_ctx_t ctx;
	gchar *chars;
	size_t len;
	
	if (gpgme_new(&ctx) != GPG_ERR_NO_ERROR)
		return NULL;

	gpgme_set_textmode(ctx, 1);
	gpgme_set_armor(ctx, 1);

	g_return_val_if_fail(mimeinfo != NULL, NULL);
	g_return_val_if_fail(pgpinline_is_encrypted(mimeinfo), NULL);
	
	if (procmime_mimeinfo_parent(mimeinfo) == NULL ||
	    mimeinfo->type != MIMETYPE_TEXT) {
		gpgme_release(ctx);
		privacy_set_error(_("Couldn't parse mime part."));
		return NULL;
	}

	textdata = get_part_as_string(mimeinfo);
	if (!textdata) {
		gpgme_release(ctx);
		privacy_set_error(_("Couldn't get text data."));
		return NULL;
	}

	debug_print("decrypting '%s'\n", textdata);
	gpgme_data_new_from_mem(&cipher, textdata, strlen(textdata), 1);

	plain = sgpgme_decrypt_verify(cipher, &sigstat, ctx);
	if (sigstat && !sigstat->signatures)
		sigstat = NULL;

	gpgme_data_release(cipher);
	
	if (plain == NULL) {
		gpgme_release(ctx);
		return NULL;
	}

    	fname = g_strdup_printf("%s%cplaintext.%08x",
		get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id);

    	if ((dstfp = g_fopen(fname, "wb")) == NULL) {
        	FILE_OP_ERROR(fname, "fopen");
		privacy_set_error(_("Couldn't open decrypted file %s"), fname);
        	g_free(fname);
        	gpgme_data_release(plain);
		gpgme_release(ctx);
		return NULL;
    	}

	src_codeset = procmime_mimeinfo_get_parameter(mimeinfo, "charset");
	if (src_codeset == NULL)
		src_codeset = CS_ISO_8859_1;
		
	fprintf(dstfp, "MIME-Version: 1.0\r\n"
			"Content-Type: text/plain; charset=%s\r\n"
			"Content-Transfer-Encoding: 8bit\r\n"
			"\r\n",
			src_codeset);
	
	chars = gpgme_data_release_and_get_mem(plain, &len);
	if (len > 0)
		fwrite(chars, len, 1, dstfp);

	fclose(dstfp);
	
	gpgme_data_release(plain);

	parseinfo = procmime_scan_file(fname);
	g_free(fname);
	
	if (parseinfo == NULL) {
		gpgme_release(ctx);
		privacy_set_error(_("Couldn't scan decrypted file."));
		return NULL;
	}
	decinfo = g_node_first_child(parseinfo->node) != NULL ?
		g_node_first_child(parseinfo->node)->data : NULL;
		
	if (decinfo == NULL) {
		gpgme_release(ctx);
		privacy_set_error(_("Couldn't scan decrypted file parts."));
		return NULL;
	}

	g_node_unlink(decinfo->node);
	procmime_mimeinfo_free_all(parseinfo);

	decinfo->tmp = TRUE;

	if (sigstat != GPGME_SIG_STAT_NONE) {
		if (decinfo->privacy != NULL) {
			data = (PrivacyDataPGP *) decinfo->privacy;
		} else {
			data = pgpinline_new_privacydata();
			decinfo->privacy = (PrivacyData *) data;	
		}
		data->done_sigtest = TRUE;
		data->is_signed = TRUE;
		data->sigstatus = sigstat;
		if (data->ctx)
			gpgme_release(data->ctx);
		data->ctx = ctx;
	} else
		gpgme_release(ctx);

	return decinfo;
}

static gboolean pgpinline_sign(MimeInfo *mimeinfo, PrefsAccount *account)
{
	MimeInfo *msgcontent;
	gchar *textstr, *tmp;
	FILE *fp;
	gchar *sigcontent;
	gpgme_ctx_t ctx;
	gpgme_data_t gpgtext, gpgsig;
	guint len;
	gpgme_error_t err;
	struct passphrase_cb_info_s info;
	gpgme_sign_result_t result = NULL;

	memset (&info, 0, sizeof info);

	/* get content node from message */
	msgcontent = (MimeInfo *) mimeinfo->node->children->data;
	if (msgcontent->type == MIMETYPE_MULTIPART)
		msgcontent = (MimeInfo *) msgcontent->node->children->data;

	/* get rid of quoted-printable or anything */
	procmime_decode_content(msgcontent);

	fp = my_tmpfile();
	if (fp == NULL) {
		perror("my_tmpfile");
		privacy_set_error(_("Couldn't create temporary file."));
		return FALSE;
	}
	procmime_write_mimeinfo(msgcontent, fp);
	rewind(fp);

	/* read temporary file into memory */
	textstr = fp_read_noconv(fp);
	
	fclose(fp);
		
	gpgme_data_new_from_mem(&gpgtext, textstr, strlen(textstr), 0);
	gpgme_data_new(&gpgsig);
	gpgme_new(&ctx);
	gpgme_set_textmode(ctx, 1);
	gpgme_set_armor(ctx, 1);

	if (!sgpgme_setup_signers(ctx, account)) {
		gpgme_release(ctx);
		privacy_set_error(_("Couldn't find private key."));
		return FALSE;
	}

	if (!getenv("GPG_AGENT_INFO")) {
    		info.c = ctx;
    		gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
	}

	if ((err = gpgme_op_sign(ctx, gpgtext, gpgsig, GPGME_SIG_MODE_CLEAR)) 
	    != GPG_ERR_NO_ERROR) {
	    	gpgme_release(ctx);
		privacy_set_error(_("Data signing failed, %s"), gpgme_strerror(err));
		return FALSE;
	}
	result = gpgme_op_sign_result(ctx);
	if (result && result->signatures) {
		gpgme_new_signature_t sig = result->signatures;
		while (sig) {
			debug_print("valid signature: %s\n", sig->fpr);
			sig = sig->next;
		}
	} else if (result && result->invalid_signers) {
		gpgme_invalid_key_t invalid = result->invalid_signers;
		while (invalid) {
			g_warning("invalid signer: %s (%s)", invalid->fpr, 
				gpgme_strerror(invalid->reason));
			privacy_set_error(_("Data signing failed due to invalid signer: %s"), 
				gpgme_strerror(invalid->reason));
			invalid = invalid->next;
		}
		gpgme_release(ctx);
		return FALSE;
	} else {
		/* can't get result (maybe no signing key?) */
		debug_print("gpgme_op_sign_result error\n");
		privacy_set_error(_("Data signing failed, no results."));
		gpgme_release(ctx);
		return FALSE;
	}


	sigcontent = gpgme_data_release_and_get_mem(gpgsig, &len);
	
	gpgme_release(ctx);
	
	if (sigcontent == NULL || len <= 0) {
		g_warning("gpgme_data_release_and_get_mem failed");
		privacy_set_error(_("Data signing failed, no contents."));
		gpgme_data_release(gpgtext);
		g_free(textstr);
		g_free(sigcontent);
		return FALSE;
	}

	tmp = g_malloc(len+1);
	g_memmove(tmp, sigcontent, len+1);
	tmp[len] = '\0';
	gpgme_data_release(gpgtext);
	g_free(textstr);
	g_free(sigcontent);

	if (msgcontent->content == MIMECONTENT_FILE &&
	    msgcontent->data.filename != NULL) {
		g_unlink(msgcontent->data.filename);
		g_free(msgcontent->data.filename);
	}
	msgcontent->data.mem = g_strdup(tmp);
	msgcontent->content = MIMECONTENT_MEM;
	g_free(tmp);

	/* avoid all sorts of clear-signing problems with non ascii
	 * chars
	 */
	procmime_encode_content(msgcontent, ENC_BASE64);
			
	return TRUE;
}

static gchar *pgpinline_get_encrypt_data(GSList *recp_names)
{
	return sgpgme_get_encrypt_data(recp_names, GPGME_PROTOCOL_OpenPGP);
}

static gboolean pgpinline_encrypt(MimeInfo *mimeinfo, const gchar *encrypt_data)
{
	MimeInfo *msgcontent;
	FILE *fp;
	gchar *enccontent;
	guint len;
	gchar *textstr, *tmp;
	gpgme_data_t gpgtext, gpgenc;
	gpgme_ctx_t ctx;
	gpgme_key_t *kset = NULL;
	gchar **fprs = g_strsplit(encrypt_data, " ", -1);
	gpgme_error_t err;
	gint i = 0;

	while (fprs[i] && strlen(fprs[i])) {
		i++;
	}
	
	kset = g_malloc(sizeof(gpgme_key_t)*(i+1));
	memset(kset, 0, sizeof(gpgme_key_t)*(i+1));
	gpgme_new(&ctx);
	i = 0;
	while (fprs[i] && strlen(fprs[i])) {
		gpgme_key_t key;
		err = gpgme_get_key(ctx, fprs[i], &key, 0);
		if (err) {
			debug_print("can't add key '%s'[%d] (%s)\n", fprs[i],i, gpgme_strerror(err));
			privacy_set_error(_("Couldn't add GPG key %s, %s"), fprs[i], gpgme_strerror(err));
			return FALSE;
		}
		debug_print("found %s at %d\n", fprs[i], i);
		kset[i] = key;
		i++;
	}
	

	debug_print("Encrypting message content\n");

	/* get content node from message */
	msgcontent = (MimeInfo *) mimeinfo->node->children->data;
	if (msgcontent->type == MIMETYPE_MULTIPART)
		msgcontent = (MimeInfo *) msgcontent->node->children->data;

	/* get rid of quoted-printable or anything */
	procmime_decode_content(msgcontent);

	fp = my_tmpfile();
	if (fp == NULL) {
		privacy_set_error(_("Couldn't create temporary file, %s"), strerror(errno));
		perror("my_tmpfile");
		return FALSE;
	}
	procmime_write_mimeinfo(msgcontent, fp);
	rewind(fp);

	/* read temporary file into memory */
	textstr = fp_read_noconv(fp);
	
	fclose(fp);

	/* encrypt data */
	gpgme_data_new_from_mem(&gpgtext, textstr, strlen(textstr), 0);
	gpgme_data_new(&gpgenc);
	gpgme_new(&ctx);
	gpgme_set_armor(ctx, 1);

	err = gpgme_op_encrypt(ctx, kset, GPGME_ENCRYPT_ALWAYS_TRUST, gpgtext, gpgenc);

	gpgme_release(ctx);
	enccontent = gpgme_data_release_and_get_mem(gpgenc, &len);

	if (enccontent == NULL || len <= 0) {
		g_warning("gpgme_data_release_and_get_mem failed");
		privacy_set_error(_("Encryption failed, %s"), gpgme_strerror(err));
		gpgme_data_release(gpgtext);
		g_free(textstr);
		return FALSE;
	}

	tmp = g_malloc(len+1);
	g_memmove(tmp, enccontent, len+1);
	tmp[len] = '\0';
	g_free(enccontent);

	gpgme_data_release(gpgtext);
	g_free(textstr);

	if (msgcontent->content == MIMECONTENT_FILE &&
	    msgcontent->data.filename != NULL) {
		g_unlink(msgcontent->data.filename);
		g_free(msgcontent->data.filename);
	}
	msgcontent->data.mem = g_strdup(tmp);
	msgcontent->content = MIMECONTENT_MEM;
	g_free(tmp);

	return TRUE;
}

static PrivacySystem pgpinline_system = {
	"pgpinline",			/* id */
	"PGP Inline",			/* name */

	pgpinline_free_privacydata,	/* free_privacydata */

	pgpinline_is_signed,		/* is_signed(MimeInfo *) */
	pgpinline_check_signature,	/* check_signature(MimeInfo *) */
	pgpinline_get_sig_status,	/* get_sig_status(MimeInfo *) */
	pgpinline_get_sig_info_short,	/* get_sig_info_short(MimeInfo *) */
	pgpinline_get_sig_info_full,	/* get_sig_info_full(MimeInfo *) */

	pgpinline_is_encrypted,		/* is_encrypted(MimeInfo *) */
	pgpinline_decrypt,		/* decrypt(MimeInfo *) */

	TRUE,
	pgpinline_sign,

	TRUE,
	pgpinline_get_encrypt_data,
	pgpinline_encrypt,
};

void pgpinline_init()
{
	privacy_register_system(&pgpinline_system);
}

void pgpinline_done()
{
	privacy_unregister_system(&pgpinline_system);
}

#endif /* USE_GPGME */
