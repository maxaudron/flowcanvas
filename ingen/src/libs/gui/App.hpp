/* This file is part of Ingen.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
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

#ifndef APP_H
#define APP_H

#include <cassert>
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <libgnomecanvasmm.h>
#include <gtkmm.h>
#include <libglademm.h>
#include <raul/RDFWorld.hpp>
#include <raul/SharedPtr.hpp>
using std::string; using std::map; using std::list;
using std::cerr; using std::endl;

namespace Ingen { 
	class Engine;
	namespace Shared {
		class EngineInterface;
	}
	namespace Client {
		class PatchModel;
		class PluginModel;
		class Store;
		class SigClientInterface;
	}
}
using namespace Ingen::Client;
using Ingen::Shared::EngineInterface;

/** \defgroup GUI GTK GUI
 */

namespace Ingen {
namespace GUI {

class MessagesWindow;
class ConfigWindow;
class PatchCanvas;
class PatchTreeView;
class PatchTreeWindow;
class ConnectWindow;
class Configuration;
class ThreadedLoader;
class WindowFactory;


/** Singleton master class most everything is contained within.
 *
 * This is a horrible god-object, but it's shrinking in size as things are
 * moved out.  Hopefully it will go away entirely some day..
 *
 * \ingroup GUI
 */
class App
{
public:
	~App();

	void error_message(const string& msg);

	void attach(const SharedPtr<EngineInterface>&    engine,
	            const SharedPtr<SigClientInterface>& client);
	
	void detach();
	
	void quit();

	ConnectWindow*   connect_window()       const { return _connect_window; }
	Gtk::Dialog*     about_dialog()         const { return _about_dialog; }
	ConfigWindow*    configuration_dialog() const { return _config_window; }
	MessagesWindow*  messages_dialog()      const { return _messages_window; }
	PatchTreeWindow* patch_tree()           const { return _patch_tree_window; }
	Configuration*   configuration()        const { return _configuration; }
	WindowFactory*   window_factory()       const { return _window_factory; }
	
	Raul::RDF::World* rdf_world() { return &_rdf_world; }

	const SharedPtr<EngineInterface>&    engine() const { return _engine; }
	const SharedPtr<SigClientInterface>& client() const { return _client; }
	const SharedPtr<Store>&              store()  const { return _store; }
	const SharedPtr<ThreadedLoader>&     loader() const { return _loader; }

	static inline App& instance() { assert(_instance); return *_instance; }

	static void run(int argc, char** argv,
			SharedPtr<Ingen::Engine> engine,
			SharedPtr<Shared::EngineInterface> interface);

protected:
	App();
	static App* _instance;
	
	static void instantiate(int argc, char** argv);

	SharedPtr<EngineInterface>    _engine;
	SharedPtr<SigClientInterface> _client;
	SharedPtr<Store>                _store;
	SharedPtr<ThreadedLoader>       _loader;

	Configuration*    _configuration;

	ConnectWindow*    _connect_window;
	MessagesWindow*   _messages_window;
	PatchTreeWindow*  _patch_tree_window;
	ConfigWindow*     _config_window;
	Gtk::Dialog*      _about_dialog;
	WindowFactory*    _window_factory;

	Raul::RDF::World _rdf_world;

	/** Used to avoid feedback loops with (eg) process checkbutton
	 * FIXME: Maybe this should be globally implemented at the Controller level,
	 * disable all command sending while handling events to avoid feedback
	 * issues with widget event callbacks?  This same pattern is duplicated
	 * too much... */
	bool _enable_signal;
};


} // namespace GUI
} // namespace Ingen

#endif // APP_H
