/* This file is part of FlowCanvas.  Copyright (C) 2005 Dave Robillard.
 * 
 * FlowCanvas is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * FlowCanvas is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Module.h"
#include "FlowCanvas.h"
#include <functional>
#include <list>
#include <cstdlib>
#include <cassert>
#include <cmath>
using std::string;

namespace LibFlowCanvas {

static const int MODULE_FILL_COLOUR           = 0x292929FF;
static const int MODULE_HILITE_FILL_COLOUR    = 0x393939FF;
static const int MODULE_OUTLINE_COLOUR        = 0x505050FF;
static const int MODULE_HILITE_OUTLINE_COLOUR = 0x606060FF;
static const int MODULE_TITLE_COLOUR          = 0xFFFFFFFF;


/** Construct a Module
 *
 * Note you must call resize() at some point or the module will look ridiculous.
 * This it to avoid unecessary text measuring and resizing, which is insanely
 * expensive.
 *
 * If @a name is the empty string, the space where the title would usually be
 * is not created (eg the module will be shorter).
 */
Module::Module(FlowCanvas* canvas, const string& name, double x, double y)
: Gnome::Canvas::Group(*canvas->root(), x, y),
  m_name(name),
  m_selected(false),
  m_canvas(canvas),
  m_module_box(*this, 0, 0, 0, 0), // w, h set later
  m_canvas_title(*this, 0, 6, name) // x set later
{
	assert(m_canvas != NULL);

	m_module_box.property_fill_color_rgba() = MODULE_FILL_COLOUR;

	m_module_box.property_outline_color_rgba() = MODULE_OUTLINE_COLOUR;
	border_width(1.0);

	m_canvas_title.property_size_set() = true;
	m_canvas_title.property_size() = 10000;
	m_canvas_title.property_weight_set() = true;
	m_canvas_title.property_weight() = 400;
	m_canvas_title.property_fill_color_rgba() = MODULE_TITLE_COLOUR;

	width(10.0);
	height(10.0);

	signal_event().connect(sigc::mem_fun(this, &Module::module_event));
}


Module::~Module()
{
	if (m_selected) {
		for (list<Module*>::iterator i = m_canvas->selected_modules().begin();
				i != m_canvas->selected_modules().end(); ++i)
		{
			if ((*i) == this) {
				m_canvas->selected_modules().erase(i);
				break;
			}
		}
	}
	for (PortList::iterator p = m_ports.begin(); p != m_ports.end(); ++p)
		delete (*p);
}


/** Set the border width of the module.
 *
 * Do NOT directly set the width_units property on the rect, use this function.
 */
void
Module::border_width(double w)
{
	m_border_width = w;
	m_module_box.property_width_units() = w;
}


bool
Module::module_event(GdkEvent* event)
{
	/* FIXME:  Some things need to be handled here (ie dragging), but handling double
	 * clicks and whatnot here is just filthy (look at this method.  I mean, c'mon now).
	 * Move double clicks out to on_double_click_event overridden methods etc. */

	assert(event != NULL);

	static double x, y;
	static double drag_start_x, drag_start_y;
	double module_x, module_y; // FIXME: bad name, actually mouse click loc
	static bool dragging = false;
	bool handled = true;
	static bool double_click = false;
	
	module_x = event->button.x;
	module_y = event->button.y;

	property_parent().get_value()->w2i(module_x, module_y);
	
	switch (event->type) {

	case GDK_2BUTTON_PRESS:
		m_canvas->clear_selection();
		double_click = true;
		on_double_click(&event->button);
		handled = true;
		break;

	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {
			x = module_x;
			y = module_y;
			// Set these so we can tell on a button release if a drag actually
			// happened (if not, it's just a click)
			drag_start_x = x;
			drag_start_y = y;
			grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK,
			           Gdk::Cursor(Gdk::FLEUR),
			           event->button.time);
			dragging = true;
			handled = true;
		} else if (event->button.button == 2) {
			on_middle_click(&event->button);
			handled = true;
		} else if (event->button.button == 3) {
			on_right_click(&event->button);
		} else {
			handled = false;
		}
		break;
	
	case GDK_MOTION_NOTIFY:
		if ((dragging && (event->motion.state & GDK_BUTTON1_MASK))) {
			double new_x = module_x;
			double new_y = module_y;

			if (event->motion.is_hint) {
				gint t_x;
				gint t_y;
				GdkModifierType state;
				gdk_window_get_pointer(event->motion.window, &t_x, &t_y, &state);
				new_x = t_x;
				new_y = t_y;
			}

			// Move any other selected modules if we're selected
			if (m_selected) {
				for (list<Module*>::iterator i = m_canvas->selected_modules().begin();
						i != m_canvas->selected_modules().end(); ++i) {
					(*i)->move(new_x - x, new_y - y);
				}
			} else {
				move(new_x - x, new_y - y);
			}

			x = new_x;
			y = new_y;
		} else {
			handled = false;
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (dragging) {
			ungrab(event->button.time);
			dragging = false;
			if (module_x != drag_start_x || module_y != drag_start_y) { // dragged
				store_location();
			} else if (!double_click) { // just a single-click release
				if (m_selected) {
					m_canvas->unselect_module(this);
					assert(!m_selected);
				} else {
					if ( !(event->button.state & GDK_CONTROL_MASK))
						m_canvas->clear_selection();
					m_canvas->select_module(this);
					assert(m_selected);
				}
			}
		} else {
			handled = false;
		}
		double_click = false;
		break;

	case GDK_ENTER_NOTIFY:
		hilite(true);
		raise_to_top();
		for (PortList::iterator p = m_ports.begin(); p != m_ports.end(); ++p)
			(*p)->raise_connections();
		break;

	case GDK_LEAVE_NOTIFY:
		hilite(false);
		break;

	default:
		handled = false;
	}

	return handled;
	//return false;
}


