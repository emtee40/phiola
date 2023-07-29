/** phiola: GUI: Main window
2023, Simon Zolin */

#include <gui/gui.h>
#include <track.h>
#include <util/util.h>
#include <util/util.hpp>
#include <afilter/pcm.h>

#define VOLUME_MAX 125

struct gui_wmain {
	ffui_wndxx wnd;
	ffui_menu mm;
	ffui_btn bpause, bstop, bprev, bnext;
	ffui_labelxx lpos;
	ffui_trackbarxx tvol, tpos;
	ffui_tabxx tabs;
	ffui_viewxx vlist;
	ffui_stbarxx stbar;

	struct phi_queue_entry *qe_active;
	uint tabs_counter;
};

FF_EXTERN const ffui_ldr_ctl wmain_ctls[] = {
	FFUI_LDR_CTL(gui_wmain, wnd),
	FFUI_LDR_CTL(gui_wmain, mm),
	FFUI_LDR_CTL(gui_wmain, bpause),
	FFUI_LDR_CTL(gui_wmain, bstop),
	FFUI_LDR_CTL(gui_wmain, bprev),
	FFUI_LDR_CTL(gui_wmain, bnext),
	FFUI_LDR_CTL(gui_wmain, lpos),
	FFUI_LDR_CTL(gui_wmain, tvol),
	FFUI_LDR_CTL(gui_wmain, tpos),
	FFUI_LDR_CTL(gui_wmain, tabs),
	FFUI_LDR_CTL(gui_wmain, vlist),
	FFUI_LDR_CTL(gui_wmain, stbar),
	FFUI_LDR_CTL_END
};

enum LIST_HDR {
	H_INDEX,
	H_ARTIST,
	H_TITLE,
	H_DUR,
	H_INFO,
	H_DATE,
	H_ALBUM,
	H_FN,
	_H_LAST,
};

static const char list_colname[][10] = {
	"", // H_INDEX
	"artist", // H_ARTIST
	"title", // H_TITLE
	"_phi_dur", // H_DUR
	"_phi_info", // H_INFO
	"date", // H_DATE
	"album", // H_ALBUM
	"", // H_FN
};

void wmain_userconf_write(ffvec *buf)
{
	gui_wmain *m = gg->wmain;
	ffui_pos pos = m->wnd.pos();
	ffvec_addfmt(buf, "wmain.pos %d %d %u %u\n", pos.x, pos.y, pos.cx, pos.cy);
	ffvec_addfmt(buf, "tvol.val %u\n", m->tvol.get());

	ffvec_addsz(buf, "vlist.col");
	ffui_viewcolxx vc = {};
	vc.width(0);
	for (uint i = 0;  i != _H_LAST;  i++) {
		ffvec_addfmt(buf, " %u", m->vlist.column(i, &vc).width());
	}
	ffvec_addsz(buf, "\n");

	ffvec_addfmt(buf, "play.cursor %u\n", gd->cursor);
}

int wmain_userconf_read(ffstr key, ffstr val)
{
	gui_wmain *m = gg->wmain;
	ffstrxx k = key, v = val;

	if (k == "wmain.pos") {
		ffui_pos pos;
		if (!v.matchf("%d %d %u %u", &pos.x, &pos.y, &pos.cx, &pos.cy))
			m->wnd.place(pos);

	} else if (k == "tvol.val") {
		uint n;
		if (!v.matchf("%u", &n))
			m->tvol.set(n);

	} else if (k == "vlist.col") {
		ffui_viewcolxx vc = {};
		for (uint i = 0;  i != _H_LAST;  i++) {
			ffstrxx s;
			v.split(' ', &s, &v);
			uint width;
			if (!s.matchf("%u", &width)) {
				vc.width(width);
				m->vlist.column(i, vc);
			}
		}

	} else if (k == "play.cursor") {
		v.matchf("%u", &gd->cursor);

	} else {
		return -1;
	}

	return 0;
}

/** Set status bar text */
void wmain_status(const char *fmt, ...)
{
	gui_wmain *m = gg->wmain;
	va_list va;
	va_start(va, fmt);
	char buf[1000];
	ffsz_formatv(buf, sizeof(buf), fmt, va);
	m->stbar.text(buf);
	va_end(va);
}

