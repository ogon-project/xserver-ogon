/**
 * ogon Remote Desktop Services
 * X11 backend
 *
 * Copyright (C) 2014-2018 Thincast Technologies GmbH
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
 * Norbert Federa <norbert.federa@thincast.com>
 */

/** the only reason for this file is that freerdp/server/rdpei.h contains definitions
 * conflicting with Xorg ones.
 */
#include <freerdp/channels/rdpei.h>
#include <freerdp/server/rdpei.h>


RdpeiServerContext *rdpeiServerContext = 0;

UINT multitouchOnClientReady(RdpeiServerContext *context);
UINT multitouchOnTouchEvent(RdpeiServerContext *context, RDPINPUT_TOUCH_EVENT *touchEvent);
UINT multitouchOnTouchReleased(RdpeiServerContext *context, BYTE contactId);
void multitouchClose(void);


/* just for warnings */
int multitouchTryToStart(void);
int multitouchHandle(void);


int multitouchTryToStart(void)
{
	HANDLE handle;

	rdpeiServerContext = rdpei_server_context_new(WTS_CURRENT_SERVER_HANDLE);
	if (!rdpeiServerContext)
		return -1;

	if (rdpei_server_init(rdpeiServerContext))
		return -1;

	rdpeiServerContext->onClientReady = multitouchOnClientReady;
	rdpeiServerContext->onTouchEvent = multitouchOnTouchEvent;
	rdpeiServerContext->onTouchReleased = multitouchOnTouchReleased;

	if (rdpei_server_send_sc_ready(rdpeiServerContext, RDPINPUT_PROTOCOL_V101))
		return -1;

	handle = rdpei_server_get_event_handle(rdpeiServerContext);
	if (!handle || handle == INVALID_HANDLE_VALUE)
		goto out_error;

	return GetEventFileDescriptor(handle);

out_error:
	/* TODO: we should close everything here */
	return -1;
}

int multitouchHandle(void) {
	return rdpei_server_handle_messages(rdpeiServerContext) ? -1 : 0;
}

void multitouchClose(void) {
	if (rdpeiServerContext) {
		rdpei_server_context_free(rdpeiServerContext);
	}

	rdpeiServerContext = 0;
}
