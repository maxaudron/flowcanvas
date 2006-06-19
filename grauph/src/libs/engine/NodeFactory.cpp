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

#include "NodeFactory.h"
#include "config.h"
#include <cstdlib>
#include <pthread.h>
#include <dirent.h>
#include <float.h>
#include <cmath>
#include <dlfcn.h>
#include "AudioDriver.h"
#include "MidiNoteNode.h"
#include "MidiTriggerNode.h"
#include "MidiControlNode.h"
#if 0
#include "AudioInputNode.h"
#include "AudioOutputNode.h"
#include "ControlInputNode.h"
#include "ControlOutputNode.h"
#include "MidiInputNode.h"
#include "MidiOutputNode.h"
#endif
#include "TransportNode.h"
#include "PluginLibrary.h"
#include "Plugin.h"
#include "Patch.h"
#include "Om.h"
#include "OmApp.h"
#ifdef HAVE_SLV2
#include "LV2Node.h"
#include <slv2/slv2.h>
#endif
#ifdef HAVE_LADSPA
#include "LADSPANode.h"
#endif
#ifdef HAVE_DSSI
#include "DSSINode.h"
#endif

using std::string;
using std::cerr; using std::cout; using std::endl;


namespace Om {


/* I am perfectly aware that the vast majority of this class is a 
 * vomit inducing nightmare at the moment ;)
 */



NodeFactory::NodeFactory()
: m_has_loaded(false)
{
	pthread_mutex_init(&m_plugin_list_mutex, NULL);
	
	// Add builtin plugin types to m_internal_plugins list
	// FIXME: ewwww, definitely a better way to do this!
	//Plugin* pi = NULL;

	Patch* parent = new Patch("dummy", 1, NULL, 1, 1, 1);

	Node* n = NULL;
#if 0
	n = new AudioInputNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
	n = new AudioOutputNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
	n = new ControlInputNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
	n = new ControlOutputNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
	n = new MidiInputNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
	n = new MidiOutputNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
#endif
	n = new MidiNoteNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
	n = new MidiTriggerNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
	n = new MidiControlNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
	n = new TransportNode("foo", 1, parent, 1, 1);
	m_internal_plugins.push_back(new Plugin(n->plugin()));
	delete n;
	

	delete parent;
}


NodeFactory::~NodeFactory()
{
	for (list<Plugin*>::iterator i = m_plugins.begin(); i != m_plugins.end(); ++i)
		delete (*i);
	
	for (list<PluginLibrary*>::iterator i = m_libraries.begin(); i != m_libraries.end(); ++i) {
		(*i)->close();
		delete (*i);
	}
}


void
NodeFactory::load_plugins()
{
	// Only load if we havn't already, so every client connecting doesn't cause
	// this (expensive!) stuff to happen.  Not the best solution - would be nice
	// if clients could refresh plugins list for whatever reason :/
	if (!m_has_loaded) {
		pthread_mutex_lock(&m_plugin_list_mutex);
		
		m_plugins.clear();
		m_plugins = m_internal_plugins;
	
#if HAVE_SLV2
		load_lv2_plugins();
#endif
#if HAVE_DSSI
		load_dssi_plugins();
#endif
#if HAVE_LADSPA
		load_ladspa_plugins();
#endif
		
		m_has_loaded = true;
		
		pthread_mutex_unlock(&m_plugin_list_mutex);
	}
}


/** Loads a plugin.
 *
 * Calls the load_*_plugin functions to actually do things, just a wrapper.
 */
Node*
NodeFactory::load_plugin(const Plugin* a_plugin, const string& name, size_t poly, Patch* parent)
{
	assert(parent != NULL);
	assert(poly == 1 || poly == parent->internal_poly());
	assert(a_plugin);

	pthread_mutex_lock(&m_plugin_list_mutex);
	
	Node* r = NULL;
	Plugin* plugin = NULL;

	// Attempt to find the plugin in loaded DB
	if (a_plugin->type() != Plugin::Internal) {
		list<Plugin*>::iterator i;
		if (a_plugin->plug_label().length() == 0) {
			for (i = m_plugins.begin(); i != m_plugins.end(); ++i) {
				if (a_plugin->uri() == (*i)->uri()) {
					plugin = *i;
					break;
				}
			}
		} else {
			for (i = m_plugins.begin(); i != m_plugins.end(); ++i) {
				if (a_plugin->uri() == (*i)->uri()) {
					plugin = *i;
					break;
				}
			}
		}

		if (plugin == NULL)
			return NULL;
	}

	switch (a_plugin->type()) {
#if HAVE_SLV2
	case Plugin::LV2:
		r = load_lv2_plugin(plugin->uri(), name, poly, parent);
		break;
#endif
#if HAVE_DSSI
	case Plugin::DSSI:
		r = load_dssi_plugin(plugin->uri(), name, poly, parent);
		break;
#endif
#if HAVE_LADSPA
	case Plugin::LADSPA:
		r = load_ladspa_plugin(plugin->uri(), name, poly, parent);
		break;
#endif
	case Plugin::Internal:
		r = load_internal_plugin(a_plugin->uri(), name, poly, parent);
		break;
	default:
		cerr << "[NodeFactory] WARNING: Unknown plugin type." << endl;
	}

	pthread_mutex_unlock(&m_plugin_list_mutex);

	return r;
}


/** Loads an internal plugin.
 */
Node*
NodeFactory::load_internal_plugin(const string& uri, const string& name, size_t poly, Patch* parent)
{
	assert(parent != NULL);
	assert(poly == 1 || poly == parent->internal_poly());
	assert(uri.length() > 3);
	assert(uri.substr(0, 3) == "om:");

	string plug_label = uri.substr(3);
#if 0
	if (plug_label == "midi_input") {
		MidiInputNode* tn = new MidiInputNode(name, 1, parent, om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return tn;
	} else if (plug_label == "midi_output") {
		MidiOutputNode* tn = new MidiOutputNode(name, 1, parent, om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return tn;
	} else if (plug_label == "audio_input") {
		AudioInputNode* in = new AudioInputNode(name, poly, parent,
			om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return in;
	} else if (plug_label == "control_input") {
		ControlInputNode* in = new ControlInputNode(name, poly, parent,
			om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return in;
	} else if (plug_label == "audio_output") {
		AudioOutputNode* on = new AudioOutputNode(name, poly, parent,
			om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return on;
	} else if (plug_label == "control_output") {
		ControlOutputNode* on = new ControlOutputNode(name, poly, parent,
			om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return on;
	} else
#endif
	if (plug_label == "note_in" || plug_label == "midi_note_in") {
		MidiNoteNode* mn = new MidiNoteNode(name, poly, parent, om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return mn;
	} else if (plug_label == "trigger_in" || plug_label == "midi_trigger_in") {
		MidiTriggerNode* mn = new MidiTriggerNode(name, 1, parent, om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return mn;
	} else if (plug_label == "midi_control_in") {
		MidiControlNode* mn = new MidiControlNode(name, 1, parent, om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return mn;
	} else if (plug_label == "transport") {
		TransportNode* tn = new TransportNode(name, 1, parent, om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		return tn;
	} else {
		cerr << "Unknown internal plugin type '" << plug_label << "'" << endl;
	}

	return NULL;
}


#ifdef HAVE_SLV2

/** Loads information about all LV2 plugins into internal plugin database.
 */
void
NodeFactory::load_lv2_plugins()
{
	SLV2List plugins = slv2_list_new();
	slv2_list_load_all(plugins);

	//cerr << "[NodeFactory] Found " << slv2_list_get_length(plugins) << " LV2 plugins." << endl;
	
	for (unsigned long i=0; i < slv2_list_get_length(plugins); ++i) {
		
		SLV2Plugin* lv2_plug = slv2_list_get_plugin_by_index(plugins, i);
		
		
		//om_plug->library(plugin_library);
		
		const char* uri = (const char*)slv2_plugin_get_uri(lv2_plug);
		//cerr << "LV2 plugin: " << uri << endl;

		bool found = false;
		for (list<Plugin*>::const_iterator i = m_plugins.begin(); i != m_plugins.end(); ++i) {
			if (!strcmp((*i)->uri().c_str(), uri)) {
				cerr << "Warning: Duplicate LV2 plugin (" << uri << ").\nUsing "
					<< (*i)->lib_path() << " version." << endl;
				found = true;
				break;
			}
		}
		if (!found) {
			//printf("[NodeFactory] Found LV2 plugin %s\n", uri);
			Plugin* om_plug = new Plugin();
			om_plug->type(Plugin::LV2);
			om_plug->slv2_plugin(lv2_plug);
			om_plug->uri(uri);
			// FIXME FIXME FIXME temporary hack
			om_plug->library(NULL);
			om_plug->lib_path("FIXMEpath");
			om_plug->plug_label("FIXMElabel");
			unsigned char* name = slv2_plugin_get_name(lv2_plug);
			om_plug->name((char*)name);
			free(name);
			om_plug->type(Plugin::LV2);
			m_plugins.push_back(om_plug);
		}
	}
	
	slv2_list_free(plugins);
}


/** Loads a LV2 plugin.
 * Returns 'poly' independant plugins as a Node*
 */
Node*
NodeFactory::load_lv2_plugin(const string& plug_uri,
                             const string& node_name,
                             size_t        poly,
                             Patch*        parent)
{
	// Find (Om) Plugin
	Plugin* plugin = NULL;
	list<Plugin*>::iterator i;
	for (i = m_plugins.begin(); i != m_plugins.end(); ++i) {
		plugin = (*i);
		if ((*i)->uri() == plug_uri) break;
	}
	
	Node* n = NULL;

	if (plugin) {
		n = new Om::LV2Node(plugin, node_name, poly, parent,
			om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
		bool success = ((LV2Node*)n)->instantiate();
		if (!success) {
			delete n;
			n = NULL;
		}
	}
	
	return n;
}

#endif // HAVE_SLV2


#if HAVE_DSSI

/** Loads information about all DSSI plugins into internal plugin database.
 */
void
NodeFactory::load_dssi_plugins()
{
	// FIXME: too much code duplication with load_ladspa_plugin
	
	char* env_dssi_path = getenv("DSSI_PATH");
	string dssi_path;
	if (!env_dssi_path) {
	 	cerr << "[NodeFactory] DSSI_PATH is empty.  Assuming /usr/lib/dssi:/usr/local/lib/dssi:~/.dssi" << endl;
		dssi_path = string("/usr/lib/dssi:/usr/local/lib/dssi:").append(
			getenv("HOME")).append("/.dssi");
	} else {
		dssi_path = env_dssi_path;
	}

	DSSI_Descriptor_Function df         = NULL;
	DSSI_Descriptor*         descriptor = NULL;
	
	string dir;
	string full_lib_name;
	
	// Yep, this should use an sstream alright..
	while (dssi_path != "") {
		dir = dssi_path.substr(0, dssi_path.find(':'));
		if (dssi_path.find(':') != string::npos)
			dssi_path = dssi_path.substr(dssi_path.find(':')+1);
		else
			dssi_path = "";
	
		DIR* pdir = opendir(dir.c_str());
		if (pdir == NULL) {
			//cerr << "[NodeFactory] Unreadable directory in DSSI_PATH: " << dir.c_str() << endl;
			continue;
		}

		struct dirent* pfile;
		while ((pfile = readdir(pdir))) {
			
			if (!strcmp(pfile->d_name, ".") || !strcmp(pfile->d_name, ".."))
				continue;
			
			full_lib_name = dir +"/"+ pfile->d_name;
			
			// Load descriptor function
			// Loaded with LAZY here, will be loaded with NOW on node loading
			void* handle = dlopen(full_lib_name.c_str(), RTLD_LAZY);
			if (handle == NULL)
				continue;
			
			df = (DSSI_Descriptor_Function)dlsym(handle, "dssi_descriptor");
			if (df == NULL) {
				// Not a DSSI plugin library
				dlclose(handle);
				continue;
			}

			PluginLibrary* plugin_library = new PluginLibrary(full_lib_name);
			m_libraries.push_back(plugin_library);

			const LADSPA_Descriptor* ld = NULL;
			
			for (unsigned long i=0; (descriptor = (DSSI_Descriptor*)df(i)) != NULL; ++i) {
				ld = descriptor->LADSPA_Plugin;
				assert(ld != NULL);
				Plugin* plugin = new Plugin();
				assert(plugin_library != NULL);
				plugin->library(plugin_library);
				plugin->lib_path(dir + "/" + pfile->d_name);
				plugin->plug_label(ld->Label);
				plugin->name(ld->Name);
				plugin->type(Plugin::DSSI);
				plugin->id(ld->UniqueID);

				bool found = false;
				for (list<Plugin*>::const_iterator i = m_plugins.begin(); i != m_plugins.end(); ++i) {
					if ((*i)->uri() == plugin->uri()) {
						cerr << "Warning: Duplicate DSSI plugin (" << plugin->lib_name() << ":"
							<< plugin->plug_label() << ")" << " found.\nUsing " << (*i)->lib_path()
							<< " version." << endl;
						found = true;
						break;
					}
				}
				if (!found)
					m_plugins.push_back(plugin);
				else
					delete plugin;
			}
			
			df = NULL;
			descriptor = NULL;
			dlclose(handle);
		}
		closedir(pdir);
	}
}


/** Creates a Node by instancing a DSSI plugin.
 */
Node*
NodeFactory::load_dssi_plugin(const string& uri,
                              const string& name, size_t poly, Patch* parent)
{
	// FIXME: awful code duplication here
	
	assert(uri != "");
	assert(name != "");
	assert(poly > 0);
	
	DSSI_Descriptor_Function df = NULL;
	const Plugin* plugin = NULL;
	Node* n = NULL;
	void* handle = NULL;
	
	// Attempt to find the lib
	list<Plugin*>::iterator i;
	for (i = m_plugins.begin(); i != m_plugins.end(); ++i) {
		plugin = (*i);
		if (plugin->uri() == uri) break;
	}

	assert(plugin->id() != 0);

	if (i == m_plugins.end()) {
		cerr << "Did not find DSSI plugin " << uri << " in database." << endl;
		return NULL;
	} else {
		assert(plugin != NULL);
		plugin->library()->open();
		handle = plugin->library()->handle();
		assert(handle != NULL);	
		
		// Load descriptor function
		dlerror();
		df = (DSSI_Descriptor_Function)dlsym(handle, "dssi_descriptor");
		if (df == NULL || dlerror() != NULL) {
			cerr << "Looks like this isn't a DSSI plugin." << endl;
			return NULL;
		}
	}

	// Attempt to find the plugin in lib
	DSSI_Descriptor* descriptor = NULL;
	for (unsigned long i=0; (descriptor = (DSSI_Descriptor*)df(i)) != NULL; ++i) {
		if (descriptor->LADSPA_Plugin != NULL
				&& descriptor->LADSPA_Plugin->UniqueID == plugin->id()) {
			break;
		}
	}
	
	if (descriptor == NULL) {
		cerr << "Could not find plugin \"" << plugin->id() << "\" in " << plugin->lib_name() << endl;
		return NULL;
	}

	n = new DSSINode(plugin, name, poly, parent, descriptor,
		om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
	bool success = ((DSSINode*)n)->instantiate();
	if (!success) {
		delete n;
		n = NULL;
	}

	return n;
}
#endif // HAVE_DSSI


#ifdef HAVE_LADSPA
/** Loads information about all LADSPA plugins into internal plugin database.
 */
void
NodeFactory::load_ladspa_plugins()
{
	char* env_ladspa_path = getenv("LADSPA_PATH");
	string ladspa_path;
	if (!env_ladspa_path) {
	 	cerr << "[NodeFactory] LADSPA_PATH is empty.  Assuming /usr/lib/ladspa:/usr/local/lib/ladspa:~/.ladspa" << endl;
		ladspa_path = string("/usr/lib/ladspa:/usr/local/lib/ladspa:").append(
			getenv("HOME")).append("/.ladspa");
	} else {
		ladspa_path = env_ladspa_path;
	}

	LADSPA_Descriptor_Function df         = NULL;
	LADSPA_Descriptor*         descriptor = NULL;
	
	string dir;
	string full_lib_name;
	
	// Yep, this should use an sstream alright..
	while (ladspa_path != "") {
		dir = ladspa_path.substr(0, ladspa_path.find(':'));
		if (ladspa_path.find(':') != string::npos)
			ladspa_path = ladspa_path.substr(ladspa_path.find(':')+1);
		else
			ladspa_path = "";
	
		DIR* pdir = opendir(dir.c_str());
		if (pdir == NULL) {
			//cerr << "[NodeFactory] Unreadable directory in LADSPA_PATH: " << dir.c_str() << endl;
			continue;
		}

		struct dirent* pfile;
		while ((pfile = readdir(pdir))) {
			
			if (!strcmp(pfile->d_name, ".") || !strcmp(pfile->d_name, ".."))
				continue;
			
			full_lib_name = dir +"/"+ pfile->d_name;
			
			// Load descriptor function
			// Loaded with LAZY here, will be loaded with NOW on node loading
			void* handle = dlopen(full_lib_name.c_str(), RTLD_LAZY);
			if (handle == NULL)
				continue;
			
			df = (LADSPA_Descriptor_Function)dlsym(handle, "ladspa_descriptor");
			if (df == NULL) {
				dlclose(handle);
				continue;
			}	

			PluginLibrary* plugin_library = new PluginLibrary(full_lib_name);
			m_libraries.push_back(plugin_library);

			for (unsigned long i=0; (descriptor = (LADSPA_Descriptor*)df(i)) != NULL; ++i) {
				Plugin* plugin = new Plugin();
				assert(plugin_library != NULL);
				plugin->library(plugin_library);
				plugin->lib_path(dir + "/" + pfile->d_name);
				plugin->plug_label(descriptor->Label);
				plugin->name(descriptor->Name);
				plugin->type(Plugin::LADSPA);
				plugin->id(descriptor->UniqueID);
				
				bool found = false;
				for (list<Plugin*>::const_iterator i = m_plugins.begin(); i != m_plugins.end(); ++i) {
					if ((*i)->uri() == plugin->uri()) {
						cerr << "Warning: Duplicate LADSPA plugin " << plugin->uri()
							<< " found.\nChoosing " << (*i)->lib_path()
							<< " over " << plugin->lib_path() << endl;
						found = true;
						break;
					}
				}
				if (!found)
					m_plugins.push_back(plugin);
				else
					delete plugin;
			}
			
			df = NULL;
			descriptor = NULL;
			dlclose(handle);
		}
		closedir(pdir);
	}
}


/** Loads a LADSPA plugin.
 * Returns 'poly' independant plugins as a Node*
 */
Node*
NodeFactory::load_ladspa_plugin(const string& uri,
                                const string& name, size_t poly, Patch* parent)
{
	assert(uri != "");
	assert(name != "");
	assert(poly > 0);
	
	LADSPA_Descriptor_Function df = NULL;
	Plugin* plugin = NULL;
	Node* n = NULL;
	void* plugin_lib = NULL;
	
	// Attempt to find the lib
	list<Plugin*>::iterator i;
	for (i = m_plugins.begin(); i != m_plugins.end(); ++i) {
		plugin = (*i);
		if (plugin->uri() == uri) break;
	}

	assert(plugin->id() != 0);

	if (i == m_plugins.end()) {
		cerr << "Did not find LADSPA plugin " << uri << " in database." << endl;
		return NULL;
	} else {
		assert(plugin != NULL);
		plugin->library()->open();
		plugin_lib = plugin->library()->handle();
		assert(plugin_lib != NULL);	
		
		// Load descriptor function
		dlerror();
		df = (LADSPA_Descriptor_Function)dlsym(plugin_lib, "ladspa_descriptor");
		if (df == NULL || dlerror() != NULL) {
			cerr << "Looks like this isn't a LADSPA plugin." << endl;
			return NULL;
		}
	}

	// Attempt to find the plugin in lib
	LADSPA_Descriptor* descriptor = NULL;
	for (unsigned long i=0; (descriptor = (LADSPA_Descriptor*)df(i)) != NULL; ++i) {
		if (descriptor->UniqueID == plugin->id()) {
			break;
		}
	}
	
	if (descriptor == NULL) {
		cerr << "Could not find plugin \"" << plugin->id() << "\" in " << plugin->lib_path() << endl;
		return NULL;
	}

	n = new LADSPANode(plugin, name, poly, parent, descriptor,
		om->audio_driver()->sample_rate(), om->audio_driver()->buffer_size());
	bool success = ((LADSPANode*)n)->instantiate();
	if (!success) {
		delete n;
		n = NULL;
	}
	
	return n;
}


#endif // HAVE_LADSPA


} // namespace Om