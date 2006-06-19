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

#include "NodeController.h"
#include <iostream>
#include <gtkmm.h>
#include "OmModule.h"
#include "NodeModel.h"
#include "PortModel.h"
#include "PortController.h"
#include "GtkObjectController.h"
#include "NodeControlWindow.h"
#include "OmModule.h"
#include "PatchController.h"
#include "OmFlowCanvas.h"
#include "RenameWindow.h"
#include "GladeFactory.h"
#include "Controller.h"
#include "PatchWindow.h"
#include "PatchModel.h"
#include "NodePropertiesWindow.h"
#include "Store.h"
using std::cerr; using std::endl;

namespace OmGtk {


NodeController::NodeController(CountedPtr<NodeModel> model)
: GtkObjectController(model),
  m_controls_menuitem(NULL),
  m_module(NULL),
  m_control_window(NULL),
  m_properties_window(NULL),
  m_bridge_port(NULL)
{
	assert(!model->controller());
	model->set_controller(this);

	// Create port controllers
	for (PortModelList::const_iterator i = node_model()->ports().begin();
			i != node_model()->ports().end(); ++i) {
		assert(!(*i)->controller());
		assert((*i)->parent());
		assert((*i)->parent().get() == node_model().get());
		// FIXME: leak
		PortController* const pc = new PortController(*i);
		assert((*i)->controller() == pc); // PortController() does this
	}
	
	// Build menu
	
	Gtk::Menu::MenuList& items = m_menu.items();
	
	Gtk::Menu_Helpers::MenuElem controls_elem
		= Gtk::Menu_Helpers::MenuElem("Controls",
		sigc::mem_fun(this, &NodeController::show_control_window));
	m_controls_menuitem = controls_elem.get_child();
	items.push_back(controls_elem);
	items.push_back(Gtk::Menu_Helpers::MenuElem("Properties",
		sigc::mem_fun(this, &NodeController::show_properties_window)));
	items.push_back(Gtk::Menu_Helpers::SeparatorElem());
	items.push_back(Gtk::Menu_Helpers::MenuElem("Rename...",
		sigc::mem_fun(this, &NodeController::show_rename_window)));
	items.push_back(Gtk::Menu_Helpers::MenuElem("Clone",
		sigc::mem_fun(this, &NodeController::on_menu_clone)));
	items.push_back(Gtk::Menu_Helpers::MenuElem("Disconnect All",
		sigc::mem_fun(this, &NodeController::on_menu_disconnect_all)));
	items.push_back(Gtk::Menu_Helpers::MenuElem("Destroy",
		sigc::mem_fun(this, &NodeController::on_menu_destroy)));
	
	m_controls_menuitem->property_sensitive() = false;
	
	if (node_model()->plugin() && node_model()->plugin()->type() == PluginModel::Internal
			&& node_model()->plugin()->plug_label() == "midi_control_in") {
		Gtk::Menu::MenuList& items = m_menu.items();
		items.push_back(Gtk::Menu_Helpers::MenuElem("Learn",
			sigc::mem_fun(this, &NodeController::on_menu_learn)));
	}

	model->new_port_sig.connect(sigc::mem_fun(this, &NodeController::add_port));
}


NodeController::~NodeController()
{
}


void
NodeController::create_module(OmFlowCanvas* canvas)
{
	//cerr << "Creating node module " << m_model->path() << endl;

	// If this is a DSSI plugin, DSSIController should be doing this
	/*assert(node_model()->plugin());
	assert(node_model()->plugin()->type() != PluginModel::DSSI);
	assert(canvas != NULL);
	assert(m_module == NULL);*/
	
	assert(canvas);
	assert(node_model());
	m_module = new OmModule(canvas, this);
	
	create_all_ports();

	m_module->move_to(node_model()->x(), node_model()->y());
}


void
NodeController::set_path(const Path& new_path)
{
	cerr << "FIXME: rename\n";
	/*
	remove_from_store();
	
	// Rename ports
	for (list<PortModel*>::const_iterator i = node_model()->ports().begin();
			i != node_model()->ports().end(); ++i) {
		GtkObjectController* const pc = (GtkObjectController*)((*i)->controller());
		assert(pc != NULL);
		pc->set_path(m_model->path().base_path() + pc->model()->name());
	}

	// Handle bridge port, if this node represents one
	if (m_bridge_port != NULL)
		m_bridge_port->set_path(new_path);

	if (m_module != NULL)
		m_module->canvas()->rename_module(node_model()->path().name(), new_path.name());
	
	GtkObjectController::set_path(new_path);
	
	add_to_store();
	*/
}


void
NodeController::destroy()
{
	PatchController* pc = ((PatchController*)m_model->parent()->controller());
	assert(pc != NULL);

	//remove_from_store();
	//pc->remove_node(m_model->path().name());
	cerr << "FIXME: remove node\n";

	if (m_bridge_port != NULL)
		m_bridge_port->destroy();
	m_bridge_port = NULL;

	//if (m_module != NULL)
	//	delete m_module;
}


void
NodeController::metadata_update(const string& key, const string& value)
{
	//cout << "[NodeController] Metadata update: " << m_model->path() << endl;

	if (m_module != NULL) {
		if (key == "module-x") {
			float x = atof(value.c_str());
			//if (x > 0 && x < m_canvas->width())
				m_module->move_to(x, m_module->property_y().get_value());
		} else if (key == "module-y") {
			float y = atof(value.c_str());
			//if (y > 0 && y < m_canvas->height())
				m_module->move_to(m_module->property_x().get_value(), y);
		}
	}

	if (m_bridge_port != NULL)
		m_bridge_port->metadata_update(key, value);

	GtkObjectController::metadata_update(key, value);
}


void
NodeController::add_port(CountedPtr<PortModel> pm)
{
	assert(pm->parent().get() == node_model().get());
	assert(pm->parent() == node_model());
	assert(node_model()->get_port(pm->name()) == pm);
	
	cout << "[NodeController] Adding port " << pm->path() << endl;
	
	// FIXME: leak
	PortController* pc = new PortController(pm);
	assert(pm->controller() == pc);
	//pc->add_to_store();
	
	if (m_module != NULL) {
		pc->create_port(m_module);
		m_module->resize();
		
		// Enable "Controls" menu item on module
		if (has_control_inputs())
			enable_controls_menuitem();
	}
		
	if (m_control_window != NULL) {
		assert(m_control_window->control_panel() != NULL);
		m_control_window->control_panel()->add_port(pc);
		m_control_window->resize();
	}
}


void
NodeController::show_control_window()
{
	size_t poly = 1;
	if (node_model()->polyphonic())
		poly = ((PatchModel*)node_model()->parent().get())->poly();
	
	if (!m_control_window)
		m_control_window = new NodeControlWindow(this, poly);
	
	if (m_control_window->control_panel()->num_controls() > 0)
		m_control_window->present();
}


void
NodeController::on_menu_destroy()
{
	Controller::instance().destroy(node_model()->path());
}


void
NodeController::show_rename_window()
{
	assert(node_model()->parent());

	// FIXME: will this be magically cleaned up?
	RenameWindow* win = NULL;
	Glib::RefPtr<Gnome::Glade::Xml> xml = GladeFactory::new_glade_reference("rename_win");
	xml->get_widget_derived("rename_win", win);
	
	PatchController* parent = ((PatchController*)node_model()->parent()->controller());
	assert(parent != NULL);
	
	if (parent->window() != NULL)
		win->set_transient_for(*parent->window());

	win->set_object(this);
	win->show();
}

void
NodeController::on_menu_clone()
{
	cerr << "FIXME: clone broken\n";
	/*
	assert(node_model());
	//assert(m_parent != NULL);
	//assert(m_parent->model() != NULL);
	
	string clone_name = node_model()->name();
	int i = 2; // postfix number (ie oldname_2)
	
	// Check if name already has a number postfix
	if (clone_name.find_last_of("_") != string::npos) {
		string name_postfix = clone_name.substr(clone_name.find_last_of("_")+1);
		clone_name = clone_name.substr(0, clone_name.find_last_of("_"));
		if (sscanf(name_postfix.c_str(), "%d", &i))
			++i;
	}
	
	char clone_postfix[4];
	for ( ; i < 100; ++i) {
		snprintf(clone_postfix, 4, "_%d", i);
		if (node_model()->parent_patch()->get_node(clone_name + clone_postfix) == NULL)
			break;
	}
	
	clone_name = clone_name + clone_postfix;
	
	const string path = node_model()->parent_patch()->base_path() + clone_name;
	NodeModel* nm = new NodeModel(node_model()->plugin(), path);
	nm->polyphonic(node_model()->polyphonic());
	nm->x(node_model()->x() + 20);
	nm->y(node_model()->y() + 20);
	Controller::instance().create_node_from_model(nm);
	*/
}


void
NodeController::on_menu_learn()
{
	Controller::instance().midi_learn(node_model()->path());
}

void
NodeController::on_menu_disconnect_all()
{
	Controller::instance().disconnect_all(node_model()->path());
}


void
NodeController::show_properties_window()
{
	PatchController* parent = ((PatchController*)node_model()->parent()->controller());
	assert(parent != NULL);
	
	if (m_properties_window == NULL) {
		Glib::RefPtr<Gnome::Glade::Xml> xml = GladeFactory::new_glade_reference("node_properties_win");
		xml->get_widget_derived("node_properties_win", m_properties_window);
	}
	assert(m_properties_window != NULL);
	assert(parent != NULL);
	m_properties_window->set_node(node_model());
	if (parent->window() != NULL)
		m_properties_window->set_transient_for(*parent->window());
	m_properties_window->show();
}


/** Create all (visual) ports and add them to module (and resize it).
 */
void
NodeController::create_all_ports()
{
	assert(m_module != NULL);

	PortController* pc = NULL;
	for (PortModelList::const_iterator i = node_model()->ports().begin();
			i != node_model()->ports().end(); ++i) {
		pc = dynamic_cast<PortController*>((*i)->controller());
		// FIXME: leak
		if (pc == NULL)
			pc = new PortController(*i);
		pc->create_port(m_module);
	}

	m_module->resize();
	
	if (has_control_inputs())
		enable_controls_menuitem();
}


bool
NodeController::has_control_inputs()
{
	for (PortModelList::const_iterator i = node_model()->ports().begin();
			i != node_model()->ports().end(); ++i)
		if ((*i)->is_input() && (*i)->is_control())
			return true;
	
	return false;
}


void
NodeController::enable_controls_menuitem()
{
	m_controls_menuitem->property_sensitive() = true;
}


void
NodeController::disable_controls_menuitem()
{
	m_controls_menuitem->property_sensitive() = false;
	
	if (m_control_window != NULL)
		m_control_window->hide();
}



} // namespace OmGtk
