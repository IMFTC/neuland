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

#ifndef __NEULAND_APPLICATION_H__
#define __NEULAND_APPLICATION_H__

#include <glib-object.h>

#define NEULAND_TYPE_APPLICATION            (neuland_application_get_type ())
#define NEULAND_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_APPLICATION, NeulandApplication))
#define NEULAND_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_APPLICATION, NeulandApplicationClass))
#define NEULAND_IS_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_APPLICATION))
#define NEULAND_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_APPLICATION))
#define NEULAND_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_APPLICATION, NeulandApplicationClass))

typedef struct _NeulandApplication NeulandApplication;
typedef struct _NeulandApplicationClass NeulandApplicationClass;
typedef struct _NeulandApplicationPrivate NeulandApplicationPrivate;

struct _NeulandApplication
{
  GtkApplication parent_instance;
  char *public_string;

  NeulandApplicationPrivate *priv;
};

struct _NeulandApplicationClass
{
  GtkApplicationClass parent_class;
};


#endif /* __NEULAND_APPLICATION_H__ */
