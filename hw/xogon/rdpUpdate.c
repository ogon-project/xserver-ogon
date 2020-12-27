/**
 * ogon Remote Desktop Services
 * X11 Backend
 *
 * Copyright (C) 2005-2013 Jay Sorg
 * Copyright (C) 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright (C) 2013-2018 Thincast Technologies GmbH
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David FORT <contact@hardening-consulting.com>
 * Jay Sorg
 * Marc-André Moreau <marcandre.moreau@gmail.com>
 * Martin Haimberger <martin.haimberger@thincast.com>
 * Norbert Federa <norbert.federa@thincast.com>
 */

#include "rdp.h"

#include <sys/select.h>
#include <winpr/crt.h>
#include <winpr/pipe.h>
#include <winpr/synch.h>
#include <winpr/collections.h>
#include <winpr/user.h>

#include <ogon/backend.h>
#include <ogon/service.h>
#include <ogon/dmgbuf.h>
#include <ogon/version.h>
#include <ogon/message.h>
#include <ogon/build-config.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "rdpScreen.h"
#include "rdpUpdate.h"
#include "rdpRandr.h"

#include <string.h>

#include <sys/stat.h>

#define LOG_LEVEL 0
#define LLOG(_level, _args) \
		do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
		do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

static int g_clientfd = -1;
static HANDLE g_serverPipe = INVALID_HANDLE_VALUE;
static int g_serverfd = -1;
static ogon_backend_service* g_service;
static int g_connected = 0;
int g_active = 0;
void* g_rdsDamage = NULL;
UINT32 g_rdsDamageLastBufferId = 0;
BOOL g_rdsDamageSyncRequested = FALSE;

static int g_button_mask = 0;

extern ScreenPtr g_pScreen;
extern int g_Bpp;
extern int g_Bpp_mask;
extern rdpScreenInfoRec g_rdpScreen;
extern int g_width_max;
extern int g_height_max;
extern DeviceIntPtr g_multitouch;

static wArrayList* g_Messages = NULL;

typedef struct winLayoutMapping
{
  unsigned int keyboardLayoutId;
  const char *xkblayout;
} winLayoutMapEntry;

winLayoutMapEntry layoutMap[] =
{
	{0x00000409, "us"},
	{0x00000407, "de"},
	{0x00010407, "de"},
	{0x0000040c, "fr"},
	{0x00000405, "cz"},
	{0x00020409, "us_intl"},
	{0x00010409, "dvorak"},
	{0x00000406, "dk"},
	{0x00000809, "gb"},
	{0x00001809, "ie"},
	{0x0000040a, "es"},
	{0x0000080c, "be"},
	{0x0000040e, "hu"},
	{0x00000410, "it"},
	{0x00000813, "be"},
	{0x00000414, "no"},
	{0x00000419, "ru"}
};

int init_messages_list(void)
{
	g_Messages = ArrayList_New(FALSE);
	return 0;
}

static const char *rds_get_keyboard_layout(unsigned int winKeyboardId)
{
  int i;
  for (i = 0; i < ARRAYSIZE(layoutMap); ++i)
  {
    if (layoutMap[i].keyboardLayoutId == winKeyboardId)
      return layoutMap[i].xkblayout;
  }
  return NULL;
}

static void system_background(char *cmd)
{
	pid_t pid = fork();
	int status;
	if (0 == pid)
	{
		/* child */
		if (0 == fork())
		{
			int retCode = system(cmd);
			if (retCode != 0)
				fprintf(stderr, "x11rdp: error executing \"%s\"\n", cmd);
			exit(0);
		}
		else
		{
			exit(0);
		}
	}
	else
	{
		/* parent */
		waitpid(pid, &status, 0);
	}
}

int rds_service_disconnect(ogon_backend_service *service);

int rdp_send_message(UINT16 type, ogon_message *msg)
{
	if (g_connected && g_active)
	{
		if (!ogon_service_write_message(g_service, type, msg))
		{
			fprintf(stderr, "%s: connection closed during write\n", __FUNCTION__);
			rds_service_disconnect(g_service);
			return -1;
		}
	}

	return 0;
}

