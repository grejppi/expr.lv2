
LV2 Note Expression
===================

Important note: please don't implement this extension yet in your plugins!
This is still very much a work in progress.  The vocabulary might change,
things are not defined in detail yet, documentation doesn't exist, and so on.
Even the base URI might change! :scream:

This extension adds a new optional event type, `expr:Expression`, to go along
with the existing `midi:MidiEvent`s, providing finer control of individual note
parameters such as pitch bend and panning.  Notes are identified by their MIDI
channel and note number, allowing for 2048-voice polyphony.

This repository contains a draft of the specification and two example plugins.
