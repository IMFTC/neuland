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

#include "neuland-utils.h"
#include <tox/tox.h>

typedef struct {
  guchar* hex_string;
  gboolean should_pass;
} HexBinTest;

HexBinTest tests[] = {
  { "951C88B7E75C867418ACDB5D273821372BB5BD652740BCDF623A4FA293E75D2F", TRUE },
  { "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67", TRUE },
  { "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038BXD822E8460FAB67", FALSE },
  { "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B D822E8460FAB67", FALSE }
};

gboolean
test_hex_to_bin_to_hex (HexBinTest data)
{
  gboolean passed = FALSE;
  g_print ("Testing: %s\n", data.hex_string);

  guint8 bin_out[TOX_CLIENT_ID_SIZE] = {0, };
  gchar hex_out[TOX_CLIENT_ID_SIZE * 2 + 1] = {0, };

  neuland_hex_string_to_bin (data.hex_string, bin_out, TOX_CLIENT_ID_SIZE);
  neuland_bin_to_hex_string (bin_out, hex_out, TOX_CLIENT_ID_SIZE);
  g_print ("Result : %s (should %s)\n", hex_out, data.should_pass ? "match" : "NOT match");

  gboolean equal = strcmp (data.hex_string, hex_out) == 0;
  if ((equal && data.should_pass) || (!equal && !data.should_pass))
    {
      g_print ("Test PASSED\n");
      passed = TRUE;
    }
  else
    g_print ("Test FAILED\n");

  g_print ("\n");
  return passed;
}

main (int argc, gchar **argv)
{
  int number_tests = sizeof (tests) / sizeof (tests[0]);
  int failed_tests = 0;
  int passed_tests = 0;
  int i;
  for (i = 0; i < number_tests; i++)
    {
      if (test_hex_to_bin_to_hex (tests[i]))
        passed_tests++;
      else
        failed_tests++;
    }

  g_print ("Number of tests: %i\n", number_tests);
  g_print ("Passed tests   : %i\n", passed_tests);
  g_print ("Failed tests   : %i\n", failed_tests);

  return failed_tests;
}
