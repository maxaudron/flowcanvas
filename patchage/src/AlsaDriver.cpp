/* This file is part of Patchage.  Copyright (C) 2005 Dave Robillard.
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

#include <string>
#include <iostream>
#include <cassert>
#include <sys/poll.h>
#include <errno.h>
#include "PatchageFlowCanvas.h"
#include "AlsaDriver.h"
#include "Patchage.h"
#include "PatchageModule.h"
#include "PatchagePort.h"

using std::cerr;
using std::string;


using namespace LibFlowCanvas;

AlsaDriver::AlsaDriver(Patchage* app)
: m_app(app),
  m_canvas(app->canvas()),
  m_seq(NULL)
{
}


AlsaDriver::~AlsaDriver() 
{
	detach();
}


/** Attach to ALSA.
 * @a launch_daemon is ignored, as ALSA has no daemon to launch/connect to.
 */
void
AlsaDriver::attach(bool launch_daemon)
{
	cout << "Connecting to Alsa... ";
	cout.flush();

	int ret = snd_seq_open(&m_seq, "default",
	                       SND_SEQ_OPEN_DUPLEX,
	                       SND_SEQ_NONBLOCK);
	if (ret) {
		cout << "Failed" << endl;
		m_seq = NULL;
	} else {
		cout << "Connected" << endl;

		snd_seq_set_client_name(m_seq, "Patchage");
	
		ret = pthread_create(&m_refresh_thread, NULL, &AlsaDriver::refresh_main, this);
		if (ret)
			cerr << "Couldn't start refresh thread" << endl;
	}
}


void
AlsaDriver::detach() 
{
	if (m_seq != NULL) {
		pthread_cancel(m_refresh_thread);
		pthread_join(m_refresh_thread, NULL);
		snd_seq_close(m_seq);
		m_seq = NULL;
		cout << "Disconnected from Alsa" << endl;
	}
}


/** Refresh all Alsa Midi ports and connections.
 */
void
AlsaDriver::refresh()
{
	if (!is_attached())
		return;

	assert(m_seq);
	
	refresh_ports();
	refresh_connections();

	undirty();
}


/** Refresh all Alsa Midi ports. 
 */
void
AlsaDriver::refresh_ports()
{
	assert(is_attached());
	assert(m_seq);
	
	snd_seq_client_info_t* cinfo;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, -1);

	snd_seq_port_info_t * pinfo;
	snd_seq_port_info_alloca(&pinfo);
	
	string client_name;
	string port_name;
	bool is_input       = false;
	bool is_duplex      = false;
	bool is_application = true;
	
	while (snd_seq_query_next_client (m_seq, cinfo) >= 0) {
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);

		client_name = snd_seq_client_info_get_name(cinfo);

		while (snd_seq_query_next_port(m_seq, pinfo) >= 0) {
			int caps = snd_seq_port_info_get_capability(pinfo);
			int type = snd_seq_port_info_get_type(pinfo);

			// Skip ports we shouldn't show
			if (caps & SND_SEQ_PORT_CAP_NO_EXPORT)
				continue;
			else if ( !( (caps & SND_SEQ_PORT_CAP_READ)
						|| (caps & SND_SEQ_PORT_CAP_WRITE)
						|| (caps & SND_SEQ_PORT_CAP_DUPLEX)))
				continue;
			else if ((snd_seq_client_info_get_type(cinfo) != SND_SEQ_USER_CLIENT)
					&& ((type == SND_SEQ_PORT_SYSTEM_TIMER
						|| type == SND_SEQ_PORT_SYSTEM_ANNOUNCE)))
				continue;
			
			const snd_seq_addr_t addr = *snd_seq_port_info_get_addr(pinfo);
			
			is_duplex = false;

			// FIXME: Should be CAP_SUBS_READ etc?
			if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_WRITE))
				is_duplex = true;
			else if (caps & SND_SEQ_PORT_CAP_READ)
				is_input = false;
			else if (caps & SND_SEQ_PORT_CAP_WRITE)
				is_input = true;

			is_application = (type & SND_SEQ_PORT_TYPE_APPLICATION);
			port_name = snd_seq_port_info_get_name(pinfo);
			PatchageModule* m = NULL;
			
			//cout << client_name << " : " << port_name << " is_application = " << is_application
			//	<< " is_duplex = " << is_duplex << endl;

			bool split =  m_app->state_manager()->get_module_split(client_name, !is_application);
			
			// Application input/output ports go on the same module
			if (!split) {
				m = (PatchageModule*)m_canvas->find_module(client_name, InputOutput);
				if (m == NULL) {
					m = new PatchageModule(m_app, client_name, InputOutput);
					m->load_location();
					m->store_location();
					m_canvas->add_module(m);
				}
				
				if (!is_duplex) {
					m->add_patchage_port(port_name, is_input, ALSA_MIDI, addr);
				} else {
					m->add_patchage_port(port_name, true, ALSA_MIDI, addr);
					m->add_patchage_port(port_name, false, ALSA_MIDI, addr);
				}
			} else { // non-application input/output ports (hw interface, etc) go on separate modules
				PatchageModule* m = NULL;
				ModuleType type = InputOutput;
				
				// The 'application' hint isn't always set by clients, so this bit
				// is pretty nasty...

				if (!is_duplex) {  // just one port to add
					if (is_input) type = Input;
					else type = Output;
					
					// See if an InputOutput module exists (maybe with Jack ports on it)
					m = (PatchageModule*)m_canvas->find_module(client_name, InputOutput);
				
					if (m == NULL)
						m = (PatchageModule*)m_canvas->find_module(client_name, type);
					if (m == NULL) {
						m = new PatchageModule(m_app, client_name, type);
						m->load_location();
						m->store_location();
						m_canvas->add_module(m);
					}
					m->add_patchage_port(port_name, is_input, ALSA_MIDI, addr);
				} else {  // two ports to add
					type = Input;
					
					// See if an InputOutput module exists (maybe with Jack ports on it)
					m = (PatchageModule*)m_canvas->find_module(client_name, InputOutput);
					
					if (m == NULL)
						m = (PatchageModule*)m_canvas->find_module(client_name, type);
					if (m == NULL) {
						m = new PatchageModule(m_app, client_name, type);
						m->load_location();
						m->store_location();
						m_canvas->add_module(m);
					}
					m->add_patchage_port(port_name, true, ALSA_MIDI, addr);

					type = Output;
					
					// See if an InputOutput module exists (maybe with Jack ports on it)
					m = (PatchageModule*)m_canvas->find_module(client_name, InputOutput);
					
					if (m == NULL)
						m = (PatchageModule*)m_canvas->find_module(client_name, type);
					if (m == NULL) {
						m = new PatchageModule(m_app, client_name, type);
						m->load_location();
						m->store_location();
						m_canvas->add_module(m);
					}
					m->add_patchage_port(port_name, false, ALSA_MIDI, addr);
				}
			}
		}
	}
}


