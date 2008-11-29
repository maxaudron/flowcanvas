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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib> // atof
#include <cstring>
#include <fstream>
#include <iostream>
#include <locale.h>
#include <stdexcept>
#include <string>
#include <utility> // pair, make_pair
#include <vector>
#include "raul/Atom.hpp"
#include "raul/AtomRDF.hpp"
#include "raul/Path.hpp"
#include "raul/TableImpl.hpp"
#include "redlandmm/Model.hpp"
#include "redlandmm/Node.hpp"
#include "redlandmm/World.hpp"
#include "module/World.hpp"
#include "interface/EngineInterface.hpp"
#include "interface/Plugin.hpp"
#include "interface/Patch.hpp"
#include "interface/Node.hpp"
#include "interface/Port.hpp"
#include "interface/Connection.hpp"
#include "Serialiser.hpp"

using namespace std;
using namespace Raul;
using namespace Redland;
using namespace Ingen;
using namespace Ingen::Shared;

namespace Ingen {
namespace Serialisation {


Serialiser::Serialiser(Shared::World& world, SharedPtr<Shared::Store> store)
	: _root_path("/")
	, _store(store)
	, _world(*world.rdf_world)
{
}
	

void
Serialiser::to_file(SharedPtr<GraphObject> object, const string& filename)
{
	_root_path = object->path();
	start_to_filename(filename);
	serialise(object);
	finish();
}


	
string
Serialiser::to_string(SharedPtr<GraphObject>        object,
	                  const string&                 base_uri,
	                  const GraphObject::Variables& extra_rdf)
{
	start_to_string(object->path(), base_uri);
	serialise(object);
	
	Redland::Node base_rdf_node(_model->world(), Redland::Node::RESOURCE, base_uri);
	for (GraphObject::Variables::const_iterator v = extra_rdf.begin(); v != extra_rdf.end(); ++v) {
		if (v->first.find(":") != string::npos) {
			_model->add_statement(base_rdf_node, v->first,
					AtomRDF::atom_to_node(_model->world(), v->second));
		} else {
			cerr << "Warning: not serialising extra RDF with key '" << v->first << "'" << endl;
		}
	}

	return finish();
}


/** Begin a serialization to a file.
 *
 * This must be called before any serializing methods.
 */
void
Serialiser::start_to_filename(const string& filename)
{
	setlocale(LC_NUMERIC, "C");

	assert(filename.find(":") == string::npos || filename.substr(0, 5) == "file:");
	if (filename.find(":") == string::npos)
		_base_uri = "file://" + filename;
	else
		_base_uri = filename;
	_model = new Redland::Model(_world);
    _model->set_base_uri(_base_uri);
	_mode = TO_FILE;
}


/** Begin a serialization to a string.
 *
 * This must be called before any serializing methods.
 *
 * The results of the serialization will be returned by the finish() method after
 * the desired objects have been serialised.
 *
 * All serialized paths will have the root path chopped from their prefix
 * (therefore all serialized paths must be descendants of the root)
 */
void
Serialiser::start_to_string(const Raul::Path& root, const string& base_uri)
{
	setlocale(LC_NUMERIC, "C");

	_root_path = root;
	_base_uri = base_uri;
	_model = new Redland::Model(_world);
    _model->set_base_uri(base_uri);
	_mode = TO_STRING;
}


/** Finish a serialization.
 *
 * If this was a serialization to a string, the serialization output
 * will be returned, otherwise the empty string is returned.
 */
string
Serialiser::finish()
{
	string ret = "";
	if (_mode == TO_FILE) {
		_model->serialise_to_file(_base_uri);
	} else {
		char* c_str = _model->serialise_to_string();
		if (c_str != NULL) {
			ret = c_str;
			free(c_str);
		}
	}

	_base_uri = "";
#ifdef USE_BLANK_NODES
	_node_map.clear();
#endif
	return ret;
}

	
Redland::Node
Serialiser::patch_path_to_rdf_node(const Path& path)
{
#ifdef USE_BLANK_NODES
	if (path == _root_path) {
		return Redland::Node(_model->world(), Redland::Node::RESOURCE, _base_uri);
	} else {
		assert(path.length() > _root_path.length());
		return Redland::Node(_model->world(), Redland::Node::RESOURCE,
				_base_uri + string("#") + path.substr(_root_path.length()));
	}
#else
	return path_to_rdf_node(path);
#endif
}



/** Convert a path to an RDF blank node ID for serializing.
 */
Redland::Node
Serialiser::path_to_rdf_node(const Path& path)
{
#if USE_BLANK_NODES
	NodeMap::iterator i = _node_map.find(path);
	if (i != _node_map.end()) {
		assert(i->second);
		assert(i->second.get_node());
		return i->second;
	} else {
		Redland::Node id = _world.blank_id(Path::nameify(path.substr(1)));
		assert(id);
		_node_map[path] = id;
		return id;
	}
#else
	assert(_model);
	assert(path.substr(0, _root_path.length()) == _root_path);
	
	if (path == _root_path)
		return Redland::Node(_model->world(), Redland::Node::RESOURCE, _base_uri);
	else
		return Redland::Node(_model->world(), Redland::Node::RESOURCE,
				path.substr(_root_path.base().length()));
#endif
}


#if 0
/** Searches for the filename passed in the path, returning the full
 * path of the file, or the empty string if not found.
 *
 * This function tries to be as friendly a black box as possible - if the path
 * passed is an absolute path and the file is found there, it will return
 * that path, etc.
 *
 * additional_path is a list (colon delimeted as usual) of additional
 * directories to look in.  ie the directory the parent patch resides in would
 * be a good idea to pass as additional_path, in the case of a subpatch.
 */
string
Serialiser::find_file(const string& filename, const string& additional_path)
{
	string search_path = additional_path + ":" + _patch_search_path;
	
	// Try to open the raw filename first
	std::ifstream is(filename.c_str(), std::ios::in);
	if (is.good()) {
		is.close();
		return filename;
	}
	
	string directory;
	string full_patch_path = "";
	
	while (search_path != "") {
		directory = search_path.substr(0, search_path.find(':'));
		if (search_path.find(':') != string::npos)
			search_path = search_path.substr(search_path.find(':')+1);
		else
			search_path = "";

		full_patch_path = directory +"/"+ filename;
		
		std::ifstream is;
		is.open(full_patch_path.c_str(), std::ios::in);
	
		if (is.good()) {
			is.close();
			return full_patch_path;
		} else {
			cerr << "[Serialiser] Could not find patch file " << full_patch_path << endl;
		}
	}

	return "";
}
#endif

void
Serialiser::serialise(SharedPtr<GraphObject> object) throw (std::logic_error)
{
	if (!_model)
		throw std::logic_error("serialise called without serialization in progress");

	SharedPtr<Shared::Patch> patch = PtrCast<Shared::Patch>(object);
	if (patch) {
		serialise_patch(patch);
		return;
	}
	
	SharedPtr<Shared::Node> node = PtrCast<Shared::Node>(object);
	if (node) {
		serialise_node(node, path_to_rdf_node(node->path()));
		return;
	}
	
	SharedPtr<Shared::Port> port = PtrCast<Shared::Port>(object);
	if (port) {
		serialise_port(port.get(), path_to_rdf_node(port->path()));
		return;
	}

	cerr << "[Serialiser] WARNING: Unsupported object type, "
		<< object->path() << " not serialised." << endl;
}


void
Serialiser::serialise_patch(SharedPtr<Shared::Patch> patch)
{
	assert(_model);

	const Redland::Node patch_id = patch_path_to_rdf_node(patch->path());
	
	_model->add_statement(
		patch_id,
		"rdf:type",
		Redland::Node(_model->world(), Redland::Node::RESOURCE, "http://drobilla.net/ns/ingen#Patch"));
	
	_model->add_statement(
		patch_id,
		"rdf:type",
		Redland::Node(_model->world(), Redland::Node::RESOURCE, "http://lv2plug.in/ns/lv2core#Plugin"));

	GraphObject::Variables::const_iterator s = patch->variables().find("lv2:symbol");
	// If symbol is stored as a variable, write that
	if (s != patch->variables().end()) {
		_model->add_statement(patch_id, "lv2:symbol",
			Redland::Node(_model->world(), Redland::Node::LITERAL, s->second.get_string()));
	// Otherwise take the one from our path (if possible)
	} else if (patch->path() != "/") {
		_model->add_statement(
			patch_id, "lv2:symbol",
			Redland::Node(_model->world(), Redland::Node::LITERAL, patch->path().name()));
	}

	_model->add_statement(
		patch_id,
		"ingen:polyphony",
		AtomRDF::atom_to_node(_model->world(), Atom((int)patch->internal_polyphony())));
	
	_model->add_statement(
		patch_id,
		"ingen:enabled",
		AtomRDF::atom_to_node(_model->world(), Atom((bool)patch->enabled())));
	
	serialise_properties(patch_id, patch->properties());
	serialise_variables(patch_id, patch->variables());

	for (GraphObject::const_iterator n = _store->children_begin(patch);
			n != _store->children_end(patch); ++n) {
		
		if (n->second->graph_parent() != patch.get())
			continue;

		SharedPtr<Shared::Patch> patch = PtrCast<Shared::Patch>(n->second);
		SharedPtr<Shared::Node>  node  = PtrCast<Shared::Node>(n->second);
		if (patch) {
			_model->add_statement(patch_id, "ingen:node", patch_path_to_rdf_node(patch->path()));
			serialise_patch(patch);
		} else if (node) {
			const Redland::Node node_id = path_to_rdf_node(n->second->path());
			_model->add_statement(patch_id, "ingen:node", node_id);
			serialise_node(node, node_id);
		}
	}
	
	for (uint32_t i=0; i < patch->num_ports(); ++i) {
		Port* p = patch->port(i);
		const Redland::Node port_id = path_to_rdf_node(p->path());
	
		// Ensure lv2:name always exists so Patch is a valid LV2 plugin
		if (p->properties().find("lv2:name") == p->properties().end())
			p->set_property("lv2:name", Atom(Atom::STRING, p->symbol())); // FIXME: use human name

		_model->add_statement(patch_id, "lv2:port", port_id);
		serialise_port(p, port_id);
	}

	for (Shared::Patch::Connections::const_iterator c = patch->connections().begin();
			c != patch->connections().end(); ++c) {
		serialise_connection(patch, *c);
	}
}


void
Serialiser::serialise_plugin(const Shared::Plugin& plugin)
{
	assert(_model);

	const Redland::Node plugin_id = Redland::Node(_model->world(), Redland::Node::RESOURCE, plugin.uri());

	_model->add_statement(
		plugin_id,
		"rdf:type",
		Redland::Node(_model->world(), Redland::Node::RESOURCE, plugin.type_uri()));
} 


void
Serialiser::serialise_node(SharedPtr<Shared::Node> node, const Redland::Node& node_id)
{
	const Redland::Node plugin_id
		= Redland::Node(_model->world(), Redland::Node::RESOURCE, node->plugin()->uri());

	_model->add_statement(
		node_id,
		"rdf:type",
		Redland::Node(_model->world(), Redland::Node::RESOURCE, "ingen:Node"));
	
	_model->add_statement(
		node_id,
		"lv2:symbol",
		Redland::Node(_model->world(), Redland::Node::LITERAL, node->path().name()));
	
	_model->add_statement(
		node_id,
		"rdf:instanceOf",
		plugin_id);
	
	_model->add_statement(
		node_id,
		"ingen:polyphonic",
		AtomRDF::atom_to_node(_model->world(), Atom(node->polyphonic())));

	//serialise_plugin(node->plugin());
	
	for (uint32_t i=0; i < node->num_ports(); ++i) {
		Port* p = node->port(i);
		assert(p);
		const Redland::Node port_id = path_to_rdf_node(p->path());
		serialise_port(p, port_id);
		_model->add_statement(node_id, "lv2:port", port_id);
	}

	serialise_properties(node_id, node->properties());
	serialise_variables(node_id, node->variables());
}


/** Writes a port subject with various information only if there are some
 * predicate/object pairs to go with it (eg if the port has variable, or a value, or..).
 * Audio output ports with no variable will not be written, for example.
 */
void
Serialiser::serialise_port(const Port* port, const Redland::Node& port_id)
{
	if (port->is_input())
		_model->add_statement(port_id, "rdf:type",
				Redland::Node(_model->world(), Redland::Node::RESOURCE, "lv2:InputPort"));
	else
		_model->add_statement(port_id, "rdf:type",
				Redland::Node(_model->world(), Redland::Node::RESOURCE, "lv2:OutputPort"));
	
	if (dynamic_cast<Patch*>(port->graph_parent()))
		_model->add_statement(port_id, "lv2:index",
				AtomRDF::atom_to_node(_model->world(), Atom((int)port->index())));

	_model->add_statement(port_id, "lv2:symbol",
			Redland::Node(_model->world(), Redland::Node::LITERAL, port->path().name()));
	
	_model->add_statement(port_id, "rdf:type",
			Redland::Node(_model->world(), Redland::Node::RESOURCE, port->type().uri()));
	
	if (port->type() == DataType::CONTROL && port->is_input())
		_model->add_statement(port_id, "ingen:value",
				AtomRDF::atom_to_node(_model->world(), Atom(port->value())));

	serialise_properties(port_id, port->properties());
	serialise_variables(port_id, port->variables());
}


void
Serialiser::serialise_connection(SharedPtr<GraphObject> parent,
                                 SharedPtr<Connection>  connection) throw (std::logic_error)
{
	if (!_model)
		throw std::logic_error("serialise_connection called without serialization in progress");

	const Redland::Node src_node = path_to_rdf_node(connection->src_port_path());
	const Redland::Node dst_node = path_to_rdf_node(connection->dst_port_path());

	/* This would allow associating data with the connection... */
	const Redland::Node connection_node = _world.blank_id();
	_model->add_statement(connection_node, "ingen:source", src_node);
	_model->add_statement(connection_node, "ingen:destination", dst_node);
	if (parent) {
		const Redland::Node parent_node = path_to_rdf_node(parent->path());
		_model->add_statement(parent_node, "ingen:connection", connection_node);
	} else {
		_model->add_statement(connection_node, "rdf:type",
				Redland::Node(_model->world(), Redland::Node::RESOURCE, "ingen:Connection"));
	}

	/* ... but this is cleaner */
	//_model->add_statement(dst_node, "ingen:connectedTo", src_node);
}


void
Serialiser::serialise_properties(Redland::Node subject, const GraphObject::Variables& properties)
{
	for (GraphObject::Variables::const_iterator v = properties.begin(); v != properties.end(); ++v) {
		if (v->first.find(":") && v->second.is_valid()) {
			const Redland::Node value = AtomRDF::atom_to_node(_model->world(), v->second);
			_model->add_statement(subject, v->first, value);
		} else {
			cerr << "Warning: unable to serialize property \'" << v->first << "\'" << endl;
		}
	}
}
	

void
Serialiser::serialise_variables(Redland::Node subject, const GraphObject::Variables& variables)
{
	for (GraphObject::Variables::const_iterator v = variables.begin(); v != variables.end(); ++v) {
		if (v->first.find(":") && v->first != "ingen:document") {
			if (v->second.is_valid()) {
				const Redland::Node var_id = _world.blank_id();
				const Redland::Node key(_model->world(), Redland::Node::RESOURCE, v->first);
				const Redland::Node value = AtomRDF::atom_to_node(_model->world(), v->second);
				if (value) {
					_model->add_statement(subject, "lv2var:variable", var_id);
					_model->add_statement(var_id, "rdf:predicate", key);
					_model->add_statement(var_id, "rdf:value", value);
				} else {
					cerr << "Warning: can not serialise value: key '" << v->first << "', type "
						<< (int)v->second.type() << endl;
				}
			} else {
				cerr << "Warning: variable with no value: key '" << v->first << "'" << endl;
			}
		} else {
			cerr << "Not serialising special variable '" << v->first << "'" << endl;
		}
	}
}


} // namespace Serialisation
} // namespace Ingen
