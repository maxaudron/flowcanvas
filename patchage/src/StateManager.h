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

#ifndef STATEMANAGER_H
#define STATEMANAGER_H

#include <string>
#include <list>
#include <map>
#include <iostream>
#include "PatchagePort.h"

using std::string; using std::list; using std::map;


enum ModuleType { Input, Output, InputOutput };
struct Coord { double x; double y; };

// This should probably be moved out in to a seperate class/file....
typedef struct ModuleLocation
{
	string name;
	ModuleType  type; // for distinguishing terminal modules (input or output)
	Coord       loc;
};


class StateManager
{
public:
	StateManager();

	void load(const string& filename);
	void save(const string& filename);

	Coord get_module_location(const string& name, ModuleType type);
	void  set_module_location(const string& name, ModuleType type, Coord loc);

	void set_module_split(const string& name, bool split);
	bool get_module_split(const string& name, bool default_val) const;
	
	Coord get_window_location();
	void  set_window_location(Coord loc);

	Coord get_window_size();
	void  set_window_size(Coord loc);

	float get_zoom();
	void  set_zoom(float zoom);
	
	int get_port_color(PortType type);

private:
	list<ModuleLocation> m_module_locations;
	map<string,bool>     m_module_splits;
	Coord                m_window_location;
	Coord                m_window_size;
	float                m_zoom;
};


#endif // STATEMANAGER_H