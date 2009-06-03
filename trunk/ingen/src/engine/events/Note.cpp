/* This file is part of Ingen.
 * Copyright (C) 2007-2009 Dave Robillard <http://drobilla.net>
 *
 * Ingen is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "Note.hpp"
#include "Responder.hpp"
#include "Engine.hpp"
#include "EngineStore.hpp"
#include "NodeImpl.hpp"
#include "InternalNote.hpp"
#include "InternalTrigger.hpp"
#include "PluginImpl.hpp"
#include "InternalPlugin.hpp"
#include "ProcessContext.hpp"

using namespace Raul;

namespace Ingen {
namespace Events {


/** Note on with Patch explicitly passed.
 *
 * Used to be triggered by MIDI.  Not used anymore.
 */
NoteEvent::NoteEvent(Engine& engine, SharedPtr<Responder> responder, SampleCount timestamp, NodeImpl* node, bool on, uint8_t note_num, uint8_t velocity)
: Event(engine, responder, timestamp),
  _node(node),
  _on(on),
  _note_num(note_num),
  _velocity(velocity)
{
}


/** Note on with Node lookup.
 *
 * Triggered by OSC.
 */
NoteEvent::NoteEvent(Engine& engine, SharedPtr<Responder> responder, SampleCount timestamp, const Path& node_path, bool on, uint8_t note_num, uint8_t velocity)
: Event(engine, responder, timestamp),
  _node(NULL),
  _node_path(node_path),
  _on(on),
  _note_num(note_num),
  _velocity(velocity)
{
}


void
NoteEvent::execute(ProcessContext& context)
{
	Event::execute(context);
	assert(_time >= context.start() && _time <= context.end());

	// Lookup if neccessary
	if (!_node)
		_node = _engine.engine_store()->find_node(_node_path);

	// FIXME: barf

	if (_node != NULL && _node->plugin()->type() == Shared::Plugin::Internal) {
		if (_on) {
			if (_node->plugin_impl()->uri().str() == NS_INTERNALS "Note")
				((NoteNode*)_node)->note_on(context, _note_num, _velocity, _time);
			else if (_node->plugin_impl()->uri().str() == NS_INTERNALS "Trigger")
				((TriggerNode*)_node)->note_on(context, _note_num, _velocity, _time);
		} else  {
			if (_node->plugin_impl()->uri().str() == NS_INTERNALS "Note")
				((NoteNode*)_node)->note_off(context, _note_num, _time);
			else if (_node->plugin_impl()->uri().str() == NS_INTERNALS "Trigger")
				((TriggerNode*)_node)->note_off(context, _note_num, _time);
		}
	}
}


void
NoteEvent::post_process()
{
	if (_responder) {
		if (_node)
			_responder->respond_ok();
		else
			_responder->respond_error("Did not find node for note on event");
	}
}


} // namespace Ingen
} // namespace Events

