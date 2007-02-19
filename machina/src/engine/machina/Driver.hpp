/* This file is part of Machina.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 * 
 * Machina is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Machina is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MACHINA_JACKDRIVER_HPP
#define MACHINA_JACKDRIVER_HPP

#include <raul/JackDriver.h>

namespace Machine {


class JackDriver : public Raul::JackDriver {
public:
	JackDriver(SharedPtr<Machine> machine);

	virtual void set_machine(SharedPtr<Machine> machine);
	
protected:
	virtual void on_process(jack_nframes_t nframes);

private:
	SharedPtr<Machine> _machine;
};


} // namespace Machina

#endif // MACHINA_JACKDRIVER_HPP