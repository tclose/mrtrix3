/*
 * Copyright (c) 2008-2016 the MRtrix3 contributors
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/
 * 
 * MRtrix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * For more details, see www.mrtrix.org
 * 
 */


#ifndef __debug_h__
#define __debug_h__

#include <iostream>
#include <string.h>

namespace MR {
  namespace App { 
    extern std::string NAME;
  } 
}

/** \defgroup debug Debugging 
 * \brief functions and macros provided to ease debugging. */

/** \addtogroup debug
 * @{ */

/** \brief Prints the current function, file and line number. */
#define TRACE \
  std::cerr << MR::App::NAME << ": at " << __FILE__ << ": " << __LINE__ << "\n";


/** \brief Prints a variable name and its value, followed by the function, file and line number. */
#define VAR(variable) \
  std::cerr << MR::App::NAME << " [" << __FILE__  << ": " << __LINE__ << "]: " << #variable << " = " << (variable) << "\n";


/** \brief Stops execution and prints current function, file and line number. 
  Remuses on user input (i.e. Return key). */
#define PAUSE { \
  std::cerr << MR::App::NAME << " [" << __FILE__  << ": " << __LINE__ << "]: paused (press any key to resume)\n"; \
  std::string __n__; std::getline (std::cin, __n__); \
}

/** @} */

#endif