void
Module::zoom(float z)
{
	m_canvas_title.property_size() = static_cast<int>(roundf(10000.0f * z));
	for (PortList::iterator p = m_ports.begin(); p != m_ports.end(); ++p)
		(*p)->zoom(z);
}


void
Module::hilite(bool b)
{
	if (b) {
		m_module_box.property_fill_color_rgba() = MODULE_HILITE_FILL_COLOUR;
	} else {
		m_module_box.property_fill_color_rgba() = MODULE_FILL_COLOUR;
	}
}


void
Module::selected(bool selected)
{
	m_selected = selected;
	if (selected) {
		m_module_box.property_fill_color_rgba() = MODULE_HILITE_FILL_COLOUR;
		m_module_box.property_outline_color_rgba() = MODULE_HILITE_OUTLINE_COLOUR;
		m_module_box.property_dash() = m_canvas->select_dash();
	} else {
		m_module_box.property_fill_color_rgba() = MODULE_FILL_COLOUR;
		m_module_box.property_outline_color_rgba() = MODULE_OUTLINE_COLOUR;
		m_module_box.property_dash() = NULL;
	}
}


bool
Module::is_within(const Gnome::Canvas::Rect* const rect)
{
	const double x1 = rect->property_x1();
	const double y1 = rect->property_y1();
	const double x2 = rect->property_x2();
	const double y2 = rect->property_y2();

	if (x1 < x2 && y1 < y2) {
		return (property_x() > x1
			&& property_y() > y1
			&& property_x() + width() < x2
			&& property_y() + height() < y2);
	} else if (x2 < x1 && y2 < y1) {
		return (property_x() > x2
			&& property_y() > y2
			&& property_x() + width() < x1
			&& property_y() + height() < y1);
	} else if (x1 < x2 && y2 < y1) {
		return (property_x() > x1
			&& property_y() > y2
			&& property_x() + width() < x2
			&& property_y() + height() < y1);
	} else if (x2 < x1 && y1 < y2) {
		return (property_x() > x2
			&& property_y() > y1
			&& property_x() + width() < x1
			&& property_y() + height() < y2);
	} else {
		return false;
	}
}


void
Module::remove_port(const string& port_name, bool resize_to_fit)
{
	for (PortList::iterator i = m_ports.begin(); i != m_ports.end(); ++i) {
		if ((*i)->name() == port_name) {
			delete (*i);
			i = m_ports.erase(i);
		}
	}

	if (resize_to_fit)
		resize();
}


void
Module::remove_all_ports(bool resize_to_fit)
{
	for (PortList::iterator i = m_ports.begin(); i != m_ports.end() ; ) {
		delete (*i);
		i = m_ports.erase(i);
	}

	if (resize_to_fit)
		resize();
}


void
Module::width(double w)
{
	m_width = w;
	m_module_box.property_x2() = m_module_box.property_x1() + w;
}


void
Module::height(double h)
{
	m_height = h;
	m_module_box.property_y2() = m_module_box.property_y1() + h;
}


