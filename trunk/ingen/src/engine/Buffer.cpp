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

#include <algorithm>
#include "AudioBuffer.hpp"
#include "EventBuffer.hpp"
#include "ObjectBuffer.hpp"

namespace Ingen {

using namespace Shared;

class Engine;

Buffer*
Buffer::create(Engine& engine, Shared::PortType type, size_t size)
{
	if (type.is_control())
		return new AudioBuffer(type, size);
	else if (type.is_audio())
		return new AudioBuffer(type, size);
	else if (type.is_events())
		return new EventBuffer(size);
	else if (type.is_value())
		return new ObjectBuffer(std::max(size,
					sizeof(LV2_Object) + sizeof(void*)));
	else
		throw;
}


} // namespace Ingen
