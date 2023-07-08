/** phiola: Vorbis input/output.
2016, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <acodec/alib3-bridge/vorbis.h>
#include <format/mmtag.h>
#include <FFOS/ffos-extern.h>

static const phi_core *core;
static const phi_meta_if *phi_metaif;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <acodec/vorbis-enc.h>
#include <acodec/vorbis-dec.h>

static const void* vorbis_iface(const char *name)
{
	if (ffsz_eq(name, "decode")) return &phi_vorbis_dec;
	else if (ffsz_eq(name, "encode")) return &phi_vorbis_enc;
	return NULL;
}

static const phi_mod phi_vorbis_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	vorbis_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	phi_metaif = core->mod("format.meta");
	return &phi_vorbis_mod;
}
