/*
 * maybe.h
 * Copyright (C) 2006  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef MAYBE_H
#define MAYBE_H

/**
 * \brief A template class either containing no value or a specific value.
 * This container is especially handy for types that do not have an isNull()
 * or isEmpty() (such as primitive types).
 *
 * Example: Returns the division of numer and denom if it is an integer
 * \code
 * Maybe<int> integer_divide(int numer, int denom) {
 *     if (numer % denom == 0) 
 *         return Maybe<int>(numer / denom);
 *     else
 *         return Maybe<int>();
 * }
 * \endcode
 */
template<class T> class Maybe 
{
public:
	/**
	 * \brief Constructs a Maybe container with no value.
	 */
	Maybe() : hasValue_(false) {}
	
	/**
	 * \brief Constructs a Maybe container with a value.
	 */
	Maybe(const T& value) : value_(value), hasValue_(true) {}

	/**
	 * \brief Checks whether this container has a value.
	 */
	bool hasValue() const { return hasValue_; }
	
	/**
	 * \brief Returns the value of the container.
	 */
	const T& value() const { return value_; }

private:
	T value_;
	bool hasValue_;
};

#endif