int rdp_send_framebuffer_info(void)
{
	ogon_msg_framebuffer_info msg;

	if (!g_active)
		return 0;

	msg.version = 1;
	msg.width = g_rdpScreen.width;
	msg.height = g_rdpScreen.height;
	msg.scanline = g_rdpScreen.scanline;
	msg.bitsPerPixel = g_rdpScreen.depth;
	msg.bytesPerPixel = g_rdpScreen.bytesPerPixel;
	msg.userId = getuid();
	msg.multiseatCapable = FALSE;

	rdp_send_message(OGON_SERVER_FRAMEBUFFER_INFO, (ogon_message*)&msg);

	return 0;
}

int rdp_send_version(void)
{
	ogon_msg_version msg;

	if (!g_active)
		return 0;

	msg.versionMajor = OGON_PROTOCOL_VERSION_MAJOR;
	msg.versionMinor = OGON_PROTOCOL_VERSION_MINOR;

	rdp_send_message(OGON_CLIENT_VERSION, (ogon_message*) &msg);

	return 0;
}

static int rdp_send_sync_framebuffer_reply(void)
{
	ogon_msg_framebuffer_sync_reply msg;

	if (!g_active)
		return 0;

	msg.bufferId = ogon_dmgbuf_get_id(g_rdsDamage);

	rdp_send_message(OGON_SERVER_FRAMEBUFFER_SYNC_REPLY, (ogon_message*) &msg);

	return 0;
}

static int rdp_sync_damage_rect(int x, int y, int w, int h, char* dst, char* src)
{
	int i, wb, offset;

	wb = w * g_rdpScreen.bytesPerPixel;

	offset = (y * g_rdpScreen.scanline) + (x * g_rdpScreen.bytesPerPixel);

	src += offset;
	dst += offset;

	for (i = 0; i < h; i++)
	{
		memcpy(dst, src, wb);
		src += g_rdpScreen.scanline;
		dst += g_rdpScreen.scanline;
	}
	return 0;
}

int rdp_handle_damage_region(int callerId)
{
	RegionPtr region;
	BoxPtr rects;
	RDP_RECT* rdsDamageRects;
	BoxRec singleRect;
	int i, numRects;
	char* src;
	char *dst;

	LLOGLN(10, ("rdp_handle_damage_region: callerId=%d", callerId));

	if (!g_rdpScreen.x11Damage || !g_active || !g_rdsDamageSyncRequested)
	{
		return 0;
	}

	/* make sure that the sync buffer is large enough */
	if (ogon_dmgbuf_get_fbsize(g_rdsDamage) < g_rdpScreen.sizeInBytes) {
		ErrorF("rdp_handle_damage_region: error sync buffer is too small\n");
		return 0;
	}

	if (g_rdpScreen.sendFullDamage)
	{
		singleRect.x1 = 0;
		singleRect.y1 = 0;
		singleRect.x2 = g_pScreen->width;
		singleRect.y2 = g_pScreen->height;

		numRects = 1;
		rects = &singleRect;
		g_rdpScreen.sendFullDamage = FALSE;
	}
	else
	{
		region = DamageRegion(g_rdpScreen.x11Damage);
		RegionIntersect(region, region, &g_rdpScreen.screenRec);

		if (!RegionNotEmpty(region))
			return 0;

		numRects = REGION_NUM_RECTS(region);
		rects = REGION_RECTS(region);

		if (numRects > ogon_dmgbuf_get_max_rects(g_rdsDamage))
		{
			/* FIXME: if required we could intelligently reduce the region to maxDamageRects instead */
			rects = REGION_EXTENTS(g_pScreen, region);
			numRects = 1;
		}
	}

	LLOGLN(10, ("rdp_handle_damage_region: numRects=%d", numRects));

	ogon_dmgbuf_set_num_rects(g_rdsDamage, numRects);

	dst = (char*)ogon_dmgbuf_get_data(g_rdsDamage);
	src = (char*)g_rdpScreen.pfbMemory;

	rdsDamageRects = ogon_dmgbuf_get_rects(g_rdsDamage, NULL);

	for (i = 0; i < numRects; i++)
	{
		int x, y, w, h;
		x = rdsDamageRects[i].x = rects[i].x1;
		y = rdsDamageRects[i].y = rects[i].y1;
		w = rdsDamageRects[i].width = rects[i].x2 - rects[i].x1;
		h = rdsDamageRects[i].height = rects[i].y2 - rects[i].y1;
		rdp_sync_damage_rect(x, y, w, h, dst, src);
	}

	DamageEmpty(g_rdpScreen.x11Damage);

	g_rdsDamageSyncRequested = FALSE;
	rdp_send_sync_framebuffer_reply();

	return 0;
}

