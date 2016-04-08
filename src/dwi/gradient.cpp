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
#include "dwi/gradient.h"

namespace MR
{
  namespace DWI
  {

    using namespace App;
    using namespace Eigen;

    OptionGroup GradImportOptions (bool include_bvalue_scaling)
    {
      OptionGroup group ("DW gradient table import options");

      group 
        + Option ("grad",
            "specify the diffusion-weighted gradient scheme used in the acquisition. "
            "The program will normally attempt to use the encoding stored in the image "
            "header. This should be supplied as a 4xN text file with each line is in "
            "the format [ X Y Z b ], where [ X Y Z ] describe the direction of the "
            "applied gradient, and b gives the b-value in units of s/mm^2.")
        +   Argument ("encoding").type_file_in()

        + Option ("fslgrad",
            "specify the diffusion-weighted gradient scheme used in the acquisition in FSL bvecs/bvals format.")
        +   Argument ("bvecs").type_file_in()
        +   Argument ("bvals").type_file_in();

      if (include_bvalue_scaling) 
        group 
          + Option ("bvalue_scaling",
              "specifies whether the b-values should be scaled by the square of "
              "the corresponding DW gradient norm, as often required for "
              "multi-shell or DSI DW acquisition schemes. The default action can "
              "also be set in the MRtrix config file, under the BValueScaling entry. "
              "Valid choices are yes/no, true/false, 0/1 (default: true).")
          +   Argument ("mode").type_bool (true);

      return group;
    }


    OptionGroup GradExportOptions() 
    {
      return OptionGroup ("DW gradient table export options")

        + Option ("export_grad_mrtrix", "export the diffusion-weighted gradient table to file in MRtrix format")
        +   Argument ("path").type_file_out()

        + Option ("export_grad_fsl", "export the diffusion-weighted gradient table to files in FSL (bvecs / bvals) format")
        +   Argument ("bvecs_path").type_file_out()
        +   Argument ("bvals_path").type_file_out();
    }




    MatrixXd load_bvecs_bvals (const Header& header, const std::string& bvecs_path, const std::string& bvals_path)
    {
      auto bvals = load_matrix<> (bvals_path);
      auto bvecs = load_matrix<> (bvecs_path);

      if (bvals.rows() != 1) throw Exception ("bvals file must contain 1 row only");
      if (bvecs.rows() != 3) throw Exception ("bvecs file must contain exactly 3 rows");

      if (bvals.cols() != bvecs.cols() || bvals.cols() != header.size (3))
        throw Exception ("bvals and bvecs files must have same number of diffusion directions as DW-image");

      // account for the fact that bvecs are specified wrt original image axes,
      // which may have been re-ordered and/or inverted by MRtrix to match the
      // expected anatomical frame of reference:
      std::vector<size_t> order = Stride::order (header, 0, 3);
      MatrixXd G (bvecs.cols(), 3);
      for (ssize_t n = 0; n < G.rows(); ++n) {
        G(n,order[0]) = header.stride(order[0]) > 0 ? bvecs(0,n) : -bvecs(0,n);
        G(n,order[1]) = header.stride(order[1]) > 0 ? bvecs(1,n) : -bvecs(1,n);
        G(n,order[2]) = header.stride(order[2]) > 0 ? bvecs(2,n) : -bvecs(2,n);
      }

      // rotate gradients into scanner coordinate system:
      MatrixXd grad (G.rows(), 4);
      //Math::Matrix<ValueType> grad_G = grad.sub (0, grad.rows(), 0, 3);
      //Math::Matrix<ValueType> rotation = header.transform().sub (0,3,0,3);
      //Math::mult (grad_G, ValueType(0.0), ValueType(1.0), CblasNoTrans, G, CblasTrans, rotation);

      grad.leftCols<3>().transpose() = header.transform().rotation() * G.transpose();
      grad.col(3) = bvals.row(0);

      return grad;
    }





