#pragma once

#include <tclap/CmdLine.h>
#include <tclap/UnlabeledValueArg.h>

#include <cstddef>
#include <string>
#include <vector>

#include "cli_classify.h"

namespace cli
{
	/**
	 * Install-path positional that declines option-like tokens.
	 *
	 * The stock UnlabeledValueArg accepts any non-blank token, and the parser
	 * tries the positional after every labelled arg, so an unknown flag would be
	 * absorbed as the install path. Declining leaves the token unmatched, which
	 * is what makes the parser report it as an unknown argument.
	 *
	 * The check is skipped once the parser is ignoring the rest ("--"), so the
	 * conventional escape for an operand that starts with '-' keeps working for
	 * any argument that is ignoreable.
	 */
	class PathValueArg : public TCLAP::UnlabeledValueArg<std::string>
	{
		public:
		PathValueArg(const std::string &name,
					 const std::string &desc,
					 bool req,
					 const std::string &value,
					 const std::string &type_desc,
					 TCLAP::CmdLineInterface &parser)
			: TCLAP::UnlabeledValueArg<std::string>(name, desc, req, value, type_desc),
			  parser(parser)
		{
		}

		bool processArg(int *i, std::vector<std::string> &args) override
		{
			const std::size_t idx = static_cast<std::size_t>(*i);
			if (!this->parser.ignoreRest() && cli::is_option_like(args[idx]))
			{
				return false;
			}

			return TCLAP::UnlabeledValueArg<std::string>::processArg(i, args);
		}

		private:
		TCLAP::CmdLineInterface &parser;
	};
}
