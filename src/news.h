/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __NEWS_H__
#define __NEWS_H__

#include <glib.h>

#include "folder.h"
#include "session.h"
#include "nntp.h"

typedef struct _NNTPSession	NNTPSession;

#define NNTP_SESSION(obj)	((NNTPSession *)obj)

struct _NNTPSession
{
	Session session;

	NNTPSockInfo *nntp_sock;
	gchar *group;
};

struct NNTPGroupInfo {
	gint first;
	gint last;
	gchar * name;
	gchar type;
};


void news_session_destroy		(NNTPSession	*session);
NNTPSession *news_session_get		(Folder		*folder);

GSList *news_get_article_list		(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 use_cache);
gchar *news_fetch_msg			(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);

void news_scan_group			(Folder		*folder,
					 FolderItem	*item);

void news_group_list_free               (GSList * list);
GSList *news_get_group_list		(Folder		*folder);
void news_remove_group_list		(Folder		*folder);

gint news_post				(Folder		*folder,
					 const gchar	*file);

gint news_group_info_compare(struct NNTPGroupInfo * info1,
			     struct NNTPGroupInfo * info2);

#endif /* __NEWS_H__ */
