/* This file is part of Machina.
 * Copyright (C) 2007-2009 David Robillard <http://drobilla.net>
 *
 * Machina is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Machina is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Machina.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include "machina/MidiAction.hpp"
#include "NodeView.hpp"
#include "NodePropertiesWindow.hpp"

using namespace std;

NodeView::NodeView(Gtk::Window*                  window,
                   SharedPtr<FlowCanvas::Canvas> canvas,
                   SharedPtr<Machina::Node>      node,
                   const std::string&            name,
                   double                        x,
                   double                        y)
	: FlowCanvas::Ellipse(canvas, name, x, y, 20, 20, false)
	, _window(window)
	, _node(node)
	, _default_border_color(_border_color)
	, _old_color(_color)
{
	signal_clicked.connect(sigc::mem_fun(this, &NodeView::handle_click));
	update_state(false);
}


void
NodeView::on_double_click(GdkEventButton*)
{
	NodePropertiesWindow::present(_window, _node);
}


void
NodeView::handle_click(GdkEventButton* event)
{
	if (event->state & GDK_CONTROL_MASK) {
		if (event->button == 1) {
			bool is_initial = _node->is_initial();
			_node->set_initial( ! is_initial );
			update_state(_label != NULL);
		} else if (event->button == 3) {
			bool is_selector = _node->is_selector();
			_node->set_selector( ! is_selector );
			update_state(_label != NULL);
		}
	}
}


static std::string
midi_note_name(uint8_t num)
{
	static const char* notes[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

	const uint8_t octave = num / 12;
	num -= octave * 12;
	
	static char str[8];
	snprintf(str, sizeof(str), "%s%d", notes[num], octave);
	return str;
}

void
NodeView::show_label(bool show)
{
	SharedPtr<Machina::MidiAction> action
		= PtrCast<Machina::MidiAction>(_node->enter_action());

	if (show) {
		if (action && action->event_size() > 1
				&& (action->event()[0] & 0xF0) == 0x90) {
			const uint8_t note_num = action->event()[1];
			set_name(midi_note_name(note_num));
		}
	} else {
		set_name("");
	}
}


/// Dash style for selector node outlines
static ArtVpathDash* selector_dash()
{
	static ArtVpathDash* selector_dash = NULL;

	if (!selector_dash) {
		selector_dash = new ArtVpathDash();
		selector_dash->n_dash = 2;
		selector_dash->dash = art_new(double, 2);
		selector_dash->dash[0] = 8;
		selector_dash->dash[1] = 8;
	}

	return selector_dash;
}

void
NodeView::set_selected(bool selected)
{
	Ellipse::set_selected(selected);
	if (!selected)
		_ellipse.property_dash() = _node->is_selector() ? selector_dash() : 0;
}


void
NodeView::update_state(bool show_labels)
{
	static const uint32_t active_color = 0x408040FF;
	static const uint32_t active_border_color = 0x00FF00FF;

	if (_node->is_active()) {
		if (_color != active_color) {
			_old_color = _color;
			set_base_color(active_color);
			set_border_color(active_border_color);
		}
	} else if (_color == active_color) {
		set_base_color(_old_color);
		set_border_color(_default_border_color);
	}

	_ellipse.property_dash() = _node->is_selector() ? selector_dash() : 0;

	set_border_width(_node->is_initial() ? 4.0 : 1.0);

	if (show_labels)
		show_label(true);
}

