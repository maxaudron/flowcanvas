/* This file is part of Ingen.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
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

#include <cassert>
#include "InternalPlugin.hpp"
#include "MidiNoteNode.hpp"
#include "MidiTriggerNode.hpp"
#include "MidiControlNode.hpp"
#include "TransportNode.hpp"

namespace Ingen {


#if 0
InternalPlugin::InternalPlugin(const InternalPlugin* const copy)
{
	_type = copy->_type;
	_uri = copy->_uri;
	_lib_path = copy->_lib_path;
	_lib_name = copy->_lib_name;
	_plug_label = copy->_plug_label;
	_name = copy->_name;
	_id = _id;
	_module = copy->_module;
}
#endif


NodeImpl*
InternalPlugin::instantiate(const string&     name,
                            bool              polyphonic,
                            Ingen::PatchImpl* parent,
                            SampleRate        srate,
                            size_t            buffer_size)
{
	assert(_type == Internal);

	if (_uri == "ingen:note_node") {
		return new MidiNoteNode(name, polyphonic, parent, srate, buffer_size);
	} else if (_uri == "ingen:trigger_node") {
		return new MidiTriggerNode(name, polyphonic, parent, srate, buffer_size);
	} else if (_uri == "ingen:control_node") {
		return new MidiControlNode(name, polyphonic, parent, srate, buffer_size);
	} else if (_uri == "ingen:transport_node") {
		return new TransportNode(name, polyphonic, parent, srate, buffer_size);
	} else {
		return NULL;
	}
}


} // namespace Ingen
