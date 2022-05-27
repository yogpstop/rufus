/*
 * unzstd implementation for Bled/busybox
 *
 * Copyright © 2014-2020 Pete Batard <pete@akeo.ie>
 * Based on xz-embedded © Lasse Collin <lasse.collin@tukaani.org> - Public Domain
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "bb_archive.h"

#include "zstd.h"
#include "zstd_errors.h"

IF_DESKTOP(long long) int FAST_FUNC unpack_zst_stream(transformer_state_t *xstate)
{
	size_t const in_size = ZSTD_DStreamInSize();
	size_t const out_size = ZSTD_DStreamOutSize();
	IF_DESKTOP(long long) int n = 0;
	ZSTD_inBuffer b_in;
	ZSTD_outBuffer b_out;
	ZSTD_DCtx* dctx;
	size_t ret;
	uint8_t *in = NULL, *out = NULL;
	ssize_t nwrote;

	dctx = ZSTD_createDCtx();
	if (!dctx)
		bb_error_msg_and_err("ZSTD_createDCtx() failed!");

	in = xmalloc(in_size);
	out = xmalloc(out_size);

	b_in.src = in;
	b_in.pos = 0;
	b_in.size = 0;
	b_out.dst = out;
	b_out.pos = 0;
	b_out.size = out_size;

	while (true) {
		if (b_in.pos == b_in.size) {
			b_in.size = safe_read(xstate->src_fd, in, (unsigned int) in_size);
			if ((int)b_in.size < 0)
				bb_error_msg_and_err("read error (errno: %d)", errno);
			b_in.pos = 0;
		}
		ret = ZSTD_decompressStream(dctx, &b_out , &b_in);

		if (b_in.size == 0 || b_out.pos == out_size) {
			nwrote = transformer_write(xstate, out, b_out.pos);
			if (nwrote == -ENOSPC) {
				ret = (size_t)-ZSTD_error_dstSize_tooSmall;
				goto out;
			}
			if (nwrote < 0) {
				ret = (size_t)-ZSTD_error_corruption_detected;
				bb_error_msg_and_err("write error (errno: %d)", errno);
			}
			IF_DESKTOP(n += nwrote;)
			b_out.pos = 0;
		}

		if (!ZSTD_isError(ret) && b_in.size == 0 && b_out.pos == 0)
			goto out;

		if (!ZSTD_isError(ret))
			continue;

		bb_error_msg_and_err("Zstd decompression error: %s", ZSTD_getErrorName(ret));
	}

out:
err:
	ZSTD_freeDCtx(dctx);
	free(in);
	free(out);
	if (!ZSTD_isError(ret))
		return n;
	else if (ret == (size_t)-ZSTD_error_dstSize_tooSmall)
		return xstate->mem_output_size_max;
	else
		return ret;
}
