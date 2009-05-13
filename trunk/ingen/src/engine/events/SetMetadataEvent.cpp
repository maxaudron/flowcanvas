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

#include "SetMetadataEvent.hpp"
#include <string>
#include <boost/format.hpp>
#include "Responder.hpp"
#include "Engine.hpp"
#include "PortImpl.hpp"
#include "ClientBroadcaster.hpp"
#include "GraphObjectImpl.hpp"
#include "PatchImpl.hpp"
#include "EngineStore.hpp"

using std::string;

namespace Ingen {


SetMetadataEvent::SetMetadataEvent(
		Engine&              engine,
		SharedPtr<Responder> responder,
		SampleCount          timestamp,
		bool                 property,
		const Path&          path,
		const URI&           key,
		const Atom&          value)
	: QueuedEvent(engine, responder, timestamp)
	, _error(NO_ERROR)
	, _special_type(NONE)
	, _property(property)
	, _path(path)
	, _key(key)
	, _value(value)
	, _object(NULL)
	, _patch(NULL)
	, _compiled_patch(NULL)
{
}


void
SetMetadataEvent::pre_process()
{
	_object = _engine.engine_store()->find_object(_path);
	if (_object == NULL) {
		_error = NOT_FOUND;
		QueuedEvent::pre_process();
		return;
	}

	/*cerr << "SET " << _object->path() << (_property ? " PROP " : " VAR ")
		<<	_key << " :: " << _value.type() << endl;*/

	if (_property)
		_object->set_property(_key, _value);
	else
		_object->set_variable(_key, _value);
	
	_patch = dynamic_cast<PatchImpl*>(_object);

	if (_key.str() == "ingen:broadcast") {
		_special_type = ENABLE_BROADCAST;
	} else if (_patch) {
		if (!_property && _key.str() == "ingen:enabled") {
			if (_value.type() == Atom::BOOL) {
				_special_type = ENABLE;
				if (_value.get_bool() && !_patch->compiled_patch())
					_compiled_patch = _patch->compile();
			} else {
				_error = BAD_TYPE;
			}
		} else if (!_property && _key.str() == "ingen:polyphonic") {
			if (_value.type() == Atom::BOOL) {
				_special_type = POLYPHONIC;
			} else {
				_error = BAD_TYPE;
			}
		} else if (_property && _key.str() == "ingen:polyphony") {
			if (_value.type() == Atom::INT) {
				_special_type = POLYPHONY;
				_patch->prepare_internal_poly(_value.get_int32());
			} else {
				_error = BAD_TYPE;
			}
		}
	}

	QueuedEvent::pre_process();
}


void
SetMetadataEvent::execute(ProcessContext& context)
{
	if (_error != NO_ERROR)
		return;

	PortImpl* port   = 0;
	switch (_special_type) {
	case ENABLE_BROADCAST:
		if ((port = dynamic_cast<PortImpl*>(_object)))
			port->broadcast(_value.get_bool());
		break;
	case ENABLE:
		if (_value.get_bool()) {
			if (!_patch->compiled_patch())
				_patch->compiled_patch(_compiled_patch);
			_patch->enable();
		} else {
			_patch->disable();
		}
		break;
	case POLYPHONIC:
		if (!_object->set_polyphonic(*_engine.maid(), _value.get_bool()))
			_error = INTERNAL;
		break;
	case POLYPHONY:
		if (!_patch->apply_internal_poly(*_engine.maid(), _value.get_int32()))
			_error = INTERNAL;
		break;
	default:
		_success = true;
	} 

	QueuedEvent::execute(context);
}


void
SetMetadataEvent::post_process()
{
	switch (_error) {
	case NO_ERROR:
		_responder->respond_ok();
		if (_property)
			_engine.broadcaster()->send_property_change(_path, _key, _value);
		else
			_engine.broadcaster()->send_variable_change(_path, _key, _value);
		break;
	case NOT_FOUND:
		_responder->respond_error((boost::format(
				"Unable to find object '%1%' to set '%2%'")
				% _path % _key).str());
	case INTERNAL:
		_responder->respond_error("Internal error");
		break;
	case BAD_TYPE:
		_responder->respond_error((boost::format("Bad type for '%1%'") % _key).str());
		break;
	}
}


} // namespace Ingen