void rdp_detach_rds_framebuffer(void)
{
	if (g_rdsDamage)
	{
		g_rdsDamageLastBufferId = ogon_dmgbuf_get_id(g_rdsDamage);
		ogon_dmgbuf_free(g_rdsDamage);
		g_rdsDamage = NULL;
		g_rdsDamageSyncRequested = FALSE;
	}
}

void rdp_attach_rds_framebuffer(int bufferId)
{
	BoxRec screenBox;
	RegionRec screenRegion;
	RegionPtr damageRegion;
	char *dst = NULL;
	char *src = NULL;
	unsigned int size = 0;

	if (g_rdsDamage)
		rdp_detach_rds_framebuffer();

	g_rdsDamage = ogon_dmgbuf_connect(bufferId);

	if (g_rdsDamage && !g_rdpScreen.x11DamageRegistered)
	{
		DamageRegister(&(g_pScreen->GetScreenPixmap(g_pScreen)->drawable), g_rdpScreen.x11Damage);
		g_rdpScreen.x11DamageRegistered = TRUE;
	}

	screenBox.x1 = 0;
	screenBox.y1 = 0;
	screenBox.x2 = g_rdpScreen.width;
	screenBox.y2 = g_rdpScreen.height;
	RegionInit(&screenRegion, &screenBox, 1);
	damageRegion = DamageRegion(g_rdpScreen.x11Damage);

	RegionUnion(damageRegion, damageRegion, &screenRegion);

	RegionUninit(&screenRegion);

	/* Initially fill the dmgbuffer with screen content. */
	dst = (char*)ogon_dmgbuf_get_data(g_rdsDamage);
	src = (char*)g_rdpScreen.pfbMemory;
	if (!dst || !src)
		return;
	size = ogon_dmgbuf_get_fbsize(g_rdsDamage);
	memcpy(dst, src, size);
}

static BOOL rds_client_framebuffer_sync_request(ogon_backend_service *backend, UINT32 bufferId)
{
	/* ignore the old buffer id that might still be in the queue */
	if (!g_rdsDamage && bufferId == g_rdsDamageLastBufferId)
	{
		LLOGLN(10, ("rds_client_framebuffer_sync_request: ignoring old bufferId %d", bufferId));
		return TRUE;
	}

	if (!g_rdsDamage || bufferId != ogon_dmgbuf_get_id(g_rdsDamage))
	{
		if (g_rdsDamage) {
			rdp_detach_rds_framebuffer();
		}
		rdp_attach_rds_framebuffer(bufferId);
	}

	if (g_rdsDamage) {
		g_rdsDamageSyncRequested = TRUE;
	}

	rdp_handle_damage_region(1);

	return TRUE;
}

static BOOL rds_client_capabilities(ogon_backend_service *backend, ogon_msg_capabilities* capabilities)
{
	int width;
	int height;
	const char *lang = rds_get_keyboard_layout(capabilities->keyboardLayout);
	char cmd[32];

	width = capabilities->desktopWidth;
	height = capabilities->desktopHeight;


	/* First approach for setting the proper keyboard layout*/
	if (NULL != lang)
	{
		snprintf(cmd, 32, "setxkbmap %s", lang);
	}
	else
	{
		snprintf(cmd, 32, "setxkbmap us");
	}

	system_background(cmd);

	if ((g_pScreen->width != width) || (g_pScreen->height != height))
	{
		RRTransformPtr transform = NULL;
		rdpRandRInfoPtr randr = rdpGetRandRFromScreen(g_pScreen);
		RRCrtcPtr  crtc = randr->crtc;
		BoxRec screenBox;
		rrScrPrivPtr pScrPriv = rrGetScrPriv(g_pScreen);

		/* In case ogon sets a higher resolution adapt the max values */
		if (g_width_max < width)
			g_width_max = pScrPriv->maxWidth = width;
		if ( g_height_max < height)
			g_height_max = pScrPriv->maxHeight = height;

		transform = RRCrtcGetTransform(crtc);

		rdpModeSelect(g_pScreen, width, height);
		RROutputSetPhysicalSize(randr->output, randr->mmWidth, randr->mmHeight);

		RRCrtcNotify(crtc, randr->mode, crtc->x, crtc->y, randr->rotations, transform, randr->numOutputs, randr->outputs);

		RRScreenSizeSet(g_pScreen, randr->width, randr->height, randr->mmWidth, randr->mmHeight);

		screenBox.x1 = 0;
		screenBox.y1 = 0;
		screenBox.x2 = width;
		screenBox.y2 = height;
		RegionUninit(&g_rdpScreen.screenRec);
		RegionInit(&g_rdpScreen.screenRec, &screenBox, 1);
	}

	g_active = 1;
	g_rdpScreen.sendFullDamage = TRUE;

	rdp_send_framebuffer_info();

	rdpSpriteSetCursor(NULL, NULL, g_rdpScreen.pCurs, 0, 0);

	return TRUE;
}

