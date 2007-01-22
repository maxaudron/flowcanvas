/* This file is part of Ingen.  Copyright (C) 2006 Dave Robillard.
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

#ifndef PATCHLIBRARIAN_H
#define PATCHLIBRARIAN_H

#include <map>
#include <utility>
#include <string>
#include <cassert>
#include <glibmm/ustring.h>
#include <libxml/tree.h>
#include "raul/SharedPtr.h"
#include "raul/Path.h"
#include "ObjectModel.h"

using std::string;

namespace Ingen {
namespace Client {

class PatchModel;
class NodeModel;
class ConnectionModel;
class PresetModel;
class ModelEngineInterface;

	
/** Loads deprecated (XML) patch files (from the Om days).
 *
 * \ingroup IngenClient
 */
class DeprecatedLoader
{
public:
	DeprecatedLoader(SharedPtr<ModelEngineInterface> engine)
	: /*_patch_search_path(".")*/ _engine(engine)
	{
		assert(_engine);
	}

	/*void          path(const string& path) { _patch_search_path = path; }
	const string& path()                   { return _patch_search_path; }*/
	
	string find_file(const string& filename, const string& additional_path = "");
	
	string load_patch(const Glib::ustring& filename,
	                  const Path&          parent_path,
	                  string               name,
	                  size_t               poly,
	                  MetadataMap          initial_data,
	                  bool                 existing = false);

private:
	void add_metadata(MetadataMap& data, string key, string value);

	string translate_load_path(const string& path);

	//string                          _patch_search_path;
	SharedPtr<ModelEngineInterface> _engine;

	/// Translations of paths from the loading file to actual paths (for deprecated patches)
	std::map<string, string> _load_path_translations;

	bool load_node(const Path& parent, xmlDocPtr doc, const xmlNodePtr cur);
	bool load_connection(const Path& parent, xmlDocPtr doc, const xmlNodePtr cur);
	bool load_preset(const Path& parent, xmlDocPtr doc, const xmlNodePtr cur);
	bool load_subpatch(const Path& parent, xmlDocPtr doc, const xmlNodePtr cur);
};


} // namespace Client
} // namespace Ingen

#endif // PATCHLIBRARIAN_H