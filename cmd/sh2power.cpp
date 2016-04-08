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


#include "command.h"
#include "image.h"
#include "math/SH.h"
#include "algo/threaded_loop.h"


using namespace MR;
using namespace App;

void usage () {
  DESCRIPTION
    + "compute the power contained within each harmonic degree.";

  ARGUMENTS
    + Argument ("SH", "the input spherical harmonics coefficients image.").type_image_in ()
    + Argument ("power", "the output power image.").type_image_out ();
}


void run () {
  auto SH_data = Image<float>::open(argument[0]);
  Math::SH::check (SH_data);

  Header power_header (SH_data);

  int lmax = Math::SH::LforN (SH_data.size (3));
  INFO ("calculating spherical harmonic power up to degree " + str (lmax));

  power_header.size (3) = 1 + lmax/2;
  power_header.datatype() = DataType::Float32;

  auto power_data = Image<float>::create(argument[1], power_header);

  auto f = [&] (decltype(power_data)& P, decltype(SH_data)& SH) {
    P.index(3) = 0;
    for (int l = 0; l <= lmax; l+=2) {
      float power = 0.0;
      for (int m = -l; m <= l; ++m) {
        SH.index(3) = Math::SH::index (l, m);
        float val = SH.value();
#ifdef USE_NON_ORTHONORMAL_SH_BASIS
        if (m != 0) 
          val *= Math::sqrt1_2;
#endif
        power += Math::pow2 (val);
      }
      P.value() = power / float (2*l+1);
      ++P.index(3);
    }
  };
  ThreadedLoop ("calculating SH power", SH_data, 0, 3)
    .run (f, power_data, SH_data);
}
