/* This file is part of Om.  Copyright (C) 2005 Dave Robillard.
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
 * You should have received a copy of the GNU General Public License alongCont
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef PRESETMODEL_H
#define PRESETMODEL_H

#include <string>
#include <list>
#include "ControlModel.h"

using std::string; using std::list;

namespace LibOmClient {


/** Model of a preset (a collection of control settings).
 *
 * \ingroup libomclient
 */
class PresetModel
{
public:
	PresetModel(const string& base_path)
	: m_base_path(base_path)
	{}

	/** Add a control value to this preset.  An empty string for a node_name
	 * means the port is on the patch itself (not a node in the patch).
	 */
	void add_control(const string& node_name,
	                 const string& port_name, float value)
	{
		if (node_name != "")
			m_controls.push_back(ControlModel(m_base_path + node_name +"/"+ port_name, value));
		else
			m_controls.push_back(ControlModel(m_base_path + port_name, value));
	}

	const string& name()                { return m_name; }
	void          name(const string& n) { m_name = n; }

	const list<ControlModel>& controls() const { return m_controls; }

private:
	string             m_name;
	string             m_base_path;
	list<ControlModel> m_controls;
};


} // namespace LibOmClient

#endif // PRESETMODEL