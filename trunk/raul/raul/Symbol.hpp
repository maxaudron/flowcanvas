/* This file is part of Raul.
 * Copyright (C) 2008-2009 Dave Robillard <http://drobilla.net>
 *
 * Raul is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Raul is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef RAUL_SYMBOL_HPP
#define RAUL_SYMBOL_HPP

#include <iostream>
#include <cctype>
#include <string>
#include <cstring>
#include <cassert>

namespace Raul {


/** A restricted string (C identifier, which is a component of a Path).
 *
 * A Symbol is a very restricted string suitable for use as an identifier.
 * It is a valid LV2 symbol, URI fragment, filename, OSC path fragment,
 * and identifier for most programming languages (including C).
 *
 * Valid characters are _, a-z, A-Z, 0-9, except the first character which
 * must not be 0-9.
 *
 * \ingroup raul
 */
class Symbol : public std::basic_string<char> {
public:

	/** Construct a Symbol from an std::string.
	 *
	 * It is a fatal error to construct a Symbol from an invalid string,
	 * use is_valid first to check.
	 */
	Symbol(const std::basic_string<char>& symbol)
		: std::basic_string<char>(symbol)
	{
		assert(is_valid(symbol));
	}


	/** Construct a Symbol from a C string.
	 *
	 * It is a fatal error to construct a Symbol from an invalid string,
	 * use is_valid first to check.
	 */
	Symbol(const char* csymbol)
		: std::basic_string<char>(csymbol)
	{
		assert(is_valid(csymbol));
	}

	static bool is_valid(const std::basic_string<char>& path);

	static std::string symbolify(const std::basic_string<char>& str);
};


} // namespace Raul

#endif // RAUL_SYMBOL_HPP
