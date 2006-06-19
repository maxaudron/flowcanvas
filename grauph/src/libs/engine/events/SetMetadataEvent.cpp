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

#include "SetMetadataEvent.h"
#include <string>
#include "Responder.h"
#include "Om.h"
#include "OmApp.h"
#include "ClientBroadcaster.h"
#include "OmObject.h"
#include "ObjectStore.h"

using std::string;

namespace Om {


SetMetadataEvent::SetMetadataEvent(CountedPtr<Responder> responder, const string& path, const string& key, const string& value)
: QueuedEvent(responder),
  m_path(path),
  m_key(key),
  m_value(value),
  m_object(NULL)
{
}


void
SetMetadataEvent::pre_process()
{
	m_object = om->object_store()->find(m_path);
	if (m_object == NULL) {
		QueuedEvent::pre_process();
		return;
	}

	m_object->set_metadata(m_key, m_value);

	QueuedEvent::pre_process();
}


void
SetMetadataEvent::execute(samplecount offset)
{
	// Do nothing
	
	QueuedEvent::execute(offset);
}


void
SetMetadataEvent::post_process()
{
	if (m_object == NULL) {
		string msg = "Unable to find object ";
		msg += m_path;
		m_responder->respond_error(msg);
	} else {
		m_responder->respond_ok();
		om->client_broadcaster()->send_metadata_update(m_path, m_key, m_value);
	}
}


} // namespace Om