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

#ifndef CONTEXT_H
#define CONTEXT_H

#include "EventSink.hpp"

namespace Ingen {

class Engine;

class Context
{
public:
	enum ID {
		AUDIO,
		MESSAGE
	};

	Context(Engine& engine, ID id)
		: _id(id)
		, _engine(engine)
		, _event_sink(engine, 1024) // FIXME: size?
		, _start(0)
		, _realtime(true)
	{}

	virtual ~Context() {}

	ID id() const { return _id; }

	void locate(FrameTime s) { _start = s; }

	inline Engine&   engine()   const { return _engine; }
	inline FrameTime start()    const { return _start; }
	inline bool      realtime() const { return _realtime; }

	inline const EventSink&  event_sink() const { return _event_sink; }
	inline EventSink&        event_sink()       { return _event_sink; }

protected:
	ID      _id;      ///< Fast ID for this context
	Engine& _engine;  ///< Engine we're running in

private:
	EventSink _event_sink; ///< Sink for events generated in a realtime context
	FrameTime _start;      ///< Start frame of this cycle, timeline relative
	bool      _realtime;   ///< True iff context is hard realtime
};


} // namespace Ingen

#endif // CONTEXT_H