static BOOL rds_client_synchronize_keyboard_event(ogon_backend_service* backend, DWORD flags)
{
	KbdAddSyncEvent(flags);
	return TRUE;
}

static BOOL rds_client_scancode_keyboard_event(ogon_backend_service* backend, DWORD flags, DWORD code, DWORD keyboardType)
{
	KbdAddScancodeEvent(flags, code, keyboardType);
	return TRUE;
}


static BOOL rds_client_unicode_keyboard_event(ogon_backend_service* backend, DWORD flags, DWORD code)
{
	KbdAddUnicodeEvent(flags, code);
	return TRUE;
}

static BOOL rds_client_mouse_event(ogon_backend_service* backend, DWORD flags, DWORD x, DWORD y)
{
	if (x > g_rdpScreen.width - 2)
		x = g_rdpScreen.width - 2;

	if (y > g_rdpScreen.height - 2)
		y = g_rdpScreen.height - 2;

	if ((flags & PTR_FLAGS_MOVE) || (flags & PTR_FLAGS_DOWN))
	{
		PtrAddMotionEvent(x, y);
	}

	if (flags & PTR_FLAGS_WHEEL)
	{
		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
		{
			g_button_mask = g_button_mask | 16;
			PtrAddButtonEvent(g_button_mask);

			g_button_mask = g_button_mask & (~16);
			PtrAddButtonEvent(g_button_mask);
		}
		else
		{
			g_button_mask = g_button_mask | 8;
			PtrAddButtonEvent(g_button_mask);

			g_button_mask = g_button_mask & (~8);
			PtrAddButtonEvent(g_button_mask);
		}
	}
	else if (flags & PTR_FLAGS_HWHEEL)
	{
		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
		{
			g_button_mask = g_button_mask | 32;
			PtrAddButtonEvent(g_button_mask);

			g_button_mask = g_button_mask & (~32);
			PtrAddButtonEvent(g_button_mask);
		}
		else
		{
			g_button_mask = g_button_mask | 64;
			PtrAddButtonEvent(g_button_mask);

			g_button_mask = g_button_mask & (~64);
			PtrAddButtonEvent(g_button_mask);
		}
	}
	else if (flags & PTR_FLAGS_BUTTON1)
	{
		if (flags & PTR_FLAGS_DOWN)
		{
			g_button_mask = g_button_mask | 1;
			PtrAddButtonEvent(g_button_mask);
		}
		else
		{
			g_button_mask = g_button_mask & (~1);
			PtrAddButtonEvent(g_button_mask);
		}
	}
	else if (flags & PTR_FLAGS_BUTTON2)
	{
		if (flags & PTR_FLAGS_DOWN)
		{
			g_button_mask = g_button_mask | 4;
			PtrAddButtonEvent(g_button_mask);
		}
		else
		{
			g_button_mask = g_button_mask & (~4);
			PtrAddButtonEvent(g_button_mask);
		}
	}
	else if (flags & PTR_FLAGS_BUTTON3)
	{
		if (flags & PTR_FLAGS_DOWN)
		{
			g_button_mask = g_button_mask | 2;
			PtrAddButtonEvent(g_button_mask);
		}
		else
		{
			g_button_mask = g_button_mask & (~2);
			PtrAddButtonEvent(g_button_mask);
		}
	}

	return TRUE;
}

