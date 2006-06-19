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

#ifndef OBJECTSENDER_H
#define OBJECTSENDER_H

namespace Om {

namespace Shared {
	class ClientInterface;
} using Shared::ClientInterface;

class Patch;
class Node;
class Port;


/** Utility class for sending OmObjects to clients through ClientInterface.
 *
 * While ClientInterface is the direct low level message-based interface
 * (protocol), this is used from the engine to easily send proper Objects
 * with these messages (which is done in a few different parts of the code).
 *
 * Basically a serializer, except to calls on ClientInterface rather than
 * eg a byte stream.
 */
class ObjectSender {
public:
	
	// FIXME: Make all object parameters const
	
	static void send_all(ClientInterface* client);
	static void send_patch(ClientInterface* client, const Patch* patch);
	static void send_node(ClientInterface* client, const Node* node);
	static void send_port(ClientInterface* client, const Port* port);
	static void send_plugins(ClientInterface* client);
};

} // namespace Om

#endif // OBJECTSENDER_H
