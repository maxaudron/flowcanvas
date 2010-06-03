/* SLV2
 * Copyright (C) 2007-2009 Dave Robillard <http://drobilla.net>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _XOPEN_SOURCE 500

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <librdf.h>
#include "slv2/types.h"
#include "slv2/collections.h"
#include "slv2/plugin.h"
#include "slv2/pluginclass.h"
#include "slv2/query.h"
#include "slv2/util.h"
#include "slv2_internal.h"
#ifdef SLV2_DYN_MANIFEST
#include <dlfcn.h>
#endif


/* private
 * ownership of uri is taken */
SLV2Plugin
slv2_plugin_new(SLV2World world, SLV2Value uri, librdf_uri* bundle_uri)
{
	assert(bundle_uri);
	struct _SLV2Plugin* plugin = malloc(sizeof(struct _SLV2Plugin));
	plugin->world = world;
	plugin->plugin_uri = uri;
	plugin->bundle_uri = slv2_value_new_librdf_uri(world, bundle_uri);
	plugin->binary_uri = NULL;
#ifdef SLV2_DYN_MANIFEST
	plugin->dynman_uri = NULL;
#endif
	plugin->plugin_class = NULL;
	plugin->data_uris = slv2_values_new();
	plugin->ports = NULL;
	plugin->num_ports = 0;
	plugin->loaded = false;

	/*printf("PLUGIN %s DATA URIs: %p\n",
			slv2_value_as_string(plugin->plugin_uri),
			(void*)plugin->data_uris);*/

	return plugin;
}


/* private */
void
slv2_plugin_free(SLV2Plugin p)
{
	slv2_value_free(p->plugin_uri);
	p->plugin_uri = NULL;

	slv2_value_free(p->bundle_uri);
	p->bundle_uri = NULL;

	slv2_value_free(p->binary_uri);
	p->binary_uri = NULL;

#ifdef SLV2_DYN_MANIFEST
	slv2_value_free(p->dynman_uri);
	p->dynman_uri = NULL;
#endif

	if (p->ports) {
		for (uint32_t i = 0; i < p->num_ports; ++i)
			slv2_port_free(p->ports[i]);
		free(p->ports);
		p->ports = NULL;
	}

	slv2_values_free(p->data_uris);
	p->data_uris = NULL;

	free(p);
}


/* private */
void
slv2_plugin_load_if_necessary(SLV2Plugin p)
{
	if (!p->loaded)
		slv2_plugin_load(p);
}


/* private */
void
slv2_plugin_load_ports_if_necessary(SLV2Plugin p)
{
	if (!p->loaded)
		slv2_plugin_load(p);

	if (!p->ports) {
		p->ports = malloc(sizeof(SLV2Port*));
		p->ports[0] = NULL;

		const unsigned char* query = (const unsigned char*)
			"PREFIX : <http://lv2plug.in/ns/lv2core#>\n"
			"SELECT DISTINCT ?type ?symbol ?index WHERE {\n"
			"<>    :port    ?port .\n"
			"?port a        ?type ;\n"
			"      :symbol  ?symbol ;\n"
			"      :index   ?index .\n"
			"}";

		librdf_query* q = librdf_new_query(p->world->world, "sparql",
				NULL, query, slv2_value_as_librdf_uri(p->plugin_uri));

		librdf_query_results* results = librdf_query_execute(q, p->world->model);

		while (!librdf_query_results_finished(results)) {
			librdf_node* type_node = librdf_query_results_get_binding_value(results, 0);
			librdf_node* symbol_node = librdf_query_results_get_binding_value(results, 1);
			librdf_node* index_node = librdf_query_results_get_binding_value(results, 2);

			if (librdf_node_is_literal(symbol_node) && librdf_node_is_literal(index_node)) {
				const char* symbol = (const char*)librdf_node_get_literal_value(symbol_node);
				const char* index = (const char*)librdf_node_get_literal_value(index_node);

				const int this_index = atoi(index);
				SLV2Port  this_port  = NULL;

				assert(this_index >= 0);

				if (p->num_ports > (unsigned)this_index) {
					this_port = p->ports[this_index];
				} else {
					p->ports = realloc(p->ports, (this_index + 1) * sizeof(SLV2Port*));
					memset(p->ports + p->num_ports, '\0',
							(this_index - p->num_ports) * sizeof(SLV2Port));
					p->num_ports = this_index + 1;
				}

				// Havn't seen this port yet, add it to array
				if (!this_port) {
					this_port = slv2_port_new(p->world, this_index, symbol);
					p->ports[this_index] = this_port;
				}

				raptor_sequence_push(this_port->classes,
						slv2_value_new_librdf_uri(p->world, librdf_node_get_uri(type_node)));
			}

			librdf_free_node(type_node);
			librdf_free_node(symbol_node);
			librdf_free_node(index_node);
			librdf_query_results_next(results);
		}

		librdf_free_query_results(results);
		librdf_free_query(q);
	}
}


