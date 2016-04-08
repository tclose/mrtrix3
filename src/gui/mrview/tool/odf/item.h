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

#ifndef __gui_mrview_tool_odf_item_h__
#define __gui_mrview_tool_odf_item_h__

#include <memory>

#include "dwi/shells.h"
#include "dwi/directions/set.h"
#include "gui/dwi/renderer.h"
#include "gui/mrview/gui_image.h"

namespace MR
{

  class Header;

  namespace GUI
  {
    namespace MRView
    {
      namespace Tool
      {



        class ODF_Item {
            typedef GUI::DWI::Renderer::mode_t mode_t;
          public:
            ODF_Item (MR::Header&& H, const float scale, const bool hide_negative, const bool color_by_direction);

            bool valid() const;

            MRView::Image image;
            mode_t mode;
            int lmax;
            float scale;
            bool hide_negative, color_by_direction;

            class DixelPlugin
            {
              public:
                enum dir_t { DW_SCHEME, HEADER, INTERNAL, NONE, FILE };

                DixelPlugin (const MR::Header&);

                void set_shell (size_t index);
                void set_header();
                void set_internal (const size_t n);
                void set_none();
                void set_from_file (const std::string& path);

                Eigen::VectorXf get_shell_data (const Eigen::VectorXf& values) const;

                size_t num_DW_shells() const;

                dir_t dir_type;
                Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic> header_dirs;
                Eigen::Matrix<double, Eigen::Dynamic, 4> grad;
                std::unique_ptr<MR::DWI::Shells> shells;
                size_t shell_index;
                std::unique_ptr<MR::DWI::Directions::Set> dirs;

            } dixel;
        };



      }
    }
  }
}

#endif





