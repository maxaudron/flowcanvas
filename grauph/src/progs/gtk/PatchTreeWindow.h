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

#ifndef PATCHTREEWINDOW_H
#define PATCHTREEWINDOW_H

#include <gtkmm.h>
#include <libglademm.h>
#include "util/Path.h"


using namespace Om;
using Om::Path;

namespace OmGtk {

class PatchWindow;
class PatchController;
class PatchTreeView;


/** Window with a TreeView of all loaded patches.
 *
 * \ingroup OmGtk
 */
class PatchTreeWindow : public Gtk::Window
{
public:
	PatchTreeWindow(BaseObjectType* cobject, const Glib::RefPtr<Gnome::Glade::Xml>& refGlade);

	void patch_enabled(const Path& path);
	void patch_disabled(const Path& path);
	void patch_renamed(const Path& old_path, const Path& new_path);

	void add_patch(PatchController* pc);
	void remove_patch(const Path& path);
	void show_patch_menu(GdkEventButton* ev);

protected:
	//void event_patch_selected();
	void event_patch_activated(const Gtk::TreeModel::Path& path, Gtk::TreeView::Column* col);
	void event_patch_enabled_toggled(const Glib::ustring& path_str);

	Gtk::TreeModel::iterator find_patch(Gtk::TreeModel::Children root, const Path& path);
	
	PatchTreeView* m_patches_treeview;

	struct PatchTreeModelColumns : public Gtk::TreeModel::ColumnRecord
	{
		PatchTreeModelColumns()
		{ add(name_col); add(enabled_col); add(patch_controller_col); }
		
		Gtk::TreeModelColumn<Glib::ustring>    name_col;
		Gtk::TreeModelColumn<bool>             enabled_col;
		Gtk::TreeModelColumn<PatchController*> patch_controller_col;
	};

	bool                             m_enable_signal;
	PatchTreeModelColumns            m_patch_tree_columns;
	Glib::RefPtr<Gtk::TreeStore>     m_patch_treestore;
	Glib::RefPtr<Gtk::TreeSelection> m_patch_tree_selection;
};


/** Derived TreeView class to support context menus for patches */
class PatchTreeView : public Gtk::TreeView
{
public:
	PatchTreeView(BaseObjectType* cobject, const Glib::RefPtr<Gnome::Glade::Xml>& xml)
	: Gtk::TreeView(cobject)
	{}

	void set_window(PatchTreeWindow* win) { m_window = win; }
	
	bool on_button_press_event(GdkEventButton* ev) {
		bool ret = Gtk::TreeView::on_button_press_event(ev);
	
		if ((ev->type == GDK_BUTTON_PRESS) && (ev->button == 3))
			m_window->show_patch_menu(ev);

		return ret;
	}
	
private:
	PatchTreeWindow* m_window;

}; // struct PatchTreeView

	
} // namespace OmGtk

#endif // PATCHTREEWINDOW_H