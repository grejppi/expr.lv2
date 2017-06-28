/*
  Copyright 2015-2017 Henna Haahti <grejppi@gmail.com>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/**
   @file expr.h C header for the LV2 Expression extension
   <urn:unfinished:lv2:expression:draft>.

   As the extension is purely data, this header merely defines URIs
   for convenience.
*/

#ifndef LV2_EXPR_H
#define LV2_EXPR_H

#define LV2_EXPR_URI    "urn:unfinished:lv2:expression:draft"
#define LV2_EXPR_PREFIX LV2_EXPR_URI ":"

#define LV2_EXPR__Expression    LV2_EXPR_PREFIX "Expression"
#define LV2_EXPR__pitchBend     LV2_EXPR_PREFIX "pitchBend"
#define LV2_EXPR__stereoPanning LV2_EXPR_PREFIX "stereoPanning"
#define LV2_EXPR__supports      LV2_EXPR_PREFIX "supports"

#endif  /* LV2_EXPR_H */
