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

#ifndef __gui_mrview_image_h__
#define __gui_mrview_image_h__

#include "image.h"
#include "types.h"
#include "gui/opengl/gl.h"
#include "gui/mrview/volume.h"
#include "interp/linear.h"
#include "interp/nearest.h"


namespace MR
{
  namespace GUI
  {

    class Projection;

    namespace MRView
    {

      class Window;

      namespace Tool
      {
        class ODF;
      }

      class ImageBase : public Volume
      {
        public:
          ImageBase (MR::Header&&);
          virtual ~ImageBase();

          void render2D (Displayable::Shader& shader_program, const Projection& projection, const int plane, const int slice);
          void render3D (Displayable::Shader& shader_program, const Projection& projection, const float depth);

          virtual void update_texture2D (const int plane, const int slice) = 0;
          virtual void update_texture3D() = 0;

          void get_axes (const int plane, int& x, int& y) const;

        protected:
          GL::Texture texture2D[3];
          std::vector<ssize_t> tex_positions;

      };

      class Image : public ImageBase
      {
        public:
          Image (MR::Header&&);

          void update_texture2D (const int plane, const int slice) override;
          void update_texture3D() override;

          void request_render_colourbar (DisplayableVisitor& visitor) override
          { if (show_colour_bar) visitor.render_image_colourbar (*this); }

          MR::Image<cfloat> image;

          cfloat trilinear_value (const Eigen::Vector3f&) const;
          cfloat nearest_neighbour_value (const Eigen::Vector3f&) const;

          const MR::Transform& transform() const { return linear_interp; }
          const std::vector<std::string>& comments() const { return _comments; }

        protected:
          mutable MR::Interp::Linear <MR::Image<cfloat>> linear_interp;
          mutable MR::Interp::Nearest<MR::Image<cfloat>> nearest_interp;
          friend class Tool::ODF;

        private:
          bool volume_unchanged ();
          bool format_unchanged ();
          size_t guess_colourmap () const;

          template <typename T> void copy_texture_3D ();
          void copy_texture_3D_complex ();

          std::vector<std::string> _comments;

      };


    }
  }
}

#endif