/** Overloaded Group::move to update connection paths and keep module on the canvas */
void
Module::move(double dx, double dy)
{
	double new_x = property_x() + dx;
	double new_y = property_y() + dy;
	
	if (new_x < 0) dx = property_x() * -1;
	else if (new_x + m_width > m_canvas->width()) dx = m_canvas->width() - property_x() - m_width;
	
	if (new_y < 0) dy = property_y() * -1;
	else if (new_y + m_height > m_canvas->height()) dy = m_canvas->height() - property_y() - m_height;

	Gnome::Canvas::Group::move(dx, dy);

	// Deal with moving the connection lines
	for (PortList::iterator p = ports().begin(); p != ports().end(); ++p)
		(*p)->move_connections();
}


/** Move to the specified absolute coordinate on the canvas */
void
Module::move_to(double x, double y)
{
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x + m_width > m_canvas->width()) x = m_canvas->width() - m_width;
	if (y + m_height > m_canvas->height()) y = m_canvas->height() - m_height;
		
	// Man, not many things left to try to get the damn things to move! :)
	property_x() = x;
	property_y() = y;
	move(0, 0);
	// Deal with moving the connection lines
	for (PortList::iterator p = ports().begin(); p != ports().end(); ++p)
		(*p)->move_connections();
}


void
Module::name(const string& n)
{
	m_name = n;
	m_canvas_title.property_text() = m_name;
	resize();
}


void
Module::add_port(Port* p, bool resize_to_fit)
{
	m_ports.push_back(p);
	
	p->signal_event().connect(
		sigc::bind<Port*>(sigc::mem_fun(m_canvas, &FlowCanvas::port_event), p));
	
	if (resize_to_fit)
		resize();
}


/** Resize the module to fit its contents best.
 */
void
Module::resize()
{
	double widest_in = 0.0;
	double widest_out = 0.0;
	
	// The amount of space between a port edge and the module edge (on the
	// side that the port isn't right on the edge).
	double hor_pad = 5.0;
	if (m_name.length() == 0)
		hor_pad = 10.0; // leave more room for something to grab for dragging

	Port* p = NULL;
	
	// Find widest in/out ports
	for (PortList::iterator i = m_ports.begin(); i != m_ports.end(); ++i) {
		p = (*i);
		if (p->is_input() && p->width() > widest_in)
			widest_in = p->width();
		else if (p->is_output() && p->width() > widest_out)
			widest_out = p->width();
	}
	
	// Make sure module is wide enough for ports
	if (widest_in > widest_out)
		width(widest_in + hor_pad + border_width()*2.0);
	else
		width(widest_out + hor_pad + border_width()*2.0);
	
	// Make sure module is wide enough for title
	if (m_canvas_title.property_text_width() + 6.0 > m_width)
		width(m_canvas_title.property_text_width() + 6.0);

	// Set height to contain ports and title
	double height_base = 2;
	if (m_name.length() > 0)
		height_base += m_canvas_title.property_text_height();

	double h = height_base;
	if (m_ports.size() > 0)
		h += m_ports.size() * ((*m_ports.begin())->height()+2.0);
	height(h);
	
	// Move ports to appropriate locations
	
	double y;
	int i = 0;
	for (PortList::iterator pi = m_ports.begin(); pi != m_ports.end(); ++pi, ++i) {
		Port* p = (*pi);

		y = height_base + (i * (p->height() + 2.0));
		if (p->is_input()) {
			p->width(widest_in);
			p->property_x() = 1.0;//border_width();
			p->property_y() = y;
		} else {
			p->width(widest_out);
			p->property_x() = m_width - p->width() - 1.0;//p->border_width();
			p->property_y() = y;
		}
	}

	// Center title
	m_canvas_title.property_x() = m_width/2.0;

	// Update connection locations if we've moved/resized
	for (PortList::iterator pi = m_ports.begin(); pi != m_ports.end(); ++pi, ++i) {
		(*pi)->move_connections();
	}
	
	// Make things actually move to their new locations (?!)
	move(0, 0);
}


/** Port offset, for connection drawing.  See doc/port_offsets.dia */
double
Module::port_connection_point_offset(Port* port)
{
	assert(port->module() == this);
	assert(ports().size() > 0);

	return (port->connection_coords().get_y()
			- m_ports.front()->connection_coords().get_y());
}


/** Range of port offsets, for connection drawing.  See doc/port_offsets.dia */
double
Module::port_connection_points_range()
{
	assert(m_ports.size() > 0);

	double ret = fabs(m_ports.back()->connection_coords().get_y()
			- m_ports.front()->connection_coords().get_y());

	return (ret < 1.0) ? 1.0 : ret;
}


} // namespace LibFlowCanvas