void
slv2_plugin_load(SLV2Plugin p)
{
	bool transaction = false;
	// Parse all the plugin's data files into RDF model
	for (unsigned i=0; i < slv2_values_size(p->data_uris); ++i) {
		SLV2Value data_uri_val = slv2_values_get_at(p->data_uris, i);
		librdf_uri* data_uri = slv2_value_as_librdf_uri(data_uri_val);
		slv2_world_load_file(p->world, data_uri, &transaction);
	}
			
#ifdef SLV2_DYN_MANIFEST
	typedef void* LV2_Dyn_Manifest_Handle;
	// Load and parse dynamic manifest data, if this is a library
	if (p->dynman_uri) {
		const char* lib_path = slv2_uri_to_path(slv2_value_as_string(p->dynman_uri));
		void* lib = dlopen(lib_path, RTLD_LAZY);
		if (!lib) {
			SLV2_WARNF("Unable to open dynamic manifest %s\n", slv2_value_as_string(p->dynman_uri));
			return;
		}

		typedef int (*OpenFunc)(LV2_Dyn_Manifest_Handle*, const LV2_Feature *const *);
		OpenFunc open_func = (OpenFunc)dlsym(lib, "lv2_dyn_manifest_open");
		LV2_Dyn_Manifest_Handle handle = NULL;
		if (open_func)
			open_func(&handle, &dman_features);

		typedef int (*GetDataFunc)(LV2_Dyn_Manifest_Handle handle,
		                           FILE*                   fp,
		                           const char*             uri);
		GetDataFunc get_data_func = (GetDataFunc)dlsym(lib, "lv2_dyn_manifest_get_data");
		if (get_data_func) {
			FILE* fd = tmpfile();
			get_data_func(handle, fd, slv2_value_as_string(p->plugin_uri));
			rewind(fd);
			if (!transaction) {
				librdf_model_transaction_begin(p->world->model);
				transaction = true;
			}
			librdf_parser_parse_file_handle_into_model(p->world->parser,
					fd, 0, slv2_value_as_librdf_uri(p->bundle_uri), p->world->model);
			fclose(fd);
		}

		typedef int (*CloseFunc)(LV2_Dyn_Manifest_Handle);
		CloseFunc close_func = (CloseFunc)dlsym(lib, "lv2_dyn_manifest_close");
		if (close_func)
			close_func(handle);
	}
#endif

	if (transaction)
		librdf_model_transaction_commit(p->world->model);

	p->loaded = true;
}


SLV2Value
slv2_plugin_get_uri(SLV2Plugin p)
{
	assert(p);
	assert(p->plugin_uri);
	return p->plugin_uri;
}


SLV2Value
slv2_plugin_get_bundle_uri(SLV2Plugin p)
{
	assert(p);
	assert(p->bundle_uri);
	return p->bundle_uri;
}


SLV2Value
slv2_plugin_get_library_uri(SLV2Plugin p)
{
	assert(p);
	slv2_plugin_load_if_necessary(p);
	if (!p->binary_uri) {
		const unsigned char* query = (const unsigned char*)
			"PREFIX : <http://lv2plug.in/ns/lv2core#>\n"
			"SELECT ?binary WHERE { <> :binary ?binary . }";

		librdf_query* q = librdf_new_query(p->world->world, "sparql",
				NULL, query, slv2_value_as_librdf_uri(p->plugin_uri));

		librdf_query_results* results = librdf_query_execute(q, p->world->model);

		if (!librdf_query_results_finished(results)) {
			librdf_node* binary_node = librdf_query_results_get_binding_value(results, 0);
			librdf_uri* binary_uri = librdf_node_get_uri(binary_node);

			if (binary_uri) {
				SLV2Value binary = slv2_value_new_librdf_uri(p->world, binary_uri);
				p->binary_uri = binary;
			}

			librdf_free_node(binary_node);
		}

		librdf_free_query_results(results);
		librdf_free_query(q);
	}
	return p->binary_uri;
}


