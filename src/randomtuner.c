/*
  Example LV2 plugin using note expressions
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

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/time/time.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "expr/expr.h"

#define RANDOM_TUNER_URI "urn:grejppi:randomtuner"

enum {
	RANDOM_TUNER_INPUT,
	RANDOM_TUNER_OUTPUT,
	RANDOM_TUNER_SPREAD,
};

typedef struct {
	LV2_URID_Map* map;

	LV2_Atom_Forge       forge;
	LV2_Atom_Forge_Frame frame;

	float bend_values[2048];
	float last_spread;

	struct {
		const LV2_Atom_Sequence* input;
		LV2_Atom_Sequence*       output;
		const float*             spread;
	} ports;

	struct {
		LV2_URID expr_Expression;
		LV2_URID expr_pitchBend;
		LV2_URID midi_MidiEvent;
		LV2_URID midi_channel;
		LV2_URID midi_noteNumber;
		LV2_URID time_frame;
	} uris;
} RandomTuner;

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
	RandomTuner* self = (RandomTuner*)calloc(1, sizeof(RandomTuner));
	if (!self) {
		return NULL;
	}

	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID__map)) {
			self->map = (LV2_URID_Map*)features[i]->data;
			break;
		}
	}
	if (!self->map) {
		return NULL;
	}

	LV2_URID_Map* map = self->map;
	lv2_atom_forge_init(&self->forge, map);
	self->uris.expr_Expression = map->map(map->handle, LV2_EXPR__Expression);
	self->uris.expr_pitchBend  = map->map(map->handle, LV2_EXPR__pitchBend);
	self->uris.midi_MidiEvent  = map->map(map->handle, LV2_MIDI__MidiEvent);
	self->uris.midi_channel    = map->map(map->handle, LV2_MIDI__channel);
	self->uris.midi_noteNumber = map->map(map->handle, LV2_MIDI__noteNumber);
	self->uris.time_frame      = map->map(map->handle, LV2_TIME__frame);

	return (LV2_Handle)self;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	RandomTuner* self = (RandomTuner*)instance;
	switch(port) {
	case RANDOM_TUNER_INPUT:
		self->ports.input = (const LV2_Atom_Sequence*)data;
		break;
	case RANDOM_TUNER_OUTPUT:
		self->ports.output = (LV2_Atom_Sequence*)data;
		break;
	case RANDOM_TUNER_SPREAD:
		self->ports.spread = (const float*)data;
		break;
	default:
		break;
	}
}

static float
cheap_rand(void)
{
	static int seed = 0;
	seed = (0x41c64e6d * seed) + 0x6073;
	return (float)seed / (float)0xffffffff;
}

static void
run(LV2_Handle instance,
    uint32_t   sample_count)
{
	RandomTuner* self = (RandomTuner*)instance;

	const uint32_t capacity = self->ports.output->atom.size;
	const float spread = (*self->ports.spread) * (*self->ports.spread);

	lv2_atom_sequence_clear(self->ports.output);
	memset(LV2_ATOM_CONTENTS(LV2_Atom_Sequence, self->ports.output), 0, capacity);

	lv2_atom_forge_set_buffer(&self->forge, (uint8_t*)self->ports.output, capacity);
	lv2_atom_forge_sequence_head(&self->forge, &self->frame, self->uris.time_frame);

	if (self->last_spread != *self->ports.spread) {
		for (int i = 0; i < 2048; ++i) {
			if (self->bend_values[i] == 0) {
				continue;
			}

			LV2_Atom_Forge_Frame frame;
			lv2_atom_forge_frame_time(&self->forge, 0);
			lv2_atom_forge_object(&self->forge, &frame, 0,
			                      self->uris.expr_Expression);

			lv2_atom_forge_key(&self->forge, self->uris.midi_channel);
			lv2_atom_forge_int(&self->forge, i >> 8);

			lv2_atom_forge_key(&self->forge, self->uris.midi_noteNumber);
			lv2_atom_forge_int(&self->forge, i & 0x7f);

			lv2_atom_forge_key(&self->forge, self->uris.expr_pitchBend);
			lv2_atom_forge_float(&self->forge, self->bend_values[i] * spread);

			lv2_atom_forge_pop(&self->forge, &frame);
		}
		self->last_spread = *self->ports.spread;
	}

	LV2_ATOM_SEQUENCE_FOREACH(self->ports.input, ev) {
		if (ev->body.type == self->uris.midi_MidiEvent) {
			const uint8_t* const msg = (const uint8_t*)(ev + 1);

			uint8_t channel = msg[0] & 0xf;
			uint32_t index = (channel << 8) | msg[1];

			// Copy the MIDI event to the output first
			lv2_atom_forge_frame_time(&self->forge, ev->time.frames);
			lv2_atom_forge_atom(&self->forge, 3, self->uris.midi_MidiEvent);
			lv2_atom_forge_write(&self->forge, msg, 3);

			if (lv2_midi_message_type (msg) == LV2_MIDI_MSG_NOTE_OFF) {
				self->bend_values[index] = 0.0;
				continue;
			}

			LV2_Atom_Forge_Frame frame;
			lv2_atom_forge_frame_time(&self->forge, ev->time.frames);
			lv2_atom_forge_object(&self->forge, &frame, 0,
			                      self->uris.expr_Expression);

			lv2_atom_forge_key(&self->forge, self->uris.midi_channel);
			lv2_atom_forge_int(&self->forge, channel);

			lv2_atom_forge_key(&self->forge, self->uris.midi_noteNumber);
			lv2_atom_forge_int(&self->forge, msg[1]);

			self->bend_values[index] = cheap_rand();

			lv2_atom_forge_key(&self->forge, self->uris.expr_pitchBend);
			lv2_atom_forge_float(&self->forge, self->bend_values[index] * spread);

			lv2_atom_forge_pop(&self->forge, &frame);
		}
	}

	lv2_atom_forge_pop(&self->forge, &self->frame);
}

static void
cleanup(LV2_Handle instance)
{
	free(instance);
}

static const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	RANDOM_TUNER_URI,
	instantiate,
	connect_port,
	NULL,
	run,
	NULL,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
