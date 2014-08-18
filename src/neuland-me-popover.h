/* -*- mode: c; indent-tabs-mode: nil; -*- */
/*
 * This file is part of Neuland.
 *
 * Copyright © 2014 Volker Sobek <reklov@live.com>
 *
 * Neuland is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Neuland is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Neuland.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NEULAND_ME_POPOVER_H__
#define __NEULAND_ME_POPOVER_H__

#include <gtk/gtk.h>

#include "neuland-tox.h"

#define NEULAND_TYPE_ME_POPOVER            (neuland_me_popover_get_type ())
#define NEULAND_ME_POPOVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_ME_POPOVER, NeulandMePopover))
#define NEULAND_ME_POPOVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_ME_POPOVER, NeulandMePopoverClass))
#define NEULAND_IS_ME_POPOVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_ME_POPOVER))
#define NEULAND_IS_ME_POPOVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_ME_POPOVER))
#define NEULAND_ME_POPOVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_ME_POPOVER, NeulandMePopoverClass))

typedef struct _NeulandMePopover        NeulandMePopover;
typedef struct _NeulandMePopoverPrivate NeulandMePopoverPrivate;
typedef struct _NeulandMePopoverClass   NeulandMePopoverClass;

struct _NeulandMePopover
{
  GtkPopover parent;

  NeulandMePopoverPrivate *priv;
};

struct _NeulandMePopoverClass
{
  GtkPopoverClass parent;
};

GType neuland_me_popover_get_type (void);

GtkWidget *
neuland_me_popover_new (NeulandTox *tox);

#endif /* __NEULAND_ME_POPOVER_H__ */
