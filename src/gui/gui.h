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

#ifndef __gui_app_h__
#define __gui_app_h__

#include "app.h"
#include "progressbar.h"
#include "file/config.h"
#include "gui/opengl/gl.h"
#include "gui/dialog/progress.h"
#include "gui/dialog/report_exception.h"
#include "gui/dialog/dicom.h"
#include "gui/dialog/file.h"

namespace MR
{
  namespace GUI
  {



    namespace Context
    {
#if QT_VERSION >= 0x050400
        std::pair<QOpenGLContext*,QSurface*> current();
        std::pair<QOpenGLContext*,QSurface*> get (QWidget*);
        std::pair<QOpenGLContext*,QSurface*> makeCurrent (QWidget*);
        void restore (std::pair<QOpenGLContext*,QSurface*>);
#else
        std::pair<int,int> current();
        std::pair<int,int> get (QWidget*);
        std::pair<int,int> makeCurrent (QWidget*);
        void restore (std::pair<int,int>);
#endif


      struct Grab {
        decltype (current()) previous_context;
        Grab (QWidget* window = nullptr) : previous_context (makeCurrent (window)) { }
        ~Grab () { restore (previous_context); }
      };
    }



    class App : public QObject {
      Q_OBJECT

      public:
        App (int& cmdline_argc, char** cmdline_argv) {
          application = this;
          ::MR::File::Config::init ();
          ::MR::GUI::GL::set_default_context ();
          QLocale::setDefault(QLocale::c());

          new QApplication (cmdline_argc, cmdline_argv);
          ::MR::App::init (cmdline_argc, cmdline_argv); 
          qApp->setAttribute (Qt::AA_DontCreateNativeWidgetSiblings);

          ::MR::ProgressInfo::display_func = Dialog::ProgressBar::display;
          ::MR::ProgressInfo::done_func = Dialog::ProgressBar::done;
          ::MR::File::Dicom::select_func = Dialog::select_dicom;
          ::MR::Exception::display_func = Dialog::display_exception;

          ::MR::App::check_overwrite_files_func = Dialog::File::check_overwrite_files_func;
        }

        ~App () {
          delete qApp;
        }

        static void set_main_window (QWidget* window);

        static QWidget* main_window;
        static App* application;

      public slots:
        void startProgressBar ();
        void displayProgressBar (QString text, int value, bool bounded);
        void doneProgressBar ();

    };

#ifndef NDEBUG
# define ASSERT_GL_CONTEXT_IS_CURRENT(window) { \
  auto __current_context = ::MR::GUI::Context::current(); \
  auto __expected_context = ::MR::GUI::Context::get (window); \
  assert (__current_context == __expected_context); \
}
#else 
# define ASSERT_GL_CONTEXT_IS_CURRENT(window)
#endif


  }
}

#endif