    void save_bvecs_bvals (const Header& header, const std::string& bvecs_path, const std::string& bvals_path)
    {
      const auto grad = header.parse_DW_scheme();

      // rotate vectors from scanner space to image space
      MatrixXd G = grad.leftCols<3>() * header.transform().rotation();

      // deal with FSL requiring gradient directions to coincide with data strides
      // also transpose matrices in preparation for file output
      auto order = Stride::order (header, 0, 3);
      MatrixXd bvecs (3, grad.rows());
      MatrixXd bvals (1, grad.rows());
      for (ssize_t n = 0; n < G.rows(); ++n) {
        bvecs(0,n) = header.stride(order[0]) > 0 ? G(n,order[0]) : -G(n,order[0]);
        bvecs(1,n) = header.stride(order[1]) > 0 ? G(n,order[1]) : -G(n,order[1]);
        bvecs(2,n) = header.stride(order[2]) > 0 ? G(n,order[2]) : -G(n,order[2]);
        bvals(0,n) = grad(n,3);
      }

      save_matrix (bvecs, bvecs_path);
      save_matrix (bvals, bvals_path);
    }






    MatrixXd get_DW_scheme (const Header& header)
    {
      DEBUG ("searching for suitable gradient encoding...");
      using namespace App;
      MatrixXd grad;

      try {
        const auto opt_mrtrix = get_options ("grad");
        if (opt_mrtrix.size())
          grad = load_matrix<> (opt_mrtrix[0][0]);
        const auto opt_fsl = get_options ("fslgrad");
        if (opt_fsl.size()) {
          if (opt_mrtrix.size())
            throw Exception ("Please provide diffusion gradient table using either -grad or -fslgrad option (not both)");
          grad = load_bvecs_bvals (header, opt_fsl[0][0], opt_fsl[0][1]);
        }
        if (!opt_mrtrix.size() && !opt_fsl.size())
          grad = header.parse_DW_scheme();
      }
      catch (Exception& E) {
        E.display (3);
        throw Exception ("error importing diffusion gradient table for image \"" + header.name() + "\"");
      }

      if (!grad.rows())
        return grad;

      if (grad.cols() < 4)
        throw Exception ("unexpected diffusion gradient table matrix dimensions");

      INFO ("found " + str (grad.rows()) + "x" + str (grad.cols()) + " diffusion gradient table");

      return grad;
    }





    MatrixXd get_valid_DW_scheme (const Header& header, bool nofail)
    {
      auto grad = get_DW_scheme (header);

      //CONF option: BValueScaling
      //CONF default: 1 (true)
      //CONF specifies whether the b-values should be scaled by the squared
      //CONF norm of the gradient vectors when loading a DW gradient scheme. 
      //CONF This is commonly required to correctly interpret images acquired
      //CONF on scanners that nominally only allow a single b-value, as the
      //CONF common workaround is to scale the gradient vectors to modulate
      //CONF the actual b-value. 
      bool scale_bvalues = true;
      auto opt = App::get_options ("bvalue_scaling");
      if (opt.size()) 
        scale_bvalues = opt[0][0];
      else
        scale_bvalues = File::Config::get_bool ("BValueScaling", scale_bvalues);

      if (scale_bvalues)
        scale_bvalue_by_G_squared (grad);

      try {
      normalise_grad (grad);
      check_DW_scheme (header, grad);
      }
      catch (Exception& e) {
        if (!nofail)
          throw;
      }
      return grad;
    }



    void export_grad_commandline (const Header& header) 
    {
      auto check = [](const Header& h) -> const Header& {
        if (h.keyval().find("dw_scheme") == h.keyval().end())
          throw Exception ("no gradient information found within image \"" + h.name() + "\"");
        return h;
      };

      auto opt = get_options ("export_grad_mrtrix");
      if (opt.size()) {
        File::OFStream out (opt[0][0]);
        out << check(header).keyval().find ("dw_scheme")->second << "\n";;
      }

      opt = get_options ("export_grad_fsl");
      if (opt.size()) 
        save_bvecs_bvals (check (header), opt[0][0], opt[0][1]);
    }




  }
}