static BOOL rds_client_extended_mouse_event(ogon_backend_service* backend, DWORD flags, DWORD x, DWORD y)
{
	if (x > g_rdpScreen.width - 2)
		x = g_rdpScreen.width - 2;

	if (y > g_rdpScreen.height - 2)
		y = g_rdpScreen.height - 2;

	if ((flags & PTR_FLAGS_MOVE) || (flags & PTR_XFLAGS_DOWN))
	{
		PtrAddMotionEvent(x, y);
	}

	if (flags & PTR_XFLAGS_BUTTON1)
	{
		if (flags & PTR_XFLAGS_DOWN)
		{
			g_button_mask = g_button_mask | (1<<7);
			PtrAddButtonEvent(g_button_mask);
		}
		else
		{
			g_button_mask = g_button_mask & (~(1<<7));
			PtrAddButtonEvent(g_button_mask);
		}
	}
	else if (flags & PTR_XFLAGS_BUTTON2)
	{
		if (flags & PTR_XFLAGS_DOWN)
		{
			g_button_mask = g_button_mask | (1<<8);
			PtrAddButtonEvent(g_button_mask);
		}
		else
		{
			g_button_mask = g_button_mask & (~(1<<8));
			PtrAddButtonEvent(g_button_mask);
		}
	}

	return TRUE;
}

static BOOL rds_client_immediate_sync_request(void *backend, INT32 bufferId)
{
	/* ignore the old buffer id that might still be in the queue */
	if (!g_rdsDamage && bufferId == g_rdsDamageLastBufferId)
	{
		LLOGLN(10, ("rds_client_framebuffer_sync_request: ignoring old bufferId %d", bufferId));
		return TRUE;
	}

	if (!g_rdsDamage || bufferId != ogon_dmgbuf_get_id(g_rdsDamage))
	{
		if (g_rdsDamage) {
			rdp_detach_rds_framebuffer();
		}
		rdp_attach_rds_framebuffer(bufferId);
	}

	ogon_dmgbuf_set_num_rects(g_rdsDamage, 0);
	rdp_send_sync_framebuffer_reply();
	return TRUE;
}

static void write_pipe_rds_client_message(int fd, BYTE* value, int size)
{
	int written;
	int totalWritten = 0;

	while (totalWritten != size) {
		written = write(fd, value + totalWritten, size - totalWritten);
		if (written < 0) {
			fprintf(stderr, "%s: socket(%d) for message display closed unexpected\n", __FUNCTION__, fd);
			close(fd);
			exit(1);
		}
		totalWritten += written;
	}
}

#define BUFFER_SIZE_MESSAGE 1024 * 4

static int display_message(ogon_msg_message* msg) {
	int retVal = 0;
	char buffer[BUFFER_SIZE_MESSAGE];
	char executableName[BUFFER_SIZE_MESSAGE];
	struct stat sb;

	//check if ogon-message is present in install dir.
	snprintf(executableName, BUFFER_SIZE_MESSAGE, "%s/ogon-message", OGON_BIN_PATH);

	if (!(stat(executableName, &sb) == 0 && sb.st_mode & S_IXUSR)) {
		// try to use the system search path
		snprintf(executableName, BUFFER_SIZE_MESSAGE, "ogon-message");
	}

	snprintf(buffer, BUFFER_SIZE_MESSAGE, "%s %u %u %u %u \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
			executableName,
			msg->message_id, msg->message_type, msg->style, msg->timeout,
			msg->parameter_num > 0 ? msg->parameter1 : "",
			msg->parameter_num > 1 ? msg->parameter2 : "",
			msg->parameter_num > 2 ? msg->parameter3 : "",
			msg->parameter_num > 3 ? msg->parameter4 : "",
			msg->parameter_num > 4 ? msg->parameter5 : ""
			);

	retVal = system(buffer);
	if (!WIFEXITED(retVal))
	{
		return -1;
	}
	retVal = WEXITSTATUS(retVal);
	if (retVal == 255) {
		retVal = -1;
	}
	return retVal;
}

static BOOL rds_client_message(ogon_backend_service* backend, ogon_msg_message* msg)
{
	pid_t pid;
	int status;
	int retVal = 0;
	int fd[2];
	int* save;

	status = pipe(fd);
	if (status != 0)
	{
		fprintf(stderr, "%s: pipe creation failed\n", __FUNCTION__);
		return FALSE;
	}
	pid = fork();
	if (0 == pid)
	{
		/* child */
		if (0 == fork())
		{
			/* Child process closes up input side of pipe */
			close(fd[0]);

			retVal = display_message(msg);

			write_pipe_rds_client_message(fd[1], (BYTE *)&retVal, sizeof(retVal));
			write_pipe_rds_client_message(fd[1], (BYTE *)&msg->message_id, sizeof(msg->message_id));

			close(fd[1]);
			exit(0);
		}
		else
		{
			exit(0);
		}
	}
	else
	{
		/* parent */
		waitpid(pid, &status, 0);
		/* Parent process closes up output side of pipe */
		close(fd[1]);
		save = malloc(sizeof(int));
		*save = fd[0];
		ArrayList_Add(g_Messages,save);
		SetNotifyFd(fd[0], check_message_fds, X_NOTIFY_READ, NULL);
	}

	return TRUE;
}