SLV2Values
slv2_plugin_get_data_uris(SLV2Plugin p)
{
	return p->data_uris;
}


SLV2PluginClass
slv2_plugin_get_class(SLV2Plugin p)
{
	slv2_plugin_load_if_necessary(p);
	if (!p->plugin_class) {
		const unsigned char* query = (const unsigned char*)
			"SELECT DISTINCT ?class WHERE { <> a ?class }";

		librdf_query* q = librdf_new_query(p->world->world, "sparql",
				NULL, query, slv2_value_as_librdf_uri(p->plugin_uri));

		librdf_query_results* results = librdf_query_execute(q, p->world->model);

		while (!librdf_query_results_finished(results)) {
			librdf_node* class_node = librdf_query_results_get_binding_value(results, 0);
			librdf_uri*  class_uri  = librdf_node_get_uri(class_node);

			if (!class_uri) {
				librdf_query_results_next(results);
				continue;
			}

			SLV2Value class = slv2_value_new_librdf_uri(p->world, class_uri);

			if ( ! slv2_value_equals(class, p->world->lv2_plugin_class->uri)) {

				SLV2PluginClass plugin_class = slv2_plugin_classes_get_by_uri(
						p->world->plugin_classes, class);

				librdf_free_node(class_node);

				if (plugin_class) {
					p->plugin_class = plugin_class;
					slv2_value_free(class);
					break;
				}
			}

			slv2_value_free(class);
			librdf_query_results_next(results);
		}

		if (p->plugin_class == NULL)
			p->plugin_class = p->world->lv2_plugin_class;

		librdf_free_query_results(results);
		librdf_free_query(q);
	}

	return p->plugin_class;
}


bool
slv2_plugin_verify(SLV2Plugin plugin)
{
	char* query_str =
		"SELECT DISTINCT ?type ?name ?license ?port WHERE {\n"
		"<> a ?type ;\n"
		"doap:name    ?name ;\n"
		"doap:license ?license ;\n"
		"lv2:port     [ lv2:index ?port ] .\n}";

	SLV2Results results = slv2_plugin_query_sparql(plugin, query_str);

	bool has_type    = false;
	bool has_name    = false;
	bool has_license = false;
	bool has_port    = false;

	while (!librdf_query_results_finished(results->rdf_results)) {
		librdf_node* type_node = librdf_query_results_get_binding_value(results->rdf_results, 0);
		librdf_node* name_node = librdf_query_results_get_binding_value(results->rdf_results, 1);
		librdf_node* license_node = librdf_query_results_get_binding_value(results->rdf_results, 2);
		librdf_node* port_node = librdf_query_results_get_binding_value(results->rdf_results, 3);

		if (librdf_node_get_type(type_node) == LIBRDF_NODE_TYPE_RESOURCE)
			has_type = true;

		if (name_node)
			has_name = true;

		if (license_node)
			has_license = true;

		if (port_node)
			has_port = true;

		librdf_free_node(type_node);
		librdf_free_node(name_node);
		librdf_free_node(license_node);
		librdf_free_node(port_node);

		librdf_query_results_next(results->rdf_results);
	}

	slv2_results_free(results);

	if ( ! (has_type && has_name && has_license && has_port) ) {
		SLV2_WARNF("Invalid plugin <%s>\n", slv2_value_as_uri(slv2_plugin_get_uri(plugin)));
		return false;
	} else {
		return true;
	}
}


SLV2Value
slv2_plugin_get_name(SLV2Plugin plugin)
{
	SLV2Values results = slv2_plugin_get_value_by_qname_i18n(plugin, "doap:name");
	SLV2Value  ret     = NULL;

	if (results) {
		SLV2Value val = slv2_values_get_at(results, 0);
		if (slv2_value_is_string(val))
			ret = slv2_value_duplicate(val);
		slv2_values_free(results);
	} else {
		results = slv2_plugin_get_value_by_qname(plugin, "doap:name");
		SLV2Value val = slv2_values_get_at(results, 0);
		if (slv2_value_is_string(val))
			ret = slv2_value_duplicate(val);
		slv2_values_free(results);
	}

	if (!ret)
		SLV2_WARNF("<%s> has no (mandatory) doap:name\n",
				slv2_value_as_string(slv2_plugin_get_uri(plugin)));

	return ret;
}