/** Refresh all Alsa Midi connections.
 */
void
AlsaDriver::refresh_connections()
{
	assert(is_attached());
	assert(m_seq);
	
	PatchageModule* m = NULL;
	PatchagePort*   p = NULL;
	
	for (ModuleMap::iterator i = m_canvas->modules().begin();
			i != m_canvas->modules().end(); ++i) {
		m = (PatchageModule*)((*i).second);
		for (PortList::iterator j = m->ports().begin(); j != m->ports().end(); ++j) {
			p = (PatchagePort*)(*j);
			if (p->type() == ALSA_MIDI)
				add_connections(p);
		}
	}
}


/** Add all connections for the given port.
 */
void
AlsaDriver::add_connections(PatchagePort* port)
{
	assert(is_attached());
	assert(m_seq);
	
	const snd_seq_addr_t* addr           = port->alsa_addr();
	PatchagePort*         connected_port = NULL;
	
	// Fix a problem with duplex->duplex connections (would show up twice)
	// No sense doing them all twice anyway..
	if (port->is_input())
		return;
	
	snd_seq_query_subscribe_t* subsinfo;
	snd_seq_query_subscribe_alloca(&subsinfo);
	snd_seq_query_subscribe_set_root(subsinfo, addr);
	snd_seq_query_subscribe_set_index(subsinfo, 0);
	
	while(!snd_seq_query_port_subscribers(m_seq, subsinfo)) {
		const snd_seq_addr_t* connected_addr = snd_seq_query_subscribe_get_addr(subsinfo);
		
		connected_port = m_canvas->find_port(connected_addr, !port->is_input());

		if (connected_port != NULL) {
			m_canvas->add_connection(port, connected_port);
		}

		snd_seq_query_subscribe_set_index(subsinfo, snd_seq_query_subscribe_get_index(subsinfo) + 1);
	}

}


/** Connects two Alsa Midi ports.
 * 
 * \return Whether connection succeeded.
 */
bool
AlsaDriver::connect(const PatchagePort* const src_port, const PatchagePort* const dst_port) 
{
	const snd_seq_addr_t* src = src_port->alsa_addr();
	const snd_seq_addr_t* dst = dst_port->alsa_addr();

	bool result = false;
	
	if (src && dst) {
		snd_seq_port_subscribe_t* subs;
		snd_seq_port_subscribe_malloc(&subs);
		snd_seq_port_subscribe_set_sender(subs, src);
		snd_seq_port_subscribe_set_dest(subs, dst);
		snd_seq_port_subscribe_set_exclusive(subs, 0);
		snd_seq_port_subscribe_set_time_update(subs, 0);
		snd_seq_port_subscribe_set_time_real(subs, 0);

		// Already connected (shouldn't happen)
		if (!snd_seq_get_port_subscription(m_seq, subs)) {
			cerr << "Error: Attempt to subscribe Alsa ports that are already subscribed." << endl;
			result = false;
		}

		int ret = snd_seq_subscribe_port(m_seq, subs);
		if (ret < 0) {
			cerr << "Alsa subscription failed: " << snd_strerror(ret) << endl;
			result = false;
		}
	}

	return (!result);
}


