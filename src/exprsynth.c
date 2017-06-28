/*
  Example LV2 synth using note expressions
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

#define EXPR_SYNTH_URI "urn:grejppi:exprsynth"
#define NVOICES 8

#define KEY2HZ(k) (440.0 * (pow(2.0, (k - 69) / 12.0)))

enum {
	EXPR_SYNTH_INPUT,
	EXPR_SYNTH_OUTPUT,
};

typedef struct {
	bool gate;
	uint8_t channel;
	uint8_t key;
	int32_t counter;
	float bend;
	float velocity;
} Voice;

typedef struct {
	double rate;

	Voice voices[NVOICES];
	int next_voice;

	LV2_URID_Map* map;

	struct {
		const LV2_Atom_Sequence* input;
		float*                   output;
	} ports;

	struct {
		LV2_URID atom_Object;
		LV2_URID expr_Expression;
		LV2_URID expr_pitchBend;
		LV2_URID midi_MidiEvent;
		LV2_URID midi_channel;
		LV2_URID midi_noteNumber;
	} uris;
} ExprSynth;

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
	ExprSynth* self = (ExprSynth*)calloc(1, sizeof(ExprSynth));
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

	self->rate = rate;

	LV2_URID_Map* map = self->map;
	self->uris.atom_Object     = map->map(map->handle, LV2_ATOM__Object);
	self->uris.expr_Expression = map->map(map->handle, LV2_EXPR__Expression);
	self->uris.expr_pitchBend  = map->map(map->handle, LV2_EXPR__pitchBend);
	self->uris.midi_MidiEvent  = map->map(map->handle, LV2_MIDI__MidiEvent);
	self->uris.midi_channel    = map->map(map->handle, LV2_MIDI__channel);
	self->uris.midi_noteNumber = map->map(map->handle, LV2_MIDI__noteNumber);

	return (LV2_Handle)self;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	ExprSynth* self = (ExprSynth*)instance;
	switch(port) {
	case EXPR_SYNTH_INPUT:
		self->ports.input = (const LV2_Atom_Sequence*)data;
		break;
	case EXPR_SYNTH_OUTPUT:
		self->ports.output = (float*)data;
		break;
	default:
		break;
	}
}

static Voice*
find_voice(ExprSynth* self)
{
	Voice* v = NULL;
	int q = NVOICES;

	for (int i = 0; i < NVOICES; ++i) {
		int idx = (i + self->next_voice) % NVOICES;
		if (!self->voices[idx].gate) {
			q = idx;
			break;
		}
	}

	if (q < NVOICES) {
		v = &self->voices[q];
	} else {
		v = &self->voices[self->next_voice % NVOICES];
	}

	++self->next_voice;
	return v;
}

static void
note_on(ExprSynth* self,
        uint8_t    channel,
        uint8_t    key,
        uint8_t    velocity)
{
	Voice* v = find_voice(self);

	v->gate = true;
	v->channel = channel;
	v->key = key;
	v->velocity = velocity / 127.0f;

	v->bend = 0.0f;
}

static void
note_off(ExprSynth* self,
         uint8_t    channel,
         uint8_t    key)
{
	for (int i = 0; i < NVOICES; ++i) {
		Voice* v = &self->voices[i];
		if (v->channel == channel && v->key == key) {
			v->gate = false;
		}
	}
}

static void
apply_bend(ExprSynth* self,
           uint8_t    channel,
           uint8_t    key,
           float      bend)
{
	for (int i = 0; i < NVOICES; ++i) {
		Voice* v = &self->voices[i];
		if (v->channel == channel && v->key == key) {
			v->bend = bend;
		}
	}
}

static void
render(ExprSynth* self,
       uint32_t a,
       uint32_t b)
{
	for (int vi = 0; vi < NVOICES; ++vi) {
		Voice* v = &self->voices[vi];

		if (!v->gate) {
			continue;
		}

		float hz = KEY2HZ(v->key + v->bend);

		for (uint32_t i = a; i < b; ++i) {
			v->counter += lrintf((float)((uint32_t)-1) / (self->rate / hz));
			float s = v->counter / (float)((uint32_t)-1 >> 1);
			self->ports.output[i] += s / (float)NVOICES;
		}
	}
}

static void
run(LV2_Handle instance,
    uint32_t sample_count)
{
	ExprSynth* self = (ExprSynth*)instance;
	uint32_t offset = 0;

	memset(self->ports.output, 0, sizeof(float) * sample_count);

	LV2_ATOM_SEQUENCE_FOREACH(self->ports.input, ev) {
		if (ev->body.type == self->uris.midi_MidiEvent) {
			const uint8_t* const msg = (const uint8_t*)(ev + 1);
			switch (lv2_midi_message_type (msg)) {
			case LV2_MIDI_MSG_NOTE_ON:
				note_on(self, msg[0] & 0xf, msg[1] & 0x7f, msg[2] & 0x7f);
				break;
			case LV2_MIDI_MSG_NOTE_OFF:
				note_off(self, msg[0] & 0xf, msg[1] & 0x7f);
				break;
			default:
				break;
			}
		} else if (ev->body.type == self->uris.atom_Object) {
			LV2_Atom_Object* object = (LV2_Atom_Object*)&ev->body;
			if (object->body.otype != self->uris.expr_Expression) {
				continue;
			}

			const LV2_Atom_Int*   channel = NULL;
			const LV2_Atom_Int*   note    = NULL;
			const LV2_Atom_Float* bend    = NULL;

			LV2_Atom_Object_Query q[] = {
				{ self->uris.midi_channel,    (const LV2_Atom**)&channel },
				{ self->uris.midi_noteNumber, (const LV2_Atom**)&note },
				{ self->uris.expr_pitchBend,  (const LV2_Atom**)&bend },
				LV2_ATOM_OBJECT_QUERY_END
			};
			lv2_atom_object_query(object, q);

			if (!channel || !note || !bend) {
				continue;
			}

			apply_bend(self, channel->body, note->body, bend->body);
		}

		render(self, offset, ev->time.frames);
		offset = (uint32_t)ev->time.frames;
	}

	render(self, offset, sample_count);
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
	EXPR_SYNTH_URI,
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
