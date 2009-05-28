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

#ifndef OSCENGINERECEIVER_H
#define OSCENGINERECEIVER_H

#include <stdint.h>
#include <lo/lo.h>
#include "QueuedEngineInterface.hpp"
#include "Responder.hpp"

namespace Ingen {

class JackDriver;
class NodeFactory;
class PatchImpl;


/* Some boilerplate killing macros... */
#define LO_HANDLER_ARGS const char* path, const char* types, lo_arg** argv, int argc, lo_message msg

/* Defines a static handler to be passed to lo_add_method, which is a trivial
 * wrapper around a non-static method that does the real work.  Makes a whoole
 * lot of ugly boiler plate go away */
#define LO_HANDLER(name) \
int _##name##_cb (LO_HANDLER_ARGS);\
inline static int name##_cb(LO_HANDLER_ARGS, void* myself)\
{ return ((OSCEngineReceiver*)myself)->_##name##_cb(path, types, argv, argc, msg); }


/* FIXME: Make this receive and preprocess in the same thread? */


/** Receives OSC messages from liblo.
 *
 * This inherits from QueuedEngineInterface and calls it's own functions
 * via OSC.  It's not actually a directly callable EngineInterface (it's
 * callable via OSC...) so it should be implemented-as-a (privately inherit)
 * QueuedEngineInterface, but it needs to be public so it's an EventSource
 * the Driver can use.  This probably should be fixed somehow..
 *
 * \ingroup engine
 */
class OSCEngineReceiver : public QueuedEngineInterface
{
public:
	OSCEngineReceiver(Engine& engine, size_t queue_size, uint16_t port);
	~OSCEngineReceiver();

	void activate_source();
	void deactivate_source();

private:
	struct ReceiveThread : public Raul::Thread {
		ReceiveThread(OSCEngineReceiver& receiver) : _receiver(receiver) {}
		virtual void _run();
	private:
		OSCEngineReceiver& _receiver;
	};

	friend class ReceiveThread;

	ReceiveThread* _receive_thread;

	static void error_cb(int num, const char* msg, const char* path);
	static int  set_response_address_cb(LO_HANDLER_ARGS, void* myself);
	static int  generic_cb(LO_HANDLER_ARGS, void* myself);
	static int  unknown_cb(LO_HANDLER_ARGS, void* myself);

	LO_HANDLER(quit);
	LO_HANDLER(ping);
	LO_HANDLER(ping_slow);
	LO_HANDLER(register_client);
	LO_HANDLER(unregister_client);
	LO_HANDLER(load_plugins);
	LO_HANDLER(engine_activate);
	LO_HANDLER(engine_deactivate);
	LO_HANDLER(get);
	LO_HANDLER(put);
	LO_HANDLER(move);
	LO_HANDLER(del);
	LO_HANDLER(clear_patch);
	LO_HANDLER(connect);
	LO_HANDLER(disconnect);
	LO_HANDLER(disconnect_all);
	LO_HANDLER(set_port_value);
	LO_HANDLER(note_on);
	LO_HANDLER(note_off);
	LO_HANDLER(all_notes_off);
	LO_HANDLER(midi_learn);
	LO_HANDLER(set_property);
	LO_HANDLER(property_set);
	LO_HANDLER(request_property);
	LO_HANDLER(request_plugins);
	LO_HANDLER(request_all_objects);

	lo_server _server;
};


} // namespace Ingen

extern "C" {
	/// Module interface
	extern Ingen::OSCEngineReceiver* new_osc_receiver(
			Ingen::Engine& engine, size_t queue_size, uint16_t port);
}

#endif // OSCENGINERECEIVER_H
