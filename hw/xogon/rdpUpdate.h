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
 * Jay Sorg
 * Marc-André Moreau <marcandre.moreau@gmail.com>
 * Martin Haimberger <martin.haimberger@thincast.com>
 * Norbert Federa <norbert.federa@thincast.com>
 */

#ifndef OGON_X11RDP_UPDATE_H
#define OGON_X11RDP_UPDATE_H

int rdp_send_message(UINT16 type, ogon_message *msg);
int rdp_send_framebuffer_info(void);
int rdp_send_version(void);
void rdp_detach_rds_framebuffer(void);
void rdp_attach_rds_framebuffer(int bufferId);
int rdp_init(void);
void rdp_check(void *blockData, int result, void *bitset);
int rdp_handle_damage_region(int callerId);
int init_messages_list(void);

void client_incoming(int fd, int ready, void *data);
void multitouch_incoming(int fd, int ready, void *data);
void server_incoming(int fd, int ready, void *data);
void check_message_fds(int fd, int ready, void *data);

#endif /* OGON_X11RDP_UPDATE_H */