SLV2Values
slv2_plugin_get_value(SLV2Plugin p,
                      SLV2Value  predicate)
{
	/* Hack around broken RASQAL, full URI predicates don't work :/ */
	char* query = slv2_strjoin(
		"PREFIX slv2predicate: <", slv2_value_as_string(predicate), ">\n",
		"SELECT DISTINCT ?value WHERE {\n"
		"<> slv2predicate: ?value .\n"
		"}\n", NULL);

	SLV2Values result = slv2_plugin_query_variable(p, query, 0);

	free(query);

	return result;
}


/* internal */
SLV2Values
slv2_plugin_get_value_by_qname(SLV2Plugin  p,
                               const char* predicate)
{
	char* query = slv2_strjoin(
			"SELECT DISTINCT ?value WHERE { \n"
			"<> ", predicate, " ?value . \n"
			"}\n", NULL);

	SLV2Values result = slv2_plugin_query_variable(p, query, 0);

	free(query);

	return result;
}


/* internal: get i18n value if possible */
SLV2Values
slv2_plugin_get_value_by_qname_i18n(SLV2Plugin  p,
                                    const char* predicate)
{
	char* query = slv2_strjoin(
			"SELECT DISTINCT ?value WHERE { \n"
			"<> ", predicate, " ?value . \n"
			"FILTER(lang(?value) = \"", slv2_get_lang(), "\") \n"
			"}\n", NULL);

	SLV2Values result = slv2_plugin_query_variable(p, query, 0);

	free(query);

	return result;
}


SLV2Values
slv2_plugin_get_value_for_subject(SLV2Plugin p,
                                  SLV2Value  subject,
                                  SLV2Value  predicate)
{
	if ( ! slv2_value_is_uri(subject)) {
		SLV2_ERROR("Subject is not a URI\n");
		return NULL;
	}

	char* query = NULL;



	char* subject_token = slv2_value_get_turtle_token(subject);

	/* Hack around broken RASQAL, full URI predicates don't work :/ */
	query = slv2_strjoin(
		"PREFIX slv2predicate: <", slv2_value_as_string(predicate), ">\n",
		"SELECT DISTINCT ?value WHERE {\n",
		subject_token, " slv2predicate: ?value .\n"
		"}\n", NULL);

	SLV2Values result = slv2_plugin_query_variable(p, query, 0);

	free(query);
	free(subject_token);

	return result;
}


SLV2Values
slv2_plugin_get_properties(SLV2Plugin p)
{
	// FIXME: APIBREAK: This predicate does not even exist.  Remove this function.
	return slv2_plugin_get_value_by_qname(p, "lv2:pluginProperty");
}


SLV2Values
slv2_plugin_get_hints(SLV2Plugin p)
{
	// FIXME: APIBREAK: This predicate does not even exist.  Remove this function.
	return slv2_plugin_get_value_by_qname(p, "lv2:pluginHint");
}


uint32_t
slv2_plugin_get_num_ports(SLV2Plugin p)
{
	slv2_plugin_load_ports_if_necessary(p);
	return p->num_ports;
}


void
slv2_plugin_get_port_float_values(SLV2Plugin  p,
                                  const char* qname,
                                  float*      values)
{
	slv2_plugin_load_ports_if_necessary(p);

	for (uint32_t i = 0; i < p->num_ports; ++i)
		values[i] = NAN;

	unsigned char* query = (unsigned char*)slv2_strjoin(
			"PREFIX : <http://lv2plug.in/ns/lv2core#>\n"
			"SELECT DISTINCT ?index ?value WHERE {\n"
			"<>    :port    ?port .\n"
			"?port :index   ?index .\n"
			"?port ", qname, " ?value .\n"
			"} ", NULL);

	librdf_query* q = librdf_new_query(p->world->world, "sparql",
			NULL, query, slv2_value_as_librdf_uri(p->plugin_uri));

	librdf_query_results* results = librdf_query_execute(q, p->world->model);

	while (!librdf_query_results_finished(results)) {
		librdf_node* idx_node = librdf_query_results_get_binding_value(results, 0);
		librdf_node* val_node = librdf_query_results_get_binding_value(results, 1);
		if (idx_node && val_node && librdf_node_is_literal(idx_node)
				&& librdf_node_is_literal(val_node)) {
			const int idx = atoi((const char*)librdf_node_get_literal_value(idx_node));
			const float val = atof((const char*)librdf_node_get_literal_value(val_node));
			values[idx] = val;
			librdf_free_node(idx_node);
			librdf_free_node(val_node);
		}
		librdf_query_results_next(results);
	}

	librdf_free_query_results(results);
	librdf_free_query(q);
	free(query);
}


