/* This file is part of Ingen.  Copyright (C) 2006 Dave Robillard.
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
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef CLIENTBROADCASTER_H
#define CLIENTBROADCASTER_H

#include <string>
#include <iostream>
#include <utility>
#include <list>
#include <lo/lo.h>
#include <pthread.h>
#include "types.h"
#include "interface/ClientInterface.h"
#include "util/CountedPtr.h"

using std::list; using std::string; using std::pair;

namespace Om {

class Node;
class Port;
class Plugin;
class Patch;
class Connection;
class Responder;
namespace Shared { class ClientKey; }
using Shared::ClientKey;
using Shared::ClientInterface;

/** Broadcaster for all clients.
 *
 * This sends messages to all client simultaneously through the opaque
 * ClientInterface.  The clients may be OSC driver, in process, theoretically
 * anything that implements ClientInterface.
 *
 * This also serves as the database of all registered clients.
 *
 * \ingroup engine
 */
class ClientBroadcaster
{
public:
	void register_client(const ClientKey key, CountedPtr<ClientInterface> client);
	bool unregister_client(const ClientKey& key);
	
	CountedPtr<ClientInterface> client(const ClientKey& key);
	
	// Notification band:
	
	//void send_client_registration(const string& url, int client_id);
	
	// Error that isn't the direct result of a request
	void send_error(const string& msg);

	void send_plugins_to(ClientInterface* client);
	
	//void send_node_creation_messages(const Node* const node);
	
	void send_patch(const Patch* const p);
	void send_node(const Node* const node);
	void send_port(const Port* port);
	void send_destroyed(const string& path);
	void send_patch_cleared(const string& patch_path);
	void send_connection(const Connection* const connection);
	void send_disconnection(const string& src_port_path, const string& dst_port_path);
	void send_rename(const string& old_path, const string& new_path);
	void send_all_objects();
	void send_patch_enable(const string& patch_path);
	void send_patch_disable(const string& patch_path);
	void send_metadata_update(const string& node_path, const string& key, const string& value);
	void send_control_change(const string& port_path, float value);
	void send_program_add(const string& node_path, int bank, int program, const string& name);
	void send_program_remove(const string& node_path, int bank, int program);

private:
	typedef list<pair<ClientKey, CountedPtr<ClientInterface> > > ClientList;
	//list<pair<ClientKey, ClientInterface* const> > _clients;
	ClientList _clients;
};



} // namespace Om

#endif // CLIENTBROADCASTER_H