/** Disconnects two Alsa Midi ports.
 * 
 * \return Whether disconnection succeeded.
 */
bool
AlsaDriver::disconnect(const PatchagePort* const src_port, const PatchagePort* const dst_port) 
{
	const snd_seq_addr_t* src = src_port->alsa_addr();
	const snd_seq_addr_t* dst = dst_port->alsa_addr();
	
	bool result = false;

	snd_seq_port_subscribe_t* subs;
	snd_seq_port_subscribe_malloc(&subs);
	snd_seq_port_subscribe_set_sender(subs, src);
	snd_seq_port_subscribe_set_dest(subs, dst);
	snd_seq_port_subscribe_set_exclusive(subs, 0);
	snd_seq_port_subscribe_set_time_update(subs, 0);
	snd_seq_port_subscribe_set_time_real(subs, 0);

	// Not connected (shouldn't happen)
	if (snd_seq_get_port_subscription(m_seq, subs) != 0) {
		cerr << "Error: Attempt to unsubscribe Alsa ports that are not subscribed." << endl;
		result = false;
	}
	
	int ret = snd_seq_unsubscribe_port(m_seq, subs);
	if (ret < 0) {
		cerr << "Alsa unsubscription failed: " << snd_strerror(ret) << endl;
		result = false;
	}

	return (!result);
}


bool
AlsaDriver::create_refresh_port()
{
	// Mostly lifted from alsa-patch-bay, (C) 2002 Robert Ham, released under GPL

	int ret;
	snd_seq_port_info_t* port_info;

	snd_seq_port_info_alloca(&port_info);
	snd_seq_port_info_set_name(port_info, "System Announcement Reciever");
	snd_seq_port_info_set_capability(port_info,
		SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT);
	
	snd_seq_port_info_set_type(port_info, SND_SEQ_PORT_TYPE_APPLICATION);

	ret = snd_seq_create_port(m_seq, port_info);
	if (ret) {
		cerr << "Error creating alsa port: " << snd_strerror(ret) << endl;
		return false;
	}

	// Subscribe the port to the system announcer
	ret = snd_seq_connect_from(m_seq,
		snd_seq_port_info_get_port(port_info),
		SND_SEQ_CLIENT_SYSTEM,
		SND_SEQ_PORT_SYSTEM_ANNOUNCE);
	
	if (ret) {
		cerr << "Could not connect to system announcer port: " << snd_strerror(ret) << endl;
		return false;
	}

	return true;
}


void*
AlsaDriver::refresh_main(void* me)
{
	AlsaDriver* ad = (AlsaDriver*)me;
	ad->m_refresh_main();
	return NULL;
}


void
AlsaDriver::m_refresh_main()
{
	// "Heavily influenced" from alsa-patch-bay
	// (C) 2002 Robert Ham, released under GPL

	int             ret;
	int             nfds    = snd_seq_poll_descriptors_count(m_seq, POLLIN);
	struct pollfd*  pfds    = new struct pollfd[nfds];
	unsigned short* revents = new unsigned short[nfds];

	if (!create_refresh_port()) {
		cerr << "Could not create Alsa listen port.  Auto refreshing will not work." << endl;
		return;
	}

	snd_seq_poll_descriptors(m_seq, pfds, nfds, POLLIN);

	while (true) {
		ret = poll(pfds, nfds, -1);
		if (ret == -1) {
			if (errno == EINTR)
				continue;

			cerr << "Error polling Alsa sequencer: " << strerror(errno) << endl;
			continue;
		}

		ret = snd_seq_poll_descriptors_revents(m_seq, pfds, nfds, revents);
		if (ret) {
			cerr << "Error getting Alsa sequencer poll events: "
				<< snd_strerror(ret) << endl;
			continue;
		}

		for (int i = 0; i < nfds; ++i) {
			if (revents[i] > 0) {
				snd_seq_event_t* ev;
				snd_seq_event_input(m_seq, &ev);

				if (ev == NULL)
					continue;

				switch (ev->type) {
				case SND_SEQ_EVENT_RESET:
				case SND_SEQ_EVENT_CLIENT_START:
				case SND_SEQ_EVENT_CLIENT_EXIT:
				case SND_SEQ_EVENT_CLIENT_CHANGE:
				case SND_SEQ_EVENT_PORT_START:
				case SND_SEQ_EVENT_PORT_EXIT:
				case SND_SEQ_EVENT_PORT_CHANGE:
				case SND_SEQ_EVENT_PORT_SUBSCRIBED:
				case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
					m_is_dirty = true;
					break;
				default:
					break;
				}
			}
		}
	}
}