void
slv2_plugin_get_port_ranges_float(SLV2Plugin p,
                                  float*     min_values,
                                  float*     max_values,
                                  float*     def_values)
{
	if (min_values)
		slv2_plugin_get_port_float_values(p, ":minimum", min_values);

	if (max_values)
		slv2_plugin_get_port_float_values(p, ":maximum", max_values);

	if (def_values)
		slv2_plugin_get_port_float_values(p, ":default", def_values);
}


uint32_t
slv2_plugin_get_num_ports_of_class(SLV2Plugin p,
                                   SLV2Value  class_1, ...)
{
	slv2_plugin_load_ports_if_necessary(p);

	uint32_t ret = 0;
	va_list  args;

	for (unsigned i=0; i < p->num_ports; ++i) {
		SLV2Port port = p->ports[i];
		if (!port || !slv2_port_is_a(p, port, class_1))
			continue;

		va_start(args, class_1);

		bool matches = true;
		for (SLV2Value class_i = NULL; (class_i = va_arg(args, SLV2Value)) != NULL ; ) {
			if (!slv2_port_is_a(p, port, class_i)) {
				va_end(args);
				matches = false;
				break;
			}
		}

		if (matches)
			++ret;

		va_end(args);
	}

	return ret;
}


bool
slv2_plugin_has_latency(SLV2Plugin p)
{
    const char* const query =
		"SELECT ?index WHERE {\n"
		"	<>      lv2:port         ?port .\n"
		"	?port   lv2:portProperty lv2:reportsLatency ;\n"
		"	        lv2:index        ?index .\n"
		"}\n";

	SLV2Values results = slv2_plugin_query_variable(p, query, 0);
	const bool latent = (slv2_values_size(results) > 0);
	slv2_values_free(results);

	return latent;
}


uint32_t
slv2_plugin_get_latency_port_index(SLV2Plugin p)
{
    const char* const query =
		"SELECT ?index WHERE {\n"
		"	<>      lv2:port         ?port .\n"
		"	?port   lv2:portProperty lv2:reportsLatency ;\n"
		"	        lv2:index        ?index .\n"
		"}\n";

	SLV2Values result = slv2_plugin_query_variable(p, query, 0);

	// FIXME: need a sane error handling strategy
	assert(slv2_values_size(result) > 0);
	SLV2Value val = slv2_values_get_at(result, 0);
	assert(slv2_value_is_int(val));

	int ret = slv2_value_as_int(val);
	slv2_values_free(result);
	return ret;
}


bool
slv2_plugin_has_feature(SLV2Plugin p,
                        SLV2Value  feature)
{
	SLV2Values features = slv2_plugin_get_supported_features(p);

	const bool ret = features && feature && slv2_values_contains(features, feature);

	slv2_values_free(features);
	return ret;
}


SLV2Values
slv2_plugin_get_supported_features(SLV2Plugin p)
{
	SLV2Values optional = slv2_plugin_get_optional_features(p);
	SLV2Values required = slv2_plugin_get_required_features(p);

	SLV2Values result = slv2_values_new();
	unsigned n_optional = slv2_values_size(optional);
	unsigned n_required = slv2_values_size(required);
	unsigned i = 0;
	for ( ; i < n_optional; ++i)
		slv2_values_set_at(result, i, raptor_sequence_pop(optional));
	for ( ; i < n_optional + n_required; ++i)
		slv2_values_set_at(result, i, raptor_sequence_pop(required));

	slv2_values_free(optional);
	slv2_values_free(required);

	return result;
}


SLV2Values
slv2_plugin_get_optional_features(SLV2Plugin p)
{
	return slv2_plugin_get_value_by_qname(p, "lv2:optionalFeature");
}