static const char pcm_fmtstr[][8] = {
	"float32",
	"float64",
	"int16",
	"int24",
	"int24-4",
	"int32",
	"int8",
};
static const ushort pcm_fmt[] = {
	PHI_PCM_FLOAT32,
	PHI_PCM_FLOAT64,
	PHI_PCM_16,
	PHI_PCM_24,
	PHI_PCM_24_4,
	PHI_PCM_32,
	PHI_PCM_8,
};

static const char* pcm_fmt_str(uint fmt)
{
	int r = ffarrint16_find(pcm_fmt, FF_COUNT(pcm_fmt), fmt);
	if (r < 0)
		return "";
	return pcm_fmtstr[r];
}

static const char _pcm_channelstr[][10] = {
	"mono", "stereo",
	"3-channel", "4-channel", "5-channel",
	"5.1", "6.1", "7.1"
};

static const char* pcm_channelstr(uint channels)
{
	return _pcm_channelstr[ffmin(channels - 1, FF_COUNT(_pcm_channelstr) - 1)];
}

/** Thread: worker */
int wmain_track_new(struct phi_queue_entry *qe, uint time_total, phi_track *t)
{
	gui_wmain *m = gg->wmain;
	if (!m) return -1;

	char buf[1000];
	ffsz_format(buf, sizeof(buf), "%u kbps, %s, %u Hz, %s, %s"
		, (t->audio.bitrate + 500) / 1000
		, t->audio.decoder
		, t->audio.format.rate
		, pcm_fmt_str(t->audio.format.format)
		, pcm_channelstr(t->audio.format.channels));
	gd->metaif->set(&t->meta, FFSTR_Z("_phi_info"), FFSTR_Z(buf));

	m->tpos.range(time_total);
	qe->length_msec = time_total * 1000;

	void *qe_active = m->qe_active;
	m->qe_active = qe;

	int idx;
	if (qe_active && -1 != (idx = gd->queue->index(qe_active)))
		m->vlist.update(idx, 0);

	if (-1 != (idx = gd->queue->index(qe))) {
		m->vlist.update(idx, 0);
		gd->cursor = idx;
	}

	ffstr artist = {}, title = {};
	gd->metaif->find(&t->meta, FFSTR_Z("artist"), &artist, 0);
	gd->metaif->find(&t->meta, FFSTR_Z("title"), &title, 0);
	if (!title.len)
		ffpath_split3_str(FFSTR_Z(qe->conf.ifile.name), NULL, &title, NULL); // use filename as a title

	ffsz_format(buf, sizeof(buf), "%S - %S - phiola", &artist, &title);
	m->wnd.title(buf);
	return 0;
}

/** Thread: worker */
void wmain_track_close()
{
	gui_wmain *m = gg->wmain;
	if (!m) return;

	if (m->qe_active != NULL) {
		int idx = gd->queue->index(m->qe_active);
		m->qe_active = NULL;
		if (idx >= 0)
			m->vlist.update(idx, 0);
	}

	m->wnd.title("phiola");
	m->lpos.text("");
	m->tpos.range(0);
}

/** Thread: worker */
void wmain_track_update(uint time_cur, uint time_total)
{
	gui_wmain *m = gg->wmain;
	if (!m) return;

	m->tpos.set(time_cur);

	char buf[256];
	ffsz_format(buf, sizeof(buf), "%u:%02u / %u:%02u"
		, time_cur / 60, time_cur % 60
		, time_total / 60, time_total % 60);
	m->lpos.text(buf);
}

static void list_display(struct ffui_view_disp *disp)
{
	gui_wmain *m = gg->wmain;
	char buf[1000];
	ffstr *val = NULL, s;
	struct phi_queue_entry *qe = gd->queue->ref(NULL, disp->idx);
	if (!qe)
		return;

	switch (disp->sub) {
	case H_INDEX:
		ffsz_format(buf, sizeof(buf), "%s%u"
			, (qe == m->qe_active) ? "> " : "", disp->idx + 1);
		ffstr_setz(&s, buf);
		val = &s;
		break;

	case H_FN:
		s = FFSTR_Z(qe->conf.ifile.name);
		val = &s;
		break;

	default:
		if (!gd->metaif->find(&qe->conf.meta, FFSTR_Z(list_colname[disp->sub]), &s, PHI_META_PRIVATE))
			val = &s;
	}

	switch (disp->sub) {
	case H_TITLE:
		if (!val || !val->len) {
			ffpath_split3_str(FFSTR_Z(qe->conf.ifile.name), NULL, &s, NULL); // use filename as a title
			val = &s;
		}
		break;

	case H_DUR:
		if (!val && qe->length_msec != 0) {
			uint sec = qe->length_msec / 1000;
			ffsz_format(buf, sizeof(buf), "%u:%02u", sec / 60, sec % 60);
			ffstr_setz(&s, buf);
			gd->metaif->set(&qe->conf.meta, FFSTR_Z("_phi_dur"), s);
			val = &s;
		}
		break;
	}

	if (val)
		disp->text.len = ffmem_ncopy(disp->text.ptr, disp->text.len, val->ptr, val->len);

	gd->queue->unref(qe);
}

