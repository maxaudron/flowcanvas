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

#ifndef MACHINA_EDGE_HPP
#define MACHINA_EDGE_HPP

#include <list>
#include <boost/utility.hpp>

#include "raul/DoubleBuffer.hpp"
#include "raul/SharedPtr.hpp"
#include "raul/WeakPtr.hpp"

#include "Action.hpp"
#include "Stateful.hpp"
#include "types.hpp"

namespace Machina {

class Node;

class Edge : public Stateful {
public:

	Edge(WeakPtr<Node> tail, SharedPtr<Node> head)
		: _probability(1.0f)
		, _tail(tail)
		, _head(head)
	{}

	void write_state(Redland::Model& model);

	WeakPtr<Node>   tail() { return _tail; }
	SharedPtr<Node> head() { return _head; }

	void set_tail(WeakPtr<Node> tail)   { _tail = tail; }
	void set_head(SharedPtr<Node> head) { _head = head; }

	inline float probability()            { return _probability.get(); }
	inline void  set_probability(float p) { _probability.set(p); }

private:
	Raul::DoubleBuffer<float> _probability;

	WeakPtr<Node>   _tail;
	SharedPtr<Node> _head;
};


} // namespace Machina

#endif // MACHINA_EDGE_HPP