SLV2Values
slv2_plugin_get_required_features(SLV2Plugin p)
{
	return slv2_plugin_get_value_by_qname(p, "lv2:requiredFeature");
}


SLV2Port
slv2_plugin_get_port_by_index(SLV2Plugin p,
                              uint32_t   index)
{
	slv2_plugin_load_ports_if_necessary(p);
	if (index < p->num_ports)
		return p->ports[index];
	else
		return NULL;
}


SLV2Port
slv2_plugin_get_port_by_symbol(SLV2Plugin p,
                               SLV2Value  symbol)
{
	slv2_plugin_load_ports_if_necessary(p);
	for (uint32_t i=0; i < p->num_ports; ++i) {
		SLV2Port port = p->ports[i];
		if (slv2_value_equals(port->symbol, symbol))
			return port;
	}

	return NULL;
}


SLV2Value
slv2_plugin_get_author_name(SLV2Plugin plugin)
{
	SLV2Value ret = NULL;

    const char* const query =
		"SELECT ?name WHERE {\n"
		"	<>      doap:maintainer ?maint . \n"
		"	?maint  foaf:name ?name . \n"
		"}\n";

	SLV2Values results = slv2_plugin_query_variable(plugin, query, 0);

	if (results && slv2_values_size(results) > 0) {
		SLV2Value val = slv2_values_get_at(results, 0);
		if (slv2_value_is_string(val))
			ret = slv2_value_duplicate(val);
	}

	if (results)
		slv2_values_free(results);

	return ret;
}


SLV2Value
slv2_plugin_get_author_email(SLV2Plugin plugin)
{
	SLV2Value ret = NULL;

    const char* const query =
		"SELECT ?email WHERE {\n"
		"	<>      doap:maintainer ?maint . \n"
		"	?maint  foaf:mbox ?email . \n"
		"}\n";

	SLV2Values results = slv2_plugin_query_variable(plugin, query, 0);

	if (results && slv2_values_size(results) > 0) {
		SLV2Value val = slv2_values_get_at(results, 0);
		if (slv2_value_is_uri(val))
			ret = slv2_value_duplicate(val);
	}

	if (results)
		slv2_values_free(results);

	return ret;
}


SLV2Value
slv2_plugin_get_author_homepage(SLV2Plugin plugin)
{
	SLV2Value ret = NULL;

    const char* const query =
		"SELECT ?page WHERE {\n"
		"	<>      doap:maintainer ?maint . \n"
		"	?maint  foaf:homepage ?page . \n"
		"}\n";

	SLV2Values results = slv2_plugin_query_variable(plugin, query, 0);

	if (results && slv2_values_size(results) > 0) {
		SLV2Value val = slv2_values_get_at(results, 0);
		if (slv2_value_is_uri(val))
			ret = slv2_value_duplicate(val);
	}

	if (results)
		slv2_values_free(results);

	return ret;
}


SLV2UIs
slv2_plugin_get_uis(SLV2Plugin plugin)
{
    const char* const query_str =
		"PREFIX uiext: <http://lv2plug.in/ns/extensions/ui#>\n"
		"SELECT DISTINCT ?uri ?type ?binary WHERE {\n"
		"<>   uiext:ui     ?uri .\n"
		"?uri a            ?type ;\n"
		"     uiext:binary ?binary .\n"
		"}\n";

	SLV2Results results = slv2_plugin_query_sparql(plugin, query_str);

	SLV2UIs result = slv2_uis_new();

	while (!librdf_query_results_finished(results->rdf_results)) {
		librdf_node* uri_node    = librdf_query_results_get_binding_value(results->rdf_results, 0);
		librdf_node* type_node   = librdf_query_results_get_binding_value(results->rdf_results, 1);
		librdf_node* binary_node = librdf_query_results_get_binding_value(results->rdf_results, 2);

		SLV2UI ui = slv2_ui_new(plugin->world,
				librdf_node_get_uri(uri_node),
				librdf_node_get_uri(type_node),
				librdf_node_get_uri(binary_node));

		raptor_sequence_push(result, ui);

		librdf_free_node(uri_node);
		librdf_free_node(type_node);
		librdf_free_node(binary_node);

		librdf_query_results_next(results->rdf_results);
	}

	slv2_results_free(results);

	if (slv2_uis_size(result) > 0) {
		return result;
	} else {
		slv2_uis_free(result);
		return NULL;
	}
}