/** Thread: worker */
static void q_on_change(struct phi_queue *q, uint flags, uint pos)
{
	gui_wmain *m = gg->wmain;
	switch (flags & 0xff) {
	case 'a':
		m->vlist.update(pos, 1);  break;

	case 'r':
		m->vlist.update(pos, -1);
		m->vlist.update(pos, 0);
		break;

	case 'c':
		m->vlist.clear();  break;

	// case 'n':
	// case 'd':
	}
}

static void vol_set(uint id)
{
	gui_wmain *m = gg->wmain;
	uint v = gd->volume;

	switch (id) {
	case A_VOL:
		v = m->tvol.get();  goto apply;

	case A_VOLUP:
		v = ffmin(v + 5, VOLUME_MAX);  break;

	case A_VOLDOWN:
		v = ffmax((int)v - 5, 0);  break;
	}

	m->tvol.set(v);

apply:
	if (v <= 100)
		gd->gain_db = vol2db(v, 48);
	else
		gd->gain_db = vol2db_inc(v - 100, 25, 6);
	gd->volume = v;
	wmain_status("Volume: %.02FdB", gd->gain_db);
	gui_core_task(ctl_volume);
}

/** Thread: main */
static void list_remove(void *ptr)
{
	ffui_sel *sel = (ffui_sel*)ptr;
	for (int i = sel->len - 1;  i >= 0;  i--) {
		gd->queue->remove(gd->queue->at(NULL, *ffslice_itemT(sel, i, uint)));
	}
	ffui_view_sel_free(sel);
}

/** Create a new tab */
static void tab_new()
{
	gui_wmain *m = gg->wmain;
	char buf[100];
	ffsz_format(buf, sizeof(buf), "Playlist %u", ++m->tabs_counter);
	m->tabs.add(buf);
}

static void wmain_action(ffui_wnd *wnd, int id)
{
	gui_wmain *m = gg->wmain;
	dbglog("%s cmd:%u", __func__, id);

	switch (id) {
	case A_QUIT:
		m->wnd.close();  break;

	case A_LIST_REMOVE:
		gui_core_task_ptr(list_remove, m->vlist.selected());  break;

	case A_LIST_CLEAR:
		gui_core_task_uint(ctl_action, A_LIST_CLEAR);  break;

	case A_PLAY: {
		int i = m->vlist.focused();
		if (i >= 0)
			gui_core_task_uint(ctl_play, i);
		break;
	}

	case A_VOL:
	case A_VOLUP:
	case A_VOLDOWN:
		vol_set(id);  break;

	case A_SEEK:
		gd->seek_pos_sec = m->tpos.get();
		gui_core_task_uint(ctl_action, A_SEEK);
		break;

	case A_LIST_DISPLAY:
		list_display(&m->vlist.disp);  break;

	case A_CLOSE:
		gui_quit();  break;

	case A_FILE_DRAGDROP: {
		ffstr d = {};
		ffstr_dupstr(&d, &m->vlist.drop_data);
		gui_core_task_data(gui_dragdrop, d);
		break;
	}

	default:
		gui_core_task_uint(ctl_action, id);
	}
}

void wmain_init()
{
	gui_wmain *m = ffmem_new(gui_wmain);
	gg->wmain = m;
	m->vlist.dispinfo_id = A_LIST_DISPLAY;
	m->wnd.on_action = wmain_action;
	m->wnd.onclose_id = A_CLOSE;
}

void wmain_show()
{
	gui_wmain *m = gg->wmain;
	tab_new();
	m->wnd.show(1);
	m->vlist.update(0, gd->queue->count(NULL));
	m->vlist.drag_drop_init(A_FILE_DRAGDROP);
	gd->queue->on_change(q_on_change);
}