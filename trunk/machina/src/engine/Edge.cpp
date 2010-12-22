/* This file is part of Machina.
 * Copyright (C) 2007-2009 David Robillard <http://drobilla.net>
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
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "raul/Atom.hpp"
#include "raul/AtomRDF.hpp"
#include "redlandmm/World.hpp"
#include "redlandmm/Model.hpp"
#include "machina/Node.hpp"
#include "machina/Edge.hpp"

namespace Machina {


void
Edge::write_state(Redland::Model& model)
{
	using namespace Raul;

	const Redland::Node& rdf_id = this->rdf_id(model.world());
	
	model.add_statement(
		rdf_id,
		"rdf:type",
		Redland::Node(model.world(), Redland::Node::RESOURCE, "machina:Edge"));

	SharedPtr<Node> tail = _tail.lock();
	SharedPtr<Node> head = _head;

	if (!tail || !head)
		return;

	assert(tail->rdf_id(model.world()) && head->rdf_id(model.world()));

	model.add_statement(rdf_id,
	                    "machina:tail",
	                    tail->rdf_id(model.world()));

	model.add_statement(rdf_id,
	                    "machina:head",
	                    head->rdf_id(model.world()));

	model.add_statement(
		rdf_id,
		"machina:probability",
		AtomRDF::atom_to_node(model, Atom(_probability.get())));
}


} // namespace Machina