static int rds_service_accept(ogon_backend_service* service)
{
	HANDLE clientPipe;

	clientPipe = ogon_service_accept(service);
	if (!clientPipe || clientPipe == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "%s: unable to get the incoming connection\n", __FUNCTION__);
		return 1;
	}

	// RemoveEnabledDevice(g_serverfd);
	RemoveNotifyFd(g_serverfd);

	g_clientfd = ogon_service_client_fd(service);
	g_connected = 1;
	rdp_send_version();

	//AddEnabledDevice(g_clientfd);
	SetNotifyFd(g_clientfd, client_incoming, X_NOTIFY_READ, NULL);

	fprintf(stderr, "RdsServiceAccept()\n");

	return 0;
}

extern int g_multitouchFd;
int rds_service_disconnect(ogon_backend_service* service)
{
	KbdSessionDisconnect();

	//RemoveEnabledDevice(g_clientfd);
	RemoveNotifyFd(g_clientfd);
	if (g_multitouchFd >= 0) {
		//RemoveEnabledDevice(g_multitouchFd);
		RemoveNotifyFd(g_multitouchFd);
		g_multitouchFd = -1;
		multitouchClose();
	}

	ogon_service_kill_client(service);

	fprintf(stderr, "RdsServiceDisconnect()\n");

	g_active = 0;
	g_connected = 0;
	g_clientfd = -1;

	rdp_detach_rds_framebuffer();

	SetNotifyFd(g_serverfd, server_incoming, X_NOTIFY_READ, NULL);
	// AddEnabledDevice(g_serverfd);

	return 0;
}

ogon_client_interface g_callbacks = {
	(pfn_ogon_client_capabilities)rds_client_capabilities,
	(pfn_ogon_client_synchronize_keyboard_event)rds_client_synchronize_keyboard_event,
	(pfn_ogon_client_scancode_keyboard_event)rds_client_scancode_keyboard_event,
	(pfn_ogon_client_unicode_keyboard_event)rds_client_unicode_keyboard_event,
	(pfn_ogon_client_mouse_event)rds_client_mouse_event,
	(pfn_ogon_client_extended_mouse_event)rds_client_extended_mouse_event,
	(pfn_ogon_client_framebuffer_sync_request)rds_client_framebuffer_sync_request,
	(pfn_ogon_client_sbp)NULL,
	(pfn_ogon_client_immediate_sync_request)rds_client_immediate_sync_request,
	NULL,
	NULL,
	(pfn_ogon_client_message)rds_client_message,
};

int rdp_init(void)
{
	int DisplayId;
	char buf[255];

	DisplayId = atoi(display);

	LLOGLN(0, ("rdp_init: display: %d", DisplayId));

	if (DisplayId < 1)
	{
		return 0;
	}

	snprintf(buf, 255, ":%s", display);
	setenv("DISPLAY", buf, 1);

	g_rdsDamage = NULL;
	g_rdsDamageSyncRequested = FALSE;

	if (!g_service)
	{
		g_service = ogon_service_new(DisplayId, "X11");
		if (!g_service) {
			fprintf(stderr, "%s: could not create the service endpoint on display %d\n", __FUNCTION__, DisplayId);
			return 0;
		}

		g_serverPipe = ogon_service_bind_endpoint(g_service);
		if (!g_serverPipe || g_serverPipe == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "%s: unable to bind endpoint for %d\n", __FUNCTION__, DisplayId);
			ogon_service_free(g_service);
			g_service = 0;
			return 0;
		}

		ogon_service_set_callbacks(g_service, &g_callbacks);

		g_serverfd = ogon_service_server_fd(g_service);
		SetNotifyFd(g_serverfd, server_incoming, X_NOTIFY_READ, NULL);
		//AddEnabledDevice( g_serverfd );
	}

	return 1;
}

