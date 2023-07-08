/** phiola: FLAC encode
2015, Simon Zolin */

#include <acodec/alib3-bridge/flac.h>

struct flac_enc {
	ffflac_enc fl;
	uint state;
};

static void* flac_enc_create(phi_track *t)
{
	if (!ffsz_eq(t->data_type, "pcm")) {
		errlog(t, "unsupported input data format: %s", t->data_type);
		return PHI_OPEN_ERR;
	}

	struct flac_enc *f = ffmem_new(struct flac_enc);
	ffflac_enc_init(&f->fl);
	f->fl.level = 6;
	return f;
}

static void flac_enc_free(struct flac_enc *f, phi_track *t)
{
	ffflac_enc_close(&f->fl);
	ffmem_free(f);
}

static int flac_enc_encode(struct flac_enc *f, phi_track *t)
{
	int r;

	switch (f->state) {

	case 0:
	case 1:
		if (0 != (r = ffflac_create(&f->fl, (void*)&t->oaudio.format))) {

			if (f->state == 0 && r == FLAC_EFMT) {
				t->oaudio.conv_format.interleaved = 0;
				f->state = 1;
				return PHI_MORE;
			}

			errlog(t, "ffflac_create(): %s", ffflac_enc_errstr(&f->fl));
			return PHI_ERR;
		}
		t->oaudio.flac_vendor = flac_vendor();
		t->data_type = "flac";
		// fallthrough

	case 2:
		if (t->oaudio.format.interleaved) {
			if (f->state == 0) {
				t->oaudio.conv_format.interleaved = 0;
				f->state = 2;
				return PHI_MORE;
			}
			errlog(t, "unsupported input PCM format");
			return PHI_ERR;
		}
		break;

	case 3:
		break;
	}

	if (t->chain_flags & PHI_FFWD) {
		f->fl.pcm = (const void**)t->data_in.ptr;
		f->fl.pcmlen = t->data_in.len;
		if (t->chain_flags & PHI_FFIRST)
			ffflac_enc_fin(&f->fl);
	}

	if (f->state != 3) {
		f->state = 3;
		ffstr_set(&t->data_out, (void*)&f->fl.info, sizeof(struct flac_info));
		return PHI_DATA;
	}

	r = ffflac_encode(&f->fl);

	switch (r) {
	case FFFLAC_RMORE:
		return PHI_MORE;

	case FFFLAC_RDATA:
		t->oaudio.flac_frame_samples = f->fl.frsamps;
		break;

	case FFFLAC_RDONE:
		ffstr_set(&t->data_out, (void*)&f->fl.info, sizeof(f->fl.info));
		return PHI_DONE;

	case FFFLAC_RERR:
	default:
		errlog(t, "ffflac_encode(): %s", ffflac_enc_errstr(&f->fl));
		return PHI_ERR;
	}

	ffstr_set(&t->data_out, f->fl.data, f->fl.datalen);
	dbglog(t, "output: %L bytes", t->data_out.len);
	return PHI_DATA;
}

const phi_filter phi_flac_enc = {
	flac_enc_create, (void*)flac_enc_free, (void*)flac_enc_encode,
	"flac-encode"
};
