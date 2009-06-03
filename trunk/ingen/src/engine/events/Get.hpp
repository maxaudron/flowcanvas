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

#ifndef REQUESTOBJECTEVENT_H
#define REQUESTOBJECTEVENT_H

#include "QueuedEvent.hpp"
#include "types.hpp"

namespace Ingen {

class GraphObjectImpl;
class PluginImpl;

namespace Events {


/** A request from a client to send an object.
 *
 * \ingroup engine
 */
class Get : public QueuedEvent
{
public:
	Get(
		Engine&              engine,
		SharedPtr<Responder> responder,
		SampleCount          timestamp,
		const Raul::URI&     uri);

	void pre_process();
	void execute(ProcessContext& context);
	void post_process();

private:
	const Raul::URI   _uri;
	GraphObjectImpl*  _object;
	const PluginImpl* _plugin;
};


} // namespace Ingen
} // namespace Events

#endif // REQUESTOBJECTEVENT_H
