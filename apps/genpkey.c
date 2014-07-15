/* apps/genpkey.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
#include <stdio.h>
#include <string.h>
#include "apps.h"
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

static int init_keygen_file(BIO *err, EVP_PKEY_CTX **pctx,
				const char *file, ENGINE *e);
static int genpkey_cb(EVP_PKEY_CTX *ctx);


enum options {
	OPT_ERR = -1, OPT_EOF = 0, OPT_HELP,
	OPT_ENGINE, OPT_OUTFORM, OPT_OUT, OPT_PASS, OPT_PARAMFILE,
	OPT_ALGORITHM, OPT_PKEYOPT, OPT_GENPARAM, OPT_TEXT, OPT_CIPHER,
};

OPTIONS genpkey_options[] = {
	{ "help", OPT_HELP, '-', "Display this summary" },
	{ "out", OPT_OUT, '>', "Output file" },
	{ "outform", OPT_OUTFORM, 'F', "output format (DER or PEM)" },
	{ "pass", OPT_PASS, 's', "Output file pass phrase source" },
	{ "paramfile", OPT_PARAMFILE, '<', "Parameters file" },
	{ "algorithm", OPT_ALGORITHM, 's', "The public key algorithm" },
	{ "pkeyopt", OPT_PKEYOPT, 's', "Set the public key algorithm option as opt:value" },
	{ "genparam", OPT_GENPARAM, '-', "Generate parameters, not key" },
	{ "text", OPT_TEXT, '-', "Print the in text" },
	{ "", OPT_CIPHER, '-', "Cipher to use to encrypt the key" },
#ifndef OPENSSL_NO_ENGINE
	{ "engine", OPT_ENGINE, 's', "Use engine, possibly a hardware device" },
#endif
	{ OPT_HELP_STR, 1, 1, "Order of options may be important!  See the documentation.\n" },
	{ NULL }
};

int genpkey_main(int argc, char **argv)
	{
	ENGINE *e=NULL;
	BIO *in=NULL, *out=NULL;
	const EVP_CIPHER *cipher=NULL;
	EVP_PKEY *pkey=NULL;
	EVP_PKEY_CTX *ctx=NULL;
	char *outfile=NULL, *passarg=NULL, *pass=NULL;
	int outformat=FORMAT_PEM, text=0, ret=1, rv, do_param=0;
	enum options o;
	char* prog;

	prog = opt_init(argc, argv, genpkey_options);
	while ((o = opt_next()) != OPT_EOF) {
		switch (o) {
		case OPT_EOF:
		case OPT_ERR:
		case OPT_HELP:
err:
			opt_help(genpkey_options);
			goto end;
		case OPT_OUTFORM:
			opt_format(opt_arg(), 1, &outformat);
			break;
		case OPT_OUT:
			outfile= opt_arg();
			break;

		case OPT_PASS:
			passarg= opt_arg();
			break;
#ifndef OPENSSL_NO_ENGINE
		case OPT_ENGINE:
        		e = setup_engine(bio_err, opt_arg(), 0);
			break;
#endif
		case OPT_PARAMFILE:
			if (do_param == 1)
				goto err;
			if (!init_keygen_file(bio_err, &ctx, opt_arg(), e))
				goto end;
			break;
		case OPT_ALGORITHM:
			if (!init_gen_str(bio_err, &ctx, opt_arg(),e, do_param))
				goto end;
			break;
		case OPT_PKEYOPT:
			if (ctx == NULL) {
				BIO_printf(bio_err,
					"%s: No keytype specified.\n", prog);
				goto err;
			}
			if (pkey_ctrl_string(ctx, opt_arg()) <= 0) {
				BIO_printf(bio_err,
					"%s: Error setting %s parameter:\n",
					prog, opt_arg());
				ERR_print_errors(bio_err);
				goto end;
				}
			break;
		case OPT_GENPARAM:
			if (ctx != NULL)
				goto err;
			do_param = 1;
			break;
		case OPT_TEXT:
			text=1;
			break;
		case OPT_CIPHER:
			if (!opt_cipher(opt_unknown(), &cipher)
			 || do_param == 1)
				goto err;
		}
	}

	if (ctx == NULL)
		goto err;

	if (!app_passwd(bio_err, passarg, NULL, &pass, NULL)) {
		BIO_puts(bio_err, "Error getting password\n");
		goto end;
		}

	out = bio_open_default(outfile, "wb");
	if (out == NULL)
		goto end;

	EVP_PKEY_CTX_set_cb(ctx, genpkey_cb);
	EVP_PKEY_CTX_set_app_data(ctx, bio_err);

	if (do_param)
		{
		if (EVP_PKEY_paramgen(ctx, &pkey) <= 0)
			{
			BIO_puts(bio_err, "Error generating parameters\n");
			ERR_print_errors(bio_err);
			goto end;
			}
		}
	else
		{
		if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
			{
			BIO_puts(bio_err, "Error generating key\n");
			ERR_print_errors(bio_err);
			goto end;
			}
		}

	if (do_param)
		rv = PEM_write_bio_Parameters(out, pkey);
	else if (outformat == FORMAT_PEM) 
		rv = PEM_write_bio_PrivateKey(out, pkey, cipher, NULL, 0,
								NULL, pass);
	else if (outformat == FORMAT_ASN1)
		rv = i2d_PrivateKey_bio(out, pkey);
	else
		{
		BIO_printf(bio_err, "Bad format specified for key\n");
		goto end;
		}

	if (rv <= 0)
		{
		BIO_puts(bio_err, "Error writing key\n");
		ERR_print_errors(bio_err);
		}

	if (text)
		{
		if (do_param)
			rv = EVP_PKEY_print_params(out, pkey, 0, NULL);
		else
			rv = EVP_PKEY_print_private(out, pkey, 0, NULL);

		if (rv <= 0)
			{
			BIO_puts(bio_err, "Error printing key\n");
			ERR_print_errors(bio_err);
			}
		}

	ret = 0;

	end:
	if (pkey)
		EVP_PKEY_free(pkey);
	if (ctx)
		EVP_PKEY_CTX_free(ctx);
	if (out)
		BIO_free_all(out);
	BIO_free(in);
	if (pass)
		OPENSSL_free(pass);

	return ret;
	}

static int init_keygen_file(BIO *err, EVP_PKEY_CTX **pctx,
				const char *file, ENGINE *e)
	{
	BIO *pbio;
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *ctx = NULL;
	if (*pctx)
		{
		BIO_puts(err, "Parameters already set!\n");
		return 0;
		}

	pbio = BIO_new_file(file, "r");
	if (!pbio)
		{
		BIO_printf(err, "Can't open parameter file %s\n", file);
		return 0;
		}

	pkey = PEM_read_bio_Parameters(pbio, NULL);
	BIO_free(pbio);

	if (!pkey)
		{
		BIO_printf(bio_err, "Error reading parameter file %s\n", file);
		return 0;
		}

	ctx = EVP_PKEY_CTX_new(pkey, e);
	if (!ctx)
		goto err;
	if (EVP_PKEY_keygen_init(ctx) <= 0)
		goto err;
	EVP_PKEY_free(pkey);
	*pctx = ctx;
	return 1;

	err:
	BIO_puts(err, "Error initializing context\n");
	ERR_print_errors(err);
	if (ctx)
		EVP_PKEY_CTX_free(ctx);
	if (pkey)
		EVP_PKEY_free(pkey);
	return 0;

	}

int init_gen_str(BIO *err, EVP_PKEY_CTX **pctx,
			const char *algname, ENGINE *e, int do_param)
	{
	EVP_PKEY_CTX *ctx = NULL;
	const EVP_PKEY_ASN1_METHOD *ameth;
	ENGINE *tmpeng = NULL;
	int pkey_id;

	if (*pctx)
		{
		BIO_puts(err, "Algorithm already set!\n");
		return 0;
		}

	ameth = EVP_PKEY_asn1_find_str(&tmpeng, algname, -1);

#ifndef OPENSSL_NO_ENGINE
	if (!ameth && e)
		ameth = ENGINE_get_pkey_asn1_meth_str(e, algname, -1);
#endif

	if (!ameth)
		{
		BIO_printf(bio_err, "Algorithm %s not found\n", algname);
		return 0;
		}

	ERR_clear_error();

	EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, ameth);
#ifndef OPENSSL_NO_ENGINE
	if (tmpeng)
		ENGINE_finish(tmpeng);
#endif
	ctx = EVP_PKEY_CTX_new_id(pkey_id, e);

	if (!ctx)
		goto err;
	if (do_param)
		{
		if (EVP_PKEY_paramgen_init(ctx) <= 0)
			goto err;
		}
	else
		{
		if (EVP_PKEY_keygen_init(ctx) <= 0)
			goto err;
		}

	*pctx = ctx;
	return 1;

	err:
	BIO_printf(err, "Error initializing %s context\n", algname);
	ERR_print_errors(err);
	if (ctx)
		EVP_PKEY_CTX_free(ctx);
	return 0;

	}

static int genpkey_cb(EVP_PKEY_CTX *ctx)
	{
	char c='*';
	BIO *b = EVP_PKEY_CTX_get_app_data(ctx);
	int p;
	p = EVP_PKEY_CTX_get_keygen_info(ctx, 0);
	if (p == 0) c='.';
	if (p == 1) c='+';
	if (p == 2) c='*';
	if (p == 3) c='\n';
	BIO_write(b,&c,1);
	(void)BIO_flush(b);
#ifdef LINT
	p=n;
#endif
	return 1;
	}