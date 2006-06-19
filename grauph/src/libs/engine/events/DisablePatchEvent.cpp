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

#include "DisablePatchEvent.h"
#include "Responder.h"
#include "Om.h"
#include "OmApp.h"
#include "Patch.h"
#include "ClientBroadcaster.h"
#include "util.h"
#include "ObjectStore.h"
#include "Port.h"

namespace Om {


DisablePatchEvent::DisablePatchEvent(CountedPtr<Responder> responder, const string& patch_path)
: QueuedEvent(responder),
  m_patch_path(patch_path),
  m_patch(NULL)
{
}


void
DisablePatchEvent::pre_process()
{
	m_patch = om->object_store()->find_patch(m_patch_path);
	
	QueuedEvent::pre_process();
}


void
DisablePatchEvent::execute(samplecount offset)
{
	if (m_patch != NULL)
		m_patch->process(false);

	QueuedEvent::execute(offset);
}


void
DisablePatchEvent::post_process()
{	
	if (m_patch != NULL) {
		m_responder->respond_ok();
		om->client_broadcaster()->send_patch_disable(m_patch_path);
	} else {
		m_responder->respond_error(string("Patch ") + m_patch_path + " not found");
	}
}


} // namespace Om
