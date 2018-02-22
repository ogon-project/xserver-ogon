/**
 * ogon Remote Desktop Services
 * X11 backend
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
 * Marc-André Moreau <marcandre.moreau@gmail.com>
 * Norbert Federa <norbert.federa@thincast.com>
 */

#ifndef OGON_X11RDP_INPUT_H
#define OGON_X11RDP_INPUT_H

#include <freerdp/channels/rdpei.h>


int rdpKeybdProc(DeviceIntPtr pDevice, int onoff);
int rdpMouseProc(DeviceIntPtr pDevice, int onoff);
Bool rdpCursorOffScreen(ScreenPtr* ppScreen, int* x, int* y);
void rdpCrossScreen(ScreenPtr pScreen, Bool entering);
void rdpPointerWarpCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y);
Bool rdpSpriteRealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs);
Bool rdpSpriteUnrealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs);
void rdpSpriteSetCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs, int x, int y);
void rdpSpriteMoveCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y);
Bool rdpSpriteDeviceCursorInitialize(DeviceIntPtr pDev, ScreenPtr pScr);
void rdpSpriteDeviceCursorCleanup(DeviceIntPtr pDev, ScreenPtr pScr);
void PtrAddMotionEvent(int x, int y);
void PtrAddButtonEvent(int buttonMask);
void KbdAddScancodeEvent(DWORD flags, DWORD scancode, DWORD keyboardType);
void KbdAddVirtualKeyCodeEvent(DWORD flags, DWORD vkcode);
void KbdAddUnicodeEvent(DWORD flags, DWORD code);
void KbdAddSyncEvent(DWORD flags);
void KbdResetKeyStatesUp(void);
void KbdSessionDisconnect(void);

int multitouchInit(void);
int multitouchTryToStart(void);
int multitouchHandle(void);

typedef struct _rdpei_server_context RdpeiServerContext;

UINT multitouchOnClientReady(RdpeiServerContext *context);
UINT multitouchOnTouchEvent(RdpeiServerContext *context, RDPINPUT_TOUCH_EVENT *touchEvent);
UINT multitouchOnTouchReleased(RdpeiServerContext *context, BYTE contactId);
void multitouchClose(void);


#endif /* OGON_X11RDP_INPUT_H */
