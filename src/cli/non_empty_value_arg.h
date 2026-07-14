#pragma once

#include <tclap/ArgException.h>
#include <tclap/ValueArg.h>

#include <cstddef>
#include <string>
#include <vector>

namespace cli
{
	/**
	 * A ValueArg that rejects an empty value instead of keeping its default.
	 *
	 * The stock extraction reads from a stringstream and stops at end-of-input,
	 * which for an empty string reads nothing, sets no failure bit and therefore
	 * throws nothing — leaving the default in place while the argument counts as
	 * set. `--cancel_install ""` then cancels the default session and
	 * `--is_app_state_bad ""` answers for the default slot, both silently. A
	 * script passing an unset variable hits this.
	 *
	 * A Constraint cannot cover it: constraints run after extraction and see the
	 * default, which is by construction a legal value.
	 *
	 * The base is left to do the matching and the value consumption; it advances
	 * the index onto the token it took, which is what makes the empty case
	 * visible here without duplicating any of that logic.
	 */
	template <class T>
	class NonEmptyValueArg : public TCLAP::ValueArg<T>
	{
		public:
		using TCLAP::ValueArg<T>::ValueArg;

		bool processArg(int *i, std::vector<std::string> &args) override
		{
			const int before = *i;
			const bool matched = TCLAP::ValueArg<T>::processArg(i, args);

			if (matched && *i > before && args[static_cast<std::size_t>(*i)].empty())
			{
				throw TCLAP::ArgParseException("value must not be empty",
											   this->toString());
			}

			return matched;
		}
	};
}
