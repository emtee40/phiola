/** phiola: ALAC input.
2016, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <FFOS/ffos-extern.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <acodec/alac-dec.h>

static const void* alac_iface(const char *name)
{
	if (ffsz_eq(name, "decode")) return &phi_alac_dec;
	return NULL;
}

static const phi_mod phi_alac_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	alac_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_alac_mod;
}
