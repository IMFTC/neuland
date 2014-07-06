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

#include "neuland-enums.h"
#include "neuland-contact.h"

GType
neuland_contact_status_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY(etype == 0)) {
    static const GEnumValue values[] =
      {
        { NEULAND_CONTACT_STATUS_NONE, "TOX_USERSTATUS_NONE", "none" },
        { NEULAND_CONTACT_STATUS_AWAY, "TOX_USERSTATUS_AWAY", "away" },
        { NEULAND_CONTACT_STATUS_BUSY, "TOX_USERSTATUS_BUSY", "busy" },
        { 0, NULL, NULL }
      };
    etype = g_enum_register_static (g_intern_static_string ("NeulandContactStatus"), values);
  }
  return etype;
}
