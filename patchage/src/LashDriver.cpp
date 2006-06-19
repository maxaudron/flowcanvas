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

#include "LashDriver.h"
#include "config.h"
#include <iostream>
#include <string>
#include "Patchage.h"
#include "StateManager.h"

using std::cerr; using std::cout; using std::endl;
using std::string;


LashDriver::LashDriver(Patchage* app, int argc, char** argv)
: m_app(app),
  m_client(NULL),
  m_args(NULL)
{
	m_args = lash_extract_args(&argc, &argv);
}


LashDriver::~LashDriver()
{
	if (m_args)
		lash_args_destroy(m_args);
}


void
LashDriver::attach(bool launch_daemon)
{
	cout << "Connecting to Lash... ";
	cout.flush();

	if (m_client != NULL) {
		cout << "already connected." << endl;
		return;
	}

	int lash_flags = LASH_Config_File;
	if (launch_daemon)
		lash_flags |= LASH_No_Start_Server;
	m_client = lash_init(m_args, PACKAGE_NAME, lash_flags, LASH_PROTOCOL(2, 0));
	if (m_client == NULL) {
		cout << "Failed.  Session management will not occur." << endl;
	} else {
		lash_event_t* event = lash_event_new_with_type(LASH_Client_Name);
		lash_event_set_string(event, "Patchage");
		lash_send_event(m_client, event);
		cout << "Connected" << endl;
	}
}


void
LashDriver::detach()
{
	// FIXME: send some notification that we're gone??
	m_client = NULL;
	cout << "Disconnected from Lash" << endl;
}


void
LashDriver::process_events()
{
	lash_event_t*  ev = NULL;
	lash_config_t* conf = NULL;

	// Process events
	while ((ev = lash_get_event(m_client)) != NULL) {
		handle_event(ev);
		lash_event_destroy(ev);	
	}

	// Process configs
	while ((conf = lash_get_config(m_client)) != NULL) {
		handle_config(conf);
		lash_config_destroy(conf);	
	}
}


void
LashDriver::handle_event(lash_event_t* ev)
{
	LASH_Event_Type type = lash_event_get_type(ev);
	const char*    c_str = lash_event_get_string(ev);
	string         str   = (c_str == NULL) ? "" : c_str;
	
	//cout << "[LashDriver] LASH Event.  Type = " << type << ", string = " << str << "**********" << endl;
	
	if (type == LASH_Save_File) {
		//cout << "[LashDriver] LASH Save File - " << str << endl;
		m_app->store_window_location();
		m_app->state_manager()->save(str.append("/locations"));
		lash_send_event(m_client, lash_event_new_with_type(LASH_Save_File));
	} else if (type == LASH_Restore_File) {
		//cout << "[LashDriver] LASH Restore File - " << str << endl;
		m_app->state_manager()->load(str.append("/locations"));
		m_app->update_state();
		lash_send_event(m_client, lash_event_new_with_type(LASH_Restore_File));
	} else if (type == LASH_Save_Data_Set) {
		//cout << "[LashDriver] LASH Save Data Set - " << endl;
		
		// Tell LASH we're done
		lash_send_event(m_client, lash_event_new_with_type(LASH_Save_Data_Set));
	} else if (type == LASH_Quit) {
		//stop_thread();
		m_client = NULL;
		m_app->quit();
	}
}


void
LashDriver::handle_config(lash_config_t* conf)
{
	const char*    key      = NULL;
	const void*    val      = NULL;
	size_t         val_size = 0;

	//cout << "[LashDriver] LASH Config.  Key = " << key << endl;

	key      = lash_config_get_key(conf);
	val      = lash_config_get_value(conf);
	val_size = lash_config_get_value_size(conf);
}


