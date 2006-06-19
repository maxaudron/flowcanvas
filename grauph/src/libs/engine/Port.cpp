/* This file is part of Om.  Copyright (C) 2006 Dave Robillard.
 * 
 * Om is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Om is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Port.h"
#include "Node.h"
#include "Om.h"
#include "OmApp.h"
#include "ObjectStore.h"
#include "DataType.h"

namespace Om {


// Yeah, this shouldn't be here.
const char* const DataType::type_uris[3] = { "UNKNOWN", "FLOAT", "MIDI" };


Port::Port(Node* const node, const string& name, size_t index, size_t poly, DataType type, size_t buffer_size)
: OmObject(node, name),
  _index(index),
  _poly(poly),
  _type(type),
  _buffer_size(buffer_size)
{
	assert(node != NULL);
	assert(_poly > 0);
}


void
Port::add_to_store()
{
	om->object_store()->add(this);
}


void
Port::remove_from_store()
{
	// Remove self
	TreeNode<OmObject*>* node = om->object_store()->remove(path());
	assert(node != NULL);
	assert(om->object_store()->find(path()) == NULL);
	delete node;
}


} // namespace Om