static int read_pipe_rds_client_message(int fd, BYTE* buffer, int size)
{
	int currentRead;
	int totalBytes = 0;
	while (totalBytes != size) {
		currentRead = read(fd, buffer + totalBytes, size - totalBytes);
		if (currentRead < 1) {
			fprintf(stderr, "%s: socket(%d) for message display closed unexpected\n", __FUNCTION__, fd);
			close(fd);
			return 0;
		}
		totalBytes += currentRead;
	}
	return 1;
}

void check_message_fds(int fd, int ready, void* data)
{
	int index = 0;
	int messagefd;
	int* save;
	int result;
	UINT32 message_id;
	ogon_msg_message_reply rep;

	if (!ready)
		return;

	save = ArrayList_GetItem(g_Messages, index++);
	while (save)
	{
		messagefd = *save;
		if (messagefd == fd) {
			RemoveNotifyFd(messagefd);
			ArrayList_Remove(g_Messages,save);
			free(save);
			if (!g_connected) {
				close(messagefd);
				return;
			}

			if (!read_pipe_rds_client_message(messagefd, (BYTE *)&result, sizeof(result)))
				return;
			if (!read_pipe_rds_client_message(messagefd, (BYTE *)&message_id, sizeof(message_id)))
				return;

			close(messagefd);

			fprintf(stderr, "%s: sending message with messageid (%d) and result(%d)\n", __FUNCTION__,message_id,result);

			rep.message_id = message_id;
			rep.result = (UINT32)result;
			rdp_send_message(OGON_SERVER_MESSAGE_REPLY, (ogon_message*) &rep);
			return;
		}
		save = ArrayList_GetItem(g_Messages, index++);
	}
	return;
}

void client_incoming(int fd, int ready, void *data)
{
	ogon_backend_service* service = g_service;
	ogon_incoming_bytes_result res;

	res = ogon_service_incoming_bytes(service, service);

	switch (res)
	{
	case OGON_INCOMING_BYTES_WANT_MORE_DATA:
	case OGON_INCOMING_BYTES_OK:
		return;

	case OGON_INCOMING_BYTES_BROKEN_PIPE:
	case OGON_INCOMING_BYTES_INVALID_MESSAGE:
	default:
		rds_service_disconnect(service);
		break;
	}
}

void multitouch_incoming(int fd, int ready, void *data)
{
	if (multitouchHandle() < 0) {
		// RemoveEnabledDevice(g_multitouchFd);
		RemoveNotifyFd(g_multitouchFd);
		g_multitouchFd = -1;
		multitouchClose();
	}
}

void server_incoming(int fd, int ready, void *data)
{
	if (rds_service_accept(g_service) == 0) {
		g_multitouchFd = multitouchTryToStart();
		if (g_multitouchFd >= 0)
			SetNotifyFd(g_multitouchFd, multitouch_incoming, X_NOTIFY_READ, NULL);
			//AddEnabledDevice(g_multitouchFd);
	}
}

void rdp_check(void *blockData, int result, void *bitset)
{
	ogon_backend_service* service = g_service;
	ogon_incoming_bytes_result res;
	fd_set *fdset = (fd_set *)bitset;

	//check_message_fds(fdset);

	if (g_connected)
	{
		if (FD_ISSET(g_clientfd, fdset)) {
			res = ogon_service_incoming_bytes(service, service);

			switch (res)
			{
			case OGON_INCOMING_BYTES_WANT_MORE_DATA:
			case OGON_INCOMING_BYTES_OK:
				return;

			case OGON_INCOMING_BYTES_BROKEN_PIPE:
			case OGON_INCOMING_BYTES_INVALID_MESSAGE:
			default:
				rds_service_disconnect(service);
				break;
			}
		}

		if (g_multitouchFd >= 0 && FD_ISSET(g_multitouchFd, fdset)) {
			if (multitouchHandle() < 0) {
				// RemoveEnabledDevice(g_multitouchFd);
				RemoveNotifyFd(g_multitouchFd);
				g_multitouchFd = -1;
				multitouchClose();
			}
		}
	}
	else
	{
		if (!FD_ISSET(g_serverfd, fdset))
			return;

		if (rds_service_accept(service) == 0) {
			g_multitouchFd = multitouchTryToStart();
			if (g_multitouchFd >= 0)
				SetNotifyFd(g_multitouchFd, multitouch_incoming, X_NOTIFY_READ, NULL);
				//AddEnabledDevice(g_multitouchFd);
		}
	}
}
